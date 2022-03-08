// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Apéritif is by necessity linked to only a minimal number of libraries.
// Code that executes in this context has the capability of compromising the
// integrity of the sandbox by acquiring resources that would remain available
// to an unprivileged process. Consult with security-dev@chromium.org before
// adding new dependencies to Aperitif.

#include "content/public/app/aperitif_mac.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/cdefs.h>

#include "base/allocator/early_zone_registration_mac.h"
#include "sandbox/mac/seatbelt_exec.h"

extern "C" {

// abort_report_np() records the message in a special section that both the
// system CrashReporter and Crashpad collect in crash reports. Using a Crashpad
// `Annotation` would be preferable, but this module cannot depend on Crashpad
// directly.
void abort_report_np(const char* fmt, ...) __abortlike __printflike(1, 2);

void AperitifFatalError(const char* format, ...) {
  va_list valist;
  va_start(valist, format);
  char message[4096];
  int rv = vsnprintf(message, sizeof(message), format, valist);
  va_end(valist);
  if (rv >= 0) {
    fprintf(stderr, "aperitif: %s\n", message);
    fflush(stderr);
    abort_report_np("aperitif: %s", message);
  }
  abort();
}

void AperitifInitializePartitionAlloc() {
  partition_alloc::EarlyMallocZoneRegistration();
}

void AperitifInitializeSandbox(const char* executable_path,
                               int argc,
                               const char* const argv[]) {
  sandbox::SeatbeltExecServer::CreateFromArgumentsResult seatbelt =
      sandbox::SeatbeltExecServer::CreateFromArguments(executable_path, argc,
                                                       argv);
  if (seatbelt.sandbox_required) {
    if (!seatbelt.server) {
      AperitifFatalError("Failed to create seatbelt sandbox server.");
    }
    if (!seatbelt.server->InitializeSandbox()) {
      AperitifFatalError("Failed to initialize sandbox.");
    }
  }
}

}  // extern "C"
