// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/extras/sqlite/sqlite_persistent_cookie_store.h"

#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <tuple>
#include <unordered_set>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "base/values.h"
#include "build/build_config.h"
#include "crypto/sha2.h"
#include "net/base/features.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_util.h"
#include "net/extras/sqlite/cookie_crypto_delegate.h"
#include "net/extras/sqlite/sqlite_persistent_store_backend_base.h"
#include "net/log/net_log.h"
#include "net/log/net_log_values.h"
#include "sql/error_delegate_util.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"

using base::Time;

namespace {

static constexpr int kHoursInOneWeek = 24 * 7;
static constexpr int kHoursInOneYear = 24 * 365;

base::Value::Dict CookieKeyedLoadNetLogParams(
    const std::string& key,
    net::NetLogCaptureMode capture_mode) {
  if (!net::NetLogCaptureIncludesSensitive(capture_mode))
    return base::Value::Dict();
  base::Value::Dict dict;
  dict.Set("key", key);
  return dict;
}

// Used to populate a histogram for problems when loading cookies.
//
// Please do not reorder or remove entries. New entries must be added to the
// end of the list, just before kMaxValue.
enum class CookieLoadProblem {
  // Entry decryption failed.
  kDecryptFailed = 0,
  // Deprecated 03/2021.
  // COOKIE_LOAD_PROBLEM_DECRYPT_TIMEOUT = 1,
  // Cookie canonical form check failed.
  kNotCanonical = 2,
  // Could not open or initialize database.
  kOpenDb = 3,
  // Attempt to delete broken (and related) rows failed.
  KRecoveryFailed = 4,
  // Attempt to delete cookies with matching top_frame_site_keys failed. Added
  // in https://crrev.com/3153340 (M96).
  kDeleteCookiePartitionFailed = 5,
  // Hash verification of encrypted value failed. Added in
  // https://crrev.com/5875192 (M131).
  kHashFailed = 6,
  // Cookie was encrypted but no crypto delegate was passed. Added in
  // https://crrev.com/5875192 (M131).
  kNoCrypto = 7,
  // Cookie had values in both the plaintext and encrypted fields of the
  // database. Added in https://crrev.com/5875190 (M131).
  kValuesExistInBothEncryptedAndPlaintext = 8,
  kMaxValue = kValuesExistInBothEncryptedAndPlaintext,
};

// Used to populate a histogram for problems when committing cookies.
//
// Please do not reorder or remove entries. New entries must be added to the
// end of the list, just before kMaxValue.
enum class CookieCommitProblem {
  // Entry encryption failed.
  kEncryptFailed = 0,
  // Adding cookie to DB failed.
  kAdd = 1,
  // Updating access time of cookie failed.
  kUpdateAccess = 2,
  // Deleting cookie failed.
  kDelete = 3,
  // Committing the transaction failed.
  kTransactionCommit = 4,
  kMaxValue = kTransactionCommit,
};

void RecordCookieLoadProblem(CookieLoadProblem event) {
  UMA_HISTOGRAM_ENUMERATION("Cookie.LoadProblem", event);
}

void RecordCookieCommitProblem(CookieCommitProblem event) {
  UMA_HISTOGRAM_ENUMERATION("Cookie.CommitProblem", event);
}

// Records metrics around the age in hours of a cookie loaded from the store via
// MakeCookiesFromSQLStatement for use by some browser context.
void HistogramCookieAge(const net::CanonicalCookie& cookie) {
  if (cookie.IsPersistent()) {
    // We are studying the age of script cookies in active use. This record is
    // split into two histograms to improve resolution.
    if (!cookie.LastUpdateDate().is_null() &&
        cookie.SourceType() == net::CookieSourceType::kScript) {
      const int script_cookie_age_since_last_update_in_hours =
          (Time::Now() - cookie.LastUpdateDate()).InHours();
      if (script_cookie_age_since_last_update_in_hours > kHoursInOneWeek) {
        UMA_HISTOGRAM_CUSTOM_COUNTS(
            "Cookie.ScriptAgeSinceLastUpdateInHoursGTOneWeek",
            script_cookie_age_since_last_update_in_hours, kHoursInOneWeek + 1,
            kHoursInOneYear, 100);
      } else {
        UMA_HISTOGRAM_CUSTOM_COUNTS(
            "Cookie.ScriptAgeSinceLastUpdateInHoursLTEOneWeek",
            script_cookie_age_since_last_update_in_hours, 1,
            kHoursInOneWeek + 1, 100);
      }
    }
  } else {
    // We are studying the age of session cookies in active use. The record is
    // split into two histograms to improve resolution.
    if (!cookie.CreationDate().is_null()) {
      const int session_cookie_age_in_hours =
          (Time::Now() - cookie.CreationDate()).InHours();
      if (session_cookie_age_in_hours > kHoursInOneWeek) {
        UMA_HISTOGRAM_CUSTOM_COUNTS("Cookie.SessionAgeInHoursGTOneWeek2",
                                    session_cookie_age_in_hours,
                                    kHoursInOneWeek + 1, kHoursInOneYear, 100);
      } else {
        UMA_HISTOGRAM_CUSTOM_COUNTS("Cookie.SessionAgeInHoursLTEOneWeek2",
                                    session_cookie_age_in_hours, 1,
                                    kHoursInOneWeek + 1, 100);
      }
    }
    // Similar to the above, except this metric tracks time since the cookie was
    // last updated and not just initial creation.
    if (!cookie.LastUpdateDate().is_null()) {
      const int session_cookie_age_since_last_update_in_hours =
          (Time::Now() - cookie.LastUpdateDate()).InHours();
      if (session_cookie_age_since_last_update_in_hours > kHoursInOneWeek) {
        UMA_HISTOGRAM_CUSTOM_COUNTS(
            "Cookie.SessionAgeSinceLastUpdateInHoursGTOneWeek",
            session_cookie_age_since_last_update_in_hours, kHoursInOneWeek + 1,
            kHoursInOneYear, 100);
      } else {
        UMA_HISTOGRAM_CUSTOM_COUNTS(
            "Cookie.SessionAgeSinceLastUpdateInHoursLTEOneWeek",
            session_cookie_age_since_last_update_in_hours, 1,
            kHoursInOneWeek + 1, 100);
      }
    }
  }
}

}  // namespace

