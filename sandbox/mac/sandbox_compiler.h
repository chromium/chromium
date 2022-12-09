// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_MAC_SANDBOX_COMPILER_H_
#define SANDBOX_MAC_SANDBOX_COMPILER_H_

#include <string>

#include "sandbox/mac/seatbelt.h"
#include "sandbox/mac/seatbelt.pb.h"
#include "sandbox/mac/seatbelt_export.h"

namespace sandbox {

// This class wraps the C-style sandbox APIs in a class to ensure proper
// initialization and cleanup.
class SEATBELT_EXPORT SandboxCompiler {
 public:
  enum class Target {
    // The result of compilation is a SandboxPolicy proto containing the policy
    // string source and a map of key/value pairs.
    kSource,

    // The result of compilation is a SandboxPolicy proto containing a sealed,
    // compiled, binary sandbox policy that can be applied immediately.
    kCompiled,
  };

  // Creates a compiler in the default mode, `Target::kSource`.
  SandboxCompiler();

  // Creates a compiler with the specified target mode.
  explicit SandboxCompiler(Target mode);

  ~SandboxCompiler();
  SandboxCompiler(const SandboxCompiler& other) = delete;
  SandboxCompiler& operator=(const SandboxCompiler& other) = delete;

  // Sets the policy source string, if not already specified in the constructor.
  void SetProfile(const std::string& policy);

  // Inserts a boolean into the parameters key/value map. A duplicate key is not
  // allowed, and will cause the function to return false. The value is not
  // inserted in this case.
  [[nodiscard]] bool SetBooleanParameter(const std::string& key, bool value);

  // Inserts a string into the parameters key/value map. A duplicate key is not
  // allowed, and will cause the function to return false. The value is not
  // inserted in this case.
  [[nodiscard]] bool SetParameter(const std::string& key,
                                  const std::string& value);

  // Compiles and applies the profile; returns true on success and false
  // on failure with a message set in the `error` parameter.
  bool CompileAndApplyProfile(std::string& error);

  // Compiles the policy into a sandbox policy proto. Returns true on success,
  // with `policy` set, or returns false on error with a message in the `error`
  // parameter.
  bool CompilePolicyToProto(mac::SandboxPolicy& policy, std::string& error);

 private:
  const Target mode_;

  mac::SourcePolicy policy_;

  Seatbelt::Parameters params_;
};

}  // namespace sandbox

#endif  // SANDBOX_MAC_SANDBOX_COMPILER_H_
