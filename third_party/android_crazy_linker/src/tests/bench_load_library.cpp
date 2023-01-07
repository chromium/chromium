// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A crazy linker test to:
// - Load a library (libfoo.so) with the linker.
// - Find the address of the "Foo" function in it.
// - Call the function.
// - Close the library.

#include <crazy_linker.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

static void Panic(const char* fmt, ...) {
  va_list args;
  fprintf(stderr, "PANIC: ");
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  exit(1);
}

static double now_ms() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (ts.tv_sec * 1000.) + (ts.tv_nsec / 1000000.);
}

static void drop_caches() {
  int fd = open("/proc/sys/vm/drop_caches", O_RDWR);
  if (fd < 0) {
    fprintf(stderr,
            "Could not drop caches! Please run this program as root!\n");
    return;
  }
  write(fd, "3\n", 2);
  close(fd);
}

class ScopedTimer {
 public:
  ScopedTimer(const char* name) {
    name_ = name;
    start_ms_ = now_ms();
  }

  ~ScopedTimer() {
    double elapsed_ms = now_ms() - start_ms_;
    printf("Timer %s: %.1f\n", name_, elapsed_ms);
  }

 private:
  const char* name_;
  double start_ms_;
};

int main(int argc, char** argv) {
  const char* library_path = "libcrazy_linker_tests_libfoo.so";
  if (argc >= 2)
    library_path = argv[1];

  { ScopedTimer null_timer("empty"); }

  // Load the library with dlopen().
  void* lib;
  drop_caches();
  {
    ScopedTimer timer("dlopen");
    lib = dlopen(library_path, RTLD_NOW);
  }
  if (!lib)
    Panic("Could not load library with dlopen(): %s\n", dlerror());

  dlclose(lib);

  crazy_library_t* library;
  crazy_context_t* context = crazy_context_create();

  // Ensure the program looks in its own directory too.
  crazy_add_search_path_for_address(reinterpret_cast<void*>(&main));

  // Load the library with the crazy linker.
  drop_caches();
  {
    ScopedTimer timer("crazy_linker");
    // Load libfoo.so
    if (!crazy_library_open(&library, library_path, context)) {
      Panic("Could not open library: %s\n", crazy_context_get_error(context));
    }
  }
  crazy_library_close(library);

  // Load the library with the crazy linker. Preload libOpenSLES.so
  drop_caches();
  void* sles_lib = dlopen("libOpenSLES.so", RTLD_NOW);
  {
    ScopedTimer timer("crazy_linker (preload libOpenSLES.so)");
    // Load libfoo.so
    if (!crazy_library_open(&library, library_path, context)) {
      Panic("Could not open library: %s\n", crazy_context_get_error(context));
    }
  }
  crazy_library_close(library);
  dlclose(sles_lib);

  // Load the library with the crazy linker. Preload libOpenSLES.so
  {
    drop_caches();
    void* sys1_lib = dlopen("libandroid.so", RTLD_NOW);
    void* sys2_lib = dlopen("libjnigraphics.so", RTLD_NOW);
    void* sys3_lib = dlopen("libOpenSLES.so", RTLD_NOW);
    {
      ScopedTimer timer("crazy_linker (preload 3 system libs)");
      // Load libfoo.so
      if (!crazy_library_open(&library, library_path, context)) {
        Panic("Could not open library: %s\n", crazy_context_get_error(context));
      }
    }
    crazy_library_close(library);
    dlclose(sys3_lib);
    dlclose(sys2_lib);
    dlclose(sys1_lib);
  }

  // Load the library with the crazy linker. Create a shared RELRO as well.
  drop_caches();
  {
    ScopedTimer timer("crazy_linker (with RELRO)");
    // Load libfoo.so
    if (!crazy_library_open(&library, library_path, context)) {
      Panic("Could not open library: %s\n", crazy_context_get_error(context));
    }

    size_t relro_start = 0;
    size_t relro_size = 0;
    int relro_fd = -1;
    if (!crazy_library_create_shared_relro(library, context,
                                           0 /* load_address */, &relro_start,
                                           &relro_size, &relro_fd)) {
      Panic("Could not create shared RELRO: %s\n",
            crazy_context_get_error(context));
    }
    close(relro_fd);
  }
  crazy_library_close(library);

  printf("OK\n");
  return 0;
}
