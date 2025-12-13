// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BTM_BTM_DATABASE_H_
#define CONTENT_BROWSER_BTM_BTM_DATABASE_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/sequence_checker.h"
#include "base/strings/cstring_view.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "content/browser/btm/btm_utils.h"
#include "content/common/content_export.h"
#include "content/public/common/content_features.h"
#include "sql/database.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"
#include "sql/statement.h"

namespace content {

// Encapsulates an SQL database that holds DIPS info.
class CONTENT_EXPORT BtmDatabase {
 public:
  enum class BounceFilterType {
    // Filter for bounces with either user activations or WebAuthn assertions.
    kProtectiveEvent,
    // Filter for bounces with user activations.
    kUserActivation,
    // Filter for bounces with WebAuthn assertions.
    kWebAuthnAssertion,
  };

  // Version number of the database schema.
  // NOTE: When changing the version, add a new golden file for the new version
  // at `//chrome/test/data/dips/v<N>.sql`.
  static constexpr int kLatestSchemaVersion = 11;

  // The minimum database schema version this Chrome code is compatible with.
  static constexpr int kMinCompatibleSchemaVersion = 11;

  static constexpr char kPrepopulatedKey[] = "prepopulated";

  // The length of time that will be waited between emitting db health metrics.
  static constexpr base::TimeDelta kMetricsInterval = base::Hours(24);

  // How long DIPS maintains popups in storage (for recording Popup Heuristic
  // storage accesses).
  static constexpr base::TimeDelta kPopupTtl = base::Days(60);

  // Passing in an std::nullopt `db_path` causes the db to be created in
  // memory. Init() must be called before using the BtmDatabase to make sure it
  // is initialized.
  explicit BtmDatabase(const std::optional<base::FilePath>& db_path);

  // This object must be destroyed on the thread where all accesses are
  // happening to avoid thread-safety problems.
  ~BtmDatabase();

  BtmDatabase(const BtmDatabase&) = delete;
  BtmDatabase& operator=(const BtmDatabase&) = delete;

  // DIPS Bounce table functions -----------------------------------------------
  bool Write(const std::string& site,
             const TimestampRange& user_activation_times,
             const TimestampRange& bounce_times,
             const TimestampRange& web_authn_assertion_times);

  bool WritePopup(const std::string& opener_site,
                  const std::string& popup_site,
                  const uint64_t access_id,
                  const base::Time& popup_time,
                  bool is_current_interaction,
                  bool is_authentication_interaction);

  std::optional<StateValue> Read(const std::string& site);

  std::optional<PopupsStateValue> ReadPopup(const std::string& opener_site,
                                            const std::string& popup_site);

  // Returns all entries from the `popups` table with a current interaction,
  // where the last popup time was more recent than `lookback` ago.
  std::vector<PopupWithTime> ReadRecentPopupsWithInteraction(
      const base::TimeDelta& lookback);

  // Note: this doesn't clear expired interactions from the database unlike
  // the other database querying methods.
  std::vector<std::string> GetAllSitesForTesting(const BtmDatabaseTable table);

  // Returns the subset of sites in `sites` WITH some property recorded, as
  // determined by the filtering condition.
  //
  // (For example, a protective event is a user activation or successful
  // WebAuthn assertion.)
  //
  // NOTE: This method's main procedure is performed after calling
  // `ClearExpiredRows()`.
  std::set<std::string> FilterSites(const std::set<std::string>& sites,
                                    BounceFilterType filter);

  // Returns all sites which bounced the user and aren't protected from DIPS.
  //
  // A site can be protected in several ways:
  // - it's still in its grace period after the first bounce
  // - it received user activation or WAA before the first bounce
  // - it received user activation or WAA in the grace period after the first
  // bounce.
  //
  // NOTE: This method's main procedure is performed after calling
  // `ClearExpiredRows()`.
  std::vector<std::string> GetSitesThatBounced(base::TimeDelta grace_period);

  // Deletes all rows in the database whose interactions have expired out.
  //
  // When an interaction happens before a DIPS-triggering action or during the
  // following grace-period, it protects that site from its data being cleared
  // by DIPS. Further interactions will prolong that protection until the last
  // one reaches the `interaction_ttl`.
  //
  // Clearing expired interactions effectively restarts the DIPS procedure for
  // determining if a site is a tracker for sites that are cleared.
  //
  // Returns the number of rows that are removed.
  size_t ClearExpiredRows();

  // Delete the row from the bounces table for `site`. Returns true if query
  // executes successfully.
  //
  // NOTE: This method's main procedure is performed after calling
  // `ClearExpiredRows()`.
  bool RemoveRow(const BtmDatabaseTable table, std::string_view site);

  bool RemoveRows(const BtmDatabaseTable table,
                  const std::vector<std::string>& sites);

  // NOTE: This method's main procedure is performed after calling
  // `ClearExpiredRows()`.
  bool RemoveEventsByTime(const base::Time& delete_begin,
                          const base::Time& delete_end,
                          const BtmEventRemovalType type);

