// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_COOKIES_COOKIE_STORE_IOS_TEST_UTIL_H_
#define IOS_NET_COOKIES_COOKIE_STORE_IOS_TEST_UTIL_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "ios/net/cookies/cookie_store_ios_client.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/log/net_log_with_source.h"
#include "url/gurl.h"

namespace net {

class CanonicalCookie;

// Test net::CookieMonster::PersistentCookieStore allowing to control when the
// initialization completes.
class TestPersistentCookieStore
    : public net::CookieMonster::PersistentCookieStore {
 public:
  TestPersistentCookieStore();

  // Runs the completion callback with a "a=b" cookie.
  void RunLoadedCallback();

  bool flushed();

 private:
  // net::CookieMonster::PersistentCookieStore implementation:
  void Load(LoadedCallback loaded_callback,
            const NetLogWithSource& net_log) override;
  void LoadCookiesForKey(const std::string& key,
                         LoadedCallback loaded_callback) override;
  void AddCookie(const net::CanonicalCookie& cc) override;
  void UpdateCookieAccessTime(const net::CanonicalCookie& cc) override;
  void DeleteCookie(const net::CanonicalCookie& cc) override;
  void SetForceKeepSessionState() override;
  void SetBeforeCommitCallback(base::RepeatingClosure callback) override;
  void Flush(base::OnceClosure callback) override;

  ~TestPersistentCookieStore() override;

  const GURL kTestCookieURL;
  LoadedCallback loaded_callback_;
  bool flushed_;
};

class TestCookieStoreIOSClient : public CookieStoreIOSClient {
 public:
  TestCookieStoreIOSClient();
  // CookieStoreIOSClient implementation.
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() const override;
};

class ScopedTestingCookieStoreIOSClient {
 public:
  explicit ScopedTestingCookieStoreIOSClient(
      std::unique_ptr<CookieStoreIOSClient> cookie_store_client);
  ~ScopedTestingCookieStoreIOSClient();

  CookieStoreIOSClient* Get();

 private:
  std::unique_ptr<CookieStoreIOSClient> cookie_store_client_;
  CookieStoreIOSClient* original_client_;
};

void RecordCookieChanges(std::vector<net::CanonicalCookie>* out_cookies,
                         std::vector<bool>* out_removes,
                         const net::CookieChangeInfo& change);

// Sets a cookie.
void SetCookie(const std::string& cookie_line,
               const GURL& url,
               net::CookieStore* store);

// Clears the underlying NSHTTPCookieStorage.
void ClearCookies();

}  // namespace net

#endif  // IOS_NET_COOKIES_COOKIE_STORE_IOS_TEST_UTIL_H_
