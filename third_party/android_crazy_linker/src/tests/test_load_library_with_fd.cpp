// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A crazy linker test to:
// - Load a library (libfoo.so) with the linker.
// - Find the address of the "Foo" function in it.
// - Call the function.
// - Close the library.
#include <crazy_linker.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <string>

#include "test_util.h"

typedef void (*FunctionPtr)();

#ifndef LIB_NAME
#define LIB_NAME "libcrazy_linker_tests_libfoo.so"
#endif

std::string GetProgramDirectory(const char* argv0) {
  const char* sep = strrchr(argv0, '/');
  if (!sep) {
    return ".";
  }
  return std::string(argv0, sep - argv0);
}

int main(int argc, const char** argv) {
  crazy_context_t* context = crazy_context_create();
  crazy_library_t* library;

  // DEBUG
  crazy_context_set_load_address(context, 0x20000000);

  // Assume library file is in the same directory as this executable.
  std::string program_dir = GetProgramDirectory(argv[0]);
  std::string lib_path = program_dir + "/" LIB_NAME;
  int fd = open(lib_path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    Panic("Could not find library file %s: %s\n", LIB_NAME, strerror(errno));
  }

  crazy_context_set_library_fd(context, fd);

  int context_fd = crazy_context_get_library_fd(context);
  if (context_fd != fd)
    Panic("Invalid context fd %d (expected %d)\n", context_fd, fd);

  // Load libfoo.so
  if (!crazy_library_open(&library, LIB_NAME, context)) {
    Panic("Could not open library: %s\n", crazy_context_get_error(context));
  }

  // Find the "Foo" symbol.
  FunctionPtr foo_func;
  if (!crazy_library_find_symbol(library, "Foo",
                                 reinterpret_cast<void**>(&foo_func))) {
    Panic("Could not find 'Foo' in %s\n", LIB_NAME);
  }

  // Call it.
  (*foo_func)();

  // Close the library.
  printf("Closing %s\n", LIB_NAME);
  crazy_library_close(library);

  // Check that the descriptor is not closed, by trying to read from it.
  char header[4];
  if (TEMP_FAILURE_RETRY(lseek(fd, 0, SEEK_SET)) < 0 ||
      TEMP_FAILURE_RETRY(read(fd, header, 4)) < 0) {
    Panic("Could not read from file descriptor after library close!: %s",
          strerror(errno));
  }
  close(fd);

  context_fd = crazy_context_get_library_fd(context);
  if (context_fd != -1)
    Panic("Invalid context fd after library load %d (expected -1)", context_fd);

  crazy_context_destroy(context);
  printf("OK\n");
  return 0;
}
