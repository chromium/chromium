// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_sets_loader.h"

#include <set>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/strings/string_split.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "content/browser/first_party_sets/addition_overlaps_union_find.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

absl::optional<FirstPartySetsLoader::SingleSet> CanonicalizeSet(
    const std::vector<std::string>& origins) {
  if (origins.empty())
    return absl::nullopt;

  const absl::optional<net::SchemefulSite> maybe_owner =
      content::FirstPartySetParser::CanonicalizeRegisteredDomain(
          origins[0], true /* emit_errors */);
  if (!maybe_owner.has_value()) {
    LOG(ERROR) << "First-Party Set owner is not valid; aborting.";
    return absl::nullopt;
  }

  const net::SchemefulSite& owner = *maybe_owner;
  base::flat_set<net::SchemefulSite> members;
  for (auto it = origins.begin() + 1; it != origins.end(); ++it) {
    const absl::optional<net::SchemefulSite> maybe_member =
        content::FirstPartySetParser::CanonicalizeRegisteredDomain(
            *it, true /* emit_errors */);
    if (maybe_member.has_value() && maybe_member != owner)
      members.emplace(std::move(*maybe_member));
  }

  if (members.empty()) {
    LOG(ERROR) << "No valid First-Party Set members were specified; aborting.";
    return absl::nullopt;
  }

  return absl::make_optional(
      std::make_pair(std::move(owner), std::move(members)));
}

std::string ReadSetsFile(base::File sets_file) {
  std::string raw_sets;
  base::ScopedFILE file(FileToFILE(std::move(sets_file), "r"));
  return base::ReadStreamToString(file.get(), &raw_sets) ? raw_sets : "";
}

// Creates a set of SchemefulSites present with the given list of SingleSets.
base::flat_set<net::SchemefulSite> FlattenSingleSetList(
    const std::vector<content::FirstPartySetsLoader::SingleSet>& sets) {
  std::vector<net::SchemefulSite> sites;
  for (const content::FirstPartySetsLoader::SingleSet& set : sets) {
    sites.push_back(set.first);
    sites.insert(sites.end(), set.second.begin(), set.second.end());
  }
  return sites;
}

// Populates the `policy_set_overlaps` out-parameter by checking
// `existing_sets`. If `site` is equal to an existing site e in `sets`, then
// `policy_set_index` will be added to the list of set indices at
// `policy_set_overlaps`[e].
void AddIfPolicySetOverlaps(
    const net::SchemefulSite& site,
    size_t policy_set_index,
    FirstPartySetsLoader::FlattenedSets existing_sets,
    base::flat_map<net::SchemefulSite, base::flat_set<size_t>>&
        policy_set_overlaps) {
  // Check `site` for membership in `existing_sets`.
  if (auto it = existing_sets.find(site); it != existing_sets.end()) {
    // Add the index of `site`'s policy set to the list of policy set indices
    // that also overlap with site_owner.
    auto [site_and_sets, inserted] =
        policy_set_overlaps.insert({it->second, {}});
    site_and_sets->second.insert(policy_set_index);
  }
}

}  // namespace

FirstPartySetsLoader::FirstPartySetsLoader(
    LoadCompleteOnceCallback on_load_complete,
    base::Value::Dict policy_overrides)
    : on_load_complete_(std::move(on_load_complete)) {
  FirstPartySetParser::ParsedPolicySetLists out_sets;
  auto error = FirstPartySetParser::ParseSetsFromEnterprisePolicy(
      policy_overrides, &out_sets);
  if (!error.has_value())
    policy_overrides_ = out_sets;
}

FirstPartySetsLoader::~FirstPartySetsLoader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FirstPartySetsLoader::SetManuallySpecifiedSet(
    const std::string& flag_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  manually_specified_set_ = {CanonicalizeSet(base::SplitString(
      flag_value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY))};
  UmaHistogramTimes(
      "Cookie.FirstPartySets.InitializationDuration.ReadCommandLineSet2",
      construction_timer_.Elapsed());

  MaybeFinishLoading();
}

