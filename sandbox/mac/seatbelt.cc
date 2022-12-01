// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/seatbelt.h"

#include <unistd.h>

extern "C" {
#include <sandbox.h>

int sandbox_init_with_parameters(const char* profile,
                                 uint64_t flags,
                                 const char* const parameters[],
                                 char** errorbuf);

// Not deprecated. The canonical usage to test if sandboxed is
// sandbox_check(getpid(), NULL, SANDBOX_FILTER_NONE), which returns
// 1 if sandboxed. Note `type` is actually a sandbox_filter_type enum value, but
// it is unused currently.
int sandbox_check(pid_t pid, const char* operation, int type, ...);
}

namespace sandbox {

namespace {

bool HandleSandboxResult(int rv, char* errorbuf, std::string* error) {
  if (rv == 0) {
    if (error)
      error->clear();
    return true;
  }

  if (error)
    *error = errorbuf;
  Seatbelt::FreeError(errorbuf);
  return false;
}

}  // namespace

// Initialize the static member variables.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
const char* Seatbelt::kProfilePureComputation = kSBXProfilePureComputation;
#pragma clang diagnostic pop

// static
bool Seatbelt::Init(const char* profile, uint64_t flags, std::string* error) {
// OS X deprecated these functions, but did not provide a suitable replacement,
// so ignore the deprecation warning.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  char* errorbuf = nullptr;
  int rv = ::sandbox_init(profile, flags, &errorbuf);
  return HandleSandboxResult(rv, errorbuf, error);
#pragma clang diagnostic pop
}

// static
bool Seatbelt::InitWithParams(const char* profile,
                              uint64_t flags,
                              const char* const parameters[],
                              std::string* error) {
  char* errorbuf = nullptr;
  int rv =
      ::sandbox_init_with_parameters(profile, flags, parameters, &errorbuf);
  return HandleSandboxResult(rv, errorbuf, error);
}

// static
void Seatbelt::FreeError(char* errorbuf) {
// OS X deprecated these functions, but did not provide a suitable replacement,
// so ignore the deprecation warning.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  return ::sandbox_free_error(errorbuf);
#pragma clang diagnostic pop
}

// static
bool Seatbelt::IsSandboxed() {
  return ::sandbox_check(getpid(), NULL, 0);
}

}  // namespace sandbox
