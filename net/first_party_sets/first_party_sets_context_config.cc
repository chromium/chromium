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

absl::optional<absl::optional<FirstPartySetEntry>>
FirstPartySetsContextConfig::FindOverride(const SchemefulSite& site) const {
  if (const auto it = customizations_.find(site); it != customizations_.end()) {
    return it->second;
  }
  return absl::nullopt;
}

bool FirstPartySetsContextConfig::Contains(const SchemefulSite& site) const {
  return FindOverride(site).has_value();
}

bool FirstPartySetsContextConfig::ForEachCustomizationEntry(
    base::FunctionRef<bool(const SchemefulSite&,
                           const absl::optional<FirstPartySetEntry>&)> f)
    const {
  for (const auto& [site, maybe_entry] : customizations_) {
    if (!f(site, maybe_entry))
      return false;
  }
  return true;
}

}  // namespace net
