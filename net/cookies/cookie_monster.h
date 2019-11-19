// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brought to you by the letter D and the number 2.

#ifndef NET_COOKIES_COOKIE_MONSTER_H_
#define NET_COOKIES_COOKIE_MONSTER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/circular_deque.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_delegate.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_monster_change_dispatcher.h"
#include "net/cookies/cookie_store.h"
#include "net/log/net_log_with_source.h"
#include "url/gurl.h"

namespace base {
class HistogramBase;
}  // namespace base

namespace net {

class CookieChangeDispatcher;

// The cookie monster is the system for storing and retrieving cookies. It has
// an in-memory list of all cookies, and synchronizes non-session cookies to an
// optional permanent storage that implements the PersistentCookieStore
// interface.
//
// Tasks may be deferred if all affected cookies are not yet loaded from the
// backing store. Otherwise, callbacks may be invoked immediately.
//
// A cookie task is either pending loading of the entire cookie store, or
// loading of cookies for a specific domain key (GetKey(), roughly eTLD+1). In
// the former case, the cookie callback will be queued in tasks_pending_ while
// PersistentCookieStore chain loads the cookie store on DB thread. In the
// latter case, the cookie callback will be queued in tasks_pending_for_key_
// while PermanentCookieStore loads cookies for the specified domain key on DB
// thread.
class NET_EXPORT CookieMonster : public CookieStore {
 public:
  class PersistentCookieStore;

  // Terminology:
  //    * The 'top level domain' (TLD) of an internet domain name is
  //      the terminal "." free substring (e.g. "com" for google.com
  //      or world.std.com).
  //    * The 'effective top level domain' (eTLD) is the longest
  //      "." initiated terminal substring of an internet domain name
  //      that is controlled by a general domain registrar.
  //      (e.g. "co.uk" for news.bbc.co.uk).
  //    * The 'effective top level domain plus one' (eTLD+1) is the
  //      shortest "." delimited terminal substring of an internet
  //      domain name that is not controlled by a general domain
  //      registrar (e.g. "bbc.co.uk" for news.bbc.co.uk, or
  //      "google.com" for news.google.com).  The general assumption
  //      is that all hosts and domains under an eTLD+1 share some
  //      administrative control.

  // CookieMap is the central data structure of the CookieMonster.  It
  // is a map whose values are pointers to CanonicalCookie data
  // structures (the data structures are owned by the CookieMonster
  // and must be destroyed when removed from the map).  The key is based on the
  // effective domain of the cookies.  If the domain of the cookie has an
  // eTLD+1, that is the key for the map.  If the domain of the cookie does not
  // have an eTLD+1, the key of the map is the host the cookie applies to (it is
  // not legal to have domain cookies without an eTLD+1).  This rule
  // excludes cookies for, e.g, ".com", ".co.uk", or ".internalnetwork".
  // This behavior is the same as the behavior in Firefox v 3.6.10.

  // NOTE(deanm):
  // I benchmarked hash_multimap vs multimap.  We're going to be query-heavy
  // so it would seem like hashing would help.  However they were very
  // close, with multimap being a tiny bit faster.  I think this is because
  // our map is at max around 1000 entries, and the additional complexity
  // for the hashing might not overcome the O(log(1000)) for querying
  // a multimap.  Also, multimap is standard, another reason to use it.
  // TODO(rdsmith): This benchmark should be re-done now that we're allowing
  // substantially more entries in the map.
  using CookieMap =
      std::multimap<std::string, std::unique_ptr<CanonicalCookie>>;
  using CookieMapItPair = std::pair<CookieMap::iterator, CookieMap::iterator>;
  using CookieItVector = std::vector<CookieMap::iterator>;

