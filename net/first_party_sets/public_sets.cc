// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/public_sets.h"

#include <tuple>

#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_sets_context_config.h"

namespace net {

namespace {

// Converts WS to HTTP, and WSS to HTTPS.
SchemefulSite NormalizeScheme(const SchemefulSite& site) {
  SchemefulSite normalized_site = site;
  normalized_site.ConvertWebSocketToHttp();
  return normalized_site;
}

}  // namespace

PublicSets::PublicSets() = default;

PublicSets::PublicSets(
    base::flat_map<SchemefulSite, FirstPartySetEntry> entries,
    base::flat_map<SchemefulSite, SchemefulSite> aliases)
    : entries_(std::move(entries)), aliases_(std::move(aliases)) {}

PublicSets::PublicSets(PublicSets&&) = default;
PublicSets& PublicSets::operator=(PublicSets&&) = default;

PublicSets::~PublicSets() = default;

bool PublicSets::operator==(const PublicSets& other) const {
  return std::tie(entries_, aliases_) ==
         std::tie(other.entries_, other.aliases_);
}

bool PublicSets::operator!=(const PublicSets& other) const {
  return !(*this == other);
}

PublicSets PublicSets::Clone() const {
  return PublicSets(entries_, aliases_);
}

absl::optional<FirstPartySetEntry> PublicSets::FindEntry(
    const SchemefulSite& site,
    const FirstPartySetsContextConfig* fps_context_config) const {
  SchemefulSite normalized_site = NormalizeScheme(site);

  // Check if `normalized_site` can be found in the customizations first.
  // If not, fall back to look up in `entries_`.
  if (fps_context_config) {
    if (const auto config_it =
            fps_context_config->customizations().find(normalized_site);
        config_it != fps_context_config->customizations().end()) {
      return config_it->second;
    }
  }

  const auto canonical_it = aliases_.find(normalized_site);
  const SchemefulSite& canonical_site =
      canonical_it == aliases_.end() ? normalized_site : canonical_it->second;
  if (const auto entry_it = entries_.find(canonical_site);
      entry_it != entries_.end()) {
    return entry_it->second;
  }

  return absl::nullopt;
}

std::ostream& operator<<(std::ostream& os, const PublicSets& ps) {
  os << "{entries = {";
  for (const auto& [site, entry] : ps.entries()) {
    os << "{" << site.Serialize() << ": " << entry << "}, ";
  }
  os << "}, aliases = {";
  for (const auto& [alias, canonical] : ps.aliases()) {
    os << "{" << alias.Serialize() << ": " << canonical.Serialize() << "}, ";
  }
  os << "}}";
  return os;
}

}  // namespace net
