// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/sandbox_compiler.h"

#include <map>
#include <string>
#include <vector>

#include "sandbox/mac/seatbelt.h"

namespace sandbox {

SandboxCompiler::SandboxCompiler(const std::string& profile_str)
    : params_map_(), profile_str_(profile_str) {}

SandboxCompiler::~SandboxCompiler() {}

bool SandboxCompiler::InsertBooleanParam(const std::string& key, bool value) {
  return params_map_.insert(std::make_pair(key, value ? "TRUE" : "FALSE"))
      .second;
}

bool SandboxCompiler::InsertStringParam(const std::string& key,
                                        const std::string& value) {
  return params_map_.insert(std::make_pair(key, value)).second;
}

bool SandboxCompiler::CompileAndApplyProfile(std::string* error) {
  char* error_internal = nullptr;
  std::vector<const char*> params;

  for (const auto& kv : params_map_) {
    params.push_back(kv.first.c_str());
    params.push_back(kv.second.c_str());
  }
  // The parameters array must be null terminated.
  params.push_back(static_cast<const char*>(0));

  if (sandbox::Seatbelt::InitWithParams(profile_str_.c_str(), 0, params.data(),
                                        &error_internal)) {
    error->assign(error_internal);
    sandbox::Seatbelt::FreeError(error_internal);
    return false;
  }
  return true;
}

}  // namespace sandbox
