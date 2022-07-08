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
    LoadCompleteOnceCallback on_load_complete)
    : on_load_complete_(std::move(on_load_complete)) {}

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
  net::SchemefulSite owner = manually_specified_set_->value().first;
  base::flat_set<net::SchemefulSite> members =
      manually_specified_set_->value().second;

  // Erase the intersection between |sets_| and |manually_specified_set_| and
  // any members whose owner was in the intersection.
  base::EraseIf(
      sets_, [&owner, members](
                 const std::pair<net::SchemefulSite, net::SchemefulSite>& p) {
        return p.first == owner || p.second == owner ||
               members.contains(p.first) || members.contains(p.second);
      });

  // Next, we must add the manually specified set to |sets_|.
  sets_.emplace(owner, owner);
  for (const net::SchemefulSite& member : members) {
    sets_.emplace(member, owner);
  }
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

void FirstPartySetsLoader::MaybeFinishLoading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!HasAllInputs())
    return;
  ApplyManuallySpecifiedSet();
  std::move(on_load_complete_).Run(std::move(sets_));
}

bool FirstPartySetsLoader::HasAllInputs() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return component_sets_parse_progress_ == Progress::kFinished &&
         manually_specified_set_.has_value();
}

}  // namespace content
