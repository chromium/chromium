// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_MAC_SEATBELT_H_
#define SANDBOX_MAC_SEATBELT_H_

#include <cstdint>
#include <string>

#include "sandbox/mac/seatbelt_export.h"

extern "C" {
struct sandbox_params_t;
}

namespace sandbox {

// This class exists because OS X deprecated the sandbox functions,
// and did not supply replacements that are suitable for Chrome.
// This class wraps the functions in deprecation warning supressions.
class SEATBELT_EXPORT Seatbelt {
 public:
  // Parameters stores policy key/value pairs that can be used for policy
  // compilation, independent of sandbox application.
  class Parameters {
   public:
    // Creates a valid parameter object.
    static Parameters Create();

    // Creates an null parameter object. Calling Set() on this object is
    // undefined.
    Parameters();

    Parameters(Parameters&&);
    Parameters& operator=(Parameters&&);

    Parameters(const Parameters&) = delete;
    Parameters& operator=(const Parameters&) = delete;

    ~Parameters();

    // Sets a key/value pair. Duplicate keys are not permitted. Both strings
    // must outlive this object.
    bool Set(const char* key, const char* value);

    sandbox_params_t* params() const { return params_; }

   private:
    sandbox_params_t* params_ = nullptr;
  };

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

  // Compiles a profile string, with optional parameters, into binary
  // representation. Returns true on success with the result of compilation
  // stored in `compiled_profile`. On error, returns false with a message
  // stored in the optional `error` parameter.
  // Note that the data are binary, but because this is used with the
  // seatbelt.pb proto, which uses std::string for binary data, this
  // interface takes std::string rather than std::vector<uint8_t>.
  static bool Compile(const char* profile,
                      const Parameters& params,
                      std::string& compiled_profile,
                      std::string* error);

  // Applies a compiled binary sandbox profile to the current process. Returns
  // true on success; on failure, returns false with a message stored in
  // the optional `error` parameter.
  static bool ApplyCompiledProfile(const std::string& profile,
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
