// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Same as test_relro_sharing.cpp, but uses two libraries at the same
// time (libfoo_with_relro.so and libbar_with_relro.so), each one of
// them gets its own shared RELRO.

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

#include <crazy_linker.h>

#include "test_util.h"

typedef void (*FunctionPtr)();

#define LIB_NAME "libcrazy_linker_tests_libfoo_with_relro.so"
#define LIB2_NAME "libcrazy_linker_tests_libbar_with_relro.so"

int main() {
  crazy_context_t* context = crazy_context_create();

  RelroLibrary foo;
  RelroLibrary bar;

  crazy_add_search_path_for_address((void*)&main);

  // Load libfoo_with_relro.so
  crazy_context_set_load_address(context, 0x20000000);
  foo.Init(LIB_NAME, context);

  crazy_context_set_load_address(context, 0x20800000);
  bar.Init(LIB2_NAME, context);

  printf("Libraries loaded\n");

  int pipes[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipes) < 0)
    Panic("Could not create socket pair: %s", strerror(errno));

  pid_t child = fork();
  if (child < 0)
    Panic("Could not fork test program!");

  if (child == 0) {
    // In the child.
    printf("Child waiting for foo relro fd\n");

    foo.ReceiveRelroInfo(pipes[0]);
    foo.UseSharedRelro(context);

    printf("Child waiting for bar relro fd\n");
    bar.ReceiveRelroInfo(pipes[0]);
    bar.UseSharedRelro(context);

    printf("RELROs used in child process\n");

    CheckRelroMaps(2);

    FunctionPtr bar_func;
    if (!crazy_library_find_symbol(
             bar.library, "Bar", reinterpret_cast<void**>(&bar_func)))
      Panic("Could not find 'Bar' in library");

    printf("Calling Bar()\n");
    (*bar_func)();

    printf("Bar() called, exiting\n");

    exit(0);

  } else {
    // In the parent.

    printf("Parent enabling foo RELRO sharing\n");

    foo.EnableSharedRelro(context);
    foo.SendRelroInfo(pipes[1]);

    printf("Parent enabling bar RELRO sharing\n");

    bar.EnableSharedRelro(context);
    bar.SendRelroInfo(pipes[1]);

    printf("RELROs enabled and sent to child\n");

    CheckRelroMaps(2);

    printf("Parent waiting for child\n");

    // Wait for child to complete.
    int status;
    waitpid(child, &status, 0);

    if (WIFSIGNALED(status))
      Panic("Child terminated by signal!!\n");
    else if (WIFEXITED(status)) {
      int child_status = WEXITSTATUS(status);
      if (child_status != 0)
        Panic("Child terminated with status=%d\n", child_status);
    } else
      Panic("Child exited for unknown reason!!\n");
  }

  printf("Closing libraries\n");
  bar.Close();
  foo.Close();

  crazy_context_destroy(context);
  return 0;
}
