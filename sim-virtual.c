// 2220373   Giovana Nogueira
// 2211667   Livian Essvein

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned int addr;   
    char rw;             
    unsigned int page;   
} Access;

typedef struct {
    int valid;                   /* quadro ocupado? */
    unsigned int page;       
    int R;                      
    int M;                       
    unsigned long last_access;  
} Frame;

int read_log(const char *filename, Access **accesses_out,
             size_t *count_out, int s_bits) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Erro ao abrir arquivo de entrada: %s\n", filename);
        return -1;
    }

    size_t capacity = 1024;
    size_t count = 0;
    Access *vec = (Access *) malloc(capacity * sizeof(Access));
    if (!vec) {
        fprintf(stderr, "Erro de alocacao de memoria para vetor de acessos\n");
        fclose(f);
        return -1;
    }

    unsigned int addr;
    char rw;
    while (fscanf(f, "%x %c", &addr, &rw) == 2) {
        if (count == capacity) {
            capacity *= 2;
            Access *tmp = (Access *) realloc(vec, capacity * sizeof(Access));
            if (!tmp) {
                fprintf(stderr, "Erro de realocacao de memoria\n");
                free(vec);
                fclose(f);
                return -1;
            }
            vec = tmp;
        }
        vec[count].addr = addr;
        vec[count].rw   = rw;
        vec[count].page = addr >> s_bits;
        count++;
    }

    fclose(f);
    *accesses_out = vec;
    *count_out    = count;
    return 0;
}

Frame *init_frames(size_t nframes) {
    Frame *frames = (Frame *) malloc(nframes * sizeof(Frame));
    if (!frames) {
        fprintf(stderr, "Erro de alocacao de memoria para frames\n");
        return NULL;
    }
    for (size_t i = 0; i < nframes; ++i) {
        frames[i].valid = 0;
        frames[i].page  = 0;
        frames[i].R     = 0;
        frames[i].M     = 0;
        frames[i].last_access = 0;
    }
    return frames;
}

int *init_page_table(unsigned int num_pages) {
    int *pt = (int *) malloc(num_pages * sizeof(int));
    if (!pt) {
        fprintf(stderr, "Erro de alocacao de memoria para tabela de paginas\n");
        return NULL;
    }
    for (unsigned int i = 0; i < num_pages; ++i) {
        pt[i] = -1;
    }
    return pt;
}

int is_lru(const char *alg) {
    return !strcmp(alg, "LRU") || !strcmp(alg, "lru");
}

int is_nru(const char *alg) {
    return !strcmp(alg, "NRU") || !strcmp(alg, "nru");
}

int is_opt(const char *alg) {
    return !strcmp(alg, "OPT") || !strcmp(alg, "opt") ||
           !strcmp(alg, "OTIMO") || !strcmp(alg, "otimo") ||
           !strcmp(alg, "ÓTIMO") || !strcmp(alg, "ótimo");
}

size_t choose_opt_victim(Frame *frames, size_t nframes,
                         const Access *A, size_t n,
                         size_t current_index) {
    size_t victim = 0;
    size_t farthest = current_index;

    for (size_t i = 0; i < nframes; ++i) {
        unsigned int page = frames[i].page;
        size_t j;
        for (j = current_index + 1; j < n; ++j) {
            if (A[j].page == page) {
                break;
            }
        }
        /* Se a página não é mais usada no futuro, é a melhor vítima */
        if (j == n) {
            return i;
        }
        /* Senão, escolhe quem será usada mais distante no futuro */
        if (j > farthest) {
            farthest = j;
            victim = i;
        }
    }
    return victim;
}

