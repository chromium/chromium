// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <errno.h>
#include <libgen.h>
#include <mach-o/dyld.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

#include "base/allocator/early_zone_registration_mac.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"

extern "C" {
// abort_report_np() records the message in a special section that both the
// system CrashReporter and Crashpad collect in crash reports. Using a Crashpad
// Annotation would be preferable, but this executable cannot depend on
// Crashpad directly.
void abort_report_np(const char* fmt, ...);
}

namespace {

typedef int (*ChromeMainPtr)(int, char**);

// Path to the framework containing the "ChromeMain" exported function.
const char kFrameworkPath[] = "@rpath/" CHROME_MAIN_FRAMEWORK_NAME
                              ".framework/" CHROME_MAIN_FRAMEWORK_NAME;

// Report an error and terminate the application.
[[noreturn]] void FatalError(const char* format, ...)
    __attribute__((__format__(printf, 1, 2)));

// Helper class to load dynamic library and lookup symbol. The library is
// never unloaded, and the code terminate the application in case of error.
class Library {
 public:
  static Library Load(const std::string& path) {
    void* handle = dlopen(path.data(), RTLD_NOW | RTLD_LOCAL | RTLD_FIRST);
    if (!handle) {
      FatalError("dlopen %s: %s.", path.data(), dlerror());
    }
    return Library(handle);
  }

  template <typename Prototype>
  Prototype LoadSymbol(const std::string& name) {
    void* symbol = dlsym(handle_, name.data());
    if (!symbol) {
      FatalError("dlsym %s: %s.", name.data(), dlerror());
    }
    return reinterpret_cast<Prototype>(symbol);
  }

 private:
  explicit Library(void* handle) : handle_(handle) {}

  void* const handle_;
};

void FatalError(const char* format, ...) {
  va_list valist;
  va_start(valist, format);
  char message[4096];
  if (vsnprintf(message, sizeof(message), format, valist) >= 0) {
    fputs(message, stderr);
    abort_report_np("%s", message);
  }
  va_end(valist);
  abort();
}

}  // namespace

int main(int argc, char* argv[], char* envp[]) {
#if BUILDFLAG(USE_PARTITION_ALLOC)
  // Perform malloc zone registration before loading any dependency as this
  // needs to be called before any thread creation which happens during the
  // initialisation of some of the runtime libraries.
  partition_alloc::EarlyMallocZoneRegistration();
#endif  // BUILDFLAG(USE_PARTITION_ALLOC)

  // Load the main application library and search for the entry point.
  Library framework = Library::Load(kFrameworkPath);
  const auto chrome_main = framework.LoadSymbol<ChromeMainPtr>("ChromeMain");

  // exit, don't return from main, to avoid the apparent removal of main from
  // stack backtraces under tail call optimization.
  exit(chrome_main(argc, argv));
}
