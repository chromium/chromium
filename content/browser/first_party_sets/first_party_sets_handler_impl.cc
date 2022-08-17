// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_sets_handler_impl.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "content/browser/first_party_sets/addition_overlaps_union_find.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "content/browser/first_party_sets/first_party_sets_loader.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "content/public/common/content_client.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/first_party_set_entry.h"
#include "services/network/public/mojom/first_party_sets.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {
using FlattenedSets = FirstPartySetsHandlerImpl::FlattenedSets;
using SingleSet = FirstPartySetParser::SingleSet;

constexpr base::FilePath::CharType kPersistedFirstPartySetsFileName[] =
    FILE_PATH_LITERAL("persisted_first_party_sets.json");

// Reads the sets as raw JSON from their storage file, returning the raw sets on
// success and empty string on failure.
std::string LoadSetsFromDisk(const base::FilePath& path) {
  DCHECK(!path.empty());

  std::string result;
  if (!base::ReadFileToString(path, &result)) {
    VLOG(1) << "Failed loading serialized First-Party Sets file from "
            << path.MaybeAsASCII();
    return "";
  }
  return result;
}

// Writes the sets as raw JSON to the storage file.
void MaybeWriteSetsToDisk(const base::FilePath& path, base::StringPiece sets) {
  DCHECK(!path.empty());
  if (!base::ImportantFileWriter::WriteFileAtomically(path, sets)) {
    VLOG(1) << "Failed writing serialized First-Party Sets to file "
            << path.MaybeAsASCII();
  }
}

// Converts a list of First-Party Sets from a SingleSet to a FlattenedSet
// representation.
FlattenedSets SetListToFlattenedSets(const std::vector<SingleSet>& set_list) {
  FlattenedSets sets;
  for (const auto& set : set_list) {
    for (const auto& site_and_entry : set) {
      bool inserted =
          sets.emplace(site_and_entry.first, site_and_entry.second).second;
      DCHECK(inserted);
    }
  }
  return sets;
}

// Adds all sets in a list of First-Party Sets into `site_to_owner` which maps
// from a site to its owner.
void UpdateCustomizationMap(
    const std::vector<SingleSet>& set_list,
    FirstPartySetsHandlerImpl::PolicyCustomization& site_to_entry) {
  for (const auto& set : set_list) {
    for (const auto& site_and_entry : set) {
      bool inserted =
          site_to_entry.emplace(site_and_entry.first, site_and_entry.second)
              .second;
      DCHECK(inserted);
    }
  }
}

// Populates the `policy_set_overlaps` out-parameter by checking
// `existing_sets`. If `site` is equal to an existing site e in `sets`, then
// `policy_set_index` will be added to the list of set indices at
// `policy_set_overlaps`[e].
void AddIfPolicySetOverlaps(
    const net::SchemefulSite& site,
    size_t policy_set_index,
    const FlattenedSets& existing_sets,
    base::flat_map<net::SchemefulSite, base::flat_set<size_t>>&
        policy_set_overlaps) {
  // Check `site` for membership in `existing_sets`.
  if (auto it = existing_sets.find(site); it != existing_sets.end()) {
    // Add the index of `site`'s policy set to the list of policy set indices
    // that also overlap with site_owner.
    auto [site_and_sets, inserted] =
        policy_set_overlaps.insert({it->second.primary(), {}});
    site_and_sets->second.insert(policy_set_index);
  }
}

