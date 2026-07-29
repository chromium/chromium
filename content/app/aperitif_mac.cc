// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Apéritif is by necessity linked to only a minimal number of libraries.
// Code that executes in this context has the capability of compromising the
// integrity of the sandbox by acquiring resources that would remain available
// to an unprivileged process. Consult with ui/base/cocoa/OWNERS before
// adding new dependencies to Aperitif.

#include "content/public/app/aperitif_mac.h"

#include <crt_externs.h>
#include <mach-o/dyld.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/cdefs.h>

#include "base/allocator/early_zone_registration_apple.h"
#include "base/compiler_specific.h"
#include "sandbox/mac/seatbelt_exec.h"

extern "C" {
// abort_report_np() records the message in a special section that both the
// system CrashReporter and Crashpad collect in crash reports. Using a Crashpad
// `Annotation` would be preferable, but this module cannot depend on Crashpad
// directly.
void abort_report_np(const char* fmt, ...) __abortlike __printflike(1, 2);
}

namespace content::aperitif {
namespace {

[[noreturn]] [[gnu::format(printf, 1, 2)]] void FatalError(const char* format,
                                                           ...) {
  va_list valist;
  va_start(valist, format);
  char message[4096];
  int rv = UNSAFE_TODO(vsnprintf(message, sizeof(message), format, valist));
  va_end(valist);
  if (rv >= 0) {
    UNSAFE_TODO(fprintf(stderr, "aperitif: %s\n", message));
    fflush(stderr);
    UNSAFE_TODO(abort_report_np("aperitif: %s", message));
  }
  abort();
}

// This class exists to provide a static initializer that will initialize the
// sandbox when run (e.g. when the Aperitif library is loaded). It also tracks
// whether Apéritif has completed initialization.
class Initializer {
 public:
  Initializer() {
    partition_alloc::EarlyMallocZoneRegistration();

    InitializeSandbox();

    initialized_ = true;
  }

  ~Initializer() = default;

  bool initialized() const { return initialized_; }

 private:
  void InitializeSandbox() {
    char exec_path[PATH_MAX];
    uint32_t exec_path_size = sizeof(exec_path);
    int rv = _NSGetExecutablePath(exec_path, &exec_path_size);
    if (rv != 0) {
      FatalError("_NSGetExecutablePath: get path failed");
    }

    sandbox::SeatbeltExecServer::CreateFromArgumentsResult seatbelt =
        sandbox::SeatbeltExecServer::CreateFromArguments(
            exec_path, *_NSGetArgc(), *_NSGetArgv());
    if (seatbelt.sandbox_required) {
      if (!seatbelt.server) {
        FatalError("Failed to create seatbelt sandbox server");
      }
      if (!seatbelt.server->InitializeSandbox()) {
        FatalError("Failed to initialize sandbox");
      }
    }
  }

  bool initialized_ = false;
};

Initializer g_aperitif;

}  // namespace
}  // namespace content::aperitif

extern "C" {

void AperitifCheckInitialized() {
  using content::aperitif::FatalError;
  using content::aperitif::g_aperitif;

  if (!g_aperitif.initialized()) {
    FatalError("AperitifCheckInitialized: Apéritif has not been initialized");
  }
}

}  // extern "C"
