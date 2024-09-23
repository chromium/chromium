// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "domatolpm.h"

namespace domatolpm {

std::string_view TextSampleBuilder::view() {
  return data_;
}

void TextSampleBuilder::append(std::string_view v) {
  data_ += v;
}

SampleBuilder* Context::GetBuilder() {
  return &builder_;
}

bool Context::HasVar(const std::string& var_type) {
  return vars_.count(var_type) > 0;
}

void Context::SetVar(const std::string& var_type, const std::string& var_name) {
  vars_[var_type].insert(var_name);
}

std::string_view Context::GetVar(const std::string& var_type, int32_t id) {
  id = id % vars_[var_type].size();
  return *std::next(std::begin(vars_[var_type]), id);
}

std::string Context::GetNewID() {
  return base::NumberToString(id_++);
}

}  // namespace domatolpm
