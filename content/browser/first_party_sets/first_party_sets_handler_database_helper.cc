// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_sets_handler_database_helper.h"

#include "base/containers/contains.h"
#include "base/logging.h"
#include "content/browser/first_party_sets/database/first_party_sets_database.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_sets_cache_filter.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"

namespace content {

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
    const net::GlobalFirstPartySets& old_sets,
    const net::FirstPartySetsContextConfig& old_config,
    const net::GlobalFirstPartySets& current_sets,
    const net::FirstPartySetsContextConfig& current_config) {
  // TODO(https://crbug.com/1219656): For now we don't clear site data if FPSs
  // is disabled. This may change with future feature ruquest.
  if ((old_sets.empty() && old_config.empty()) ||
      (current_sets.empty() && current_config.empty())) {
    return {};
  }

  std::vector<net::SchemefulSite> result;

  old_sets.ForEachEffectiveSetEntry(
      old_config, [&](const net::SchemefulSite& old_member,
                      const net::FirstPartySetEntry& old_entry) {
        absl::optional<net::FirstPartySetEntry> current_entry =
            current_sets.FindEntry(old_member, current_config);
        // Look for the removed sites and the ones whose primary has changed.
        if (!current_entry.has_value() ||
            current_entry.value().primary() != old_entry.primary()) {
          result.push_back(old_member);
        }
        return true;
      });

  return result;
}

std::pair<std::vector<net::SchemefulSite>, net::FirstPartySetsCacheFilter>
FirstPartySetsHandlerDatabaseHelper::UpdateAndGetSitesToClearForContext(
    const std::string& browser_context_id,
    const net::GlobalFirstPartySets& current_sets,
    const net::FirstPartySetsContextConfig& current_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!browser_context_id.empty());
  std::pair<net::GlobalFirstPartySets, net::FirstPartySetsContextConfig>
      old_sets_with_config = db_->GetGlobalSetsAndConfig(browser_context_id);
  base::flat_set<net::SchemefulSite> diff =
      ComputeSetsDiff(old_sets_with_config.first, old_sets_with_config.second,
                      current_sets, current_config);

  if (!db_->InsertSitesToClear(browser_context_id, diff)) {
    DVLOG(1) << "Failed to update the sites to clear for browser_context_id="
             << browser_context_id;
    return {};
  }
  return db_->GetSitesToClearFilters(browser_context_id);
}

void FirstPartySetsHandlerDatabaseHelper::UpdateClearStatusForContext(
    const std::string& browser_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_->InsertBrowserContextCleared(browser_context_id)) {
    DVLOG(1) << "Failed to update clear state for browser_context_id="
             << browser_context_id;
  }
}

void FirstPartySetsHandlerDatabaseHelper::PersistSets(
    const std::string& browser_context_id,
    const net::GlobalFirstPartySets& sets,
    const net::FirstPartySetsContextConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!browser_context_id.empty());
  if (!db_->PersistSets(browser_context_id, sets, config))
    DVLOG(1) << "Failed to write sets into the database.";
}

std::pair<net::GlobalFirstPartySets, net::FirstPartySetsContextConfig>
FirstPartySetsHandlerDatabaseHelper::GetGlobalSetsAndConfigForTesting(
    const std::string& browser_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!browser_context_id.empty());
  return db_->GetGlobalSetsAndConfig(browser_context_id);
}

// Wraps FirstPartySetsDatabase::HasEntryInBrowserContextsClearedForTesting.
bool FirstPartySetsHandlerDatabaseHelper::
    HasEntryInBrowserContextsClearedForTesting(
        const std::string& browser_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!browser_context_id.empty());
  return db_->HasEntryInBrowserContextsClearedForTesting(  // IN-TEST
      browser_context_id);
}

}  // namespace content