void simulate_LRU(const Access *A, size_t n,
                  size_t nframes, int s_bits,
                  unsigned long long *page_faults,
                  unsigned long long *writes_back) {

    (void)s_bits; 

    unsigned int num_pages = 1U << (32 - s_bits);

    Frame *frames = init_frames(nframes);
    if (!frames) return;

    int *page_table = init_page_table(num_pages);
    if (!page_table) {
        free(frames);
        return;
    }

    unsigned long time = 0;
    *page_faults = 0;
    *writes_back = 0;

    for (size_t i = 0; i < n; ++i) {
        time++;
        unsigned int page = A[i].page;
        char rw = A[i].rw;

        int frame_index = page_table[page];

        if (frame_index != -1 && frames[frame_index].valid) {
            frames[frame_index].R = 1;
            if (rw == 'W' || rw == 'w') {
                frames[frame_index].M = 1;
            }
            frames[frame_index].last_access = time;
        } else {
            (*page_faults)++;

            int free_index = -1;
            for (size_t f = 0; f < nframes; ++f) {
                if (!frames[f].valid) {
                    free_index = (int)f;
                    break;
                }
            }

            int target;
            if (free_index != -1) {
                target = free_index;
            } else {
                unsigned long oldest_time = (unsigned long) -1;
                int victim = -1;
                for (size_t f = 0; f < nframes; ++f) {
                    if (frames[f].last_access < oldest_time) {
                        oldest_time = frames[f].last_access;
                        victim = (int)f;
                    }
                }
                target = victim;

                if (frames[target].M) {
                    (*writes_back)++;
                }

                page_table[frames[target].page] = -1;
            }

            frames[target].valid = 1;
            frames[target].page  = page;
            frames[target].R     = 1;
            frames[target].M     = (rw == 'W' || rw == 'w') ? 1 : 0;
            frames[target].last_access = time;
            page_table[page] = target;
        }
    }

    free(frames);
    free(page_table);
}

void simulate_NRU(const Access *A, size_t n,
                  size_t nframes, int s_bits,
                  unsigned long long *page_faults,
                  unsigned long long *writes_back) {

    (void)s_bits; 

    unsigned int num_pages = 1U << (32 - s_bits);

    Frame *frames = init_frames(nframes);
    if (!frames) return;

    int *page_table = init_page_table(num_pages);
    if (!page_table) {
        free(frames);
        return;
    }

    unsigned long time = 0;
    *page_faults = 0;
    *writes_back = 0;

    const unsigned long NRU_RESET_INTERVAL = 1000;

    for (size_t i = 0; i < n; ++i) {
        time++;
        unsigned int page = A[i].page;
        char rw = A[i].rw;

        if (time % NRU_RESET_INTERVAL == 0) {
            for (size_t f = 0; f < nframes; ++f) {
                if (frames[f].valid) {
                    frames[f].R = 0;
                }
            }
        }

        int frame_index = page_table[page];

        if (frame_index != -1 && frames[frame_index].valid) {
            frames[frame_index].R = 1;
            if (rw == 'W' || rw == 'w') {
                frames[frame_index].M = 1;
            }
            frames[frame_index].last_access = time;
        } else {
            (*page_faults)++;

            int free_index = -1;
            for (size_t f = 0; f < nframes; ++f) {
                if (!frames[f].valid) {
                    free_index = (int)f;
                    break;
                }
            }

            int target;
            if (free_index != -1) {
                target = free_index;
            } else {
                int best_class = 4;
                int victim = -1;

                for (size_t f = 0; f < nframes; ++f) {
                    if (!frames[f].valid) continue; 
                    int R = frames[f].R;
                    int M = frames[f].M;
                    int class = 2 * R + M; 

                    if (class < best_class) {
                        best_class = class;
                        victim = (int)f;
                        if (class == 0) {
                            break;
                        }
                    }
                }
                target = victim;

                if (frames[target].M) {
                    (*writes_back)++;
                }
                page_table[frames[target].page] = -1;
            }

            frames[target].valid = 1;
            frames[target].page  = page;
            frames[target].R     = 1;
            frames[target].M     = (rw == 'W' || rw == 'w') ? 1 : 0;
            frames[target].last_access = time;
            page_table[page] = target;
        }
    }

    free(frames);
    free(page_table);
}