  // Because |sites| is taken from a ClearDataFilter, this only removes
  // storage and stateful bounce timestamps at the moment.
  bool RemoveEventsBySite(bool preserve,
                          const std::vector<std::string>& sites,
                          const BtmEventRemovalType type);

  // Returns the number of entries present in the database.
  //
  // NOTE: This method's main procedure is performed after calling
  // `ClearExpiredRows()`.
  size_t GetEntryCount(const BtmDatabaseTable table);

  // If the number of entries in the database is greater than
  // |GetMaxEntries()|, garbage collect. Returns the number of entries deleted
  // (useful for debugging).
  size_t GarbageCollect();

  // Removes the |purge_goal| entries with the oldest
  // |MAX(last_user_activation_time,last_site_storage_time)| value. Returns the
  // number of entries deleted.
  //
  // NOTE: The SQLITE sub-query in this method must match that of
  // `GetGarbageCollectOldestSitesForTesting()`'s query.
  size_t GarbageCollectOldest(const BtmDatabaseTable table, int purge_goal);

  // Used for testing the intended behavior `GarbageCollectOldest()`.
  //
  // NOTE: The SQLITE query in this method must match that of
  // `GarbageCollectOldest()`'s sub-query.
  std::vector<std::string> GetGarbageCollectOldestSitesForTesting(
      const BtmDatabaseTable table);

  bool in_memory() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return db_path_.empty();
  }

  // Checks that the internal SQLite database is initialized.
  bool CheckDBInit();

  size_t GetMaxEntries() const { return max_entries_; }
  size_t GetPurgeEntries() const { return purge_entries_; }

  std::optional<base::Time> GetTimerLastFired();
  bool SetTimerLastFired(base::Time time);

  // Testing functions --------------------------------------------------
  void SetMaxEntriesForTesting(size_t entries) { max_entries_ = entries; }
  void SetPurgeEntriesForTesting(size_t entries) { purge_entries_ = entries; }
  void SetClockForTesting(base::Clock* clock) { clock_ = clock; }
  bool ExecuteSqlForTesting(const base::cstring_view sql);

  bool SetConfigValueForTesting(std::string_view name, int64_t value) {
    return SetConfigValue(name, value);
  }
  std::optional<int64_t> GetConfigValueForTesting(std::string_view name) {
    return GetConfigValue(name);
  }

 protected:
  // Initialization functions --------------------------------------------------
  sql::InitStatus Init();
  sql::InitStatus InitImpl();
  sql::InitStatus OpenDatabase();
  // Creates the bounce table following the latest schema.
  bool InitTables();

  // Internal utility functions ------------------------------------------------

  // NOTE: This method's main procedure is performed after calling
  // `ClearExpiredRows()`.
  bool ClearTimestamps(const base::Time& delete_begin,
                       const base::Time& delete_end,
                       const BtmEventRemovalType type);
  bool ClearTimestampsBySite(bool preserve,
                             const std::vector<std::string>& sites,
                             const BtmEventRemovalType type);
  bool RemoveEmptyRows();

  void LogDatabaseMetrics();

 private:
  // Callback for database errors.
  void DatabaseErrorCallback(int extended_error, sql::Statement* stmt);

  // Only ClearTimestamps() should call this method.
  //
  // NOTE: This method's main procedure is performed after calling
  // `ClearExpiredRows()`.
  bool AdjustFirstTimestamps(const base::Time& delete_begin,
                             const base::Time& delete_end,
                             const BtmEventRemovalType type);
  // Only ClearTimestamps() should call this method.
  //
  // NOTE: This method's main procedure is performed after calling
  // `ClearExpiredRows()`.
  bool AdjustLastTimestamps(const base::Time& delete_begin,
                            const base::Time& delete_end,
                            const BtmEventRemovalType type);

  // Upsert the row for `key` in the config table to contain `value`.
  bool SetConfigValue(std::string_view key, int64_t value);
  // Get the value for `key` from the config table, or nullopt if absent.
  std::optional<int64_t> GetConfigValue(std::string_view key);

  // Checks if a given timestamp is missing or past its BTM interaction TTL.
  bool IsNullOrExpired(std::optional<base::Time> time);

  // When the number of entries in the database exceeds |max_entries_|, purge
  // down to |max_entries_| - |purge_entries_|.
  size_t max_entries_ = 3500;
  size_t purge_entries_ = 300;
  const base::FilePath db_path_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<sql::Database> db_ GUARDED_BY_CONTEXT(sequence_checker_);
  bool db_init_ = false;
  raw_ptr<base::Clock> clock_ = base::DefaultClock::GetInstance();
  sql::MetaTable meta_table_ GUARDED_BY_CONTEXT(sequence_checker_);
  mutable base::Time last_health_metrics_time_
      GUARDED_BY_CONTEXT(sequence_checker_) = base::Time::Min();
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BTM_BTM_DATABASE_H_
