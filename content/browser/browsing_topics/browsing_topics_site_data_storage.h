// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSING_TOPICS_BROWSING_TOPICS_SITE_DATA_STORAGE_H_
#define CONTENT_BROWSER_BROWSING_TOPICS_BROWSING_TOPICS_SITE_DATA_STORAGE_H_

#include <map>
#include <set>
#include <string>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/browsing_topics/common/common_types.h"
#include "content/common/content_export.h"
#include "sql/meta_table.h"

namespace sql {
class Database;
class Statement;
}  // namespace sql

namespace content {

class CONTENT_EXPORT BrowsingTopicsSiteDataStorage {
 public:
  explicit BrowsingTopicsSiteDataStorage(
      const base::FilePath& path_to_database);

  BrowsingTopicsSiteDataStorage(const BrowsingTopicsSiteDataStorage&) = delete;
  BrowsingTopicsSiteDataStorage& operator=(
      const BrowsingTopicsSiteDataStorage&) = delete;
  BrowsingTopicsSiteDataStorage(BrowsingTopicsSiteDataStorage&&) = delete;
  BrowsingTopicsSiteDataStorage& operator=(BrowsingTopicsSiteDataStorage&&) =
      delete;

  ~BrowsingTopicsSiteDataStorage();

  // Expire all data before the given time.
  void ExpireDataBefore(base::Time time);

  // Clear per-context-domain data.
  void ClearContextDomain(
      const browsing_topics::HashedDomain& hashed_context_domain);

  // Get all browsing topics `ApiUsageContext` with its `last_usage_time` within
  // [`begin_time`, `end_time`). Note that it's possible for a usage to occur
  // within the specified time range, and a more recent usage has renewed its
  // `last_usage_time`, so that the corresponding context is not retrieved in
  // this query. In practice, this method will be called with
  // `end_time` being very close to the current time, so the amount of missed
  // data should be negligible. This query also deletes all data with
  // last_usage_time (non-inclusive) less than `begin_time`.
  browsing_topics::ApiUsageContextQueryResult GetBrowsingTopicsApiUsage(
      base::Time begin_time,
      base::Time end_time);

  // For each hashed context domain, get the stored unhashed version. Only
  // hashed domains for which there is a corresponding unhashed domain will be
  // included in the output.
  std::map<browsing_topics::HashedDomain, std::string>
  GetContextDomainsFromHashedContextDomains(
      const std::set<browsing_topics::HashedDomain>& hashed_context_domains);

  // Persist the browsing topics api usage context to storage. Called when the
  // usage is detected in a context on a page.
  void OnBrowsingTopicsApiUsed(
      const browsing_topics::HashedHost& hashed_main_frame_host,
      const browsing_topics::HashedDomain& hashed_context_domain,
      const std::string& context_domain,
      base::Time time);

 private:
  enum class InitStatus {
    kUnattempted = 0,  // `LazyInit()` has not yet been called.
    kSuccess = 1,      // `LazyInit()` succeeded.
    kFailure = 2,      // `LazyInit()` failed.
  };

  // Initializes the database if necessary, and returns whether the database is
  // open.
  bool LazyInit() VALID_CONTEXT_REQUIRED(sequence_checker_);

  bool InitializeTables() VALID_CONTEXT_REQUIRED(sequence_checker_);
  bool CreateSchema() VALID_CONTEXT_REQUIRED(sequence_checker_);

  void HandleInitializationFailure() VALID_CONTEXT_REQUIRED(sequence_checker_);

  void DatabaseErrorCallback(int extended_error, sql::Statement* stmt);

  const base::FilePath path_to_database_;

  // Current status of the database initialization. Tracks what stage |this| is
  // at for lazy initialization, and used as a signal for if the database is
  // closed. This is initialized in the first call to LazyInit() to avoid doing
  // additional work in the constructor.
  InitStatus db_init_status_ GUARDED_BY_CONTEXT(sequence_checker_){
      InitStatus::kUnattempted};

  // May be null if the database:
  //  - could not be opened
  //  - table/index initialization failed
  std::unique_ptr<sql::Database> db_ GUARDED_BY_CONTEXT(sequence_checker_);

  sql::MetaTable meta_table_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<BrowsingTopicsSiteDataStorage> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSING_TOPICS_BROWSING_TOPICS_SITE_DATA_STORAGE_H_
