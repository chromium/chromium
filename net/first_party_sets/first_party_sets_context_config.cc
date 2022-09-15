// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/first_party_sets_context_config.h"

namespace net {

FirstPartySetsContextConfig::FirstPartySetsContextConfig() = default;
FirstPartySetsContextConfig::FirstPartySetsContextConfig(
    FirstPartySetsContextConfig::OverrideSets customizations)
    : customizations_(std::move(customizations)) {}

FirstPartySetsContextConfig::FirstPartySetsContextConfig(
    FirstPartySetsContextConfig&& other) = default;
FirstPartySetsContextConfig& FirstPartySetsContextConfig::operator=(
    FirstPartySetsContextConfig&& other) = default;

FirstPartySetsContextConfig::~FirstPartySetsContextConfig() = default;

FirstPartySetsContextConfig FirstPartySetsContextConfig::Clone() const {
  return FirstPartySetsContextConfig(customizations_);
}

bool FirstPartySetsContextConfig::operator==(
    const FirstPartySetsContextConfig& other) const {
  return customizations_ == other.customizations_;
}

}  // namespace net
