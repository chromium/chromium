// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A crazy linker test to:
// - Load a library (libfoo.so) with the linker.
// - Find the address of the "Foo" function in it.
// - Call the function.
// - Close the library.

#include <errno.h>
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

int main() {
  crazy_context_t* context = crazy_context_create();

  RelroLibrary foo;

  // Load at fixed address to simplify testing.
  crazy_context_set_load_address(context, 0x20000000);
  foo.Init(LIB_NAME, context);

  printf("Library loaded\n");

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

    printf("RELRO used in child process\n");

    CheckRelroMaps(1);

    FunctionPtr foo_func;
    if (!crazy_library_find_symbol(
             foo.library, "Foo", reinterpret_cast<void**>(&foo_func)))
      Panic("Could not find 'Foo' in library");

    printf("Calling Foo()\n");
    (*foo_func)();

    printf("Foo called, exiting\n");

    exit(0);

  } else {
    // In the parent.

    printf("Parent enabling foo RELRO sharing\n");

    foo.EnableSharedRelro(context);
    foo.SendRelroInfo(pipes[1]);

    printf("RELRO enabled and sent to child\n");

    CheckRelroMaps(1);

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

  crazy_context_destroy(context);
  return 0;
}