namespace net {

base::TaskPriority GetCookieStoreBackgroundSequencePriority() {
  return base::TaskPriority::USER_BLOCKING;
}

namespace {

// Version number of the database.
//
// Version 24 - 2024/08/15 - https://crrev.com/c/5792044
// Version 23 - 2024/04/10 - https://crrev.com/c/5169630
// Version 22 - 2024/03/22 - https://crrev.com/c/5378176
// Version 21 - 2023/11/22 - https://crrev.com/c/5049032
// Version 20 - 2023/11/14 - https://crrev.com/c/5030577
// Version 19 - 2023/09/22 - https://crrev.com/c/4704672
// Version 18 - 2022/04/19 - https://crrev.com/c/3594203
//
// Versions older than two years should be removed and marked as unsupported.
// This was last done in February 2024. https://crrev.com/c/5300252
// Be sure to update SQLitePersistentCookieStoreTest.TestInvalidVersionRecovery
// to test the latest unsupported version number.
//
// Unsupported versions:
// Version 17 - 2022/01/25 - https://crrev.com/c/3416230
// Version 16 - 2021/09/10 - https://crrev.com/c/3152897
// Version 15 - 2021/07/01 - https://crrev.com/c/3001822
// Version 14 - 2021/02/23 - https://crrev.com/c/2036899
// Version 13 - 2020/10/28 - https://crrev.com/c/2505468
// Version 12 - 2019/11/20 - https://crrev.com/c/1898301
// Version 11 - 2019/04/17 - https://crrev.com/c/1570416
// Version 10 - 2018/02/13 - https://crrev.com/c/906675
// Version 9  - 2015/04/17 - https://codereview.chromium.org/1083623003
// Version 8  - 2015/02/23 - https://codereview.chromium.org/876973003
// Version 7  - 2013/12/16 - https://codereview.chromium.org/24734007
// Version 6  - 2013/04/23 - https://codereview.chromium.org/14208017
// Version 5  - 2011/12/05 - https://codereview.chromium.org/8533013
// Version 4  - 2009/09/01 - https://codereview.chromium.org/183021
//

// Version 24 adds a SHA256 hash of the domain value to front of the the
// encrypted_value.
//
// Version 23 adds the value for has_cross_site_ancestor and updates any
// preexisting cookies with a source_scheme value of kUnset and a is_secure of
// true to have a source_scheme value of kSecure.
//
// Version 22 adds one new field: "source_type". This reflects the source of
// the last set/update to the cookie (unknown, http, script, other). Existing
// cookies in the DB default to "unknown".
//
// Version 21 removes the is_same_party column.
//
// Version 20 changes the UNIQUE constraint to include the source_scheme and
// source_port and begins to insert, update, and delete cookies based on their
// source_scheme and source_port.
//
// Version 19 caps expires_utc to no more than 400 days in the future for all
// stored cookies with has_expires. This is in compliance with section 7.2 of
// draft-ietf-httpbis-rfc6265bis-12.
//
// Version 18 adds one new field: "last_update_utc" (if not 0 this represents
// the last time the cookie was updated). This is distinct from creation_utc
// which is carried forward when cookies are updated.
//
// Version 17 fixes crbug.com/1290841: Bug in V16 migration.
//
// Version 16 changes the unique constraint's order of columns to have
// top_frame_site_key be after host_key. This allows us to use the internal
// index created by the UNIQUE keyword without to load cookies by domain
// without us needing to supply a top_frame_site_key. This is necessary because
// CookieMonster tracks pending cookie loading tasks by host key only.
// Version 16 also removes the DEFAULT value from several columns.
//
// Version 15 adds one new field: "top_frame_site_key" (if not empty then the
// string is the scheme and site of the topmost-level frame the cookie was
// created in). This field is deserialized into the cookie's partition key.
// top_frame_site_key is *NOT* the site-for-cookies when the cookie was created.
// In migrating, top_frame_site_key defaults to empty string. This change also
// changes the uniqueness constraint on cookies to include the
// top_frame_site_key as well.
//
// Version 14 just reads all encrypted cookies and re-writes them out again to
// make sure the new encryption key is in use. This active migration only
// happens on Windows, on other OS, this migration is a no-op.
//
// Version 13 adds two new fields: "source_port" (the port number of the source
// origin, and "is_same_party" (boolean indicating whether the cookie had a
// SameParty attribute). In migrating, source_port defaults to -1
// (url::PORT_UNSPECIFIED) for old entries for which the source port is unknown,
// and is_same_party defaults to false.
//
// Version 12 adds a column for "source_scheme" to store whether the
// cookie was set from a URL with a cryptographic scheme.
//
// Version 11 renames the "firstpartyonly" column to "samesite", and changes any
// stored values of kCookieSameSiteNoRestriction into
// kCookieSameSiteUnspecified to reflect the fact that those cookies were set
// without a SameSite attribute specified. Support for a value of
// kCookieSameSiteExtended for "samesite" was added, however, that value is now
// deprecated and is mapped to CookieSameSite::UNSPECIFIED when loading from the
// database.
//
// Version 10 removes the uniqueness constraint on the creation time (which
// was not propagated up the stack and caused problems in
// http://crbug.com/800414 and others).  It replaces that constraint by a
// constraint on (name, domain, path), which is spec-compliant (see
// https://tools.ietf.org/html/rfc6265#section-5.3 step 11).  Those fields
// can then be used in place of the creation time for updating access
// time and deleting cookies.
// Version 10 also marks all booleans in the store with an "is_" prefix
// to indicated their booleanness, as SQLite has no such concept.
//
// Version 9 adds a partial index to track non-persistent cookies.
// Non-persistent cookies sometimes need to be deleted on startup. There are
// frequently few or no non-persistent cookies, so the partial index allows the
// deletion to be sped up or skipped, without having to page in the DB.
//
// Version 8 adds "first-party only" cookies.
//
// Version 7 adds encrypted values.  Old values will continue to be used but
// all new values written will be encrypted on selected operating systems.  New
// records read by old clients will simply get an empty cookie value while old
// records read by new clients will continue to operate with the unencrypted
// version.  New and old clients alike will always write/update records with
// what they support.
//
// Version 6 adds cookie priorities. This allows developers to influence the
// order in which cookies are evicted in order to meet domain cookie limits.
//
// Version 5 adds the columns has_expires and is_persistent, so that the
// database can store session cookies as well as persistent cookies. Databases
// of version 5 are incompatible with older versions of code. If a database of
// version 5 is read by older code, session cookies will be treated as normal
// cookies. Currently, these fields are written, but not read anymore.
//
// In version 4, we migrated the time epoch.  If you open the DB with an older
// version on Mac or Linux, the times will look wonky, but the file will likely
// be usable. On Windows version 3 and 4 are the same.
//
// Version 3 updated the database to include the last access time, so we can
// expire them in decreasing order of use when we've reached the maximum
// number of cookies.
const int kCurrentVersionNumber = 24;
const int kCompatibleVersionNumber = 24;

}  // namespace

// This class is designed to be shared between any client thread and the
// background task runner. It batches operations and commits them on a timer.
//
// SQLitePersistentCookieStore::Load is called to load all cookies.  It
// delegates to Backend::Load, which posts a Backend::LoadAndNotifyOnDBThread
// task to the background runner.  This task calls Backend::ChainLoadCookies(),
// which repeatedly posts itself to the BG runner to load each eTLD+1's cookies
// in separate tasks.  When this is complete, Backend::CompleteLoadOnIOThread is
// posted to the client runner, which notifies the caller of
// SQLitePersistentCookieStore::Load that the load is complete.
//
// If a priority load request is invoked via SQLitePersistentCookieStore::
// LoadCookiesForKey, it is delegated to Backend::LoadCookiesForKey, which posts
// Backend::LoadKeyAndNotifyOnDBThread to the BG runner. That routine loads just
// that single domain key (eTLD+1)'s cookies, and posts a Backend::
// CompleteLoadForKeyOnIOThread to the client runner to notify the caller of
// SQLitePersistentCookieStore::LoadCookiesForKey that that load is complete.
//
// Subsequent to loading, mutations may be queued by any thread using
// AddCookie, UpdateCookieAccessTime, and DeleteCookie. These are flushed to
// disk on the BG runner every 30 seconds, 512 operations, or call to Flush(),
// whichever occurs first.
class SQLitePersistentCookieStore::Backend
    : public SQLitePersistentStoreBackendBase {
 public:
  Backend(const base::FilePath& path,
          scoped_refptr<base::SequencedTaskRunner> client_task_runner,
          scoped_refptr<base::SequencedTaskRunner> background_task_runner,
          bool restore_old_session_cookies,
          std::unique_ptr<CookieCryptoDelegate> crypto_delegate,
          bool enable_exclusive_access)
      : SQLitePersistentStoreBackendBase(path,
                                         /* histogram_tag = */ "Cookie",
                                         kCurrentVersionNumber,
                                         kCompatibleVersionNumber,
                                         std::move(background_task_runner),
                                         std::move(client_task_runner),
                                         enable_exclusive_access),
        restore_old_session_cookies_(restore_old_session_cookies),
        crypto_(std::move(crypto_delegate)) {}

  Backend(const Backend&) = delete;
  Backend& operator=(const Backend&) = delete;

  // Creates or loads the SQLite database.
  void Load(LoadedCallback loaded_callback);

  // Loads cookies for the domain key (eTLD+1). If no key is supplied then this
  // behaves identically to `Load`.
  void LoadCookiesForKey(base::optional_ref<const std::string> key,
                         LoadedCallback loaded_callback);

  // Steps through all results of |statement|, makes a cookie from each, and
  // adds the cookie to |cookies|. Returns true if everything loaded
  // successfully.
  bool MakeCookiesFromSQLStatement(
      std::vector<std::unique_ptr<CanonicalCookie>>& cookies,
      sql::Statement& statement,
      std::unordered_set<std::string>& top_frame_site_keys_to_delete);

  // Batch a cookie addition.
  void AddCookie(const CanonicalCookie& cc);

  // Batch a cookie access time update.
  void UpdateCookieAccessTime(const CanonicalCookie& cc);

  // Batch a cookie deletion.
  void DeleteCookie(const CanonicalCookie& cc);

  size_t GetQueueLengthForTesting();

  // Post background delete of all cookies that match |cookies|.
  void DeleteAllInList(const std::list<CookieOrigin>& cookies);

 private:
  // You should call Close() before destructing this object.
  ~Backend() override {
    DCHECK_EQ(0u, num_pending_);
    DCHECK(pending_.empty());
  }

  // Database upgrade statements.
  std::optional<int> DoMigrateDatabaseSchema() override;

  class PendingOperation {
   public:
    enum OperationType {
      COOKIE_ADD,
      COOKIE_UPDATEACCESS,
      COOKIE_DELETE,
    };

    PendingOperation(OperationType op, const CanonicalCookie& cc)
        : op_(op), cc_(cc) {}

    OperationType op() const { return op_; }
    const CanonicalCookie& cc() const { return cc_; }

   private:
    OperationType op_;
    CanonicalCookie cc_;
  };

 private:
  // Creates or loads the SQLite database on background runner. Supply domain
  // key (eTLD+1) to only load for this domain.
  void LoadAndNotifyInBackground(base::optional_ref<const std::string> key,
                                 LoadedCallback loaded_callback);

  // Notifies the CookieMonster when loading completes for a specific domain key
  // or for all domain keys. Triggers the callback and passes it all cookies
  // that have been loaded from DB since last IO notification.
  void NotifyLoadCompleteInForeground(LoadedCallback loaded_callback,
                                      bool load_success);

  // Called from Load when crypto gets obtained.
  void CryptoHasInitFromLoad(base::optional_ref<const std::string> key,
                             LoadedCallback loaded_callback);

  // Initialize the Cookies table.
  bool CreateDatabaseSchema() override;

  // Initialize the data base.
  bool DoInitializeDatabase() override;

  // Loads cookies for the next domain key from the DB, then either reschedules
  // itself or schedules the provided callback to run on the client runner (if
  // all domains are loaded).
  void ChainLoadCookies(LoadedCallback loaded_callback);

  // Load all cookies for a set of domains/hosts. The error recovery code
  // assumes |key| includes all related domains within an eTLD + 1.
  bool LoadCookiesForDomains(const std::set<std::string>& key);

  void DeleteTopFrameSiteKeys(
      const std::unordered_set<std::string>& top_frame_site_keys);

  // Batch a cookie operation (add or delete)
  void BatchOperation(PendingOperation::OperationType op,
                      const CanonicalCookie& cc);
  // Commit our pending operations to the database.
  void DoCommit() override;

  void DeleteSessionCookiesOnStartup();

  void BackgroundDeleteAllInList(const std::list<CookieOrigin>& cookies);

  // Shared code between the different load strategies to be used after all
  // cookies have been loaded.
  void FinishedLoadingCookies(LoadedCallback loaded_callback, bool success);

  void RecordOpenDBProblem() override {
    RecordCookieLoadProblem(CookieLoadProblem::kOpenDb);
  }

  void RecordDBMigrationProblem() override {
    RecordCookieLoadProblem(CookieLoadProblem::kOpenDb);
  }

  typedef std::list<std::unique_ptr<PendingOperation>> PendingOperationsForKey;
  typedef std::map<CanonicalCookie::StrictlyUniqueCookieKey,
                   PendingOperationsForKey>
      PendingOperationsMap;
  PendingOperationsMap pending_ GUARDED_BY(lock_);
  PendingOperationsMap::size_type num_pending_ GUARDED_BY(lock_) = 0;
  // Guard |cookies_|, |pending_|, |num_pending_|.
  base::Lock lock_;

  // Temporary buffer for cookies loaded from DB. Accumulates cookies to reduce
  // the number of messages sent to the client runner. Sent back in response to
  // individual load requests for domain keys or when all loading completes.
  std::vector<std::unique_ptr<CanonicalCookie>> cookies_ GUARDED_BY(lock_);

  // Map of domain keys(eTLD+1) to domains/hosts that are to be loaded from DB.
  std::map<std::string, std::set<std::string>> keys_to_load_;

  // If false, we should filter out session cookies when reading the DB.
  bool restore_old_session_cookies_;

  // Crypto instance, or nullptr if encryption is disabled.
  std::unique_ptr<CookieCryptoDelegate> crypto_;
};

namespace {

// Possible values for the 'priority' column.
enum DBCookiePriority {
  kCookiePriorityLow = 0,
  kCookiePriorityMedium = 1,
  kCookiePriorityHigh = 2,
};

DBCookiePriority CookiePriorityToDBCookiePriority(CookiePriority value) {
  switch (value) {
    case COOKIE_PRIORITY_LOW:
      return kCookiePriorityLow;
    case COOKIE_PRIORITY_MEDIUM:
      return kCookiePriorityMedium;
    case COOKIE_PRIORITY_HIGH:
      return kCookiePriorityHigh;
  }

  NOTREACHED_IN_MIGRATION();
  return kCookiePriorityMedium;
}

CookiePriority DBCookiePriorityToCookiePriority(DBCookiePriority value) {
  switch (value) {
    case kCookiePriorityLow:
      return COOKIE_PRIORITY_LOW;
    case kCookiePriorityMedium:
      return COOKIE_PRIORITY_MEDIUM;
    case kCookiePriorityHigh:
      return COOKIE_PRIORITY_HIGH;
  }

  NOTREACHED_IN_MIGRATION();
  return COOKIE_PRIORITY_DEFAULT;
}

// Possible values for the 'samesite' column
enum DBCookieSameSite {
  kCookieSameSiteUnspecified = -1,
  kCookieSameSiteNoRestriction = 0,
  kCookieSameSiteLax = 1,
  kCookieSameSiteStrict = 2,
  // Deprecated, mapped to kCookieSameSiteUnspecified.
  kCookieSameSiteExtended = 3
};

DBCookieSameSite CookieSameSiteToDBCookieSameSite(CookieSameSite value) {
  switch (value) {
    case CookieSameSite::NO_RESTRICTION:
      return kCookieSameSiteNoRestriction;
    case CookieSameSite::LAX_MODE:
      return kCookieSameSiteLax;
    case CookieSameSite::STRICT_MODE:
      return kCookieSameSiteStrict;
    case CookieSameSite::UNSPECIFIED:
      return kCookieSameSiteUnspecified;
  }
}

CookieSameSite DBCookieSameSiteToCookieSameSite(DBCookieSameSite value) {
  CookieSameSite samesite = CookieSameSite::UNSPECIFIED;
  switch (value) {
    case kCookieSameSiteNoRestriction:
      samesite = CookieSameSite::NO_RESTRICTION;
      break;
    case kCookieSameSiteLax:
      samesite = CookieSameSite::LAX_MODE;
      break;
    case kCookieSameSiteStrict:
      samesite = CookieSameSite::STRICT_MODE;
      break;
    // SameSite=Extended is deprecated, so we map to UNSPECIFIED.
    case kCookieSameSiteExtended:
    case kCookieSameSiteUnspecified:
      samesite = CookieSameSite::UNSPECIFIED;
      break;
  }
  return samesite;
}

// Possible values for the `source` column
enum DBCookieSourceType {
  kDBCookieSourceTypeUnknown = 0,
  kDBCookieSourceTypeHTTP = 1,
  kDBCookieSourceTypeScript = 2,
  kDBCookieSourceTypeOther = 3,
};

DBCookieSourceType CookieSourceTypeToDBCookieSourceType(
    CookieSourceType value) {
  switch (value) {
    case CookieSourceType::kUnknown:
      return kDBCookieSourceTypeUnknown;
    case CookieSourceType::kHTTP:
      return kDBCookieSourceTypeHTTP;
    case CookieSourceType::kScript:
      return kDBCookieSourceTypeScript;
    case CookieSourceType::kOther:
      return kDBCookieSourceTypeOther;
  }
}

CookieSourceType DBCookieSourceTypeToCookieSourceType(
    DBCookieSourceType value) {
  switch (value) {
    case kDBCookieSourceTypeUnknown:
      return CookieSourceType::kUnknown;
    case kDBCookieSourceTypeHTTP:
      return CookieSourceType::kHTTP;
    case kDBCookieSourceTypeScript:
      return CookieSourceType::kScript;
    case kDBCookieSourceTypeOther:
      return CookieSourceType::kOther;
    default:
      return CookieSourceType::kUnknown;
  }
}

CookieSourceScheme DBToCookieSourceScheme(int value) {
  int enum_max_value = static_cast<int>(CookieSourceScheme::kMaxValue);

  if (value < 0 || value > enum_max_value) {
    DLOG(WARNING) << "DB read of cookie's source scheme is invalid. Resetting "
                     "value to unset.";
    value = static_cast<int>(
        CookieSourceScheme::kUnset);  // Reset value to a known, useful, state.
  }

  return static_cast<CookieSourceScheme>(value);
}

// Increments a specified TimeDelta by the duration between this object's
// constructor and destructor. Not thread safe. Multiple instances may be
// created with the same delta instance as long as their lifetimes are nested.
// The shortest lived instances have no impact.
class IncrementTimeDelta {
 public:
  explicit IncrementTimeDelta(base::TimeDelta* delta)
      : delta_(delta), original_value_(*delta), start_(base::Time::Now()) {}

