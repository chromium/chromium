// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/sandbox_compiler.h"

#include <string>
#include <vector>

#include "sandbox/mac/seatbelt.h"

namespace sandbox {

SandboxCompiler::SandboxCompiler() : SandboxCompiler(Target::kSource) {}

SandboxCompiler::SandboxCompiler(Target mode) : mode_(mode) {
  if (mode_ == Target::kCompiled) {
    params_ = Seatbelt::Parameters::Create();
  }
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
  // Regardless of the mode, add the strings to the proto map because
  // Seatbelt::Parameters::Set does not copy the strings, which means temporary
  // std::string references need to be owned somewhere.
  auto it = policy_.mutable_params()->insert({key, value});

  if (mode_ == Target::kCompiled && it.second) {
    if (!params_.Set(it.first->first.c_str(), it.first->second.c_str())) {
      policy_.mutable_params()->erase(it.first);
      return false;
    }
  }

  return it.second;
}

bool SandboxCompiler::CompileAndApplyProfile(std::string& error) {
  if (mode_ == Target::kSource) {
    std::vector<const char*> params;

    for (const auto& kv : policy_.params()) {
      params.push_back(kv.first.c_str());
      params.push_back(kv.second.c_str());
    }
    // The parameters array must be null terminated.
    params.push_back(nullptr);

    return Seatbelt::InitWithParams(policy_.profile().c_str(), 0, params.data(),
                                    &error);
  } else if (mode_ == Target::kCompiled) {
    std::string profile;
    if (Seatbelt::Compile(policy_.profile().c_str(), params_, profile,
                          &error)) {
      return Seatbelt::ApplyCompiledProfile(profile, &error);
    }
  }
  return false;
}

bool SandboxCompiler::CompilePolicyToProto(mac::SandboxPolicy& policy,
                                           std::string& error) {
  if (mode_ == Target::kSource) {
    policy.mutable_source()->CopyFrom(policy_);
    return true;
  } else if (mode_ == Target::kCompiled) {
    return Seatbelt::Compile(policy_.profile().c_str(), params_,
                             *policy.mutable_compiled()->mutable_data(),
                             &error);
  }
  return false;
}

}  // namespace sandbox