void FirstPartySetsLoader::SetComponentSets(base::File sets_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (component_sets_parse_progress_ != Progress::kNotStarted) {
    DisposeFile(std::move(sets_file));
    return;
  }

  component_sets_parse_progress_ = Progress::kStarted;

  if (!sets_file.IsValid()) {
    OnReadSetsFile("");
    return;
  }

  // We use USER_BLOCKING here since First-Party Set initialization blocks
  // network navigations at startup.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&ReadSetsFile, std::move(sets_file)),
      base::BindOnce(&FirstPartySetsLoader::OnReadSetsFile,
                     weak_factory_.GetWeakPtr()));
}

void FirstPartySetsLoader::OnReadSetsFile(const std::string& raw_sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(component_sets_parse_progress_, Progress::kStarted);

  std::istringstream stream(raw_sets);
  sets_ = FirstPartySetParser::ParseSetsFromStream(stream);

  component_sets_parse_progress_ = Progress::kFinished;
  UmaHistogramTimes(
      "Cookie.FirstPartySets.InitializationDuration.ReadComponentSets2",
      construction_timer_.Elapsed());
  MaybeFinishLoading();
}

void FirstPartySetsLoader::DisposeFile(base::File sets_file) {
  if (sets_file.IsValid()) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            [](base::File sets_file) {
              // Run `sets_file`'s dtor in the threadpool.
            },
            std::move(sets_file)));
  }
}

std::vector<FirstPartySetsLoader::SingleSet>
FirstPartySetsLoader::NormalizeAdditionSets(
    const FlattenedSets& existing_sets,
    const std::vector<SingleSet>& addition_sets) {
  // Create a mapping from an owner site in `existing_sets` to all policy sets
  // that intersect with the set that it owns.
  base::flat_map<net::SchemefulSite, base::flat_set<size_t>>
      policy_set_overlaps;
  for (size_t set_idx = 0; set_idx < addition_sets.size(); set_idx++) {
    const net::SchemefulSite& owner = addition_sets[set_idx].first;
    AddIfPolicySetOverlaps(owner, set_idx, existing_sets, policy_set_overlaps);
    for (const net::SchemefulSite& member : addition_sets[set_idx].second) {
      AddIfPolicySetOverlaps(member, set_idx, existing_sets,
                             policy_set_overlaps);
    }
  }

  AdditionOverlapsUnionFind union_finder(addition_sets.size());
  for (auto& [public_site, policy_set_indices] : policy_set_overlaps) {
    // Union together all overlapping policy sets to determine which one will
    // take ownership.
    for (size_t representative : policy_set_indices) {
      union_finder.Union(*policy_set_indices.begin(), representative);
    }
  }

  // The union-find data structure now knows which policy set should be given
  // the role of representative for each entry in policy_set_overlaps.
  // AdditionOverlapsUnionFind::SetsMapping returns a map from representative
  // index to list of its children.
  std::vector<SingleSet> normalized_additions;
  for (auto& [rep, children] : union_finder.SetsMapping()) {
    SingleSet normalized = addition_sets[rep];
    for (size_t child_set_idx : children) {
      // Update normalized to absorb the child_set_idx-th addition set.
      const SingleSet& child_set = addition_sets[child_set_idx];
      normalized.second.insert(child_set.first);
      normalized.second.insert(child_set.second.begin(),
                               child_set.second.end());
    }
    normalized_additions.push_back(normalized);
  }
  return normalized_additions;
}

void FirstPartySetsLoader::ApplyManuallySpecifiedSet() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(HasAllInputs());
  if (!manually_specified_set_.value().has_value())
    return;
  ApplyReplacementOverrides({manually_specified_set_->value()});
  RemoveAllSingletons();
}