  IncrementTimeDelta(const IncrementTimeDelta&) = delete;
  IncrementTimeDelta& operator=(const IncrementTimeDelta&) = delete;

  ~IncrementTimeDelta() {
    *delta_ = original_value_ + base::Time::Now() - start_;
  }

 private:
  raw_ptr<base::TimeDelta> delta_;
  base::TimeDelta original_value_;
  base::Time start_;
};

bool CreateV20Schema(sql::Database* db) {
  CHECK(!db->DoesTableExist("cookies"));

  static constexpr char kCreateTableQuery[] =
      "CREATE TABLE cookies("
      "creation_utc INTEGER NOT NULL,"
      "host_key TEXT NOT NULL,"
      "top_frame_site_key TEXT NOT NULL,"
      "name TEXT NOT NULL,"
      "value TEXT NOT NULL,"
      "encrypted_value BLOB NOT NULL,"
      "path TEXT NOT NULL,"
      "expires_utc INTEGER NOT NULL,"
      "is_secure INTEGER NOT NULL,"
      "is_httponly INTEGER NOT NULL,"
      "last_access_utc INTEGER NOT NULL,"
      "has_expires INTEGER NOT NULL,"
      "is_persistent INTEGER NOT NULL,"
      "priority INTEGER NOT NULL,"
      "samesite INTEGER NOT NULL,"
      "source_scheme INTEGER NOT NULL,"
      "source_port INTEGER NOT NULL,"
      "is_same_party INTEGER NOT NULL,"
      "last_update_utc INTEGER NOT NULL);";

  static constexpr char kCreateIndexQuery[] =
      "CREATE UNIQUE INDEX cookies_unique_index "
      "ON cookies(host_key, top_frame_site_key, name, path, source_scheme, "
      "source_port)";

  return db->Execute(kCreateTableQuery) && db->Execute(kCreateIndexQuery);
}

bool CreateV21Schema(sql::Database* db) {
  CHECK(!db->DoesTableExist("cookies"));

  static constexpr char kCreateTableQuery[] =
      "CREATE TABLE cookies("
      "creation_utc INTEGER NOT NULL,"
      "host_key TEXT NOT NULL,"
      "top_frame_site_key TEXT NOT NULL,"
      "name TEXT NOT NULL,"
      "value TEXT NOT NULL,"
      "encrypted_value BLOB NOT NULL,"
      "path TEXT NOT NULL,"
      "expires_utc INTEGER NOT NULL,"
      "is_secure INTEGER NOT NULL,"
      "is_httponly INTEGER NOT NULL,"
      "last_access_utc INTEGER NOT NULL,"
      "has_expires INTEGER NOT NULL,"
      "is_persistent INTEGER NOT NULL,"
      "priority INTEGER NOT NULL,"
      "samesite INTEGER NOT NULL,"
      "source_scheme INTEGER NOT NULL,"
      "source_port INTEGER NOT NULL,"
      "last_update_utc INTEGER NOT NULL);";

  static constexpr char kCreateIndexQuery[] =
      "CREATE UNIQUE INDEX cookies_unique_index "
      "ON cookies(host_key, top_frame_site_key, name, path, source_scheme, "
      "source_port)";

  return db->Execute(kCreateTableQuery) && db->Execute(kCreateIndexQuery);
}

bool CreateV22Schema(sql::Database* db) {
  CHECK(!db->DoesTableExist("cookies"));

  static constexpr char kCreateTableQuery[] =
      "CREATE TABLE cookies("
      "creation_utc INTEGER NOT NULL,"
      "host_key TEXT NOT NULL,"
      "top_frame_site_key TEXT NOT NULL,"
      "name TEXT NOT NULL,"
      "value TEXT NOT NULL,"
      "encrypted_value BLOB NOT NULL,"
      "path TEXT NOT NULL,"
      "expires_utc INTEGER NOT NULL,"
      "is_secure INTEGER NOT NULL,"
      "is_httponly INTEGER NOT NULL,"
      "last_access_utc INTEGER NOT NULL,"
      "has_expires INTEGER NOT NULL,"
      "is_persistent INTEGER NOT NULL,"
      "priority INTEGER NOT NULL,"
      "samesite INTEGER NOT NULL,"
      "source_scheme INTEGER NOT NULL,"
      "source_port INTEGER NOT NULL,"
      "last_update_utc INTEGER NOT NULL,"
      "source_type INTEGER NOT NULL);";

  static constexpr char kCreateIndexQuery[] =
      "CREATE UNIQUE INDEX cookies_unique_index "
      "ON cookies(host_key, top_frame_site_key, name, path, source_scheme, "
      "source_port)";

  return db->Execute(kCreateTableQuery) && db->Execute(kCreateIndexQuery);
}

bool CreateV23Schema(sql::Database* db) {
  CHECK(!db->DoesTableExist("cookies"));

  static constexpr char kCreateTableQuery[] =
      "CREATE TABLE cookies("
      "creation_utc INTEGER NOT NULL,"
      "host_key TEXT NOT NULL,"
      "top_frame_site_key TEXT NOT NULL,"
      "name TEXT NOT NULL,"
      "value TEXT NOT NULL,"
      "encrypted_value BLOB NOT NULL,"
      "path TEXT NOT NULL,"
      "expires_utc INTEGER NOT NULL,"
      "is_secure INTEGER NOT NULL,"
      "is_httponly INTEGER NOT NULL,"
      "last_access_utc INTEGER NOT NULL,"
      "has_expires INTEGER NOT NULL,"
      "is_persistent INTEGER NOT NULL,"
      "priority INTEGER NOT NULL,"
      "samesite INTEGER NOT NULL,"
      "source_scheme INTEGER NOT NULL,"
      "source_port INTEGER NOT NULL,"
      "last_update_utc INTEGER NOT NULL,"
      "source_type INTEGER NOT NULL,"
      "has_cross_site_ancestor INTEGER NOT NULL);";

  static constexpr char kCreateIndexQuery[] =
      "CREATE UNIQUE INDEX cookies_unique_index "
      "ON cookies(host_key, top_frame_site_key, has_cross_site_ancestor, "
      "name, path, source_scheme, source_port)";

  return db->Execute(kCreateTableQuery) && db->Execute(kCreateIndexQuery);
}

// v24 schema is identical to v23 schema.
bool CreateV24Schema(sql::Database* db) {
  return CreateV23Schema(db);
}

}  // namespace

void SQLitePersistentCookieStore::Backend::Load(
    LoadedCallback loaded_callback) {
  LoadCookiesForKey(std::nullopt, std::move(loaded_callback));
}

void SQLitePersistentCookieStore::Backend::LoadCookiesForKey(
    base::optional_ref<const std::string> key,
    LoadedCallback loaded_callback) {
  if (crypto_) {
    crypto_->Init(base::BindOnce(&Backend::CryptoHasInitFromLoad, this,
                                 key.CopyAsOptional(),
                                 std::move(loaded_callback)));
  } else {
    CryptoHasInitFromLoad(key, std::move(loaded_callback));
  }
}

void SQLitePersistentCookieStore::Backend::CryptoHasInitFromLoad(
    base::optional_ref<const std::string> key,
    LoadedCallback loaded_callback) {
  PostBackgroundTask(
      FROM_HERE,
      base::BindOnce(&Backend::LoadAndNotifyInBackground, this,
                     key.CopyAsOptional(), std::move(loaded_callback)));
}

void SQLitePersistentCookieStore::Backend::LoadAndNotifyInBackground(
    base::optional_ref<const std::string> key,
    LoadedCallback loaded_callback) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  bool success = false;

