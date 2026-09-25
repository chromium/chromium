// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/global_first_party_sets.h"

#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/map_util.h"
#include "base/functional/function_ref.h"
#include "base/types/optional_ref.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_entry_override.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/first_party_sets_validator.h"

namespace net {

GlobalFirstPartySets::GlobalFirstPartySets() = default;

GlobalFirstPartySets::GlobalFirstPartySets(
    base::Version public_sets_version,
    FirstPartySetsContextConfig public_config)
    : public_sets_version_(std::move(public_sets_version)),
      public_config_(public_sets_version_.IsValid()
                         ? std::move(public_config)
                         : FirstPartySetsContextConfig{}) {
  if (!public_sets_version_.IsValid()) {
    CHECK(public_config_.empty());
  }

  public_config_.ForEachCustomizationEntry(
      [&](const SchemefulSite& site,
          const FirstPartySetEntryOverride& entry_overide) -> bool {
        return true;
      });
  CHECK(IsValid()) << "Sets must be valid";
}

// static
GlobalFirstPartySets GlobalFirstPartySets::CreateForTesting(
    base::Version public_sets_version,
    base::flat_map<SchemefulSite, FirstPartySetEntry> entries,
    base::flat_map<SchemefulSite, SchemefulSite> aliases) {
  std::optional<FirstPartySetsContextConfig> config =
      FirstPartySetsContextConfig::Create(std::move(entries),
                                          std::move(aliases));
  CHECK(config.has_value())
      << "Public RWS entries and aliases must satisfy invariants";
  return GlobalFirstPartySets(std::move(public_sets_version),
                              std::move(config).value());
}

GlobalFirstPartySets::GlobalFirstPartySets(GlobalFirstPartySets&&) = default;
GlobalFirstPartySets& GlobalFirstPartySets::operator=(GlobalFirstPartySets&&) =
    default;

GlobalFirstPartySets::~GlobalFirstPartySets() = default;

bool GlobalFirstPartySets::operator==(const GlobalFirstPartySets& other) const =
    default;

GlobalFirstPartySets GlobalFirstPartySets::Clone() const {
  return GlobalFirstPartySets(public_sets_version_, public_config_.Clone());
}

std::optional<FirstPartySetEntry> GlobalFirstPartySets::FindEntry(
    const SchemefulSite& site) const {
  if (const auto entry_override = public_config_.FindOverride(site);
      entry_override.has_value()) {
    return std::make_optional(entry_override->GetEntry());
  }
  return std::nullopt;
}

FirstPartySetMetadata GlobalFirstPartySets::ComputeMetadata(
    const SchemefulSite& site,
    base::optional_ref<const SchemefulSite> top_frame_site) const {
  return FirstPartySetMetadata(FindEntry(site), top_frame_site
                                                    ? FindEntry(*top_frame_site)
                                                    : std::nullopt);
}

bool GlobalFirstPartySets::ForEachPublicSetEntry(
    base::FunctionRef<bool(const SchemefulSite&, const FirstPartySetEntry&)> f)
    const {
  return public_config_.ForEachCustomizationEntry(
      [&](const SchemefulSite& site,
          const FirstPartySetEntryOverride& entry_overide) -> bool {
        return f(site, entry_overide.GetEntry());
      });
}

bool GlobalFirstPartySets::ForEachEffectiveSetEntry(
    base::FunctionRef<bool(const SchemefulSite&, const FirstPartySetEntry&)> f)
    const {
  return public_config_.ForEachCustomizationEntry(
      [&](const SchemefulSite& site,
          const FirstPartySetEntryOverride& entry_override) {
        return f(site, entry_override.GetEntry());
      });
}

bool GlobalFirstPartySets::IsValid() const {
  FirstPartySetsValidator validator;
  ForEachEffectiveSetEntry(
      [&](const SchemefulSite& site, const FirstPartySetEntry& entry) -> bool {
        validator.Update(site, entry.primary());
        return true;
      });

  return validator.IsValid();
}

const SchemefulSite& GlobalFirstPartySets::ResolveAlias(
    const SchemefulSite& site) const {
  CHECK(public_config_.Contains(site));
  return public_config_.ResolveAlias(site);
}

std::ostream& operator<<(std::ostream& os, const GlobalFirstPartySets& sets) {
  os << "{public_config = {";
  sets.ForEachPublicSetEntry(
      [&](const SchemefulSite& site, const FirstPartySetEntry& entry) {
        os << "{" << site.Serialize() << ": " << entry << "},";
        return true;
      });
  os << "}}";
  return os;
}

}  // namespace net
