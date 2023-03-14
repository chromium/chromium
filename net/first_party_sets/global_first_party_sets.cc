// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/global_first_party_sets.h"

#include <set>
#include <tuple>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/function_ref.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/optional_util.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/addition_overlaps_union_find.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_entry_override.h"
#include "net/first_party_sets/first_party_set_metadata.h"
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
      CHECK(inserted);
    }
  }
  return sets;
}

// Adds all sets in a list of First-Party Sets into `site_to_entry` which maps
// from a site to its entry.
void UpdateCustomizations(
    const std::vector<SingleSet>& set_list,
    std::vector<std::pair<SchemefulSite, FirstPartySetEntryOverride>>&
        site_to_entry) {
  for (const auto& set : set_list) {
    for (const auto& site_and_entry : set) {
      site_to_entry.emplace_back(site_and_entry);
    }
  }
}

const SchemefulSite& ProjectKey(
    const std::pair<SchemefulSite, FirstPartySetEntryOverride>& p) {
  return p.first;
}

SamePartyContext::Type ContextTypeFromBool(bool is_same_party) {
  return is_same_party ? SamePartyContext::Type::kSameParty
                       : SamePartyContext::Type::kCrossParty;
}

}  // namespace

GlobalFirstPartySets::GlobalFirstPartySets() = default;

GlobalFirstPartySets::GlobalFirstPartySets(
    base::Version public_sets_version,
    base::flat_map<SchemefulSite, FirstPartySetEntry> entries,
    base::flat_map<SchemefulSite, SchemefulSite> aliases)
    : GlobalFirstPartySets(
          public_sets_version,
          public_sets_version.IsValid()
              ? std::move(entries)
              : base::flat_map<SchemefulSite, FirstPartySetEntry>(),
          public_sets_version.IsValid()
              ? std::move(aliases)
              : base::flat_map<SchemefulSite, SchemefulSite>(),
          FirstPartySetsContextConfig()) {}

GlobalFirstPartySets::GlobalFirstPartySets(
    base::Version public_sets_version,
    base::flat_map<SchemefulSite, FirstPartySetEntry> entries,
    base::flat_map<SchemefulSite, SchemefulSite> aliases,
    FirstPartySetsContextConfig manual_config)
    : public_sets_version_(std::move(public_sets_version)),
      entries_(std::move(entries)),
      aliases_(std::move(aliases)),
      manual_config_(std::move(manual_config)) {
  if (public_sets_version_.IsValid()) {
    DCHECK(base::ranges::all_of(aliases_, [&](const auto& pair) {
      return entries_.contains(pair.second);
    }));
  } else {
    CHECK(entries_.empty());
    CHECK(aliases_.empty());
  }
}

GlobalFirstPartySets::GlobalFirstPartySets(GlobalFirstPartySets&&) = default;
GlobalFirstPartySets& GlobalFirstPartySets::operator=(GlobalFirstPartySets&&) =
    default;

GlobalFirstPartySets::~GlobalFirstPartySets() = default;

bool GlobalFirstPartySets::operator==(const GlobalFirstPartySets& other) const {
  return std::tie(public_sets_version_, entries_, aliases_, manual_config_) ==
         std::tie(other.public_sets_version_, other.entries_, other.aliases_,
                  other.manual_config_);
}

bool GlobalFirstPartySets::operator!=(const GlobalFirstPartySets& other) const {
  return !(*this == other);
}

GlobalFirstPartySets GlobalFirstPartySets::Clone() const {
  return GlobalFirstPartySets(public_sets_version_, entries_, aliases_,
                              manual_config_.Clone());
}

absl::optional<FirstPartySetEntry> GlobalFirstPartySets::FindEntry(
    const SchemefulSite& site,
    const FirstPartySetsContextConfig& config) const {
  return FindEntry(site, &config);
}

absl::optional<FirstPartySetEntry> GlobalFirstPartySets::FindEntry(
    const SchemefulSite& site,
    const FirstPartySetsContextConfig* config) const {
  const SchemefulSite normalized_site = NormalizeScheme(site);

  // Check if `normalized_site` can be found in the customizations first.
  if (config) {
    if (const auto override = config->FindOverride(normalized_site);
        override.has_value()) {
      return override->IsDeletion() ? absl::nullopt
                                    : absl::make_optional(override->GetEntry());
    }
  }

  // Now see if it's in the manual config (with or without a manual alias).
  if (const auto manual_override = manual_config_.FindOverride(normalized_site);
      manual_override.has_value()) {
    return manual_override->IsDeletion()
               ? absl::nullopt
               : absl::make_optional(manual_override->GetEntry());
  }

  // Finally, look up in `entries_`, applying an alias if applicable.
  const auto canonical_it = aliases_.find(normalized_site);
  const SchemefulSite& canonical_site =
      canonical_it == aliases_.end() ? normalized_site : canonical_it->second;
  if (const auto entry_it = entries_.find(canonical_site);
      entry_it != entries_.end()) {
    return entry_it->second;
  }

  return absl::nullopt;
}

