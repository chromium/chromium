// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_sets_handler_database_helper.h"

#include "base/containers/contains.h"
#include "base/logging.h"
#include "content/browser/first_party_sets/database/first_party_sets_database.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"

namespace content {

namespace {

// TODO(https://crbug.com/1349487): Since this is basically the same as
// FirstPartySetsManager::FindOwnerInternal(), move the common algorithm into
// //net to be reused in both here and FirstPartySetsManager.
absl::optional<net::FirstPartySetEntry> FindOwner(
    const net::SchemefulSite& site,
    const FirstPartySetsHandlerDatabaseHelper::FlattenedSets& sets,
    const FirstPartySetsHandlerDatabaseHelper::PolicyCustomization&
        policy_sets) {
  absl::optional<net::FirstPartySetEntry> owner;
  if (const auto policy_it = policy_sets.find(site);
      policy_it != policy_sets.end()) {
    owner = policy_it->second;
  } else if (const auto it = sets.find(site); it != sets.end()) {
    owner = it->second;
  }
  return owner;
}

}  // namespace

FirstPartySetsHandlerDatabaseHelper::FirstPartySetsHandlerDatabaseHelper(
    const base::FilePath& db_path) {
  DCHECK(!db_path.empty());
  db_ = std::make_unique<FirstPartySetsDatabase>(db_path);
}

FirstPartySetsHandlerDatabaseHelper::~FirstPartySetsHandlerDatabaseHelper() =
    default;

// static
base::flat_set<net::SchemefulSite>
FirstPartySetsHandlerDatabaseHelper::ComputeSetsDiff(
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

std::vector<net::SchemefulSite>
FirstPartySetsHandlerDatabaseHelper::UpdateAndGetSitesToClearForContext(
    const std::string& browser_context_id,
    const FlattenedSets& current_sets,
    const PolicyCustomization& current_policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::flat_set<net::SchemefulSite> diff = ComputeSetsDiff(
      db_->GetPublicSets(), db_->FetchPolicyModifications(browser_context_id),
      current_sets, current_policy);

  if (!db_->InsertSitesToClear(browser_context_id, diff)) {
    DVLOG(1) << "Failed to update the sites to clear for browser_context_id="
             << browser_context_id;
    return {};
  }
  return db_->FetchSitesToClear(browser_context_id);
}

void FirstPartySetsHandlerDatabaseHelper::UpdateClearStatusForContext(
    const std::string& browser_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_->InsertBrowserContextCleared(browser_context_id)) {
    DVLOG(1) << "Failed to update clear state for browser_context_id="
             << browser_context_id;
  }
}

void FirstPartySetsHandlerDatabaseHelper::PersistPublicSets(
    const FirstPartySetsHandlerDatabaseHelper::FlattenedSets& sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_->SetPublicSets(sets)) {
    DVLOG(1) << "Failed to write public sets into the database.";
  }
}

FirstPartySetsHandlerDatabaseHelper::FlattenedSets
FirstPartySetsHandlerDatabaseHelper::GetPersistedPublicSets() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_->GetPublicSets();
}

}  // namespace content
