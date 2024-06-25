// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/first_party_sets/global_first_party_sets.h"

#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/function_ref.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/types/optional_util.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/addition_overlaps_union_find.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_entry_override.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/first_party_sets_validator.h"
#include "net/first_party_sets/local_set_declaration.h"

namespace net {

namespace {

using FlattenedSets = base::flat_map<SchemefulSite, FirstPartySetEntry>;
using SingleSet = base::flat_map<SchemefulSite, FirstPartySetEntry>;

// Converts a list of First-Party Sets from a SingleSet to a FlattenedSet
// representation.
FlattenedSets Flatten(const std::vector<SingleSet>& set_list) {
  FlattenedSets sets;
  for (const auto& set : set_list) {
    for (const auto& site_and_entry : set) {
      bool inserted = sets.emplace(site_and_entry).second;
      CHECK(inserted);
    }
  }
  return sets;
}

std::pair<SchemefulSite, FirstPartySetEntryOverride>
SiteAndEntryToSiteAndOverride(
    const std::pair<SchemefulSite, FirstPartySetEntry>& pair) {
  return std::make_pair(pair.first, FirstPartySetEntryOverride(pair.second));
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
          FirstPartySetsContextConfig(),
          base::flat_map<SchemefulSite, SchemefulSite>()) {}

GlobalFirstPartySets::GlobalFirstPartySets(
    base::Version public_sets_version,
    base::flat_map<SchemefulSite, FirstPartySetEntry> entries,
    base::flat_map<SchemefulSite, SchemefulSite> aliases,
    FirstPartySetsContextConfig manual_config,
    base::flat_map<SchemefulSite, SchemefulSite> manual_aliases)
    : public_sets_version_(std::move(public_sets_version)),
      entries_(std::move(entries)),
      aliases_(std::move(aliases)),
      manual_config_(std::move(manual_config)),
      manual_aliases_(std::move(manual_aliases)) {
  if (!public_sets_version_.IsValid()) {
    CHECK(entries_.empty());
    CHECK(aliases_.empty());
  }

  CHECK(base::ranges::all_of(aliases_, [&](const auto& pair) {
    return entries_.contains(pair.second);
  }));
  CHECK(IsValid(), base::NotFatalUntil::M130) << "Sets must be valid";
}

GlobalFirstPartySets::GlobalFirstPartySets(GlobalFirstPartySets&&) = default;
GlobalFirstPartySets& GlobalFirstPartySets::operator=(GlobalFirstPartySets&&) =
    default;

GlobalFirstPartySets::~GlobalFirstPartySets() = default;

bool GlobalFirstPartySets::operator==(const GlobalFirstPartySets& other) const =
    default;

bool GlobalFirstPartySets::operator!=(const GlobalFirstPartySets& other) const =
    default;

GlobalFirstPartySets GlobalFirstPartySets::Clone() const {
  return GlobalFirstPartySets(public_sets_version_, entries_, aliases_,
                              manual_config_.Clone(), manual_aliases_);
}

std::optional<FirstPartySetEntry> GlobalFirstPartySets::FindEntry(
    const SchemefulSite& site,
    const FirstPartySetsContextConfig& config) const {
  return FindEntry(site, &config);
}

std::optional<FirstPartySetEntry> GlobalFirstPartySets::FindEntry(
    const SchemefulSite& site,
    const FirstPartySetsContextConfig* config) const {
  // Check if `site` can be found in the customizations first.
  if (config) {
    if (const auto override = config->FindOverride(site);
        override.has_value()) {
      return override->IsDeletion() ? std::nullopt
                                    : std::make_optional(override->GetEntry());
    }
  }

  // Now see if it's in the manual config (with or without a manual alias).
  if (const auto manual_override = manual_config_.FindOverride(site);
      manual_override.has_value()) {
    return manual_override->IsDeletion()
               ? std::nullopt
               : std::make_optional(manual_override->GetEntry());
  }

  // Finally, look up in `entries_`, applying an alias if applicable.
  const auto canonical_it = aliases_.find(site);
  const SchemefulSite& canonical_site =
      canonical_it == aliases_.end() ? site : canonical_it->second;
  if (const auto entry_it = entries_.find(canonical_site);
      entry_it != entries_.end()) {
    return entry_it->second;
  }

  return std::nullopt;
}

base::flat_map<SchemefulSite, FirstPartySetEntry>
GlobalFirstPartySets::FindEntries(
    const base::flat_set<SchemefulSite>& sites,
    const FirstPartySetsContextConfig& config) const {
  std::vector<std::pair<SchemefulSite, FirstPartySetEntry>> sites_to_entries;
  for (const SchemefulSite& site : sites) {
    const std::optional<FirstPartySetEntry> entry = FindEntry(site, config);
    if (entry.has_value()) {
      sites_to_entries.emplace_back(site, entry.value());
    }
  }
  return sites_to_entries;
}

FirstPartySetMetadata GlobalFirstPartySets::ComputeMetadata(
    const SchemefulSite& site,
    const SchemefulSite* top_frame_site,
    const FirstPartySetsContextConfig& fps_context_config) const {
  std::optional<FirstPartySetEntry> top_frame_entry =
      top_frame_site ? FindEntry(*top_frame_site, fps_context_config)
                     : std::nullopt;

  return FirstPartySetMetadata(
      base::OptionalToPtr(FindEntry(site, fps_context_config)),
      base::OptionalToPtr(top_frame_entry));
}

void GlobalFirstPartySets::ApplyManuallySpecifiedSet(
    const LocalSetDeclaration& local_set_declaration) {
  CHECK(manual_config_.empty());
  CHECK(manual_aliases_.empty());
  if (local_set_declaration.empty()) {
    // Nothing to do.
    return;
  }

  base::flat_map<SchemefulSite, SchemefulSite> manual_aliases =
      local_set_declaration.aliases();

  base::flat_map<SchemefulSite, FirstPartySetEntry> manual_entries =
      local_set_declaration.entries();
  for (const auto& [alias, canonical] : manual_aliases) {
    manual_entries.emplace(alias, manual_entries.find(canonical)->second);
  }

  // We handle the manually-specified set the same way as we handle
  // replacement enterprise policy sets.
  manual_config_ = ComputeConfig(SetsMutation(
      /*replacement_sets=*/{manual_entries},
      /*addition_sets=*/{}));
  manual_aliases_ = std::move(manual_aliases);

  CHECK(IsValid(), base::NotFatalUntil::M130) << "Sets must be valid";
}

void GlobalFirstPartySets::UnsafeSetManualConfig(
    FirstPartySetsContextConfig manual_config) {
  CHECK(manual_config_.empty());
  manual_config_ = std::move(manual_config);
}

base::flat_map<SchemefulSite, FirstPartySetEntry>
GlobalFirstPartySets::FindPrimariesAffectedByAdditions(
    const FlattenedSets& additions) const {
  std::vector<std::pair<SchemefulSite, FirstPartySetEntry>>
      addition_intersected_primaries;
  for (const auto& [new_member, new_entry] : additions) {
    if (const auto entry = FindEntry(new_member, /*config=*/nullptr);
        entry.has_value()) {
      // Found an overlap with the existing list of sets.
      addition_intersected_primaries.emplace_back(entry->primary(), new_entry);
    }
  }
  return addition_intersected_primaries;
}

std::pair<base::flat_map<SchemefulSite, base::flat_set<SchemefulSite>>,
          base::flat_set<SchemefulSite>>
GlobalFirstPartySets::FindPrimariesAffectedByReplacements(
    const FlattenedSets& replacements,
    const FlattenedSets& additions,
    const base::flat_map<SchemefulSite, FirstPartySetEntry>&
        addition_intersected_primaries) const {
  if (replacements.empty()) {
    return {{}, {}};
  }

  const auto canonicalize = [&](const SchemefulSite& site) {
    const auto it = aliases_.find(site);
    return it != aliases_.end() ? it->second : site;
  };
  std::map<SchemefulSite, std::set<SchemefulSite>> canonical_to_aliases;
  ForEachAlias([&](const SchemefulSite& alias, const SchemefulSite& canonical) {
    canonical_to_aliases[canonical].insert(alias);
  });
  // Runs the given FunctionRef for all (existing) variants of the given site,
  // i.e. all the aliases and the "canonical" variant.
  const auto for_all_variants =
      [canonical_to_aliases = std::move(canonical_to_aliases),
       canonicalize = std::move(canonicalize)](
          const SchemefulSite& site,
          const base::FunctionRef<void(const SchemefulSite&)> f) {
        const SchemefulSite canonical = canonicalize(site);
        f(canonical);
        if (const auto it = canonical_to_aliases.find(canonical);
            it != canonical_to_aliases.end()) {
          for (const auto& alias : it->second) {
            f(alias);
          }
        }
      };

  // Maps an existing primary site to the members it lost due to replacement.
  base::flat_map<SchemefulSite, base::flat_set<SchemefulSite>>
      potential_singletons;
  // Stores existing primary sites which have left their sets (via
  // replacement), and whose existing members should be removed from the set
  // (excluding any custom sets that those members are involved in).
  base::flat_set<SchemefulSite> replaced_existing_primaries;
  for (const auto& [new_site, unused_entry] : replacements) {
    const auto existing_entry = FindEntry(new_site, /*config=*/nullptr);
    if (!existing_entry.has_value()) {
      continue;
    }
    if (!addition_intersected_primaries.contains(existing_entry->primary()) &&
        !additions.contains(existing_entry->primary()) &&
        !replacements.contains(existing_entry->primary())) {
      // The existing site's primary isn't involved in any of the customized
      // sets, so it might become a singleton (if all of its variants and
      // non-primaries [and their variants] are replaced by the
      // customizations).
      for_all_variants(new_site, [&](const SchemefulSite& variant) {
        if (existing_entry->primary() != variant) {
          potential_singletons[existing_entry->primary()].insert(variant);
        }
      });
    }

    if (existing_entry->primary() == new_site) {
      // `new_site` was a primary in the existing sets, but is in the
      // replacement sets, so its non-primaries (and aliases) might need to be
      // deleted/hidden.
      bool inserted =
          replaced_existing_primaries.emplace(existing_entry->primary()).second;
      CHECK(inserted);
    }
  }

  return std::make_pair(potential_singletons, replaced_existing_primaries);
}

FirstPartySetsContextConfig GlobalFirstPartySets::ComputeConfig(
    const SetsMutation& mutation) const {
  if (base::ranges::all_of(mutation.replacements(), &SingleSet::empty) &&
      base::ranges::all_of(mutation.additions(), &SingleSet::empty)) {
    // Nothing to do.
    return FirstPartySetsContextConfig();
  }

  const FlattenedSets replacements = Flatten(mutation.replacements());
  const FlattenedSets additions =
      Flatten(NormalizeAdditionSets(mutation.additions()));

  // Maps a site to its override.
  std::vector<std::pair<SchemefulSite, FirstPartySetEntryOverride>>
      site_to_override;
  base::ranges::transform(replacements, std::back_inserter(site_to_override),
                          SiteAndEntryToSiteAndOverride);
  base::ranges::transform(additions, std::back_inserter(site_to_override),
                          SiteAndEntryToSiteAndOverride);

  // Maps old primary site to new entry.
  const base::flat_map<SchemefulSite, FirstPartySetEntry>
      addition_intersected_primaries =
          FindPrimariesAffectedByAdditions(additions);

  auto [potential_singletons, replaced_existing_primaries] =
      FindPrimariesAffectedByReplacements(replacements, additions,
                                          addition_intersected_primaries);

  if (!addition_intersected_primaries.empty() ||
      !potential_singletons.empty() || !replaced_existing_primaries.empty()) {
    // Find out which potential singletons are actually singletons; delete
    // members whose primaries left; and reparent the sets that intersected with
    // an addition set.
    // Note: use a null config here, to avoid taking unrelated policy sets into
    // account.
    ForEachEffectiveSetEntry(
        /*config=*/nullptr,
        [&](const SchemefulSite& member, const FirstPartySetEntry& set_entry) {
          // Reparent all sites in any intersecting addition sets.
          if (const auto entry =
                  addition_intersected_primaries.find(set_entry.primary());
              entry != addition_intersected_primaries.end() &&
              !replacements.contains(member)) {
            site_to_override.emplace_back(
                member, FirstPartySetEntry(entry->second.primary(),
                                           member == entry->second.primary()
                                               ? SiteType::kPrimary
                                               : SiteType::kAssociated,
                                           std::nullopt));
          }
          if (member == set_entry.primary())
            return true;
          // Remove non-singletons from the potential list.
          if (const auto entry = potential_singletons.find(set_entry.primary());
              entry != potential_singletons.end() &&
              !entry->second.contains(member)) {
            // This primary lost members, but it still has at least one
            // (`member`), so it's not a singleton.
            potential_singletons.erase(entry);
          }
          // Remove members from sets whose primary left.
          if (replaced_existing_primaries.contains(set_entry.primary()) &&
              !replacements.contains(member) &&
              !addition_intersected_primaries.contains(set_entry.primary())) {
            site_to_override.emplace_back(member, FirstPartySetEntryOverride());
          }

          return true;
        });

    // Any primary remaining in `potential_singleton` is a real singleton, so
    // delete it:
    for (const auto& [primary, members] : potential_singletons) {
      site_to_override.emplace_back(primary, FirstPartySetEntryOverride());
    }
  }

  // For every pre-existing alias that would now refer to a site in the overlay,
  // which is not already contained in the overlay, we explicitly ignore that
  // alias.
  ForEachAlias([&](const SchemefulSite& alias, const SchemefulSite& canonical) {
    if (base::Contains(
            site_to_override, canonical,
            &std::pair<SchemefulSite, FirstPartySetEntryOverride>::first) &&
        !base::Contains(
            site_to_override, alias,
            &std::pair<SchemefulSite, FirstPartySetEntryOverride>::first)) {
      site_to_override.emplace_back(alias, FirstPartySetEntryOverride());
    }
  });

  FirstPartySetsContextConfig config(std::move(site_to_override));
  CHECK(IsValid(&config), base::NotFatalUntil::M130)
      << "Sets must not contain singleton or orphan";
  return config;
}

std::vector<base::flat_map<SchemefulSite, FirstPartySetEntry>>
GlobalFirstPartySets::NormalizeAdditionSets(
    const std::vector<base::flat_map<SchemefulSite, FirstPartySetEntry>>&
        addition_sets) const {
  if (base::ranges::all_of(addition_sets, &SingleSet::empty)) {
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
                                            std::nullopt))
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

void GlobalFirstPartySets::ForEachAlias(
    base::FunctionRef<void(const SchemefulSite&, const SchemefulSite&)> f)
    const {
  for (const auto& [alias, site] : manual_aliases_) {
    f(alias, site);
  }
  for (const auto& [alias, site] : aliases_) {
    if (manual_config_.Contains(alias)) {
      continue;
    }
    f(alias, site);
  }
}

bool GlobalFirstPartySets::IsValid(
    const FirstPartySetsContextConfig* config) const {
  FirstPartySetsValidator validator;
  ForEachEffectiveSetEntry(
      config,
      [&](const SchemefulSite& site, const FirstPartySetEntry& entry) -> bool {
        validator.Update(site, entry.primary());
        return true;
      });

  return validator.IsValid();
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
  os << "}, manual_aliases = {";
  for (const auto& [alias, canonical] : sets.manual_aliases_) {
    os << "{" << alias.Serialize() << ": " << canonical.Serialize() << "}, ";
  }
  os << "}}";
  return os;
}

}  // namespace net
