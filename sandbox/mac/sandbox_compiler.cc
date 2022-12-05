// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/sandbox_compiler.h"

#include <string>
#include <vector>

#include "sandbox/mac/seatbelt.h"

namespace sandbox {

SandboxCompiler::SandboxCompiler() = default;

SandboxCompiler::SandboxCompiler(const std::string& profile_str) {
  SetProfile(profile_str);
}

SandboxCompiler::~SandboxCompiler() {}

void SandboxCompiler::SetProfile(const std::string& policy) {
  policy_.set_profile(policy);
}

bool SandboxCompiler::SetBooleanParameter(const std::string& key, bool value) {
  return SetParameter(key, value ? "TRUE" : "FALSE");
}

bool SandboxCompiler::SetParameter(const std::string& key,
                                   const std::string& value) {
  google::protobuf::MapPair<std::string, std::string> pair(key, value);
  return policy_.mutable_params()->insert(pair).second;
}

bool SandboxCompiler::CompileAndApplyProfile(std::string* error) {
  std::vector<const char*> params;

  for (const auto& kv : policy_.params()) {
    params.push_back(kv.first.c_str());
    params.push_back(kv.second.c_str());
  }
  // The parameters array must be null terminated.
  params.push_back(static_cast<const char*>(0));

  return sandbox::Seatbelt::InitWithParams(policy_.profile().c_str(), 0,
                                           params.data(), error);
}

const mac::SandboxPolicy& SandboxCompiler::CompilePolicyToProto() {
  return policy_;
}

}  // namespace sandbox