base::flat_map<SchemefulSite, FirstPartySetEntry>
GlobalFirstPartySets::FindEntries(
    const base::flat_set<SchemefulSite>& sites,
    const FirstPartySetsContextConfig& config) const {
  std::vector<std::pair<SchemefulSite, FirstPartySetEntry>> sites_to_entries;
  for (const SchemefulSite& site : sites) {
    const absl::optional<FirstPartySetEntry> entry = FindEntry(site, config);
    if (entry.has_value()) {
      sites_to_entries.emplace_back(site, entry.value());
    }
  }
  return sites_to_entries;
}

FirstPartySetMetadata GlobalFirstPartySets::ComputeMetadata(
    const SchemefulSite& site,
    const SchemefulSite* top_frame_site,
    const std::set<SchemefulSite>& party_context,
    const FirstPartySetsContextConfig& fps_context_config) const {
  const base::ElapsedTimer timer;

  SamePartyContext::Type context_type =
      ContextTypeFromBool(IsContextSamePartyWithSite(
          site, top_frame_site, party_context, fps_context_config));

  SamePartyContext context(context_type);

  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Cookie.FirstPartySets.ComputeContext.Latency", timer.Elapsed(),
      base::Microseconds(1), base::Milliseconds(100), 50);

  absl::optional<FirstPartySetEntry> top_frame_entry =
      top_frame_site ? FindEntry(*top_frame_site, fps_context_config)
                     : absl::nullopt;

  return FirstPartySetMetadata(
      context, base::OptionalToPtr(FindEntry(site, fps_context_config)),
      base::OptionalToPtr(top_frame_entry));
}

bool GlobalFirstPartySets::IsContextSamePartyWithSite(
    const SchemefulSite& site,
    const SchemefulSite* top_frame_site,
    const std::set<SchemefulSite>& party_context,
    const FirstPartySetsContextConfig& fps_context_config) const {
  const absl::optional<FirstPartySetEntry> site_entry =
      FindEntry(site, fps_context_config);
  if (!site_entry.has_value())
    return false;

  const auto is_in_same_set_as_frame_site =
      [this, &site_entry,
       &fps_context_config](const SchemefulSite& context_site) -> bool {
    const absl::optional<FirstPartySetEntry> context_entry =
        FindEntry(context_site, fps_context_config);
    return context_entry.has_value() &&
           context_entry->primary() == site_entry->primary();
  };

  if (top_frame_site && !is_in_same_set_as_frame_site(*top_frame_site))
    return false;

  return base::ranges::all_of(party_context, is_in_same_set_as_frame_site);
}

void GlobalFirstPartySets::ApplyManuallySpecifiedSet(
    const base::flat_map<SchemefulSite, FirstPartySetEntry>& manual_entries) {
  CHECK(manual_config_.empty());
  // We handle the manually-specified set the same way as we handle
  // replacement enterprise policy sets.
  manual_config_ = ComputeConfig(
      /*replacement_sets=*/{manual_entries}, /*addition_sets=*/{});
}

void GlobalFirstPartySets::UnsafeSetManualConfig(
    FirstPartySetsContextConfig manual_config) {
  CHECK(manual_config_.empty());
  manual_config_ = std::move(manual_config);
}