  if (InitializeDatabase()) {
    if (!key.has_value()) {
      ChainLoadCookies(std::move(loaded_callback));
      return;
    }

    auto it = keys_to_load_.find(*key);
    if (it != keys_to_load_.end()) {
      success = LoadCookiesForDomains(it->second);
      keys_to_load_.erase(it);
    } else {
      success = true;
    }
  }

  FinishedLoadingCookies(std::move(loaded_callback), success);
}

void SQLitePersistentCookieStore::Backend::NotifyLoadCompleteInForeground(
    LoadedCallback loaded_callback,
    bool load_success) {
  DCHECK(client_task_runner()->RunsTasksInCurrentSequence());

  std::vector<std::unique_ptr<CanonicalCookie>> cookies;
  {
    base::AutoLock locked(lock_);
    cookies.swap(cookies_);
  }

  std::move(loaded_callback).Run(std::move(cookies));
}

bool SQLitePersistentCookieStore::Backend::CreateDatabaseSchema() {
  DCHECK(db());

  return db()->DoesTableExist("cookies") || CreateV24Schema(db());
}

bool SQLitePersistentCookieStore::Backend::DoInitializeDatabase() {
  DCHECK(db());

  // Retrieve all the domains
  sql::Statement smt(
      db()->GetUniqueStatement("SELECT DISTINCT host_key FROM cookies"));

  if (!smt.is_valid()) {
    Reset();
    return false;
  }

  std::vector<std::string> host_keys;
  while (smt.Step())
    host_keys.push_back(smt.ColumnString(0));

  // Build a map of domain keys (always eTLD+1) to domains.
  for (const auto& domain : host_keys) {
    std::string key = CookieMonster::GetKey(domain);
    keys_to_load_[key].insert(domain);
  }

  if (!restore_old_session_cookies_)
    DeleteSessionCookiesOnStartup();

  return true;
}

void SQLitePersistentCookieStore::Backend::ChainLoadCookies(
    LoadedCallback loaded_callback) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());

  bool load_success = true;

  if (!db()) {
    // Close() has been called on this store.
    load_success = false;
  } else if (keys_to_load_.size() > 0) {
    // Load cookies for the first domain key.
    auto it = keys_to_load_.begin();
    load_success = LoadCookiesForDomains(it->second);
    keys_to_load_.erase(it);
  }

  // If load is successful and there are more domain keys to be loaded,
  // then post a background task to continue chain-load;
  // Otherwise notify on client runner.
  if (load_success && keys_to_load_.size() > 0) {
    bool success = background_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&Backend::ChainLoadCookies, this,
                                  std::move(loaded_callback)));
    if (!success) {
      LOG(WARNING) << "Failed to post task from " << FROM_HERE.ToString()
                   << " to background_task_runner().";
    }
  } else {
    FinishedLoadingCookies(std::move(loaded_callback), load_success);
  }
}