void simulate_OPT(const Access *A, size_t n,
                  size_t nframes, int s_bits,
                  unsigned long long *page_faults,
                  unsigned long long *writes_back) {

    (void)s_bits;

    unsigned int num_pages = 1U << (32 - s_bits);

    Frame *frames = init_frames(nframes);
    if (!frames) return;

    int *page_table = init_page_table(num_pages);
    if (!page_table) {
        free(frames);
        return;
    }

    unsigned long time = 0;
    *page_faults = 0;
    *writes_back = 0;

    for (size_t i = 0; i < n; ++i) {
        time++;
        unsigned int page = A[i].page;
        char rw = A[i].rw;

        int frame_index = page_table[page];

        if (frame_index != -1 && frames[frame_index].valid) {
            frames[frame_index].R = 1;
            if (rw == 'W' || rw == 'w') {
                frames[frame_index].M = 1;
            }
            frames[frame_index].last_access = time;
        } else {
            (*page_faults)++;

            int free_index = -1;
            for (size_t f = 0; f < nframes; ++f) {
                if (!frames[f].valid) {
                    free_index = (int)f;
                    break;
                }
            }

            int target;
            if (free_index != -1) {
                target = free_index;
            } else {
                target = (int) choose_opt_victim(frames, nframes, A, n, i);

                if (frames[target].M) {
                    (*writes_back)++;
                }
                page_table[frames[target].page] = -1;
            }

            frames[target].valid = 1;
            frames[target].page  = page;
            frames[target].R     = 1;
            frames[target].M     = (rw == 'W' || rw == 'w') ? 1 : 0;
            frames[target].last_access = time;
            page_table[page] = target;
        }
    }
    free(frames);
    free(page_table);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr,
            "Uso: %s <algoritmo> <arquivo.log> <tam_pagina_KB> <mem_fisica_MB>\n"
            "  algoritmo: LRU | NRU | OTIMO | OPT\n"
            "  tam_pagina_KB: 8 | 16 | 32\n"
            "  mem_fisica_MB: 1 | 2 | 4\n",
            argv[0]);
        return 1;
    }

    const char *alg = argv[1];
    const char *filename = argv[2];

    int page_kb = atoi(argv[3]);
    int mem_mb  = atoi(argv[4]);

    if (page_kb != 8 && page_kb != 16 && page_kb != 32) {
        fprintf(stderr, "Tamanho de pagina invalido: %d (use 8, 16 ou 32 KB)\n", page_kb);
        return 1;
    }

    if (mem_mb != 1 && mem_mb != 2 && mem_mb != 4) {
        fprintf(stderr, "Tamanho de memoria fisica invalido: %d (use 1, 2 ou 4 MB)\n", mem_mb);
        return 1;
    }

    if (!is_lru(alg) && !is_nru(alg) && !is_opt(alg)) {
        fprintf(stderr, "Algoritmo invalido: %s (use LRU, NRU, OTIMO ou OPT)\n", alg);
        return 1;
    }

    int s_bits;
    if (page_kb == 8)      s_bits = 13; /* 8 KB  = 2^13  */
    else if (page_kb == 16) s_bits = 14; /* 16 KB = 2^14  */
    else                    s_bits = 15; /* 32 KB = 2^15  */

    Access *accesses = NULL;
    size_t n_accesses = 0;

    if (read_log(filename, &accesses, &n_accesses, s_bits) != 0) {
        return 1;
    }

    if (n_accesses == 0) {
        fprintf(stderr, "Arquivo de log vazio ou invalido: %s\n", filename);
        free(accesses);
        return 1;
    }

    size_t page_bytes = (size_t)page_kb * 1024;
    size_t mem_bytes  = (size_t)mem_mb  * 1024 * 1024;
    size_t nframes    = mem_bytes / page_bytes;

    if (nframes == 0) {
        fprintf(stderr, "Configuracao invalida: nenhum quadro disponivel\n");
        free(accesses);
        return 1;
    }

    unsigned long long page_faults = 0;
    unsigned long long writes_back = 0;

    printf("Executando o simulador...\n\n");
    printf("Arquivo de entrada: %s\n", filename);
    printf("Tamanho da memoria fisica: %d MB\n", mem_mb);
    printf("Tamanho das paginas: %d KB\n", page_kb);
    printf("Numero de quadros: %zu\n", nframes);
    printf("Algoritmo de substituicao: %s\n\n", alg);

    if (is_lru(alg)) {
        simulate_LRU(accesses, n_accesses, nframes, s_bits,
                     &page_faults, &writes_back);
    } else if (is_nru(alg)) {
        simulate_NRU(accesses, n_accesses, nframes, s_bits,
                     &page_faults, &writes_back);
    } else if (is_opt(alg)) {
        simulate_OPT(accesses, n_accesses, nframes, s_bits,
                     &page_faults, &writes_back);
    }

    printf("Numero de Faltas de Paginas: %llu\n", page_faults);
    printf("Numero de Paginas Escritas: %llu\n", writes_back);

    free(accesses);
    return 0;
}
