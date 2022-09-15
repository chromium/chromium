// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_sets_handler_impl.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "content/browser/first_party_sets/addition_overlaps_union_find.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "content/browser/first_party_sets/first_party_sets_loader.h"
#include "content/browser/first_party_sets/local_set_declaration.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "content/public/common/content_client.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/public_sets.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {
using FlattenedSets = FirstPartySetsHandlerImpl::FlattenedSets;
using SingleSet = FirstPartySetParser::SingleSet;

constexpr base::FilePath::CharType kFirstPartySetsDatabase[] =
    FILE_PATH_LITERAL("first_party_sets.db");

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

// Adds all sets in a list of First-Party Sets into `site_to_owner` which maps
// from a site to its owner.
void UpdateCustomizationMap(
    const std::vector<SingleSet>& set_list,
    base::flat_map<net::SchemefulSite, absl::optional<net::FirstPartySetEntry>>&
        site_to_entry) {
  for (const auto& set : set_list) {
    for (const auto& site_and_entry : set) {
      bool inserted = site_to_entry.emplace(site_and_entry).second;
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
    const net::PublicSets& existing_sets,
    base::flat_map<net::SchemefulSite, base::flat_set<size_t>>&
        policy_set_overlaps) {
  // Check `site` for membership in `existing_sets`.
  if (auto entry = existing_sets.FindEntry(site, /*config=*/nullptr);
      entry.has_value()) {
    // Add the index of `site`'s policy set to the list of policy set indices
    // that also overlap with site_owner.
    policy_set_overlaps[entry->primary()].insert(policy_set_index);
  }
}

std::vector<SingleSet> NormalizeAdditionSets(
    const net::PublicSets& public_sets,
    const std::vector<SingleSet>& addition_sets) {
  // Create a mapping from an owner site in `existing_sets` to all policy sets
  // that intersect with the set that it owns.
  base::flat_map<net::SchemefulSite, base::flat_set<size_t>>
      policy_set_overlaps;
  for (size_t set_idx = 0; set_idx < addition_sets.size(); set_idx++) {
    for (const auto& site_and_entry : addition_sets[set_idx]) {
      AddIfPolicySetOverlaps(site_and_entry.first, set_idx, public_sets,
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

}  // namespace

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
std::pair<absl::optional<FirstPartySetsHandler::ParseError>,
          std::vector<FirstPartySetsHandler::ParseWarning>>
FirstPartySetsHandler::ValidateEnterprisePolicy(
    const base::Value::Dict& policy) {
  FirstPartySetParser::PolicyParseResult parsed_or_error =
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy);
  if (!parsed_or_error.has_value()) {
    return {parsed_or_error.error().first, parsed_or_error.error().second};
  }
  return {absl::nullopt, parsed_or_error.value().second};
}

void FirstPartySetsHandlerImpl::GetCustomizationForPolicy(
    const base::Value::Dict& policy,
    base::OnceCallback<void(net::FirstPartySetsContextConfig)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (public_sets_.has_value()) {
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

net::FirstPartySetsContextConfig
FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
    const net::PublicSets& public_sets,
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
    if (const auto entry =
            public_sets.FindEntry(new_member, /*config=*/nullptr);
        entry.has_value()) {
      // Found an overlap with the existing list of sets.
      addition_intersected_owners.emplace(entry->primary(), new_entry);
    }
  }

  // Maps an existing owner to the members it lost due to replacement.
  base::flat_map<net::SchemefulSite, base::flat_set<net::SchemefulSite>>
      potential_singletons;
  for (const auto& [member, set_entry] : flattened_replacements) {
    if (member == set_entry.primary())
      continue;
    if (auto existing_entry = public_sets.FindEntry(member, /*config=*/nullptr);
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
  base::flat_set<net::SchemefulSite> replaced_existing_owners;
  for (const auto& [site, unused_owner] : flattened_replacements) {
    if (const auto entry = public_sets.FindEntry(site, /*config=*/nullptr);
        entry.has_value() && entry->primary() == site) {
      // Site was an owner in the existing sets.
      bool inserted = replaced_existing_owners.emplace(site).second;
      DCHECK(inserted);
    }
  }

  // Find out which potential singletons are actually singletons; delete
  // members whose owners left; and reparent the sets that intersected with
  // an addition set.
  for (const auto& [member, set_entry] : public_sets.entries()) {
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

  return net::FirstPartySetsContextConfig(std::move(site_to_entry));
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

absl::optional<net::PublicSets> FirstPartySetsHandlerImpl::GetSets(
    SetsReadyOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsEnabled());
  if (public_sets_.has_value())
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
                                     const LocalSetDeclaration& local_set) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!initialized_);

  initialized_ = true;
  SetDatabase(user_data_dir);

  if (IsEnabled()) {
    sets_loader_->SetManuallySpecifiedSet(local_set);
    if (!embedder_will_provide_public_sets_) {
      sets_loader_->SetComponentSets(base::File());
    }
  } else {
    SetCompleteSets(net::PublicSets());
  }
}

bool FirstPartySetsHandlerImpl::IsEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return enabled_;
}

void FirstPartySetsHandlerImpl::SetPublicFirstPartySets(
    const base::Version& version,
    base::File sets_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(enabled_);
  DCHECK(embedder_will_provide_public_sets_);

  // TODO(crbug.com/1219656): Use this value to compute sets diff and then
  // persisting to DB if valid.
  version_ = version;
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
  public_sets_ = absl::nullopt;
  db_helper_.Reset();
}

void FirstPartySetsHandlerImpl::GetPersistedPublicSetsForTesting(
    base::OnceCallback<void(
        absl::optional<FirstPartySetsHandlerImpl::FlattenedSets>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_helper_.is_null()) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  db_helper_
      .AsyncCall(&FirstPartySetsHandlerDatabaseHelper::GetPersistedPublicSets)
      .Then(std::move(callback));
}

void FirstPartySetsHandlerImpl::SetCompleteSets(net::PublicSets public_sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!public_sets_.has_value());
  public_sets_ = std::move(public_sets);

  if (IsEnabled())
    InvokePendingQueries();
}

void FirstPartySetsHandlerImpl::SetDatabase(
    const base::FilePath& user_data_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_helper_.is_null());

  if (user_data_dir.empty()) {
    VLOG(1) << "Empty path. Failed initializing First-Party Sets database.";
    return;
  }
  db_helper_.emplace(base::ThreadPool::CreateSequencedTaskRunner(
                         {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
                          base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
                     user_data_dir.Append(kFirstPartySetsDatabase));
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

net::PublicSets FirstPartySetsHandlerImpl::GetSetsSync() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(public_sets_.has_value());
  return public_sets_->Clone();
}

void FirstPartySetsHandlerImpl::ClearSiteDataOnChangedSetsForContext(
    base::RepeatingCallback<BrowserContext*()> browser_context_getter,
    const std::string& browser_context_id,
    const net::FirstPartySetsContextConfig* context_config,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(public_sets_.has_value());
  DCHECK(!browser_context_id.empty());

  if (!db_helper_.is_null()) {
    // TODO(crbug.com/1219656): Call site state clearing.
    db_helper_
        .AsyncCall(&FirstPartySetsHandlerDatabaseHelper::PersistPublicSets)
        .WithArgs(public_sets_->entries());
  }
  std::move(callback).Run();
}

net::FirstPartySetsContextConfig
FirstPartySetsHandlerImpl::GetCustomizationForPolicyInternal(
    const base::Value::Dict& policy) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FirstPartySetParser::PolicyParseResult parsed_or_error =
      FirstPartySetParser::ParseSetsFromEnterprisePolicy(policy);
  // Provide empty customization if the policy is malformed.
  return parsed_or_error.has_value()
             ? FirstPartySetsHandlerImpl::ComputeEnterpriseCustomizations(
                   public_sets_.value(), parsed_or_error.value().first)
             : net::FirstPartySetsContextConfig();
}

}  // namespace content
