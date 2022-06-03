// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_MAC_SANDBOX_COMPILER_H_
#define SANDBOX_MAC_SANDBOX_COMPILER_H_

#include <map>
#include <string>

#include "sandbox/mac/seatbelt_export.h"

namespace sandbox {

// This class wraps the C-style sandbox APIs in a class to ensure proper
// initialization and cleanup.
class SEATBELT_EXPORT SandboxCompiler {
 public:
  explicit SandboxCompiler(const std::string& profile_str);

  ~SandboxCompiler();
  SandboxCompiler(const SandboxCompiler& other) = delete;
  SandboxCompiler& operator=(const SandboxCompiler& other) = delete;

  // Inserts a boolean into the parameters key/value map. A duplicate key is not
  // allowed, and will cause the function to return false. The value is not
  // inserted in this case.
  bool InsertBooleanParam(const std::string& key, bool value);

  // Inserts a string into the parameters key/value map. A duplicate key is not
  // allowed, and will cause the function to return false. The value is not
  // inserted in this case.
  bool InsertStringParam(const std::string& key, const std::string& value);

  // Compiles and applies the profile; returns true on success.
  bool CompileAndApplyProfile(std::string* error);

 private:
  // Storage of the key/value pairs of strings that are used in the sandbox
  // profile.
  std::map<std::string, std::string> params_map_;

  // The sandbox profile source code.
  const std::string profile_str_;
};

}  // namespace sandbox

#endif  // SANDBOX_MAC_SANDBOX_COMPILER_H_
