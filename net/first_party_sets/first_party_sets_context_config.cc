// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/first_party_sets_context_config.h"

#include "net/first_party_sets/first_party_set_entry_override.h"

namespace net {

FirstPartySetsContextConfig::FirstPartySetsContextConfig() = default;
FirstPartySetsContextConfig::FirstPartySetsContextConfig(
    base::flat_map<SchemefulSite, FirstPartySetEntryOverride> customizations)
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
    const FirstPartySetsContextConfig& other) const = default;

std::optional<FirstPartySetEntryOverride>
FirstPartySetsContextConfig::FindOverride(const SchemefulSite& site) const {
  if (const auto it = customizations_.find(site); it != customizations_.end()) {
    return it->second;
  }
  return std::nullopt;
}

bool FirstPartySetsContextConfig::Contains(const SchemefulSite& site) const {
  return FindOverride(site).has_value();
}

bool FirstPartySetsContextConfig::ForEachCustomizationEntry(
    base::FunctionRef<bool(const SchemefulSite&,
                           const FirstPartySetEntryOverride&)> f) const {
  for (const auto& [site, override] : customizations_) {
    if (!f(site, override))
      return false;
  }
  return true;
}

}  // namespace net