bool SQLitePersistentCookieStore::Backend::LoadCookiesForDomains(
    const std::set<std::string>& domains) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());

  sql::Statement smt, delete_statement;
  if (restore_old_session_cookies_) {
    smt.Assign(db()->GetCachedStatement(
        SQL_FROM_HERE,
        "SELECT creation_utc, host_key, top_frame_site_key, name, value, path, "
        "expires_utc, is_secure, is_httponly, last_access_utc, has_expires, "
        "is_persistent, priority, encrypted_value, samesite, source_scheme, "
        "source_port, last_update_utc, source_type, has_cross_site_ancestor "
        "FROM cookies WHERE host_key "
        "= "
        "?"));
  } else {
    smt.Assign(db()->GetCachedStatement(
        SQL_FROM_HERE,
        "SELECT creation_utc, host_key, top_frame_site_key, name, value, path, "
        "expires_utc, is_secure, is_httponly, last_access_utc, has_expires, "
        "is_persistent, priority, encrypted_value, samesite, source_scheme, "
        "source_port, last_update_utc, source_type, has_cross_site_ancestor "
        "FROM cookies WHERE "
        "host_key = ? AND "
        "is_persistent = 1"));
  }
  delete_statement.Assign(db()->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM cookies WHERE host_key = ?"));
  if (!smt.is_valid() || !delete_statement.is_valid()) {
    delete_statement.Clear();
    smt.Clear();  // Disconnect smt_ref from db_.
    Reset();
    return false;
  }

  std::vector<std::unique_ptr<CanonicalCookie>> cookies;
  std::unordered_set<std::string> top_frame_site_keys_to_delete;
  auto it = domains.begin();
  bool ok = true;
  for (; it != domains.end() && ok; ++it) {
    smt.BindString(0, *it);
    ok = MakeCookiesFromSQLStatement(cookies, smt,
                                     top_frame_site_keys_to_delete);
    smt.Reset(true);
  }

  DeleteTopFrameSiteKeys(std::move(top_frame_site_keys_to_delete));

  if (ok) {
    base::AutoLock locked(lock_);
    std::move(cookies.begin(), cookies.end(), std::back_inserter(cookies_));
  } else {
    // There were some cookies that were in database but could not be loaded
    // and handed over to CookieMonster. This is trouble since it means that
    // if some website tries to send them again, CookieMonster won't know to
    // issue a delete, and then the addition would violate the uniqueness
    // constraints and not go through.
    //
    // For data consistency, we drop the entire eTLD group.
    for (const std::string& domain : domains) {
      delete_statement.BindString(0, domain);
      if (!delete_statement.Run()) {
        // TODO(morlovich): Is something more drastic called for here?
        RecordCookieLoadProblem(CookieLoadProblem::KRecoveryFailed);
      }
      delete_statement.Reset(true);
    }
  }
  return true;
}

void SQLitePersistentCookieStore::Backend::DeleteTopFrameSiteKeys(
    const std::unordered_set<std::string>& top_frame_site_keys) {
  if (top_frame_site_keys.empty())
    return;

  sql::Statement delete_statement;
  delete_statement.Assign(db()->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM cookies WHERE top_frame_site_key = ?"));
  if (!delete_statement.is_valid())
    return;

  for (const std::string& key : top_frame_site_keys) {
    delete_statement.BindString(0, key);
    if (!delete_statement.Run())
      RecordCookieLoadProblem(CookieLoadProblem::kDeleteCookiePartitionFailed);
    delete_statement.Reset(true);
  }
}

bool SQLitePersistentCookieStore::Backend::MakeCookiesFromSQLStatement(
    std::vector<std::unique_ptr<CanonicalCookie>>& cookies,
    sql::Statement& statement,
    std::unordered_set<std::string>& top_frame_site_keys_to_delete) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  bool ok = true;
  while (statement.Step()) {
    std::string domain = statement.ColumnString(1);
    std::string value = statement.ColumnString(4);
    std::string encrypted_value = statement.ColumnString(13);
    const bool encrypted_and_plaintext_values =
        !value.empty() && !encrypted_value.empty();
    UMA_HISTOGRAM_BOOLEAN("Cookie.EncryptedAndPlaintextValues",
                          encrypted_and_plaintext_values);

    // Ensure feature is fully activated for all users who load cookies, before
    // checking the validity of the row.
    if (base::FeatureList::IsEnabled(
            features::kEncryptedAndPlaintextValuesAreInvalid)) {
      if (encrypted_and_plaintext_values) {
        RecordCookieLoadProblem(
            CookieLoadProblem::kValuesExistInBothEncryptedAndPlaintext);
        ok = false;
        continue;
      }
    }

    if (!encrypted_value.empty()) {
      if (!crypto_) {
        RecordCookieLoadProblem(CookieLoadProblem::kNoCrypto);
        ok = false;
        continue;
      }
      bool decrypt_ok = crypto_->DecryptString(encrypted_value, &value);
      if (!decrypt_ok) {
        RecordCookieLoadProblem(CookieLoadProblem::kDecryptFailed);
        ok = false;
        continue;
      }
      std::string correct_hash = crypto::SHA256HashString(domain);
      if (!base::StartsWith(value, correct_hash,
                            base::CompareCase::SENSITIVE)) {
        RecordCookieLoadProblem(CookieLoadProblem::kHashFailed);
        ok = false;
        continue;
      }
      value = value.substr(correct_hash.length());
    }

    // If we can't create a CookiePartitionKey from SQL values, we delete any
    // cookie with the same top_frame_site_key value.
    base::expected<std::optional<CookiePartitionKey>, std::string>
        partition_key = CookiePartitionKey::FromStorage(
            statement.ColumnString(2), statement.ColumnBool(19));
    if (!partition_key.has_value()) {
      top_frame_site_keys_to_delete.insert(statement.ColumnString(2));
      continue;
    }
    // Returns nullptr if the resulting cookie is not canonical.
    std::unique_ptr<net::CanonicalCookie> cc = CanonicalCookie::FromStorage(
        /*name=*/statement.ColumnString(3),        //
        value,                                     //
        domain,                                    //
        /*path=*/statement.ColumnString(5),        //
        /*creation=*/statement.ColumnTime(0),      //
        /*expiration=*/statement.ColumnTime(6),    //
        /*last_access=*/statement.ColumnTime(9),   //
        /*last_update=*/statement.ColumnTime(17),  //
        /*secure=*/statement.ColumnBool(7),        //
        /*httponly=*/statement.ColumnBool(8),      //
                                                   /*same_site=*/
        DBCookieSameSiteToCookieSameSite(
            static_cast<DBCookieSameSite>(statement.ColumnInt(14))),  //
        /*priority=*/
        DBCookiePriorityToCookiePriority(
            static_cast<DBCookiePriority>(statement.ColumnInt(12))),        //
        /*partition_key=*/std::move(partition_key.value()),                 //
        /*source_scheme=*/DBToCookieSourceScheme(statement.ColumnInt(15)),  //
        /*source_port=*/statement.ColumnInt(16),                            //
        /*source_type=*/
        DBCookieSourceTypeToCookieSourceType(
            static_cast<DBCookieSourceType>(statement.ColumnInt(18))));  //
    if (cc) {
      DLOG_IF(WARNING, cc->CreationDate() > Time::Now())
          << "CreationDate too recent";
      if (!cc->LastUpdateDate().is_null()) {
        DLOG_IF(WARNING, cc->LastUpdateDate() > Time::Now())
            << "LastUpdateDate too recent";
        // In order to anticipate the potential effects of the expiry limit in
        // rfc6265bis, we need to check how long it's been since the cookie was
        // refreshed (if LastUpdateDate is populated). We use 100 buckets for
        // the highest reasonable granularity, set 1 day as the minimum and
        // don't track over a 400 max (since these cookies will expire anyway).
        UMA_HISTOGRAM_CUSTOM_COUNTS(
            "Cookie.DaysSinceRefreshForRetrieval",
            (base::Time::Now() - cc->LastUpdateDate()).InDays(), 1, 400, 100);
      }
      HistogramCookieAge(*cc);
      cookies.push_back(std::move(cc));
    } else {
      RecordCookieLoadProblem(CookieLoadProblem::kNotCanonical);
      ok = false;
    }
  }

  return ok;
}

