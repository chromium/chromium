// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_MAC_SEATBELT_H_
#define SANDBOX_MAC_SEATBELT_H_

#include <cstdint>
#include <string>

#include "sandbox/mac/seatbelt_export.h"

namespace sandbox {

// This class exists because OS X deprecated the sandbox functions,
// and did not supply replacements that are suitable for Chrome.
// This class wraps the functions in deprecation warning supressions.
class SEATBELT_EXPORT Seatbelt {
 public:
  // Initializes the specified sandbox profile. Returns true on success with
  // the sandbox applied; otherwise, returns false and outputs the error in
  // `error`.
  static bool Init(const char* profile, uint64_t flags, std::string* error);

  // Initializes the specified sandbox profile and passes the parameters to the
  // `profile`. `parameters` is a null terminated list containing key,value
  // pairs in sequence. [key1,val1,key2,val2,nullptr]. Returns true on success
  // with the sandbox applied; otherwise, returns false and outputs the
  // error in `error`.
  static bool InitWithParams(const char* profile,
                             uint64_t flags,
                             const char* const parameters[],
                             std::string* error);

  // Frees an error buffer allocated from libsandbox.dylib routines.
  static void FreeError(char* errorbuf);

  // Returns whether or not the process is currently sandboxed.
  static bool IsSandboxed();

  static const char* kProfilePureComputation;

  Seatbelt(const Seatbelt& other) = delete;
  Seatbelt& operator=(const Seatbelt& other) = delete;

 private:
  Seatbelt();
};

}  // sandbox

#endif  // SANDBOX_MAC_SEATBELT_H_
