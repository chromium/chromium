// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_STORAGE_SQL_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_STORAGE_SQL_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "content/browser/conversions/conversion_report.h"
#include "content/browser/conversions/conversion_storage.h"
#include "content/common/content_export.h"
#include "sql/database.h"

namespace base {
class Clock;
}  // namespace base

namespace content {

// Provides an implementation of ConversionStorage that is backed by SQLite.
// This class may be constructed on any sequence but must be accessed and
// destroyed on the same sequence. The sequence must outlive |this|.
class CONTENT_EXPORT ConversionStorageSql : public ConversionStorage {
 public:
  static void RunInMemoryForTesting();

  ConversionStorageSql(const base::FilePath& path_to_database,
                       std::unique_ptr<Delegate> delegate,
                       const base::Clock* clock);
  ConversionStorageSql(const ConversionStorageSql& other) = delete;
  ConversionStorageSql& operator=(const ConversionStorageSql& other) = delete;
  ~ConversionStorageSql() override;

  void set_ignore_errors_for_testing(bool ignore_for_testing) {
    ignore_errors_for_testing_ = ignore_for_testing;
  }

 private:
  enum class DbStatus {
    // The database has never been created, i.e. there is no database file at
    // all.
    kDeferringCreation,
    // The database exists but is not open yet.
    kDeferringOpen,
    // The database initialization failed, or the db suffered from an
    // unrecoverable error.
    kClosed,
    kOpen,
  };

  enum class DbCreationPolicy {
    // Create the db if it does not exist.
    kCreateIfAbsent,
    // Do not create the db if it does not exist.
    kIgnoreIfAbsent,
  };

  // ConversionStorage
  void StoreImpression(const StorableImpression& impression) override;
  int MaybeCreateAndStoreConversionReports(
      const StorableConversion& conversion) override;
  std::vector<ConversionReport> GetConversionsToReport(
      base::Time expiry_time) override;
  std::vector<StorableImpression> GetActiveImpressions() override;
  int DeleteExpiredImpressions() override;
  bool DeleteConversion(int64_t conversion_id) override;
  void ClearData(
      base::Time delete_begin,
      base::Time delete_end,
      base::RepeatingCallback<bool(const url::Origin&)> filter) override;

  // Variants of ClearData that assume all Origins match the filter.
  void ClearAllDataInRange(base::Time delete_begin, base::Time delete_end);
  void ClearAllDataAllTime();

  bool HasCapacityForStoringImpression(const std::string& serialized_origin);
  bool HasCapacityForStoringConversion(const std::string& serialized_origin);

  // Initializes the database if necessary, and returns whether the database is
  // open. |should_create| indicates whether the database should be created if
  // it is not already.
  bool LazyInit(DbCreationPolicy creation_policy);
  bool InitializeSchema();

  void DatabaseErrorCallback(int extended_error, sql::Statement* stmt);

  static bool g_run_in_memory_;

  // If set, database errors will not crash the client when run in debug mode.
  bool ignore_errors_for_testing_ = false;

  const base::FilePath path_to_database_;

  // Current status of the database initialization. Tracks what stage |this| is
  // at for lazy initialization, and used as a signal for if the database is
  // closed. This is initialized in the first call to LazyInit() to avoid doing
  // additional work in the constructor, see https://crbug.com/1121307.
  base::Optional<DbStatus> db_init_status_;

  // May be null if the database:
  //  - could not be opened
  //  - table/index initialization failed
  std::unique_ptr<sql::Database> db_;

  // Must outlive |this|.
  const base::Clock* clock_;

  std::unique_ptr<Delegate> delegate_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ConversionStorageSql> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_STORAGE_SQL_H_