  // Cookie garbage collection thresholds.  Based off of the Mozilla defaults.
  // When the number of cookies gets to k{Domain,}MaxCookies
  // purge down to k{Domain,}MaxCookies - k{Domain,}PurgeCookies.
  // It might seem scary to have a high purge value, but really it's not.
  // You just make sure that you increase the max to cover the increase
  // in purge, and we would have been purging the same number of cookies.
  // We're just going through the garbage collection process less often.
  // Note that the DOMAIN values are per eTLD+1; see comment for the
  // CookieMap typedef.  So, e.g., the maximum number of cookies allowed for
  // google.com and all of its subdomains will be 150-180.
  //
  // Any cookies accessed more recently than kSafeFromGlobalPurgeDays will not
  // be evicted by global garbage collection, even if we have more than
  // kMaxCookies.  This does not affect domain garbage collection.
  static const size_t kDomainMaxCookies;
  static const size_t kDomainPurgeCookies;
  static const size_t kMaxCookies;
  static const size_t kPurgeCookies;

  // Quota for cookies with {low, medium, high} priorities within a domain.
  static const size_t kDomainCookiesQuotaLow;
  static const size_t kDomainCookiesQuotaMedium;
  static const size_t kDomainCookiesQuotaHigh;

  // The number of days since last access that cookies will not be subject
  // to global garbage collection.
  static const int kSafeFromGlobalPurgeDays;

  // The store passed in should not have had Init() called on it yet. This
  // class will take care of initializing it. The backing store is NOT owned by
  // this class, but it must remain valid for the duration of the cookie
  // monster's existence. If |store| is NULL, then no backing store will be
  // updated. |net_log| must outlive the CookieMonster and can be null.
  CookieMonster(scoped_refptr<PersistentCookieStore> store,
                NetLog* net_log);

  // Only used during unit testing.
  // |net_log| must outlive the CookieMonster.
  CookieMonster(scoped_refptr<PersistentCookieStore> store,
                base::TimeDelta last_access_threshold,
                NetLog* net_log);

  ~CookieMonster() override;

  // Writes all the cookies in |list| into the store, replacing all cookies
  // currently present in store.
  // This method does not flush the backend.
  // TODO(rdsmith, mmenke): Do not use this function; it is deprecated
  // and should be removed.
  // See https://codereview.chromium.org/2882063002/#msg64.
  void SetAllCookiesAsync(const CookieList& list, SetCookiesCallback callback);

  // CookieStore implementation.
  void SetCanonicalCookieAsync(std::unique_ptr<CanonicalCookie> cookie,
                               std::string source_scheme,
                               const CookieOptions& options,
                               SetCookiesCallback callback) override;
  void GetCookieListWithOptionsAsync(const GURL& url,
                                     const CookieOptions& options,
                                     GetCookieListCallback callback) override;
  void GetAllCookiesAsync(GetAllCookiesCallback callback) override;
  void GetAllCookiesWithAccessSemanticsAsync(
      GetAllCookiesWithAccessSemanticsCallback callback) override;
  void DeleteCanonicalCookieAsync(const CanonicalCookie& cookie,
                                  DeleteCallback callback) override;
  void DeleteAllCreatedInTimeRangeAsync(
      const CookieDeletionInfo::TimeRange& creation_range,
      DeleteCallback callback) override;
  void DeleteAllMatchingInfoAsync(CookieDeletionInfo delete_info,
                                  DeleteCallback callback) override;
  void DeleteSessionCookiesAsync(DeleteCallback) override;
  void FlushStore(base::OnceClosure callback) override;
  void SetForceKeepSessionState() override;
  CookieChangeDispatcher& GetChangeDispatcher() override;
  void SetCookieableSchemes(const std::vector<std::string>& schemes,
                            SetCookieableSchemesCallback callback) override;

  // Enables writing session cookies into the cookie database. If this this
  // method is called, it must be called before first use of the instance
  // (i.e. as part of the instance initialization process).
  void SetPersistSessionCookies(bool persist_session_cookies);

  // Determines if the scheme of the URL is a scheme that cookies will be
  // stored for.
  bool IsCookieableScheme(const std::string& scheme);

  // The default list of schemes the cookie monster can handle.
  static const char* const kDefaultCookieableSchemes[];
  static const int kDefaultCookieableSchemesCount;

