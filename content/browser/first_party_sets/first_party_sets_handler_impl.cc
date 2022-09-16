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

std::vector<SingleSet> NormalizeAdditionSets(
    const net::PublicSets& public_sets,
    const std::vector<SingleSet>& addition_sets) {
  // Create a mapping from an owner site in `existing_sets` to all policy sets
  // that intersect with the set that it owns.
  base::flat_map<net::SchemefulSite, base::flat_set<size_t>>
      policy_set_overlaps;
  for (size_t set_idx = 0; set_idx < addition_sets.size(); set_idx++) {
    for (const auto& site_and_entry : addition_sets[set_idx]) {
      if (auto entry =
              public_sets.FindEntry(site_and_entry.first, /*config=*/nullptr);
          entry.has_value()) {
        policy_set_overlaps[entry->primary()].insert(set_idx);
      }
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
  return public_sets.ComputeConfig(
      /*replacement_sets=*/policy.replacements,
      /*normalized_additions=*/NormalizeAdditionSets(public_sets,
                                                     policy.additions));
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
