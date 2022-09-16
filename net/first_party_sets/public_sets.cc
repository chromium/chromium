// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/public_sets.h"

#include <tuple>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

namespace {

using FlattenedSets = base::flat_map<SchemefulSite, FirstPartySetEntry>;
using SingleSet = base::flat_map<SchemefulSite, FirstPartySetEntry>;

// Converts WS to HTTP, and WSS to HTTPS.
SchemefulSite NormalizeScheme(const SchemefulSite& site) {
  SchemefulSite normalized_site = site;
  normalized_site.ConvertWebSocketToHttp();
  return normalized_site;
}

// Converts a list of First-Party Sets from a SingleSet to a FlattenedSet
// representation.
FlattenedSets SetListToFlattenedSets(const std::vector<SingleSet>& set_list) {
  FlattenedSets sets;
  for (const auto& set : set_list) {
    for (const auto& site_and_entry : set) {
      bool inserted = sets.emplace(site_and_entry).second;
      DCHECK(inserted);
    }
  }
  return sets;
}

// Adds all sets in a list of First-Party Sets into `site_to_entry` which maps
// from a site to its entry.
void UpdateCustomizationMap(
    const std::vector<SingleSet>& set_list,
    base::flat_map<SchemefulSite, absl::optional<FirstPartySetEntry>>&
        site_to_entry) {
  for (const auto& set : set_list) {
    for (const auto& site_and_entry : set) {
      bool inserted = site_to_entry.emplace(site_and_entry).second;
      DCHECK(inserted);
    }
  }
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

base::flat_map<SchemefulSite, FirstPartySetEntry> PublicSets::FindEntries(
    const base::flat_set<SchemefulSite>& sites,
    const FirstPartySetsContextConfig* config) const {
  std::vector<std::pair<SchemefulSite, FirstPartySetEntry>> sites_to_entries;
  for (const SchemefulSite& site : sites) {
    const absl::optional<FirstPartySetEntry> entry = FindEntry(site, config);
    if (entry.has_value()) {
      sites_to_entries.emplace_back(site, entry.value());
    }
  }
  return sites_to_entries;
}

base::flat_set<SchemefulSite> PublicSets::FindIntersection(
    const SchemefulSite& manual_primary,
    const base::flat_map<SchemefulSite, FirstPartySetEntry>& manual_entries)
    const {
  std::vector<SchemefulSite> intersection;
  for (const std::pair<SchemefulSite, FirstPartySetEntry>&
           public_site_and_entry : entries_) {
    const SchemefulSite& public_site = public_site_and_entry.first;
    const SchemefulSite& public_primary =
        public_site_and_entry.second.primary();
    bool is_affected_by_local_set =
        public_site == manual_primary || public_primary == manual_primary ||
        base::ranges::any_of(
            manual_entries,
            [&](const std::pair<SchemefulSite, FirstPartySetEntry>&
                    manual_site_and_entry) {
              const SchemefulSite& manual_site = manual_site_and_entry.first;
              return manual_site == public_site ||
                     manual_site == public_primary;
            });
    if (is_affected_by_local_set) {
      intersection.push_back(public_site_and_entry.first);
    }
  };

  return intersection;
}

base::flat_set<SchemefulSite> PublicSets::FindSingletons() const {
  std::vector<SchemefulSite> primaries_with_members;
  for (const auto& [site, entry] : entries_) {
    if (site != entry.primary())
      primaries_with_members.push_back(entry.primary());
  }
  std::vector<SchemefulSite> singletons;
  for (const auto& [site, entry] : entries_) {
    if (site == entry.primary() &&
        !base::Contains(primaries_with_members, site)) {
      singletons.push_back(site);
    }
  }

  return singletons;
}

void PublicSets::ApplyManuallySpecifiedSet(
    const SchemefulSite& manual_primary,
    const base::flat_map<SchemefulSite, FirstPartySetEntry>& manual_entries,
    const base::flat_map<SchemefulSite, SchemefulSite>& manual_aliases) {
  base::flat_set<SchemefulSite> intersection =
      FindIntersection(manual_primary, manual_entries);
  for (const auto& site : intersection) {
    entries_.erase(site);
  }

  base::flat_set<SchemefulSite> singletons = FindSingletons();
  for (const auto& singleton : singletons) {
    entries_.erase(singleton);
  }

  base::ranges::copy(manual_entries, std::inserter(entries_, entries_.end()));

  // Finally, remove any aliases for public sites that were affected (deleted),
  // and add any aliases defined in the local set.
  base::EraseIf(
      aliases_, [&](const std::pair<SchemefulSite, SchemefulSite>& alias) {
        return intersection.contains(alias.second) ||
               singletons.contains(alias.second);
      });
  aliases_.insert(manual_aliases.begin(), manual_aliases.end());
}

FirstPartySetsContextConfig PublicSets::ComputeConfig(
    const std::vector<SingleSet>& replacement_sets,
    const std::vector<SingleSet>& normalized_additions) const {
  // Maps a site to its new entry if it has one.
  base::flat_map<SchemefulSite, absl::optional<FirstPartySetEntry>>
      site_to_entry;

  // Create flattened versions of the sets for easier lookup.
  FlattenedSets flattened_replacements =
      SetListToFlattenedSets(replacement_sets);
  FlattenedSets flattened_additions =
      SetListToFlattenedSets(normalized_additions);

  // All of the policy sets are automatically inserted into site_to_owner.
  UpdateCustomizationMap(replacement_sets, site_to_entry);
  UpdateCustomizationMap(normalized_additions, site_to_entry);

  // Maps old owner to new entry.
  base::flat_map<SchemefulSite, FirstPartySetEntry> addition_intersected_owners;
  for (const auto& [new_member, new_entry] : flattened_additions) {
    if (const auto entry = FindEntry(new_member, /*config=*/nullptr);
        entry.has_value()) {
      // Found an overlap with the existing list of sets.
      addition_intersected_owners.emplace(entry->primary(), new_entry);
    }
  }

  // Maps an existing owner to the members it lost due to replacement.
  base::flat_map<SchemefulSite, base::flat_set<SchemefulSite>>
      potential_singletons;
  for (const auto& [member, set_entry] : flattened_replacements) {
    if (member == set_entry.primary())
      continue;
    if (auto existing_entry = FindEntry(member, /*config=*/nullptr);
        existing_entry.has_value() && existing_entry->primary() != member) {
      if (!addition_intersected_owners.contains(existing_entry->primary()) &&
          !flattened_additions.contains(existing_entry->primary()) &&
          !flattened_replacements.contains(existing_entry->primary())) {
        potential_singletons[existing_entry->primary()].insert(member);
      }
    }
  }

  // Find the existing owners that have left their existing sets, and whose
  // existing members should be removed from their set (excl any policy sets
  // that those members are involved in).
  base::flat_set<SchemefulSite> replaced_existing_owners;
  for (const auto& [site, unused_owner] : flattened_replacements) {
    if (const auto entry = FindEntry(site, /*config=*/nullptr);
        entry.has_value() && entry->primary() == site) {
      // Site was an owner in the existing sets.
      bool inserted = replaced_existing_owners.emplace(site).second;
      DCHECK(inserted);
    }
  }

  // Find out which potential singletons are actually singletons; delete
  // members whose owners left; and reparent the sets that intersected with
  // an addition set.
  for (const auto& [member, set_entry] : entries_) {
    // Reparent all sites in any intersecting addition sets.
    if (auto entry = addition_intersected_owners.find(set_entry.primary());
        entry != addition_intersected_owners.end() &&
        !flattened_replacements.contains(member)) {
      site_to_entry.emplace(member,
                            FirstPartySetEntry(entry->second.primary(),
                                               member == entry->second.primary()
                                                   ? SiteType::kPrimary
                                                   : SiteType::kAssociated,
                                               absl::nullopt));
    }
    if (member == set_entry.primary())
      continue;
    // Remove non-singletons from the potential list.
    if (auto entry = potential_singletons.find(set_entry.primary());
        entry != potential_singletons.end() &&
        !entry->second.contains(member)) {
      // This owner lost members, but it still has at least one (`member`),
      // so it's not a singleton.
      potential_singletons.erase(entry);
    }
    // Remove members from sets whose owner left.
    if (replaced_existing_owners.contains(set_entry.primary()) &&
        !flattened_replacements.contains(member) &&
        !addition_intersected_owners.contains(set_entry.primary())) {
      bool inserted = site_to_entry.emplace(member, absl::nullopt).second;
      DCHECK(inserted);
    }
  }
  // Any owner remaining in `potential_singleton` is a real singleton, so delete
  // it:
  for (auto& [owner, members] : potential_singletons) {
    bool inserted = site_to_entry.emplace(owner, absl::nullopt).second;
    DCHECK(inserted);
  }

  return FirstPartySetsContextConfig(std::move(site_to_entry));
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