FirstPartySetsContextConfig GlobalFirstPartySets::ComputeConfig(
    const std::vector<SingleSet>& replacement_sets,
    const std::vector<SingleSet>& addition_sets) const {
  if (base::ranges::all_of(replacement_sets,
                           [](const SingleSet& set) { return set.empty(); }) &&
      base::ranges::all_of(addition_sets,
                           [](const SingleSet& set) { return set.empty(); })) {
    // Nothing to do.
    return FirstPartySetsContextConfig();
  }

  // Maps a site to its override.
  std::vector<std::pair<SchemefulSite, FirstPartySetEntryOverride>>
      site_to_entry;

  std::vector<base::flat_map<SchemefulSite, FirstPartySetEntry>>
      normalized_additions = NormalizeAdditionSets(addition_sets);

  // Create flattened versions of the sets for easier lookup.
  FlattenedSets flattened_replacements =
      SetListToFlattenedSets(replacement_sets);
  FlattenedSets flattened_additions =
      SetListToFlattenedSets(normalized_additions);

  // All of the custom sets are automatically inserted into site_to_owner.
  UpdateCustomizations(replacement_sets, site_to_entry);
  UpdateCustomizations(normalized_additions, site_to_entry);

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
    if (const auto existing_entry = FindEntry(member, /*config=*/nullptr);
        existing_entry.has_value() && existing_entry->primary() != member &&
        !addition_intersected_owners.contains(existing_entry->primary()) &&
        !flattened_additions.contains(existing_entry->primary()) &&
        !flattened_replacements.contains(existing_entry->primary())) {
      potential_singletons[existing_entry->primary()].insert(member);
    }
  }

  // Find the existing owners that have left their existing sets, and whose
  // existing members should be removed from their set (excluding any custom
  // sets that those members are involved in).
  base::flat_set<SchemefulSite> replaced_existing_owners;
  for (const auto& [site, unused_owner] : flattened_replacements) {
    if (const auto entry = FindEntry(site, /*config=*/nullptr);
        entry.has_value() && entry->primary() == site) {
      // Site was an owner in the existing sets.
      bool inserted = replaced_existing_owners.emplace(site).second;
      CHECK(inserted);
    }
  }

  if (!addition_intersected_owners.empty() || !potential_singletons.empty() ||
      !replaced_existing_owners.empty()) {
    // Find out which potential singletons are actually singletons; delete
    // members whose owners left; and reparent the sets that intersected with
    // an addition set.
    // Note: use a null config here, to avoid taking unrelated policy sets into
    // account.
    ForEachEffectiveSetEntry(
        /*config=*/nullptr,
        [&](const SchemefulSite& member, const FirstPartySetEntry& set_entry) {
          // Reparent all sites in any intersecting addition sets.
          if (const auto entry =
                  addition_intersected_owners.find(set_entry.primary());
              entry != addition_intersected_owners.end() &&
              !flattened_replacements.contains(member)) {
            site_to_entry.emplace_back(
                member, FirstPartySetEntry(entry->second.primary(),
                                           member == entry->second.primary()
                                               ? SiteType::kPrimary
                                               : SiteType::kAssociated,
                                           absl::nullopt));
          }
          if (member == set_entry.primary())
            return true;
          // Remove non-singletons from the potential list.
          if (const auto entry = potential_singletons.find(set_entry.primary());
              entry != potential_singletons.end() &&
              !entry->second.contains(member)) {
            // This owner lost members, but it still has at least one
            // (`member`), so it's not a singleton.
            potential_singletons.erase(entry);
          }
          // Remove members from sets whose owner left.
          if (replaced_existing_owners.contains(set_entry.primary()) &&
              !flattened_replacements.contains(member) &&
              !addition_intersected_owners.contains(set_entry.primary())) {
            site_to_entry.emplace_back(member, FirstPartySetEntryOverride());
          }

          return true;
        });

    // Any owner remaining in `potential_singleton` is a real singleton, so
    // delete it:
    for (const auto& [owner, members] : potential_singletons) {
      site_to_entry.emplace_back(owner, FirstPartySetEntryOverride());
    }
  }

  // For every public alias that would now refer to a site in the overlay, which
  // is not already contained in the overlay, we explicitly ignore that alias.
  for (const auto& [alias, site] : aliases_) {
    if (base::Contains(site_to_entry, site, ProjectKey) &&
        !base::Contains(site_to_entry, alias, ProjectKey)) {
      site_to_entry.emplace_back(alias, FirstPartySetEntryOverride());
    }
  }

  return FirstPartySetsContextConfig(std::move(site_to_entry));
}