  void DumpMemoryStats(base::trace_event::ProcessMemoryDump* pmd,
                       const std::string& parent_absolute_name) const override;

  // Find a key based on the given domain, which will be used to find all
  // cookies potentially relevant to it. This is used for lookup in cookies_ as
  // well as for PersistentCookieStore::LoadCookiesForKey. See comment on keys
  // before the CookieMap typedef.
  static std::string GetKey(base::StringPiece domain);

 private:
  // For garbage collection constants.
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest, TestHostGarbageCollection);
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest,
                           GarbageCollectWithSecureCookiesOnly);
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest, TestGCTimes);

  // For validation of key values.
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest, TestDomainTree);
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest, TestImport);
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest, GetKey);
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest, TestGetKey);

  // For FindCookiesForKey.
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest, ShortLivedSessionCookies);

  // For CookieSource histogram enum.
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest, CookieSourceHistogram);

  // For kSafeFromGlobalPurgeDays in CookieStore.
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest, EvictSecureCookies);

  // For CookieDeleteEquivalent histogram enum.
  FRIEND_TEST_ALL_PREFIXES(CookieMonsterTest,
                           CookieDeleteEquivalentHistogramTest);

  // Internal reasons for deletion, used to populate informative histograms
  // and to provide a public cause for onCookieChange notifications.
  //
  // If you add or remove causes from this list, please be sure to also update
  // the CookieChangeCause mapping inside ChangeCauseMapping. New items (if
  // necessary) should be added at the end of the list, just before
  // DELETE_COOKIE_LAST_ENTRY.
  enum DeletionCause {
    DELETE_COOKIE_EXPLICIT = 0,
    DELETE_COOKIE_OVERWRITE = 1,
    DELETE_COOKIE_EXPIRED = 2,
    DELETE_COOKIE_EVICTED = 3,
    DELETE_COOKIE_DUPLICATE_IN_BACKING_STORE = 4,
    DELETE_COOKIE_DONT_RECORD = 5,  // For final cleanup after flush to store.

    // Cookies evicted during domain-level garbage collection.
    DELETE_COOKIE_EVICTED_DOMAIN = 6,

    // Cookies evicted during global garbage collection (which takes place after
    // domain-level garbage collection fails to bring the cookie store under
    // the overall quota.
    DELETE_COOKIE_EVICTED_GLOBAL = 7,

    // #8 was DELETE_COOKIE_EVICTED_DOMAIN_PRE_SAFE
    // #9 was DELETE_COOKIE_EVICTED_DOMAIN_POST_SAFE

    // A common idiom is to remove a cookie by overwriting it with an
    // already-expired expiration date. This captures that case.
    DELETE_COOKIE_EXPIRED_OVERWRITE = 10,

    // Cookies are not allowed to contain control characters in the name or
    // value. However, we used to allow them, so we are now evicting any such
    // cookies as we load them. See http://crbug.com/238041.
    DELETE_COOKIE_CONTROL_CHAR = 11,

    // When strict secure cookies is enabled, non-secure cookies are evicted
    // right after expired cookies.
    DELETE_COOKIE_NON_SECURE = 12,

    DELETE_COOKIE_LAST_ENTRY = 13
  };

  // This enum is used to generate a histogramed bitmask measureing the types
  // of stored cookies. Please do not reorder the list when adding new entries.
  // New items MUST be added at the end of the list, just before
  // COOKIE_TYPE_LAST_ENTRY;
  enum CookieType {
    COOKIE_TYPE_SAME_SITE = 0,
    COOKIE_TYPE_HTTPONLY,
    COOKIE_TYPE_SECURE,
    COOKIE_TYPE_LAST_ENTRY
  };

  // Used to populate a histogram containing information about the
  // sources of Secure and non-Secure cookies: that is, whether such
  // cookies are set by origins with cryptographic or non-cryptographic
  // schemes. Please do not reorder the list when adding new
  // entries. New items MUST be added at the end of the list, just
  // before COOKIE_SOURCE_LAST_ENTRY.
  //
  // COOKIE_SOURCE_(NON)SECURE_COOKIE_(NON)CRYPTOGRAPHIC_SCHEME means
  // that a cookie was set or overwritten from a URL with the given type
  // of scheme. This enum should not be used when cookies are *cleared*,
  // because its purpose is to understand if Chrome can deprecate the
  // ability of HTTP urls to set/overwrite Secure cookies.
  enum CookieSource {
    COOKIE_SOURCE_SECURE_COOKIE_CRYPTOGRAPHIC_SCHEME = 0,
    COOKIE_SOURCE_SECURE_COOKIE_NONCRYPTOGRAPHIC_SCHEME,
    COOKIE_SOURCE_NONSECURE_COOKIE_CRYPTOGRAPHIC_SCHEME,
    COOKIE_SOURCE_NONSECURE_COOKIE_NONCRYPTOGRAPHIC_SCHEME,
    COOKIE_SOURCE_LAST_ENTRY
  };

  // Record statistics every kRecordStatisticsIntervalSeconds of uptime.
  static const int kRecordStatisticsIntervalSeconds = 10 * 60;

  // Sets a canonical cookie, deletes equivalents and performs garbage
  // collection.  |source_scheme| indicates what scheme the cookie is being set
  // from; secure cookies cannot be altered from insecure schemes, and some
  // schemes may not be authorized.
  //
  // |options| indicates if this setting operation is allowed
  // to affect http_only or same-site cookies.
  void SetCanonicalCookie(std::unique_ptr<CanonicalCookie> cookie,
                          std::string source_scheme,
                          const CookieOptions& options,
                          SetCookiesCallback callback);

  void GetAllCookies(GetAllCookiesCallback callback);

  void AttachAccessSemanticsListForCookieList(
      GetAllCookiesWithAccessSemanticsCallback callback,
      const CookieList& cookie_list);

  void GetCookieListWithOptions(const GURL& url,
                                const CookieOptions& options,
                                GetCookieListCallback callback);

  void DeleteAllCreatedInTimeRange(
      const CookieDeletionInfo::TimeRange& creation_range,
      DeleteCallback callback);

  void DeleteAllMatchingInfo(net::CookieDeletionInfo delete_info,
                             DeleteCallback callback);

  void DeleteCanonicalCookie(const CanonicalCookie& cookie,
                             DeleteCallback callback);

  void DeleteSessionCookies(DeleteCallback callback);

  // The first access to the cookie store initializes it. This method should be
  // called before any access to the cookie store.
  void MarkCookieStoreAsInitialized();

  // Fetches all cookies if the backing store exists and they're not already
  // being fetched.
  void FetchAllCookiesIfNecessary();

  // Fetches all cookies from the backing store.
  void FetchAllCookies();

  // Whether all cookies should be fetched as soon as any is requested.
  bool ShouldFetchAllCookiesWhenFetchingAnyCookie();

  // Stores cookies loaded from the backing store and invokes any deferred
  // calls. |beginning_time| should be the moment PersistentCookieStore::Load
  // was invoked and is used for reporting histogram_time_blocked_on_load_.
  // See PersistentCookieStore::Load for details on the contents of cookies.
  void OnLoaded(base::TimeTicks beginning_time,
                std::vector<std::unique_ptr<CanonicalCookie>> cookies);

  // Stores cookies loaded from the backing store and invokes the deferred
  // task(s) pending loading of cookies associated with the domain key
  // (GetKey, roughly eTLD+1). Called when all cookies for the domain key have
  // been loaded from DB. See PersistentCookieStore::Load for details on the
  // contents of cookies.
  void OnKeyLoaded(const std::string& key,
                   std::vector<std::unique_ptr<CanonicalCookie>> cookies);

  // Stores the loaded cookies.
  void StoreLoadedCookies(
      std::vector<std::unique_ptr<CanonicalCookie>> cookies);

  // Invokes deferred calls.
  void InvokeQueue();

  // Checks that |cookies_| matches our invariants, and tries to repair any
  // inconsistencies. (In other words, it does not have duplicate cookies).
  void EnsureCookiesMapIsValid();

  // Checks for any duplicate cookies for CookieMap key |key| which lie between
  // |begin| and |end|. If any are found, all but the most recent are deleted.
  void TrimDuplicateCookiesForKey(const std::string& key,
                                  CookieMap::iterator begin,
                                  CookieMap::iterator end);

  void SetDefaultCookieableSchemes();

  void FindCookiesForRegistryControlledHost(
      const GURL& url,
      std::vector<CanonicalCookie*>* cookies);

  void FilterCookiesWithOptions(const GURL url,
                                const CookieOptions options,
                                std::vector<CanonicalCookie*>* cookie_ptrs,
                                CookieStatusList* included_cookies,
                                CookieStatusList* excluded_cookies);

  // Possibly delete an existing cookie equivalent to |cookie_being_set| (same
  // path, domain, and name).
  //
  // |source_secure| indicates if the source may override existing secure
  // cookies. If the source is not secure, and there is an existing "equivalent"
  // cookie that is Secure, that cookie will be preserved, under "Leave Secure
  // Cookies Alone" (see
  // https://tools.ietf.org/html/draft-ietf-httpbis-cookie-alone-01).
  // ("equivalent" here is in quotes because the equivalency check for the
  // purposes of preserving existing Secure cookies is slightly more inclusive.)
  //
  // If |skip_httponly| is true, httponly cookies will not be deleted even if
  // they are equivalent.
  // |key| is the key to find the cookie in cookies_; see the comment before the
  // CookieMap typedef for details.
  //
  // If a cookie is deleted, and its value matches |cookie_being_set|'s value,
  // then |creation_date_to_inherit| will be set to that cookie's creation date.
  //
  // The cookie will not be deleted if |*status| is not "include" when calling
  // the function. The function will update |*status| with exclusion reasons if
  // a secure cookie was skipped or an httponly cookie was skipped.
  //
  // NOTE: There should never be more than a single matching equivalent cookie.
  void MaybeDeleteEquivalentCookieAndUpdateStatus(
      const std::string& key,
      const CanonicalCookie& cookie_being_set,
      bool source_secure,
      bool skip_httponly,
      bool already_expired,
      base::Time* creation_date_to_inherit,
      CanonicalCookie::CookieInclusionStatus* status);

  // Inserts |cc| into cookies_. Returns an iterator that points to the inserted
  // cookie in cookies_. Guarantee: all iterators to cookies_ remain valid.
  CookieMap::iterator InternalInsertCookie(const std::string& key,
                                           std::unique_ptr<CanonicalCookie> cc,
                                           bool sync_to_store);

  // Sets all cookies from |list| after deleting any equivalent cookie.
  // For data gathering purposes, this routine is treated as if it is
  // restoring saved cookies; some statistics are not gathered in this case.
  void SetAllCookies(CookieList list, SetCookiesCallback callback);

  void InternalUpdateCookieAccessTime(CanonicalCookie* cc,
                                      const base::Time& current_time);

  // |deletion_cause| argument is used for collecting statistics and choosing
  // the correct CookieChangeCause for OnCookieChange notifications. Guarantee:
  // All iterators to cookies_, except for the deleted entry, remain valid.
  void InternalDeleteCookie(CookieMap::iterator it,
                            bool sync_to_store,
                            DeletionCause deletion_cause);

  // If the number of cookies for CookieMap key |key|, or globally, are
  // over the preset maximums above, garbage collect, first for the host and
  // then globally.  See comments above garbage collection threshold
  // constants for details.
  //
  // Returns the number of cookies deleted (useful for debugging).
  size_t GarbageCollect(const base::Time& current, const std::string& key);

  // Helper for GarbageCollect(). Deletes up to |purge_goal| cookies with a
  // priority less than or equal to |priority| from |cookies|, while ensuring
  // that at least the |to_protect| most-recent cookies are retained.
  // |protected_secure_cookies| specifies whether or not secure cookies should
  // be protected from deletion.
  //
  // |cookies| must be sorted from least-recent to most-recent.
  //
  // Returns the number of cookies deleted.
  size_t PurgeLeastRecentMatches(CookieItVector* cookies,
                                 CookiePriority priority,
                                 size_t to_protect,
                                 size_t purge_goal,
                                 bool protect_secure_cookies);

  // Helper for GarbageCollect(); can be called directly as well.  Deletes all
  // expired cookies in |itpair|.  If |cookie_its| is non-NULL, all the
  // non-expired cookies from |itpair| are appended to |cookie_its|.
  //
  // Returns the number of cookies deleted.
  size_t GarbageCollectExpired(const base::Time& current,
                               const CookieMapItPair& itpair,
                               CookieItVector* cookie_its);

  // Helper for GarbageCollect(). Deletes all cookies in the range specified by
  // [|it_begin|, |it_end|). Returns the number of cookies deleted.
  size_t GarbageCollectDeleteRange(const base::Time& current,
                                   DeletionCause cause,
                                   CookieItVector::iterator cookie_its_begin,
                                   CookieItVector::iterator cookie_its_end);

  // Helper for GarbageCollect(). Deletes cookies in |cookie_its| from least to
  // most recently used, but only before |safe_date|. Also will stop deleting
  // when the number of remaining cookies hits |purge_goal|.
  //
  // Sets |earliest_time| to be the earliest last access time of a cookie that
  // was not deleted, or base::Time() if no such cookie exists.
  size_t GarbageCollectLeastRecentlyAccessed(const base::Time& current,
                                             const base::Time& safe_date,
                                             size_t purge_goal,
                                             CookieItVector cookie_its,
                                             base::Time* earliest_time);

  bool HasCookieableScheme(const GURL& url);

  // Get the cookie's access semantics (LEGACY or NONLEGACY) from the cookie
  // access delegate, if it is non-null. Otherwise return UNKNOWN.
  CookieAccessSemantics GetAccessSemanticsForCookie(
      const CanonicalCookie& cookie) const;

  // Statistics support

  // This function should be called repeatedly, and will record
  // statistics if a sufficient time period has passed.
  void RecordPeriodicStats(const base::Time& current_time);

  // Initialize the above variables; should only be called from
  // the constructor.
  void InitializeHistograms();

  // Defers the callback until the full coookie database has been loaded. If
  // it's already been loaded, runs the callback synchronously.
  void DoCookieCallback(base::OnceClosure callback);

  // Defers the callback until the cookies relevant to given URL have been
  // loaded. If they've already been loaded, runs the callback synchronously.
  void DoCookieCallbackForURL(base::OnceClosure callback, const GURL& url);

  // Defers the callback until the cookies relevant to given host or domain
  // have been loaded. If they've already been loaded, runs the callback
  // synchronously.
  void DoCookieCallbackForHostOrDomain(base::OnceClosure callback,
                                       base::StringPiece host_or_domain);

  // Histogram variables; see CookieMonster::InitializeHistograms() in
  // cookie_monster.cc for details.
  base::HistogramBase* histogram_expiration_duration_minutes_;
  base::HistogramBase* histogram_count_;
  base::HistogramBase* histogram_cookie_type_;
  base::HistogramBase* histogram_cookie_source_scheme_;
  base::HistogramBase* histogram_time_blocked_on_load_;

  CookieMap cookies_;

  CookieMonsterChangeDispatcher change_dispatcher_;

  // Indicates whether the cookie store has been initialized.
  bool initialized_;

  // Indicates whether the cookie store has started fetching all cookies.
  bool started_fetching_all_cookies_;
  // Indicates whether the cookie store has finished fetching all cookies.
  bool finished_fetching_all_cookies_;

  // List of domain keys that have been loaded from the DB.
  std::set<std::string> keys_loaded_;

  // Map of domain keys to their associated task queues. These tasks are blocked
  // until all cookies for the associated domain key eTLD+1 are loaded from the
  // backend store.
  std::map<std::string, base::circular_deque<base::OnceClosure>>
      tasks_pending_for_key_;

  // Queues tasks that are blocked until all cookies are loaded from the backend
  // store.
  base::circular_deque<base::OnceClosure> tasks_pending_;

  // Once a global cookie task has been seen, all per-key tasks must be put in
  // |tasks_pending_| instead of |tasks_pending_for_key_| to ensure a reasonable
  // view of the cookie store. This is more to ensure fancy cookie export/import
  // code has a consistent view of the CookieStore, rather than out of concern
  // for typical use.
  bool seen_global_task_;

  NetLogWithSource net_log_;

  scoped_refptr<PersistentCookieStore> store_;

  // Minimum delay after updating a cookie's LastAccessDate before we will
  // update it again.
  const base::TimeDelta last_access_threshold_;

  // Approximate date of access time of least recently accessed cookie
  // in |cookies_|.  Note that this is not guaranteed to be accurate, only a)
  // to be before or equal to the actual time, and b) to be accurate
  // immediately after a garbage collection that scans through all the cookies
  // (When garbage collection does not scan through all cookies, it may not be
  // updated). This value is used to determine whether global garbage collection
  // might find cookies to purge. Note: The default Time() constructor will
  // create a value that compares earlier than any other time value, which is
  // wanted.  Thus this value is not initialized.
  base::Time earliest_access_time_;

  std::vector<std::string> cookieable_schemes_;

  base::Time last_statistic_record_time_;

  bool persist_session_cookies_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<CookieMonster> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CookieMonster);
};

