// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_monster_store_test.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

CookieStoreCommand::CookieStoreCommand(
    Type type,
    CookieMonster::PersistentCookieStore::LoadedCallback loaded_callback,
    const std::string& key)
    : type(type), loaded_callback(std::move(loaded_callback)), key(key) {}

CookieStoreCommand::CookieStoreCommand(Type type, const CanonicalCookie& cookie)
    : type(type), cookie(cookie) {}

CookieStoreCommand::CookieStoreCommand(CookieStoreCommand&& other) = default;
CookieStoreCommand::~CookieStoreCommand() = default;

MockPersistentCookieStore::MockPersistentCookieStore()
    : store_load_commands_(false), load_return_value_(true), loaded_(false) {}

void MockPersistentCookieStore::SetLoadExpectation(
    bool return_value,
    std::vector<std::unique_ptr<CanonicalCookie>> result) {
  load_return_value_ = return_value;
  load_result_.swap(result);
}

void MockPersistentCookieStore::Load(LoadedCallback loaded_callback,
                                     const NetLogWithSource& /* net_log */) {
  if (store_load_commands_) {
    commands_.push_back(CookieStoreCommand(CookieStoreCommand::LOAD,
                                           std::move(loaded_callback), ""));
    return;
  }
  std::vector<std::unique_ptr<CanonicalCookie>> out_cookies;
  if (load_return_value_) {
    out_cookies.swap(load_result_);
    loaded_ = true;
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(loaded_callback), std::move(out_cookies)));
}

void MockPersistentCookieStore::LoadCookiesForKey(
    const std::string& key,
    LoadedCallback loaded_callback) {
  if (store_load_commands_) {
    commands_.push_back(
        CookieStoreCommand(CookieStoreCommand::LOAD_COOKIES_FOR_KEY,
                           std::move(loaded_callback), key));
    return;
  }
  if (!loaded_) {
    Load(std::move(loaded_callback), NetLogWithSource());
  } else {
    std::vector<std::unique_ptr<CanonicalCookie>> empty_cookies;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(loaded_callback), std::move(empty_cookies)));
  }
}

void MockPersistentCookieStore::AddCookie(const CanonicalCookie& cookie) {
  commands_.push_back(CookieStoreCommand(CookieStoreCommand::ADD, cookie));
}

void MockPersistentCookieStore::UpdateCookieAccessTime(
    const CanonicalCookie& cookie) {
}

void MockPersistentCookieStore::DeleteCookie(const CanonicalCookie& cookie) {
  commands_.push_back(CookieStoreCommand(CookieStoreCommand::REMOVE, cookie));
}

void MockPersistentCookieStore::SetForceKeepSessionState() {}

void MockPersistentCookieStore::SetBeforeCommitCallback(
    base::RepeatingClosure callback) {}

void MockPersistentCookieStore::Flush(base::OnceClosure callback) {
  if (!callback.is_null())
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
}

MockPersistentCookieStore::~MockPersistentCookieStore() = default;

std::unique_ptr<CanonicalCookie> BuildCanonicalCookie(
    const GURL& url,
    const std::string& cookie_line,
    const base::Time& creation_time) {
  // Parse the cookie line.
  ParsedCookie pc(cookie_line);
  EXPECT_TRUE(pc.IsValid());

  // This helper is simplistic in interpreting a parsed cookie, in order to
  // avoid duplicated CookieMonster's CanonPath() and CanonExpiration()
  // functions. Would be nice to export them, and re-use here.
  EXPECT_FALSE(pc.HasMaxAge());
  EXPECT_TRUE(pc.HasPath());
  base::Time cookie_expires =
      pc.HasExpires() ? cookie_util::ParseCookieExpirationTime(pc.Expires())
                      : base::Time();
  std::string cookie_path = pc.Path();

  return std::make_unique<CanonicalCookie>(
      pc.Name(), pc.Value(), "." + url.host(), cookie_path, creation_time,
      cookie_expires, base::Time(), pc.IsSecure(), pc.IsHttpOnly(),
      pc.SameSite(), pc.Priority());
}

void AddCookieToList(const GURL& url,
                     const std::string& cookie_line,
                     const base::Time& creation_time,
                     std::vector<std::unique_ptr<CanonicalCookie>>* out_list) {
  std::unique_ptr<CanonicalCookie> cookie(
      BuildCanonicalCookie(url, cookie_line, creation_time));

  out_list->push_back(std::move(cookie));
}