std::vector<SingleSet> NormalizeAdditionSets(
    const network::mojom::PublicFirstPartySetsPtr& public_sets,
    const std::vector<SingleSet>& addition_sets) {
  // Create a mapping from an owner site in `existing_sets` to all policy sets
  // that intersect with the set that it owns.
  base::flat_map<net::SchemefulSite, base::flat_set<size_t>>
      policy_set_overlaps;
  for (size_t set_idx = 0; set_idx < addition_sets.size(); set_idx++) {
    for (const auto& site_and_entry : addition_sets[set_idx]) {
      AddIfPolicySetOverlaps(site_and_entry.first, set_idx, public_sets->sets,
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
    const net::SchemefulSite& rep_primary =
        addition_sets[rep].begin()->second.primary();
    for (size_t child_set_idx : children) {
      // Update normalized to absorb the child_set_idx-th addition set. Rewrite
      // the entry's primary as needed.
      for (const auto& child_site_and_entry : addition_sets[child_set_idx]) {
        bool inserted =
            normalized
                .emplace(
                    child_site_and_entry.first,
                    net::FirstPartySetEntry(
                        rep_primary, net::SiteType::kAssociated, absl::nullopt))
                .second;
        DCHECK(inserted);
      }
    }
    normalized_additions.push_back(normalized);
  }
  return normalized_additions;
}

// TODO(https://crbug.com/1349487): Since this is basically the same as
// FirstPartySetsManager::FindOwnerInternal(), move the common algorithm into
// //net to be reused in both here and FirstPartySetsManager.
absl::optional<net::FirstPartySetEntry> FindOwner(
    const net::SchemefulSite& site,
    const FlattenedSets& sets,
    const FirstPartySetsHandlerImpl::PolicyCustomization& policy_sets) {
  absl::optional<net::FirstPartySetEntry> owner;
  if (const auto it = policy_sets.find(site); it != policy_sets.end()) {
    owner = it->second;
  } else if (const auto it = sets.find(site); it != sets.end()) {
    owner = it->second;
  }
  return owner;
}

}  // namespace

bool FirstPartySetsHandler::PolicyParsingError::operator==(
    const FirstPartySetsHandler::PolicyParsingError& other) const {
  return std::tie(error, set_type, error_index) ==
         std::tie(other.error, other.set_type, other.error_index);
}

// static
FirstPartySetsHandler* FirstPartySetsHandler::GetInstance() {
  return FirstPartySetsHandlerImpl::GetInstance();
}

// static
FirstPartySetsHandlerImpl* FirstPartySetsHandlerImpl::GetInstance() {
  static base::NoDestructor<FirstPartySetsHandlerImpl> instance(
      GetContentClient()->browser()->IsFirstPartySetsEnabled(),
      GetContentClient()->browser()->WillProvidePublicFirstPartySets());
  return instance.get();
}

// static
absl::optional<FirstPartySetsHandler::PolicyParsingError>
FirstPartySetsHandler::ValidateEnterprisePolicy(
    const base::Value::Dict& policy) {
  base::expected<FirstPartySetParser::ParsedPolicySetLists,
                 FirstPartySetParser::PolicyParsingError>
      parsed = FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy);
  return parsed.has_value() ? absl::nullopt
                            : absl::make_optional(parsed.error());
}

void FirstPartySetsHandlerImpl::GetCustomizationForPolicy(
    const base::Value::Dict& policy,
    base::OnceCallback<void(FirstPartySetsHandler::PolicyCustomization)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PolicyCustomization customization;
  if (!public_sets_.is_null()) {
    std::move(callback).Run(GetCustomizationForPolicyInternal(policy));
    return;
  }
  // Add to the deque of callbacks that will be processed once the list
  // of First-Party Sets has been fully initialized.
  on_sets_ready_callbacks_.push_back(
      base::BindOnce(
          &FirstPartySetsHandlerImpl::GetCustomizationForPolicyInternal,
          // base::Unretained(this) is safe here because this is a static
          // singleton.
          base::Unretained(this), policy.Clone())
          .Then(std::move(callback)));
}