std::vector<base::flat_map<SchemefulSite, FirstPartySetEntry>>
GlobalFirstPartySets::NormalizeAdditionSets(
    const std::vector<base::flat_map<SchemefulSite, FirstPartySetEntry>>&
        addition_sets) const {
  if (base::ranges::all_of(addition_sets,
                           [](const SingleSet& set) { return set.empty(); })) {
    // Nothing to do.
    return {};
  }

  // Find all the addition sets that intersect with any given public set.
  base::flat_map<SchemefulSite, base::flat_set<size_t>> addition_set_overlaps;
  for (size_t set_idx = 0; set_idx < addition_sets.size(); set_idx++) {
    for (const auto& site_and_entry : addition_sets[set_idx]) {
      if (const auto entry =
              FindEntry(site_and_entry.first, /*config=*/nullptr);
          entry.has_value()) {
        addition_set_overlaps[entry->primary()].insert(set_idx);
      }
    }
  }

  // Union together all transitively-overlapping addition sets.
  AdditionOverlapsUnionFind union_finder(addition_sets.size());
  for (const auto& [public_site, addition_set_indices] :
       addition_set_overlaps) {
    for (size_t representative : addition_set_indices) {
      union_finder.Union(*addition_set_indices.begin(), representative);
    }
  }

  // Now build the new addition sets, with all transitive overlaps eliminated.
  std::vector<SingleSet> normalized_additions;
  for (const auto& [rep, children] : union_finder.SetsMapping()) {
    SingleSet normalized = addition_sets[rep];
    const SchemefulSite& rep_primary =
        addition_sets[rep].begin()->second.primary();
    for (size_t child_set_idx : children) {
      for (const auto& child_site_and_entry : addition_sets[child_set_idx]) {
        bool inserted =
            normalized
                .emplace(child_site_and_entry.first,
                         FirstPartySetEntry(rep_primary, SiteType::kAssociated,
                                            absl::nullopt))
                .second;
        CHECK(inserted);
      }
    }
    normalized_additions.push_back(normalized);
  }
  return normalized_additions;
}

bool GlobalFirstPartySets::ForEachPublicSetEntry(
    base::FunctionRef<bool(const SchemefulSite&, const FirstPartySetEntry&)> f)
    const {
  for (const auto& [site, entry] : entries_) {
    if (!f(site, entry))
      return false;
  }
  for (const auto& [alias, canonical] : aliases_) {
    auto it = entries_.find(canonical);
    CHECK(it != entries_.end());
    if (!f(alias, it->second))
      return false;
  }
  return true;
}

bool GlobalFirstPartySets::ForEachManualConfigEntry(
    base::FunctionRef<bool(const SchemefulSite&,
                           const FirstPartySetEntryOverride&)> f) const {
  return manual_config_.ForEachCustomizationEntry(f);
}

bool GlobalFirstPartySets::ForEachEffectiveSetEntry(
    const FirstPartySetsContextConfig& config,
    base::FunctionRef<bool(const SchemefulSite&, const FirstPartySetEntry&)> f)
    const {
  return ForEachEffectiveSetEntry(&config, f);
}

bool GlobalFirstPartySets::ForEachEffectiveSetEntry(
    const FirstPartySetsContextConfig* config,
    base::FunctionRef<bool(const SchemefulSite&, const FirstPartySetEntry&)> f)
    const {
  // Policy sets have highest precedence:
  if (config != nullptr) {
    if (!config->ForEachCustomizationEntry(
            [&](const SchemefulSite& site,
                const FirstPartySetEntryOverride& override) {
              if (!override.IsDeletion())
                return f(site, override.GetEntry());
              return true;
            })) {
      return false;
    }
  }

  // Then the manual set:
  if (!manual_config_.ForEachCustomizationEntry(
          [&](const SchemefulSite& site,
              const FirstPartySetEntryOverride& override) {
            if (!override.IsDeletion() && (!config || !config->Contains(site)))
              return f(site, override.GetEntry());
            return true;
          })) {
    return false;
  }

  // Finally, the public sets.
  return ForEachPublicSetEntry([&](const SchemefulSite& site,
                                   const FirstPartySetEntry& entry) {
    if ((!config || !config->Contains(site)) && !manual_config_.Contains(site))
      return f(site, entry);
    return true;
  });
}

std::ostream& operator<<(std::ostream& os, const GlobalFirstPartySets& sets) {
  os << "{entries = {";
  for (const auto& [site, entry] : sets.entries_) {
    os << "{" << site.Serialize() << ": " << entry << "}, ";
  }
  os << "}, aliases = {";
  for (const auto& [alias, canonical] : sets.aliases_) {
    os << "{" << alias.Serialize() << ": " << canonical.Serialize() << "}, ";
  }
  os << "}, manual_config = {";
  sets.ForEachManualConfigEntry(
      [&](const net::SchemefulSite& site,
          const FirstPartySetEntryOverride& override) {
        os << "{" << site.Serialize() << ": " << override << "},";
        return true;
      });
  os << "}}";
  return os;
}

}  // namespace net
