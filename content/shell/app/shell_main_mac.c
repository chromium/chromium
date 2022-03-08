// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <errno.h>
#include <libgen.h>
#include <mach-o/dyld.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

#include "content/public/app/aperitif_mac.h"

typedef int (*ContentMainPtr)(int, char**);

int main(int argc, char* argv[]) {
  AperitifInitializePartitionAlloc();

  char exec_path[PATH_MAX];
  uint32_t exec_path_size = sizeof(exec_path);
  int rv = _NSGetExecutablePath(exec_path, &exec_path_size);
  if (rv != 0) {
    AperitifFatalError("_NSGetExecutablePath: get path failed.");
  }

#if defined(HELPER_EXECUTABLE)
  AperitifInitializeSandbox(exec_path, argc, (const char**)argv);

  // The Helper app is in the versioned framework directory, so just go up to
  // the version folder to locate the dylib.
  static const char rel_path[] = "../../../../" SHELL_PRODUCT_NAME " Framework";
#else
  static const char rel_path[] =
      "../Frameworks/" SHELL_PRODUCT_NAME
      " Framework.framework/" SHELL_PRODUCT_NAME " Framework";
#endif  // defined(HELPER_EXECUTABLE)

  // Slice off the last part of the main executable path, and append the
  // version framework information.
  const char* parent_dir = dirname(exec_path);
  if (!parent_dir) {
    AperitifFatalError("dirname %s: %s", exec_path, strerror(errno));
  }

  char framework_path[PATH_MAX];
  rv = snprintf(framework_path, sizeof(framework_path), "%s/%s", parent_dir,
                rel_path);
  if (rv < 0 || (size_t)rv >= sizeof(framework_path)) {
    AperitifFatalError("snprintf: %d", rv);
  }

  void* library = dlopen(framework_path, RTLD_LAZY | RTLD_LOCAL | RTLD_FIRST);
  if (!library) {
    AperitifFatalError("dlopen %s: %s", framework_path, dlerror());
  }

  const ContentMainPtr content_main = dlsym(library, "ContentMain");
  if (!content_main) {
    AperitifFatalError("dlsym ContentMain: %s", dlerror());
  }
  rv = content_main(argc, argv);

  // exit, don't return from main, to avoid the apparent removal of main from
  // stack backtraces under tail call optimization.
  exit(rv);
}