std::optional<int>
SQLitePersistentCookieStore::Backend::DoMigrateDatabaseSchema() {
  int cur_version = meta_table()->GetVersionNumber();

  if (cur_version == 18) {
    SCOPED_UMA_HISTOGRAM_TIMER("Cookie.TimeDatabaseMigrationToV19");

    sql::Statement update_statement(
        db()->GetCachedStatement(SQL_FROM_HERE,
                                 "UPDATE cookies SET expires_utc = ? WHERE "
                                 "has_expires = 1 AND expires_utc > ?"));
    if (!update_statement.is_valid()) {
      return std::nullopt;
    }

    sql::Transaction transaction(db());
    if (!transaction.Begin()) {
      return std::nullopt;
    }

    base::Time expires_cap = base::Time::Now() + base::Days(400);
    update_statement.BindTime(0, expires_cap);
    update_statement.BindTime(1, expires_cap);
    if (!update_statement.Run()) {
      return std::nullopt;
    }

    ++cur_version;
    if (!meta_table()->SetVersionNumber(cur_version) ||
        !meta_table()->SetCompatibleVersionNumber(
            std::min(cur_version, kCompatibleVersionNumber)) ||
        !transaction.Commit()) {
      return std::nullopt;
    }
  }

  if (cur_version == 19) {
    SCOPED_UMA_HISTOGRAM_TIMER("Cookie.TimeDatabaseMigrationToV20");

    sql::Transaction transaction(db());
    if (!transaction.Begin()) {
      return std::nullopt;
    }

    if (!db()->Execute("DROP TABLE IF EXISTS cookies_old")) {
      return std::nullopt;
    }
    if (!db()->Execute("ALTER TABLE cookies RENAME TO cookies_old")) {
      return std::nullopt;
    }
    if (!db()->Execute("DROP INDEX IF EXISTS cookies_unique_index")) {
      return std::nullopt;
    }

    if (!CreateV20Schema(db())) {
      return std::nullopt;
    }

    static constexpr char insert_cookies_sql[] =
        "INSERT OR REPLACE INTO cookies "
        "(creation_utc, host_key, top_frame_site_key, name, value, "
        "encrypted_value, path, expires_utc, is_secure, is_httponly, "
        "last_access_utc, has_expires, is_persistent, priority, samesite, "
        "source_scheme, source_port, is_same_party, last_update_utc) "
        "SELECT creation_utc, host_key, top_frame_site_key, name, value,"
        "       encrypted_value, path, expires_utc, is_secure, is_httponly,"
        "       last_access_utc, has_expires, is_persistent, priority, "
        "       samesite, source_scheme, source_port, is_same_party, "
        "last_update_utc "
        "FROM cookies_old ORDER BY creation_utc ASC";
    if (!db()->Execute(insert_cookies_sql)) {
      return std::nullopt;
    }
    if (!db()->Execute("DROP TABLE cookies_old")) {
      return std::nullopt;
    }

    ++cur_version;
    if (!meta_table()->SetVersionNumber(cur_version) ||
        !meta_table()->SetCompatibleVersionNumber(
            std::min(cur_version, kCompatibleVersionNumber)) ||
        !transaction.Commit()) {
      return std::nullopt;
    }
  }

  if (cur_version == 20) {
    SCOPED_UMA_HISTOGRAM_TIMER("Cookie.TimeDatabaseMigrationToV21");

    sql::Transaction transaction(db());
    if (!transaction.Begin()) {
      return std::nullopt;
    }

    if (!db()->Execute("DROP TABLE IF EXISTS cookies_old")) {
      return std::nullopt;
    }
    if (!db()->Execute("ALTER TABLE cookies RENAME TO cookies_old")) {
      return std::nullopt;
    }
    if (!db()->Execute("DROP INDEX IF EXISTS cookies_unique_index")) {
      return std::nullopt;
    }

    if (!CreateV21Schema(db())) {
      return std::nullopt;
    }

    static constexpr char insert_cookies_sql[] =
        "INSERT OR REPLACE INTO cookies "
        "(creation_utc, host_key, top_frame_site_key, name, value, "
        "encrypted_value, path, expires_utc, is_secure, is_httponly, "
        "last_access_utc, has_expires, is_persistent, priority, samesite, "
        "source_scheme, source_port, last_update_utc) "
        "SELECT creation_utc, host_key, top_frame_site_key, name, value,"
        "       encrypted_value, path, expires_utc, is_secure, is_httponly,"
        "       last_access_utc, has_expires, is_persistent, priority, "
        "       samesite, source_scheme, source_port, last_update_utc "
        "FROM cookies_old ORDER BY creation_utc ASC";
    if (!db()->Execute(insert_cookies_sql)) {
      return std::nullopt;
    }
    if (!db()->Execute("DROP TABLE cookies_old")) {
      return std::nullopt;
    }

    ++cur_version;
    if (!meta_table()->SetVersionNumber(cur_version) ||
        !meta_table()->SetCompatibleVersionNumber(
            std::min(cur_version, kCompatibleVersionNumber)) ||
        !transaction.Commit()) {
      return std::nullopt;
    }
  }

  if (cur_version == 21) {
    SCOPED_UMA_HISTOGRAM_TIMER("Cookie.TimeDatabaseMigrationToV22");

    sql::Transaction transaction(db());
    if (!transaction.Begin()) {
      return std::nullopt;
    }

    if (!db()->Execute("DROP TABLE IF EXISTS cookies_old")) {
      return std::nullopt;
    }
    if (!db()->Execute("ALTER TABLE cookies RENAME TO cookies_old")) {
      return std::nullopt;
    }
    if (!db()->Execute("DROP INDEX IF EXISTS cookies_unique_index")) {
      return std::nullopt;
    }

    if (!CreateV22Schema(db())) {
      return std::nullopt;
    }

    // The default `source_type` is 0 which is CookieSourceType::kUnknown.
    static constexpr char insert_cookies_sql[] =
        "INSERT OR REPLACE INTO cookies "
        "(creation_utc, host_key, top_frame_site_key, name, value, "
        "encrypted_value, path, expires_utc, is_secure, is_httponly, "
        "last_access_utc, has_expires, is_persistent, priority, samesite, "
        "source_scheme, source_port, last_update_utc, source_type) "
        "SELECT creation_utc, host_key, top_frame_site_key, name, value,"
        "       encrypted_value, path, expires_utc, is_secure, is_httponly,"
        "       last_access_utc, has_expires, is_persistent, priority, "
        "       samesite, source_scheme, source_port, last_update_utc, 0 "
        "FROM cookies_old ORDER BY creation_utc ASC";
    if (!db()->Execute(insert_cookies_sql)) {
      return std::nullopt;
    }
    if (!db()->Execute("DROP TABLE cookies_old")) {
      return std::nullopt;
    }

    ++cur_version;
    if (!meta_table()->SetVersionNumber(cur_version) ||
        !meta_table()->SetCompatibleVersionNumber(
            std::min(cur_version, kCompatibleVersionNumber)) ||
        !transaction.Commit()) {
      return std::nullopt;
    }
  }

  if (cur_version == 22) {
    SCOPED_UMA_HISTOGRAM_TIMER("Cookie.TimeDatabaseMigrationToV23");
    sql::Transaction transaction(db());
    if (!transaction.Begin()) {
      return std::nullopt;
    }

    if (!db()->Execute("DROP TABLE IF EXISTS cookies_old")) {
      return std::nullopt;
    }
    if (!db()->Execute("ALTER TABLE cookies RENAME TO cookies_old")) {
      return std::nullopt;
    }
    if (!db()->Execute("DROP INDEX IF EXISTS cookies_unique_index")) {
      return std::nullopt;
    }

    if (!CreateV23Schema(db())) {
      return std::nullopt;
    }
    /*
     For the case statement setting source_scheme,
     value of 0 reflects int value of CookieSourceScheme::kUnset
     value of 2 reflects int value of CookieSourceScheme::kSecure

     For the case statement setting has_cross_site_ancestor, it has the
     potential to have a origin mismatch due to substring operations.
      EX: the domain ample.com will appear as a substring of the domain
      example.com even though they are different origins.
     We are ok with this because the other elements of the UNIQUE INDEX
     will always be different preventing accidental access.
    */

    static constexpr char insert_cookies_sql[] =
        "INSERT OR REPLACE INTO cookies "
        "(creation_utc, host_key, top_frame_site_key, name, value, "
        "encrypted_value, path, expires_utc, is_secure, is_httponly, "
        "last_access_utc, has_expires, is_persistent, priority, samesite, "
        "source_scheme, source_port, last_update_utc, source_type, "
        "has_cross_site_ancestor) "
        "SELECT creation_utc, host_key, top_frame_site_key, name, value,"
        "       encrypted_value, path, expires_utc, is_secure, is_httponly,"
        "       last_access_utc, has_expires, is_persistent, priority, "
        "       samesite, "
        "       CASE WHEN source_scheme = 0 AND is_secure = 1 "
        "           THEN 2 ELSE source_scheme END, "
        "       source_port, last_update_utc, source_type, "
        "       CASE WHEN INSTR(top_frame_site_key, '://') > 0 AND host_key "
        "           LIKE CONCAT('%', SUBSTR(top_frame_site_key, "
        "           INSTR(top_frame_site_key,'://') + 3),  '%') "
        "           THEN 0 ELSE 1 "
        "           END AS has_cross_site_ancestor "
        "FROM cookies_old ORDER BY creation_utc ASC";
    if (!db()->Execute(insert_cookies_sql)) {
      return std::nullopt;
    }
    if (!db()->Execute("DROP TABLE cookies_old")) {
      return std::nullopt;
    }

    ++cur_version;
    if (!meta_table()->SetVersionNumber(cur_version) ||
        !meta_table()->SetCompatibleVersionNumber(
            std::min(cur_version, kCompatibleVersionNumber)) ||
        !transaction.Commit()) {
      return std::nullopt;
    }
  }

  if (cur_version == 23) {
    SCOPED_UMA_HISTOGRAM_TIMER("Cookie.TimeDatabaseMigrationToV24");
    sql::Transaction transaction(db());
    if (!transaction.Begin()) {
      return std::nullopt;
    }

    if (crypto_) {
      sql::Statement select_smt, update_smt;

      select_smt.Assign(db()->GetCachedStatement(
          SQL_FROM_HERE,
          "SELECT rowid, host_key, encrypted_value, value FROM cookies"));

      update_smt.Assign(db()->GetCachedStatement(
          SQL_FROM_HERE,
          "UPDATE cookies SET encrypted_value=?, value=? WHERE "
          "rowid=?"));

      if (!select_smt.is_valid() || !update_smt.is_valid()) {
        return std::nullopt;
      }

      std::map<int64_t, std::string> encrypted_values;

      while (select_smt.Step()) {
        int64_t rowid = select_smt.ColumnInt64(0);
        std::string domain = select_smt.ColumnString(1);
        std::string encrypted_value = select_smt.ColumnString(2);
        std::string value = select_smt.ColumnString(3);
        // If encrypted value is empty but value is non-empty it means that in a
        // previous version of the database, there was no crypto and the value
        // was stored unencrypted. In this case, since we have crypto now, we
        // should encrypt the value.
        // In the case that both plaintext and encrypted values exist, the
        // encrypted value always takes precedence.
        std::string decrypted_value;
        if (encrypted_value.empty() && !value.empty()) {
          decrypted_value = value;
        } else {
          if (!crypto_->DecryptString(encrypted_value, &decrypted_value)) {
            RecordCookieLoadProblem(CookieLoadProblem::kDecryptFailed);
            continue;
          }
        }
        std::string new_encrypted_value;

        if (!crypto_->EncryptString(
                base::StrCat(
                    {crypto::SHA256HashString(domain), decrypted_value}),
                &new_encrypted_value)) {
          RecordCookieCommitProblem(CookieCommitProblem::kEncryptFailed);
          continue;
        }
        encrypted_values[rowid] = new_encrypted_value;
      }

      for (const auto& entry : encrypted_values) {
        update_smt.Reset(true);
        update_smt.BindString(/*encrypted_value*/ 0, entry.second);
        // Clear the value, since it is now encrypted.
        update_smt.BindString(/*value*/ 1, {});
        update_smt.BindInt64(/*rowid*/ 2, entry.first);
        if (!update_smt.Run()) {
          return std::nullopt;
        }
      }
    }

    ++cur_version;
    if (!meta_table()->SetVersionNumber(cur_version) ||
        !meta_table()->SetCompatibleVersionNumber(
            std::min(cur_version, kCompatibleVersionNumber)) ||
        !transaction.Commit()) {
      return std::nullopt;
    }
  }

  // Put future migration cases here.
  return std::make_optional(cur_version);
}

