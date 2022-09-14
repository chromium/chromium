// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/first_party_sets_context_config.h"

namespace net {

FirstPartySetsContextConfig::FirstPartySetsContextConfig() = default;

FirstPartySetsContextConfig::FirstPartySetsContextConfig(
    const FirstPartySetsContextConfig& other) = default;

FirstPartySetsContextConfig::~FirstPartySetsContextConfig() = default;

void FirstPartySetsContextConfig::SetCustomizations(
    OverrideSets customizations) {
  DCHECK(customizations_.empty());
  customizations_ = std::move(customizations);
}

}  // namespace net