MockSimplePersistentCookieStore::MockSimplePersistentCookieStore()
    : loaded_(false) {
}

void MockSimplePersistentCookieStore::Load(
    LoadedCallback loaded_callback,
    const NetLogWithSource& /* net_log */) {
  std::vector<std::unique_ptr<CanonicalCookie>> out_cookies;

  for (auto it = cookies_.begin(); it != cookies_.end(); it++)
    out_cookies.push_back(std::make_unique<CanonicalCookie>(it->second));

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(loaded_callback), std::move(out_cookies)));
  loaded_ = true;
}

void MockSimplePersistentCookieStore::LoadCookiesForKey(
    const std::string& key,
    LoadedCallback loaded_callback) {
  if (!loaded_) {
    Load(std::move(loaded_callback), NetLogWithSource());
  } else {
    std::vector<std::unique_ptr<CanonicalCookie>> empty_cookies;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(loaded_callback), std::move(empty_cookies)));
  }
}

void MockSimplePersistentCookieStore::AddCookie(const CanonicalCookie& cookie) {
  const auto& key = cookie.UniqueKey();
  EXPECT_TRUE(cookies_.find(key) == cookies_.end());
  cookies_[key] = cookie;
}

void MockSimplePersistentCookieStore::UpdateCookieAccessTime(
    const CanonicalCookie& cookie) {
  const auto& key = cookie.UniqueKey();
  ASSERT_TRUE(cookies_.find(key) != cookies_.end());
  cookies_[key].SetLastAccessDate(base::Time::Now());
}

void MockSimplePersistentCookieStore::DeleteCookie(
    const CanonicalCookie& cookie) {
  const auto& key = cookie.UniqueKey();
  auto it = cookies_.find(key);
  ASSERT_TRUE(it != cookies_.end());
  cookies_.erase(it);
}

void MockSimplePersistentCookieStore::SetForceKeepSessionState() {}

void MockSimplePersistentCookieStore::SetBeforeCommitCallback(
    base::RepeatingClosure callback) {}

void MockSimplePersistentCookieStore::Flush(base::OnceClosure callback) {
  if (!callback.is_null())
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
}

std::unique_ptr<CookieMonster> CreateMonsterFromStoreForGC(
    int num_secure_cookies,
    int num_old_secure_cookies,
    int num_non_secure_cookies,
    int num_old_non_secure_cookies,
    int days_old) {
  base::Time current(base::Time::Now());
  base::Time past_creation(base::Time::Now() - base::TimeDelta::FromDays(1000));
  scoped_refptr<MockSimplePersistentCookieStore> store(
      new MockSimplePersistentCookieStore);
  int total_cookies = num_secure_cookies + num_non_secure_cookies;
  int base = 0;
  // Must expire to be persistent
  for (int i = 0; i < total_cookies; i++) {
    int num_old_cookies;
    bool secure;
    if (i < num_secure_cookies) {
      num_old_cookies = num_old_secure_cookies;
      secure = true;
    } else {
      base = num_secure_cookies;
      num_old_cookies = num_old_non_secure_cookies;
      secure = false;
    }
    base::Time creation_time =
        past_creation + base::TimeDelta::FromMicroseconds(i);
    base::Time expiration_time = current + base::TimeDelta::FromDays(30);
    base::Time last_access_time =
        ((i - base) < num_old_cookies)
            ? current - base::TimeDelta::FromDays(days_old)
            : current;

    // The URL must be HTTPS since |secure| can be true or false, and because
    // strict secure cookies are enforced, the cookie will fail to be created if
    // |secure| is true but the URL is an insecure scheme.
    std::unique_ptr<CanonicalCookie> cc(std::make_unique<CanonicalCookie>(
        "a", "1", base::StringPrintf("h%05d.izzle", i), "/path", creation_time,
        expiration_time, base::Time(), secure, false,
        CookieSameSite::NO_RESTRICTION, COOKIE_PRIORITY_DEFAULT));
    cc->SetLastAccessDate(last_access_time);
    store->AddCookie(*cc);
  }

  return std::make_unique<CookieMonster>(store.get(), nullptr);
}

MockSimplePersistentCookieStore::~MockSimplePersistentCookieStore() = default;

}  // namespace net