void SQLitePersistentCookieStore::Backend::AddCookie(
    const CanonicalCookie& cc) {
  BatchOperation(PendingOperation::COOKIE_ADD, cc);
}

void SQLitePersistentCookieStore::Backend::UpdateCookieAccessTime(
    const CanonicalCookie& cc) {
  BatchOperation(PendingOperation::COOKIE_UPDATEACCESS, cc);
}

void SQLitePersistentCookieStore::Backend::DeleteCookie(
    const CanonicalCookie& cc) {
  BatchOperation(PendingOperation::COOKIE_DELETE, cc);
}

void SQLitePersistentCookieStore::Backend::BatchOperation(
    PendingOperation::OperationType op,
    const CanonicalCookie& cc) {
  // Commit every 30 seconds.
  constexpr base::TimeDelta kCommitInterval = base::Seconds(30);
  // Commit right away if we have more than 512 outstanding operations.
  constexpr size_t kCommitAfterBatchSize = 512;
  DCHECK(!background_task_runner()->RunsTasksInCurrentSequence());

  // We do a full copy of the cookie here, and hopefully just here.
  auto po = std::make_unique<PendingOperation>(op, cc);

  PendingOperationsMap::size_type num_pending;
  {
    base::AutoLock locked(lock_);
    // When queueing the operation, see if it overwrites any already pending
    // ones for the same row.
    auto key = cc.StrictlyUniqueKey();
    auto iter_and_result = pending_.emplace(key, PendingOperationsForKey());
    PendingOperationsForKey& ops_for_key = iter_and_result.first->second;
    if (!iter_and_result.second) {
      // Insert failed -> already have ops.
      if (po->op() == PendingOperation::COOKIE_DELETE) {
        // A delete op makes all the previous ones irrelevant.
        ops_for_key.clear();
      } else if (po->op() == PendingOperation::COOKIE_UPDATEACCESS) {
        if (!ops_for_key.empty() &&
            ops_for_key.back()->op() == PendingOperation::COOKIE_UPDATEACCESS) {
          // If access timestamp is updated twice in a row, can dump the earlier
          // one.
          ops_for_key.pop_back();
        }
        // At most delete + add before (and no access time updates after above
        // conditional).
        DCHECK_LE(ops_for_key.size(), 2u);
      } else {
        // Nothing special is done for adds, since if they're overwriting,
        // they'll be preceded by deletes anyway.
        DCHECK_LE(ops_for_key.size(), 1u);
      }
    }
    ops_for_key.push_back(std::move(po));
    // Note that num_pending_ counts number of calls to BatchOperation(), not
    // the current length of the queue; this is intentional to guarantee
    // progress, as the length of the queue may decrease in some cases.
    num_pending = ++num_pending_;
  }

  if (num_pending == 1) {
    // We've gotten our first entry for this batch, fire off the timer.
    if (!background_task_runner()->PostDelayedTask(
            FROM_HERE, base::BindOnce(&Backend::Commit, this),
            kCommitInterval)) {
      NOTREACHED_IN_MIGRATION() << "background_task_runner() is not running.";
    }
  } else if (num_pending == kCommitAfterBatchSize) {
    // We've reached a big enough batch, fire off a commit now.
    PostBackgroundTask(FROM_HERE, base::BindOnce(&Backend::Commit, this));
  }
}

void SQLitePersistentCookieStore::Backend::DoCommit() {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());

  PendingOperationsMap ops;
  {
    base::AutoLock locked(lock_);
    pending_.swap(ops);
    num_pending_ = 0;
  }

  // Maybe an old timer fired or we are already Close()'ed.
  if (!db() || ops.empty())
    return;

  sql::Statement add_statement(db()->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO cookies (creation_utc, host_key, top_frame_site_key, name, "
      "value, encrypted_value, path, expires_utc, is_secure, is_httponly, "
      "last_access_utc, has_expires, is_persistent, priority, samesite, "
      "source_scheme, source_port, last_update_utc, source_type, "
      "has_cross_site_ancestor) "
      "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
  if (!add_statement.is_valid())
    return;

  sql::Statement update_access_statement(db()->GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE cookies SET last_access_utc=? WHERE "
      "name=? AND host_key=? AND top_frame_site_key=? AND path=? AND "
      "source_scheme=? AND source_port=? AND has_cross_site_ancestor=?"));
  if (!update_access_statement.is_valid())
    return;

  sql::Statement delete_statement(db()->GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM cookies WHERE "
      "name=? AND host_key=? AND top_frame_site_key=? AND path=? AND "
      "source_scheme=? AND source_port=? AND has_cross_site_ancestor=?"));
  if (!delete_statement.is_valid())
    return;

  sql::Transaction transaction(db());
  if (!transaction.Begin())
    return;

  for (auto& kv : ops) {
    for (std::unique_ptr<PendingOperation>& po_entry : kv.second) {
      // Free the cookies as we commit them to the database.
      std::unique_ptr<PendingOperation> po(std::move(po_entry));
      base::expected<CookiePartitionKey::SerializedCookiePartitionKey,
                     std::string>
          serialized_partition_key =
              CookiePartitionKey::Serialize(po->cc().PartitionKey());
      if (!serialized_partition_key.has_value()) {
        continue;
      }

      switch (po->op()) {
        case PendingOperation::COOKIE_ADD:
          add_statement.Reset(true);
          add_statement.BindTime(0, po->cc().CreationDate());
          add_statement.BindString(1, po->cc().Domain());
          add_statement.BindString(2, serialized_partition_key->TopLevelSite());
          add_statement.BindString(3, po->cc().Name());
          if (crypto_) {
            std::string encrypted_value;
            if (!crypto_->EncryptString(
                    base::StrCat({crypto::SHA256HashString(po->cc().Domain()),
                                  po->cc().Value()}),
                    &encrypted_value)) {
              DLOG(WARNING) << "Could not encrypt a cookie, skipping add.";
              RecordCookieCommitProblem(CookieCommitProblem::kEncryptFailed);
              continue;
            }
            add_statement.BindCString(4, "");  // value
            // BindBlob() immediately makes an internal copy of the data.
            add_statement.BindBlob(5, encrypted_value);
          } else {
            add_statement.BindString(4, po->cc().Value());
            add_statement.BindBlob(5,
                                   base::span<uint8_t>());  // encrypted_value
          }
          add_statement.BindString(6, po->cc().Path());
          add_statement.BindTime(7, po->cc().ExpiryDate());
          add_statement.BindBool(8, po->cc().SecureAttribute());
          add_statement.BindBool(9, po->cc().IsHttpOnly());
          add_statement.BindTime(10, po->cc().LastAccessDate());
          add_statement.BindBool(11, po->cc().IsPersistent());
          add_statement.BindBool(12, po->cc().IsPersistent());
          add_statement.BindInt(
              13, CookiePriorityToDBCookiePriority(po->cc().Priority()));
          add_statement.BindInt(
              14, CookieSameSiteToDBCookieSameSite(po->cc().SameSite()));
          add_statement.BindInt(15, static_cast<int>(po->cc().SourceScheme()));
          add_statement.BindInt(16, po->cc().SourcePort());
          add_statement.BindTime(17, po->cc().LastUpdateDate());
          add_statement.BindInt(
              18, CookieSourceTypeToDBCookieSourceType(po->cc().SourceType()));
          add_statement.BindBool(
              19, serialized_partition_key->has_cross_site_ancestor());

          if (!add_statement.Run()) {
            DLOG(WARNING) << "Could not add a cookie to the DB.";
            RecordCookieCommitProblem(CookieCommitProblem::kAdd);
          }
          break;

        case PendingOperation::COOKIE_UPDATEACCESS:
          update_access_statement.Reset(true);
          update_access_statement.BindTime(0, po->cc().LastAccessDate());
          update_access_statement.BindString(1, po->cc().Name());
          update_access_statement.BindString(2, po->cc().Domain());
          update_access_statement.BindString(
              3, serialized_partition_key->TopLevelSite());
          update_access_statement.BindString(4, po->cc().Path());
          update_access_statement.BindInt(
              5, static_cast<int>(po->cc().SourceScheme()));
          update_access_statement.BindInt(6, po->cc().SourcePort());
          update_access_statement.BindBool(
              7, serialized_partition_key->has_cross_site_ancestor());
          if (!update_access_statement.Run()) {
            DLOG(WARNING)
                << "Could not update cookie last access time in the DB.";
            RecordCookieCommitProblem(CookieCommitProblem::kUpdateAccess);
          }
          break;

        case PendingOperation::COOKIE_DELETE:
          delete_statement.Reset(true);
          delete_statement.BindString(0, po->cc().Name());
          delete_statement.BindString(1, po->cc().Domain());
          delete_statement.BindString(2,
                                      serialized_partition_key->TopLevelSite());
          delete_statement.BindString(3, po->cc().Path());
          delete_statement.BindInt(4,
                                   static_cast<int>(po->cc().SourceScheme()));
          delete_statement.BindInt(5, po->cc().SourcePort());
          delete_statement.BindBool(
              6, serialized_partition_key->has_cross_site_ancestor());
          if (!delete_statement.Run()) {
            DLOG(WARNING) << "Could not delete a cookie from the DB.";
            RecordCookieCommitProblem(CookieCommitProblem::kDelete);
          }
          break;

        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
    }
  }
  bool commit_ok = transaction.Commit();
  if (!commit_ok) {
    RecordCookieCommitProblem(CookieCommitProblem::kTransactionCommit);
  }
}

