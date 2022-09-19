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

void FirstPartySetsContextConfig::IngestAliases(
    base::flat_map<SchemefulSite, SchemefulSite> aliases) {
  for (const auto& [alias, canonical] : aliases) {
    absl::optional<absl::optional<FirstPartySetEntry>> result =
        FindOverride(canonical);
    DCHECK(result.has_value());
    bool inserted = customizations_.emplace(alias, result.value()).second;
    DCHECK(inserted);
  }
}

}  // namespace net
