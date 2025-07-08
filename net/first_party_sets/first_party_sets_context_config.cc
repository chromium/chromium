// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/first_party_sets_context_config.h"

#include "base/containers/map_util.h"
#include "base/types/optional_util.h"
#include "net/first_party_sets/first_party_set_entry_override.h"

namespace net {

namespace {
// Verifies all preconditions.
//
// All aliases must have an explicit non-deletion entry in `customizations`,
// and must map to a canonical site that has an identical entry.
bool VerifyPreconditions(
    const base::flat_map<SchemefulSite, FirstPartySetEntryOverride>&
        customizations,
    const base::flat_map<SchemefulSite, SchemefulSite>& aliases) {
  return std::ranges::all_of(aliases, [&](const auto& pair) {
    const FirstPartySetEntryOverride* alias_override =
        base::FindOrNull(customizations, pair.first);
    if (!alias_override || alias_override->IsDeletion()) {
      return false;
    }
    const FirstPartySetEntryOverride* canonical_override =
        base::FindOrNull(customizations, pair.second);
    return canonical_override && *alias_override == *canonical_override;
  });
}
}  // namespace

FirstPartySetsContextConfig::FirstPartySetsContextConfig() = default;

std::optional<FirstPartySetsContextConfig> FirstPartySetsContextConfig::Create(
    base::flat_map<SchemefulSite, FirstPartySetEntryOverride> customizations,
    base::flat_map<SchemefulSite, SchemefulSite> aliases) {
  if (!VerifyPreconditions(customizations, aliases)) {
    return std::nullopt;
  }
  return FirstPartySetsContextConfig(std::move(customizations),
                                     std::move(aliases));
}

FirstPartySetsContextConfig::FirstPartySetsContextConfig(
    base::flat_map<SchemefulSite, FirstPartySetEntryOverride> customizations,
    base::flat_map<SchemefulSite, SchemefulSite> aliases)
    : customizations_(std::move(customizations)),
      aliases_(std::move(aliases)) {}

FirstPartySetsContextConfig::FirstPartySetsContextConfig(
    FirstPartySetsContextConfig&& other) = default;
FirstPartySetsContextConfig& FirstPartySetsContextConfig::operator=(
    FirstPartySetsContextConfig&& other) = default;

FirstPartySetsContextConfig::~FirstPartySetsContextConfig() = default;

FirstPartySetsContextConfig FirstPartySetsContextConfig::Clone() const {
  return FirstPartySetsContextConfig(customizations_, aliases_);
}

bool FirstPartySetsContextConfig::operator==(
    const FirstPartySetsContextConfig& other) const = default;

std::optional<FirstPartySetEntryOverride>
FirstPartySetsContextConfig::FindOverride(const SchemefulSite& site) const {
  return base::OptionalFromPtr(base::FindOrNull(customizations_, site));
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

void FirstPartySetsContextConfig::ForEachAlias(
    base::FunctionRef<void(const SchemefulSite&, const SchemefulSite&)> f)
    const {
  for (const auto& [alias, canonical] : aliases_) {
    f(alias, canonical);
  }
}

}  // namespace net
