// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_STORE_TEST_HELPERS_H_
#define NET_COOKIES_COOKIE_STORE_TEST_HELPERS_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/synchronization/lock.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "net/cookies/cookie_monster.h"
#include "net/log/net_log_with_source.h"
#include "testing/gtest/include/gtest/gtest.h"

class GURL;

namespace net {

class DelayedCookieMonsterChangeDispatcher : public CookieChangeDispatcher {
 public:
  DelayedCookieMonsterChangeDispatcher();

  DelayedCookieMonsterChangeDispatcher(
      const DelayedCookieMonsterChangeDispatcher&) = delete;
  DelayedCookieMonsterChangeDispatcher& operator=(
      const DelayedCookieMonsterChangeDispatcher&) = delete;

  ~DelayedCookieMonsterChangeDispatcher() override;

  // net::CookieChangeDispatcher
  [[nodiscard]] std::unique_ptr<CookieChangeSubscription> AddCallbackForCookie(
      const GURL& url,
      const std::string& name,
      const std::optional<CookiePartitionKey>& cookie_partition_key,
      CookieChangeCallback callback) override;
  [[nodiscard]] std::unique_ptr<CookieChangeSubscription> AddCallbackForUrl(
      const GURL& url,
      const std::optional<CookiePartitionKey>& cookie_partition_key,
      CookieChangeCallback callback) override;
  [[nodiscard]] std::unique_ptr<CookieChangeSubscription>
  AddCallbackForAllChanges(CookieChangeCallback callback) override;
};

class DelayedCookieMonster : public CookieStore {
 public:
  DelayedCookieMonster();

  DelayedCookieMonster(const DelayedCookieMonster&) = delete;
  DelayedCookieMonster& operator=(const DelayedCookieMonster&) = delete;

  ~DelayedCookieMonster() override;

  // Call the asynchronous CookieMonster function, expect it to immediately
  // invoke the internal callback.
  // Post a delayed task to invoke the original callback with the results.

  void SetCanonicalCookieAsync(
      std::unique_ptr<CanonicalCookie> cookie,
      const GURL& source_url,
      const CookieOptions& options,
      SetCookiesCallback callback,
      const std::optional<CookieAccessResult> cookie_access_result =
          std::nullopt) override;

  void GetCookieListWithOptionsAsync(
      const GURL& url,
      const CookieOptions& options,
      const CookiePartitionKeyCollection& cookie_partition_key_collection,
      GetCookieListCallback callback) override;

  void GetAllCookiesAsync(GetAllCookiesCallback callback) override;

  void DeleteCanonicalCookieAsync(const CanonicalCookie& cookie,
                                  DeleteCallback callback) override;

  void DeleteAllCreatedInTimeRangeAsync(
      const CookieDeletionInfo::TimeRange& creation_range,
      DeleteCallback callback) override;

  void DeleteAllMatchingInfoAsync(net::CookieDeletionInfo delete_info,
                                  DeleteCallback callback) override;

  void DeleteSessionCookiesAsync(DeleteCallback) override;

  void DeleteMatchingCookiesAsync(DeletePredicate, DeleteCallback) override;

  void FlushStore(base::OnceClosure callback) override;

  CookieChangeDispatcher& GetChangeDispatcher() override;

  void SetCookieableSchemes(const std::vector<std::string>& schemes,
                            SetCookieableSchemesCallback callback) override;

 private:
  // Be called immediately from CookieMonster.

  void SetCookiesInternalCallback(CookieAccessResult result);

  void GetCookiesWithOptionsInternalCallback(const std::string& cookie);
  void GetCookieListWithOptionsInternalCallback(
      const CookieAccessResultList& cookie,
      const CookieAccessResultList& excluded_cookies);

  // Invoke the original callbacks.

  void InvokeSetCookiesCallback(CookieMonster::SetCookiesCallback callback);

  void InvokeGetCookieListCallback(
      CookieMonster::GetCookieListCallback callback);

  friend class base::RefCountedThreadSafe<DelayedCookieMonster>;

  std::unique_ptr<CookieMonster> cookie_monster_;
  DelayedCookieMonsterChangeDispatcher change_dispatcher_;

  bool did_run_ = false;
  CookieAccessResult result_;
  std::string cookie_;
  std::string cookie_line_;
  CookieAccessResultList cookie_access_result_list_;
  CookieList cookie_list_;
};

class CookieURLHelper {
 public:
  explicit CookieURLHelper(const std::string& url_string);

  const std::string& domain() const { return domain_and_registry_; }
  std::string host() const { return url_.host(); }
  const GURL& url() const { return url_; }
  const GURL AppendPath(const std::string& path) const;

  // Return a new string with the following substitutions:
  // 1. "%R" -> Domain registry (i.e. "com")
  // 2. "%D" -> Domain + registry (i.e. "google.com")
  std::string Format(const std::string& format_string) const;

 private:
  const GURL url_;
  const std::string registry_;
  const std::string domain_and_registry_;
};

// Mock PersistentCookieStore that keeps track of the number of Flush() calls.
class FlushablePersistentStore : public CookieMonster::PersistentCookieStore {
 public:
  FlushablePersistentStore();

  // CookieMonster::PersistentCookieStore implementation:
  void Load(LoadedCallback loaded_callback,
            const NetLogWithSource& net_log) override;
  void LoadCookiesForKey(const std::string& key,
                         LoadedCallback loaded_callback) override;
  void AddCookie(const CanonicalCookie&) override;
  void UpdateCookieAccessTime(const CanonicalCookie&) override;
  void DeleteCookie(const CanonicalCookie&) override;
  void SetForceKeepSessionState() override;
  void SetBeforeCommitCallback(base::RepeatingClosure callback) override;
  void Flush(base::OnceClosure callback) override;

  int flush_count();

 private:
  ~FlushablePersistentStore() override;

  int flush_count_ = 0;
  base::Lock flush_count_lock_;  // Protects |flush_count_|.
};

// Counts the number of times Callback() has been run.
class CallbackCounter : public base::RefCountedThreadSafe<CallbackCounter> {
 public:
  CallbackCounter();
  void Callback();
  int callback_count();

 private:
  friend class base::RefCountedThreadSafe<CallbackCounter>;
  ~CallbackCounter();

  int callback_count_ = 0;
  base::Lock callback_count_lock_;  // Protects |callback_count_|.
};

// Returns a cookie expiration string in the form of "; expires=<date>", where
// date is an RFC 7231 date a year in the future, which can be appended to
// cookie lines.
std::string FutureCookieExpirationString();

}  // namespace net

#endif  // NET_COOKIES_COOKIE_STORE_TEST_HELPERS_H_
