// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_MAC_SEATBELT_H_
#define SANDBOX_MAC_SEATBELT_H_

#include <cstdint>

#include "sandbox/mac/seatbelt_export.h"

namespace sandbox {

// This class exists because OS X deprecated the sandbox functions,
// and did not supply replacements that are suitable for Chrome.
// This class wraps the functions in deprecation warning supressions.
class SEATBELT_EXPORT Seatbelt {
 public:
  // Initializes the specified sandbox profile. Returns 0 on success, else -1
  // and |errorbuf| is populated. |errorbuf| is allocated by the API and must be
  // freed with FreeError().
  static int Init(const char* profile, uint64_t flags, char** errorbuf);

  // Initializes the specified sandbox profile and passes the parameters to the
  // |profile|. |parameters| is a null terminated list containing key,value
  // pairs in sequence. [key1,val1,key2,val2,nullptr]. |errorbuf| is allocated
  // by the API and is set to a string description of the error. |errorbuf| must
  // be freed with FreeError(). This function eturns 0 on success, else -1 and
  // |errorbuf| is populated.
  static int InitWithParams(const char* profile,
                            uint64_t flags,
                            const char* const parameters[],
                            char** errorbuf);

  // Frees the |errorbuf| allocated and set by InitWithParams.
  static void FreeError(char* errorbuf);

  // Returns whether or not the process is currently sandboxed.
  static bool IsSandboxed();

  static const char* kProfileNoInternet;

  static const char* kProfileNoNetwork;

  static const char* kProfileNoWrite;

  static const char* kProfileNoWriteExceptTemporary;

  static const char* kProfilePureComputation;

  Seatbelt(const Seatbelt& other) = delete;
  Seatbelt& operator=(const Seatbelt& other) = delete;

 private:
  Seatbelt();
};

}  // sandbox

#endif  // SANDBOX_MAC_SEATBELT_H_