typedef base::RefCountedThreadSafe<CookieMonster::PersistentCookieStore>
    RefcountedPersistentCookieStore;

class NET_EXPORT CookieMonster::PersistentCookieStore
    : public RefcountedPersistentCookieStore {
 public:
  typedef base::OnceCallback<void(
      std::vector<std::unique_ptr<CanonicalCookie>>)>
      LoadedCallback;

  // Initializes the store and retrieves the existing cookies. This will be
  // called only once at startup. The callback will return all the cookies
  // that are not yet returned to CookieMonster by previous priority loads.
  //
  // |loaded_callback| may not be NULL.
  // |net_log| is a NetLogWithSource that may be copied if the persistent
  // store wishes to log NetLog events.
  virtual void Load(LoadedCallback loaded_callback,
                    const NetLogWithSource& net_log) = 0;

  // Does a priority load of all cookies for the domain key (eTLD+1). The
  // callback will return all the cookies that are not yet returned by previous
  // loads, which includes cookies for the requested domain key if they are not
  // already returned, plus all cookies that are chain-loaded and not yet
  // returned to CookieMonster.
  //
  // |loaded_callback| may not be NULL.
  virtual void LoadCookiesForKey(const std::string& key,
                                 LoadedCallback loaded_callback) = 0;

  virtual void AddCookie(const CanonicalCookie& cc) = 0;
  virtual void UpdateCookieAccessTime(const CanonicalCookie& cc) = 0;
  virtual void DeleteCookie(const CanonicalCookie& cc) = 0;

  // Instructs the store to not discard session only cookies on shutdown.
  virtual void SetForceKeepSessionState() = 0;

  // Sets a callback that will be run before the store flushes.  If |callback|
  // performs any async operations, the store will not wait for those to finish
  // before flushing.
  virtual void SetBeforeCommitCallback(base::RepeatingClosure callback) = 0;

  // Flushes the store and posts |callback| when complete. |callback| may be
  // NULL.
  virtual void Flush(base::OnceClosure callback) = 0;

 protected:
  PersistentCookieStore() {}
  virtual ~PersistentCookieStore() {}

 private:
  friend class base::RefCountedThreadSafe<PersistentCookieStore>;
  DISALLOW_COPY_AND_ASSIGN(PersistentCookieStore);
};

}  // namespace net

#endif  // NET_COOKIES_COOKIE_MONSTER_H_
