// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/first_party_sets_context_config.h"

namespace network {

FirstPartySetsContextConfig::FirstPartySetsContextConfig(bool enabled)
    : enabled_(enabled) {}

FirstPartySetsContextConfig::FirstPartySetsContextConfig(
    const FirstPartySetsContextConfig& other) = default;

FirstPartySetsContextConfig::~FirstPartySetsContextConfig() = default;

void FirstPartySetsContextConfig::SetCustomizations(
    OverrideSets customizations) {
  DCHECK(customizations_.empty());
  if (enabled_)
    customizations_ = std::move(customizations);
}

}  // namespace network