FirstPartySetsHandlerImpl::PolicyCustomization
FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
    const network::mojom::PublicFirstPartySetsPtr& public_sets,
    const FirstPartySetParser::ParsedPolicySetLists& policy) {
  // Maps a site to its new entry if it has one.
  base::flat_map<net::SchemefulSite, absl::optional<net::FirstPartySetEntry>>
      site_to_entry;

  // Normalize the addition sets to prevent them from affecting the same
  // existing set.
  std::vector<SingleSet> normalized_additions =
      NormalizeAdditionSets(public_sets, policy.additions);

  // Create flattened versions of the sets for easier lookup.
  FlattenedSets flattened_replacements =
      SetListToFlattenedSets(policy.replacements);
  FlattenedSets flattened_additions =
      SetListToFlattenedSets(normalized_additions);

  // All of the policy sets are automatically inserted into site_to_owner.
  UpdateCustomizationMap(policy.replacements, site_to_entry);
  UpdateCustomizationMap(normalized_additions, site_to_entry);

  // Maps old owner to new entry.
  base::flat_map<net::SchemefulSite, net::FirstPartySetEntry>
      addition_intersected_owners;
  for (const auto& [new_member, new_entry] : flattened_additions) {
    if (const auto entry = public_sets->sets.find(new_member);
        entry != public_sets->sets.end()) {
      // Found an overlap with the existing list of sets.
      addition_intersected_owners.emplace(entry->second.primary(), new_entry);
    }
  }

  // Maps an existing owner to the members it lost due to replacement.
  base::flat_map<net::SchemefulSite, base::flat_set<net::SchemefulSite>>
      potential_singletons;
  for (const auto& [member, set_entry] : flattened_replacements) {
    if (member == set_entry.primary())
      continue;
    if (auto entry = public_sets->sets.find(member);
        entry != public_sets->sets.end() && entry->second.primary() != member) {
      const net::FirstPartySetEntry& existing_entry = entry->second;
      if (!addition_intersected_owners.contains(existing_entry.primary()) &&
          !flattened_additions.contains(existing_entry.primary()) &&
          !flattened_replacements.contains(existing_entry.primary())) {
        potential_singletons[existing_entry.primary()].insert(member);
      }
    }
  }

  // Find the existing owners that have left their existing sets, and whose
  // existing members should be removed from their set (excl any policy sets
  // that those members are involved in).
  base::flat_set<net::SchemefulSite> replaced_existing_owners;
  for (const auto& [site, unused_owner] : flattened_replacements) {
    if (const auto entry = public_sets->sets.find(site);
        entry != public_sets->sets.end() && entry->second.primary() == site) {
      // Site was an owner in the existing sets.
      bool inserted = replaced_existing_owners.emplace(site).second;
      DCHECK(inserted);
    }
  }

  // Find out which potential singletons are actually singletons; delete
  // members whose owners left; and reparent the sets that intersected with
  // an addition set.
  for (const auto& [member, set_entry] : public_sets->sets) {
    // Reparent all sites in any intersecting addition sets.
    if (auto entry = addition_intersected_owners.find(set_entry.primary());
        entry != addition_intersected_owners.end() &&
        !flattened_replacements.contains(member)) {
      site_to_entry.emplace(
          member, net::FirstPartySetEntry(entry->second.primary(),
                                          member == entry->second.primary()
                                              ? net::SiteType::kPrimary
                                              : net::SiteType::kAssociated,
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

  return site_to_entry;
}

FirstPartySetsHandlerImpl::FirstPartySetsHandlerImpl(
    bool enabled,
    bool embedder_will_provide_public_sets)
    : enabled_(enabled),
      embedder_will_provide_public_sets_(enabled &&
                                         embedder_will_provide_public_sets) {
  sets_loader_ = std::make_unique<FirstPartySetsLoader>(
      base::BindOnce(&FirstPartySetsHandlerImpl::SetCompleteSets,
                     // base::Unretained(this) is safe here because
                     // this is a static singleton.
                     base::Unretained(this)));
}

FirstPartySetsHandlerImpl::~FirstPartySetsHandlerImpl() = default;

absl::optional<network::mojom::PublicFirstPartySetsPtr>
FirstPartySetsHandlerImpl::GetSets(SetsReadyOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsEnabled());
  if (!public_sets_.is_null())
    return public_sets_->Clone();

  if (!callback.is_null()) {
    // base::Unretained(this) is safe here because this is a static singleton.
    on_sets_ready_callbacks_.push_back(
        base::BindOnce(&FirstPartySetsHandlerImpl::GetSetsSync,
                       base::Unretained(this))
            .Then(std::move(callback)));
  }

  return absl::nullopt;
}

void FirstPartySetsHandlerImpl::Init(const base::FilePath& user_data_dir,
                                     const std::string& flag_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!initialized_);
  DCHECK(persisted_sets_path_.empty());

  initialized_ = true;
  SetPersistedSets(user_data_dir);

  if (IsEnabled()) {
    sets_loader_->SetManuallySpecifiedSet(flag_value);
    if (!embedder_will_provide_public_sets_) {
      sets_loader_->SetComponentSets(base::File());
    }
  } else {
    SetCompleteSets(network::mojom::PublicFirstPartySets::New());
  }
}

bool FirstPartySetsHandlerImpl::IsEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return enabled_;
}

void FirstPartySetsHandlerImpl::SetPublicFirstPartySets(base::File sets_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(enabled_);
  DCHECK(embedder_will_provide_public_sets_);
  sets_loader_->SetComponentSets(std::move(sets_file));
}

void FirstPartySetsHandlerImpl::ResetForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  initialized_ = false;
  enabled_ = GetContentClient()->browser()->IsFirstPartySetsEnabled();
  embedder_will_provide_public_sets_ =
      GetContentClient()->browser()->WillProvidePublicFirstPartySets();

  // Initializes the `sets_loader_` member with a callback to SetCompleteSets
  // and the result of content::GetFirstPartySetsOverrides.
  sets_loader_ = std::make_unique<FirstPartySetsLoader>(
      base::BindOnce(&FirstPartySetsHandlerImpl::SetCompleteSets,
                     // base::Unretained(this) is safe here because
                     // this is a static singleton.
                     base::Unretained(this)));
  on_sets_ready_callbacks_.clear();
  persisted_sets_path_ = base::FilePath();
  public_sets_ = nullptr;
  raw_persisted_sets_ = absl::nullopt;
}