void FirstPartySetsLoader::ApplyReplacementOverrides(
    const std::vector<SingleSet>& override_sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(HasAllInputs());

  base::flat_set<net::SchemefulSite> all_override_sites =
      FlattenSingleSetList(override_sets);

  // Erase the intersection between |sets_| and the list of |override_sets| and
  // any members whose owner was in the intersection.
  base::EraseIf(
      sets_, [&all_override_sites](
                 const std::pair<net::SchemefulSite, net::SchemefulSite>& p) {
        return all_override_sites.contains(p.first) ||
               all_override_sites.contains(p.second);
      });

  // Next, we must add each site in the override_sets to |sets_|.
  for (auto& [owner, members] : override_sets) {
    sets_.emplace(owner, owner);
    for (const net::SchemefulSite& member : members) {
      sets_.emplace(member, owner);
    }
  }
}

void FirstPartySetsLoader::ApplyAdditionOverrides(
    const std::vector<SingleSet>& new_sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(HasAllInputs());

  if (new_sets.empty())
    return;

  std::vector<SingleSet> normalized_additions =
      NormalizeAdditionSets(sets_, new_sets);

  FlattenedSets flattened_additions;
  for (const auto& [owner, members] : normalized_additions) {
    for (const net::SchemefulSite& member : members)
      flattened_additions.emplace(member, owner);
    flattened_additions.emplace(owner, owner);
  }

  // Identify intersections between addition sets and existing sets. This will
  // be used to reparent existing sets if they intersect with an addition set.
  //
  // Since we reparent every member of an existing set (regardless of whether
  // the intersection was via one of its members or its owner), we just keep
  // track of the set itself, via its owner.
  base::flat_map<net::SchemefulSite, net::SchemefulSite> owners_in_intersection;
  for (const auto& [site, owner] : flattened_additions) {
    // Found an overlap with an existing set. Add the existing owner to the
    // map.
    if (auto it = sets_.find(site); it != sets_.end()) {
      owners_in_intersection[it->second] = owner;
    }
  }

  // Update the (site, owner) mappings in sets_ such that if owner is in the
  // intersection, then the site is mapped to owners_in_intersection[owner].
  //
  // This reparents existing sets to their owner given by the normalized
  // addition sets.
  for (auto& [site, owner] : sets_) {
    if (auto owner_entry = owners_in_intersection.find(owner);
        owner_entry != owners_in_intersection.end()) {
      owner = owner_entry->second;
    }
  }

  // Since the intersection between sets_ and flattened_additions has already
  // been updated above, we can insert flattened_additions into sets_ without
  // affecting any existing mappings in sets_.
  sets_.insert(flattened_additions.begin(), flattened_additions.end());
}

void FirstPartySetsLoader::RemoveAllSingletons() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Now remove singleton sets, which are sets that just contain sites that
  // *are* owners, but no longer have any (other) members.
  std::set<net::SchemefulSite> owners_with_members;
  for (const auto& it : sets_) {
    if (it.first != it.second)
      owners_with_members.insert(it.second);
  }
  base::EraseIf(sets_, [&owners_with_members](const auto& p) {
    return p.first == p.second && !base::Contains(owners_with_members, p.first);
  });
}

void FirstPartySetsLoader::ApplyAllPolicyOverrides() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(HasAllInputs());
  ApplyReplacementOverrides(policy_overrides_.replacements);
  ApplyAdditionOverrides(policy_overrides_.additions);
  RemoveAllSingletons();
}

void FirstPartySetsLoader::MaybeFinishLoading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!HasAllInputs())
    return;
  ApplyManuallySpecifiedSet();
  ApplyAllPolicyOverrides();
  std::move(on_load_complete_).Run(std::move(sets_));
}

bool FirstPartySetsLoader::HasAllInputs() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return component_sets_parse_progress_ == Progress::kFinished &&
         manually_specified_set_.has_value();
}

}  // namespace content