size_t SQLitePersistentCookieStore::Backend::GetQueueLengthForTesting() {
  DCHECK(client_task_runner()->RunsTasksInCurrentSequence());
  size_t total = 0u;
  {
    base::AutoLock locked(lock_);
    for (const auto& key_val : pending_) {
      total += key_val.second.size();
    }
  }
  return total;
}

void SQLitePersistentCookieStore::Backend::DeleteAllInList(
    const std::list<CookieOrigin>& cookies) {
  if (cookies.empty())
    return;

  if (background_task_runner()->RunsTasksInCurrentSequence()) {
    BackgroundDeleteAllInList(cookies);
  } else {
    // Perform deletion on background task runner.
    PostBackgroundTask(
        FROM_HERE,
        base::BindOnce(&Backend::BackgroundDeleteAllInList, this, cookies));
  }
}

void SQLitePersistentCookieStore::Backend::DeleteSessionCookiesOnStartup() {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());
  if (!db()->Execute("DELETE FROM cookies WHERE is_persistent != 1"))
    LOG(WARNING) << "Unable to delete session cookies.";
}

// TODO(crbug.com/40188414) Investigate including top_frame_site_key in the
// WHERE clause.
void SQLitePersistentCookieStore::Backend::BackgroundDeleteAllInList(
    const std::list<CookieOrigin>& cookies) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());

  if (!db())
    return;

  // Force a commit of any pending writes before issuing deletes.
  // TODO(rohitrao): Remove the need for this Commit() by instead pruning the
  // list of pending operations. https://crbug.com/486742.
  Commit();

  sql::Statement delete_statement(db()->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM cookies WHERE host_key=? AND is_secure=?"));
  if (!delete_statement.is_valid()) {
    LOG(WARNING) << "Unable to delete cookies on shutdown.";
    return;
  }

  sql::Transaction transaction(db());
  if (!transaction.Begin()) {
    LOG(WARNING) << "Unable to delete cookies on shutdown.";
    return;
  }

  for (const auto& cookie : cookies) {
    const GURL url(cookie_util::CookieOriginToURL(cookie.first, cookie.second));
    if (!url.is_valid())
      continue;

    delete_statement.Reset(true);
    delete_statement.BindString(0, cookie.first);
    delete_statement.BindInt(1, cookie.second);
    if (!delete_statement.Run()) {
      LOG(WARNING) << "Could not delete a cookie from the DB.";
    }
  }

  if (!transaction.Commit())
    LOG(WARNING) << "Unable to delete cookies on shutdown.";
}

void SQLitePersistentCookieStore::Backend::FinishedLoadingCookies(
    LoadedCallback loaded_callback,
    bool success) {
  PostClientTask(FROM_HERE,
                 base::BindOnce(&Backend::NotifyLoadCompleteInForeground, this,
                                std::move(loaded_callback), success));
}

SQLitePersistentCookieStore::SQLitePersistentCookieStore(
    const base::FilePath& path,
    const scoped_refptr<base::SequencedTaskRunner>& client_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
    bool restore_old_session_cookies,
    std::unique_ptr<CookieCryptoDelegate> crypto_delegate,
    bool enable_exclusive_access)
    : backend_(base::MakeRefCounted<Backend>(path,
                                             client_task_runner,
                                             background_task_runner,
                                             restore_old_session_cookies,
                                             std::move(crypto_delegate),
                                             enable_exclusive_access)) {}

void SQLitePersistentCookieStore::DeleteAllInList(
    const std::list<CookieOrigin>& cookies) {
  backend_->DeleteAllInList(cookies);
}

void SQLitePersistentCookieStore::Load(LoadedCallback loaded_callback,
                                       const NetLogWithSource& net_log) {
  DCHECK(!loaded_callback.is_null());
  net_log_ = net_log;
  net_log_.BeginEvent(NetLogEventType::COOKIE_PERSISTENT_STORE_LOAD);
  // Note that |backend_| keeps |this| alive by keeping a reference count.
  // If this class is ever converted over to a WeakPtr<> pattern (as TODO it
  // should be) this will need to be replaced by a more complex pattern that
  // guarantees |loaded_callback| being called even if the class has been
  // destroyed. |backend_| needs to outlive |this| to commit changes to disk.
  backend_->Load(base::BindOnce(&SQLitePersistentCookieStore::CompleteLoad,
                                this, std::move(loaded_callback)));
}

void SQLitePersistentCookieStore::LoadCookiesForKey(
    const std::string& key,
    LoadedCallback loaded_callback) {
  DCHECK(!loaded_callback.is_null());
  net_log_.AddEvent(NetLogEventType::COOKIE_PERSISTENT_STORE_KEY_LOAD_STARTED,
                    [&](NetLogCaptureMode capture_mode) {
                      return CookieKeyedLoadNetLogParams(key, capture_mode);
                    });
  // Note that |backend_| keeps |this| alive by keeping a reference count.
  // If this class is ever converted over to a WeakPtr<> pattern (as TODO it
  // should be) this will need to be replaced by a more complex pattern that
  // guarantees |loaded_callback| being called even if the class has been
  // destroyed. |backend_| needs to outlive |this| to commit changes to disk.
  backend_->LoadCookiesForKey(
      key, base::BindOnce(&SQLitePersistentCookieStore::CompleteKeyedLoad, this,
                          key, std::move(loaded_callback)));
}

void SQLitePersistentCookieStore::AddCookie(const CanonicalCookie& cc) {
  backend_->AddCookie(cc);
}

void SQLitePersistentCookieStore::UpdateCookieAccessTime(
    const CanonicalCookie& cc) {
  backend_->UpdateCookieAccessTime(cc);
}

void SQLitePersistentCookieStore::DeleteCookie(const CanonicalCookie& cc) {
  backend_->DeleteCookie(cc);
}

void SQLitePersistentCookieStore::SetForceKeepSessionState() {
  // This store never discards session-only cookies, so this call has no effect.
}

void SQLitePersistentCookieStore::SetBeforeCommitCallback(
    base::RepeatingClosure callback) {
  backend_->SetBeforeCommitCallback(std::move(callback));
}

void SQLitePersistentCookieStore::Flush(base::OnceClosure callback) {
  backend_->Flush(std::move(callback));
}

size_t SQLitePersistentCookieStore::GetQueueLengthForTesting() {
  return backend_->GetQueueLengthForTesting();
}

SQLitePersistentCookieStore::~SQLitePersistentCookieStore() {
  net_log_.AddEventWithStringParams(
      NetLogEventType::COOKIE_PERSISTENT_STORE_CLOSED, "type",
      "SQLitePersistentCookieStore");
  backend_->Close();
}

void SQLitePersistentCookieStore::CompleteLoad(
    LoadedCallback callback,
    std::vector<std::unique_ptr<CanonicalCookie>> cookie_list) {
  net_log_.EndEvent(NetLogEventType::COOKIE_PERSISTENT_STORE_LOAD);
  std::move(callback).Run(std::move(cookie_list));
}

void SQLitePersistentCookieStore::CompleteKeyedLoad(
    const std::string& key,
    LoadedCallback callback,
    std::vector<std::unique_ptr<CanonicalCookie>> cookie_list) {
  net_log_.AddEventWithStringParams(
      NetLogEventType::COOKIE_PERSISTENT_STORE_KEY_LOAD_COMPLETED, "domain",
      key);
  std::move(callback).Run(std::move(cookie_list));
}

}  // namespace net