void FirstPartySetsHandlerImpl::SetPersistedSets(
    const base::FilePath& user_data_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!raw_persisted_sets_.has_value());
  DCHECK(persisted_sets_path_.empty());
  if (user_data_dir.empty()) {
    VLOG(1) << "Empty path. Failed loading serialized First-Party Sets file.";
    // We have to continue, in case the embedder has enabled FPS but has not
    // provided a directory to store persisted sets.
    OnReadPersistedSetsFile("");
    return;
  }
  persisted_sets_path_ = user_data_dir.Append(kPersistedFirstPartySetsFileName);

  // We use USER_BLOCKING here since First-Party Set initialization blocks
  // network navigations at startup.
  //
  // base::Unretained(this) is safe because this is a static singleton.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&LoadSetsFromDisk, persisted_sets_path_),
      base::BindOnce(&FirstPartySetsHandlerImpl::OnReadPersistedSetsFile,
                     base::Unretained(this)));
}

void FirstPartySetsHandlerImpl::OnReadPersistedSetsFile(
    const std::string& raw_persisted_sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!raw_persisted_sets_.has_value());
  raw_persisted_sets_ = raw_persisted_sets;
  UmaHistogramTimes(
      "Cookie.FirstPartySets.InitializationDuration.ReadPersistedSets2",
      construction_timer_.Elapsed());

  if (!public_sets_.is_null()) {
    ClearSiteDataOnChangedSets();

    if (IsEnabled()) {
      InvokePendingQueries();
    }
  }
}

void FirstPartySetsHandlerImpl::SetCompleteSets(
    network::mojom::PublicFirstPartySetsPtr public_sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(public_sets_.is_null());
  DCHECK(!public_sets.is_null());
  public_sets_ = std::move(public_sets);

  if (raw_persisted_sets_.has_value()) {
    ClearSiteDataOnChangedSets();

    if (IsEnabled()) {
      InvokePendingQueries();
    }
  }
}

void FirstPartySetsHandlerImpl::InvokePendingQueries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (!on_sets_ready_callbacks_.empty()) {
    base::OnceCallback callback = std::move(on_sets_ready_callbacks_.front());
    on_sets_ready_callbacks_.pop_front();
    std::move(callback).Run();
  }
  on_sets_ready_callbacks_.shrink_to_fit();
}

network::mojom::PublicFirstPartySetsPtr FirstPartySetsHandlerImpl::GetSetsSync()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!public_sets_.is_null());
  return public_sets_->Clone();
}

// static
base::flat_set<net::SchemefulSite> FirstPartySetsHandlerImpl::ComputeSetsDiff(
    const FlattenedSets& old_sets,
    const PolicyCustomization& old_policy,
    const FlattenedSets& current_sets,
    const PolicyCustomization& current_policy) {
  // TODO(https://crbug.com/1219656): For now we don't clear site data if FPSs
  // is disabled. This may change with future feature ruquest.
  if ((old_sets.empty() && old_policy.empty()) ||
      (current_sets.empty() && current_policy.empty()))
    return {};

  std::vector<net::SchemefulSite> result;
  for (const auto& old_pair : old_sets) {
    const net::SchemefulSite& old_member = old_pair.first;
    const net::FirstPartySetEntry& old_entry = old_pair.second;

    if (base::Contains(old_policy, old_member))
      continue;

    absl::optional<net::FirstPartySetEntry> current_entry =
        FindOwner(old_member, current_sets, current_policy);
    // Look for the removed sites and the ones have owner changed.
    if (!current_entry.has_value() ||
        current_entry.value().primary() != old_entry.primary()) {
      result.push_back(old_member);
    }
  }

  for (const auto& old_pair : old_policy) {
    const net::SchemefulSite& old_member = old_pair.first;
    const absl::optional<net::FirstPartySetEntry>& old_entry = old_pair.second;

    const absl::optional<net::FirstPartySetEntry> current_entry =
        FindOwner(old_member, current_sets, current_policy);
    // Look for the ones have owner changed.
    if (old_entry.has_value() && current_entry != old_entry) {
      result.push_back(old_member);
    }
  }
  return result;
}

void FirstPartySetsHandlerImpl::ClearSiteDataOnChangedSets() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!public_sets_.is_null());
  DCHECK(raw_persisted_sets_.has_value());

  // TODO(shuuran@chromium.org): Implement site state clearing.

  if (!persisted_sets_path_.empty()) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(
            &MaybeWriteSetsToDisk, persisted_sets_path_,
            FirstPartySetParser::SerializeFirstPartySets(public_sets_->sets)));
  }
}

FirstPartySetsHandler::PolicyCustomization
FirstPartySetsHandlerImpl::GetCustomizationForPolicyInternal(
    const base::Value::Dict& policy) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::expected<FirstPartySetParser::ParsedPolicySetLists,
                 FirstPartySetParser::PolicyParsingError>
      parsed_policy =
          FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy);
  // Provide empty customization if the policy is malformed.
  return parsed_policy.has_value()
             ? FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
                   public_sets_, parsed_policy.value())
             : FirstPartySetsHandlerImpl::PolicyCustomization();
}

}  // namespace content
