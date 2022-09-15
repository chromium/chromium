// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_HANDLER_DATABASE_HELPER_H_
#define CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_HANDLER_DATABASE_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {
class SchemefulSite;
class FirstPartySetEntry;
}  // namespace net

namespace content {

class FirstPartySetsDatabase;

// A helper class for the access of the underlying First-Party Sets database.
// Owned by the FirstPartySetsHandlerImpl, and will be created on a different
// sequence that supports blocking, i.e. a database sequence, so that none of
// these methods should be called directly on the main thread.
class CONTENT_EXPORT FirstPartySetsHandlerDatabaseHelper {
 public:
  using FlattenedSets =
      base::flat_map<net::SchemefulSite, net::FirstPartySetEntry>;
  using PolicyCustomization =
      base::flat_map<net::SchemefulSite,
                     absl::optional<net::FirstPartySetEntry>>;

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
  // FPSs info by comparing the combined `old_sets` and `old_policy` with the
  // combined `current_sets` and `current_policy`. Returns the set of sites
  // that: 1) were in old FPSs but are no longer in current FPSs i.e. leave the
  // FPSs; or, 2) mapped to a different owner site.
  //
  // This method assumes that the sites were normalized properly when the maps
  // were created. Made public only for testing,
  static base::flat_set<net::SchemefulSite> ComputeSetsDiff(
      const FlattenedSets& old_sets,
      const PolicyCustomization& old_policy,
      const FlattenedSets& current_sets,
      const PolicyCustomization& current_policy);

  // Gets the list of sites to clear for the `browser_context_id`. This method
  // wraps a few DB operations: reads the old public sets and policy
  // customization from DB, call `ComputeSetsDiff` with required inputs to
  // compute the list of sites to clear, stores the sites into DB, then reads
  // the final list of sites to be cleared from DB, which can include sites
  // stored during previous browser runs that did not have state cleared.
  std::vector<net::SchemefulSite> UpdateAndGetSitesToClearForContext(
      const std::string& browser_context_id,
      const FlattenedSets& current_sets,
      const PolicyCustomization& current_policy);

  // Wraps FirstPartySetsDatabase::InsertBrowserContextCleared.
  // Update DB whether site data clearing has been performed for the
  // `browser_context_id`.
  void UpdateClearStatusForContext(const std::string& browser_context_id);

  // Wraps FirstPartySetsDatabase::SetPublicSets.
  void PersistPublicSets(const FlattenedSets& sets);

  // Wraps FirstPartySetsDatabase::GetPublicSets.
  FlattenedSets GetPersistedPublicSets();

 private:
  std::unique_ptr<FirstPartySetsDatabase> db_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_HANDLER_DATABASE_HELPER_H_
