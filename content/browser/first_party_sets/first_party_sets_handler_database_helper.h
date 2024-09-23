// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_HANDLER_DATABASE_HELPER_H_
#define CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_HANDLER_DATABASE_HELPER_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "content/common/content_export.h"

namespace net {
class FirstPartySetsCacheFilter;
class FirstPartySetsContextConfig;
class GlobalFirstPartySets;
class SchemefulSite;
}  // namespace net

namespace content {

class FirstPartySetsDatabase;

// A helper class for the access of the underlying First-Party Sets database.
// Owned by the FirstPartySetsHandlerImpl, and will be created on a different
// sequence that supports blocking, i.e. a database sequence, so that none of
// these methods should be called directly on the main thread.
class CONTENT_EXPORT FirstPartySetsHandlerDatabaseHelper {
 public:
  explicit FirstPartySetsHandlerDatabaseHelper(
      const base::FilePath& user_data_directory);

  FirstPartySetsHandlerDatabaseHelper(
      const FirstPartySetsHandlerDatabaseHelper&) = delete;
  FirstPartySetsHandlerDatabaseHelper& operator=(
      const FirstPartySetsHandlerDatabaseHelper&) = delete;
  FirstPartySetsHandlerDatabaseHelper(FirstPartySetsHandlerDatabaseHelper&&) =
      delete;
  FirstPartySetsHandlerDatabaseHelper& operator=(
      FirstPartySetsHandlerDatabaseHelper&&) = delete;

  ~FirstPartySetsHandlerDatabaseHelper();

  // Gets the difference between the previously used FPSs info with the current
  // FPSs info by comparing the combined `old_sets` and `old_config` with the
  // combined `current_sets` and `current_config`. Returns the set of sites
  // that: 1) were in old FPSs but are no longer in current FPSs i.e. leave the
  // FPSs; or, 2) mapped to a different primary site.
  //
  // This method assumes that the sites were normalized properly when the maps
  // were created. Made public only for testing,
  static base::flat_set<net::SchemefulSite> ComputeSetsDiff(
      const net::GlobalFirstPartySets& old_sets,
      const net::FirstPartySetsContextConfig& old_config,
      const net::GlobalFirstPartySets& current_sets,
      const net::FirstPartySetsContextConfig& current_config);

  // Gets the list of sites to clear for the `browser_context_id`. This method
  // wraps a few DB operations: reads the old global sets and policy
  // customization from DB, call `ComputeSetsDiff` with required inputs to
  // compute the list of sites to clear, stores the sites into DB, then reads
  // the final list of sites to be cleared from DB, which can include sites
  // stored during previous browser runs that did not have state cleared.
  std::optional<std::pair<std::vector<net::SchemefulSite>,
                          net::FirstPartySetsCacheFilter>>
  UpdateAndGetSitesToClearForContext(
      const std::string& browser_context_id,
      const net::GlobalFirstPartySets& current_sets,
      const net::FirstPartySetsContextConfig& current_config);

  // Wraps FirstPartySetsDatabase::InsertBrowserContextCleared.
  // Update DB whether site data clearing has been performed for the
  // `browser_context_id`.
  void UpdateClearStatusForContext(const std::string& browser_context_id);

  // Wraps FirstPartySetsDatabase::PersistSets.
  void PersistSets(const std::string& browser_context_id,
                   const net::GlobalFirstPartySets& sets,
                   const net::FirstPartySetsContextConfig& config);

  std::optional<
      std::pair<net::GlobalFirstPartySets, net::FirstPartySetsContextConfig>>
  GetGlobalSetsAndConfigForTesting(const std::string& browser_context_id);

  // Wraps FirstPartySetsDatabase::HasEntryInBrowserContextClearedForTesting.
  bool HasEntryInBrowserContextsClearedForTesting(
      const std::string& browser_context_id);

 private:
  std::unique_ptr<FirstPartySetsDatabase> db_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_HANDLER_DATABASE_HELPER_H_
