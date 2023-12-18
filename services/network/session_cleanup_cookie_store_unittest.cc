// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/session_cleanup_cookie_store.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {
namespace {

using CanonicalCookieVector =
    std::vector<std::unique_ptr<net::CanonicalCookie>>;

const base::FilePath::CharType kTestCookiesFilename[] =
    FILE_PATH_LITERAL("Cookies");

class SessionCleanupCookieStoreTest : public testing::Test {
 public:
  SessionCleanupCookieStoreTest() {}

  void OnLoaded(base::RunLoop* run_loop,
                CanonicalCookieVector* cookies_out,
                CanonicalCookieVector cookies) {
    cookies_out->swap(cookies);
    run_loop->Quit();
  }

  CanonicalCookieVector Load() {
    base::RunLoop run_loop;
    CanonicalCookieVector cookies;
    store_->Load(base::BindOnce(&SessionCleanupCookieStoreTest::OnLoaded,
                                base::Unretained(this), &run_loop, &cookies),
                 net::NetLogWithSource::Make(net::NetLogSourceType::NONE));
    run_loop.Run();
    return cookies;
  }

 protected:
  CanonicalCookieVector CreateAndLoad() {
    auto sqlite_store = base::MakeRefCounted<net::SQLitePersistentCookieStore>(
        temp_dir_.GetPath().Append(kTestCookiesFilename),
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}),
        background_task_runner_, /*restore_old_session_cookies=*/true,
        /*crypto_delegate=*/nullptr, /*enable_exclusive_access=*/false);
    store_ =
        base::MakeRefCounted<SessionCleanupCookieStore>(sqlite_store.get());
    return Load();
  }

  // Adds a persistent cookie to store_.
  void AddCookie(const std::string& name,
                 const std::string& value,
                 const std::string& domain,
                 const std::string& path,
                 base::Time creation) {
    store_->AddCookie(*net::CanonicalCookie::CreateUnsafeCookieForTesting(
        name, value, domain, path, creation, creation, base::Time(),
        base::Time(), false, false, net::CookieSameSite::NO_RESTRICTION,
        net::COOKIE_PRIORITY_DEFAULT));
  }

  void DestroyStore() {
    store_ = nullptr;
    // Ensure that |store_|'s destructor has run by flushing ThreadPool.
    base::ThreadPoolInstance::Get()->FlushForTesting();
  }

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void TearDown() override { DestroyStore(); }

  base::test::TaskEnvironment task_environment_;
  const scoped_refptr<base::SequencedTaskRunner> background_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  base::ScopedTempDir temp_dir_;
  scoped_refptr<SessionCleanupCookieStore> store_;
  net::RecordingNetLogObserver net_log_observer_;
};

TEST_F(SessionCleanupCookieStoreTest, TestPersistence) {
  CanonicalCookieVector cookies = CreateAndLoad();
  ASSERT_EQ(0u, cookies.size());

  base::Time t = base::Time::Now();
  AddCookie("A", "B", "foo.com", "/", t);
  t += base::Days(10);
  AddCookie("A", "B", "persistent.com", "/", t);

  // Replace the store, which forces the current store to flush data to
  // disk. Then, after reloading the store, confirm that the data was flushed by
  // making sure it loads successfully.  This ensures that all pending commits
  // are made to the store before allowing it to be closed.
  DestroyStore();

  // Reload and test for persistence.
  cookies = CreateAndLoad();
  EXPECT_EQ(2u, cookies.size());
  bool found_foo_cookie = false;
  bool found_persistent_cookie = false;
  for (const auto& cookie : cookies) {
    if (cookie->Domain() == "foo.com")
      found_foo_cookie = true;
    else if (cookie->Domain() == "persistent.com")
      found_persistent_cookie = true;
  }
  EXPECT_TRUE(found_foo_cookie);
  EXPECT_TRUE(found_persistent_cookie);

  // Now delete the cookies and check persistence again.
  store_->DeleteCookie(*cookies[0]);
  store_->DeleteCookie(*cookies[1]);
  DestroyStore();

  // Reload and check if the cookies have been removed.
  cookies = CreateAndLoad();
  EXPECT_EQ(0u, cookies.size());
  cookies.clear();
}

TEST_F(SessionCleanupCookieStoreTest, TestNetLogIncludeCookies) {
  CanonicalCookieVector cookies = CreateAndLoad();
  base::Time t = base::Time::Now();
  AddCookie("A", "B", "nonpersistent.com", "/", t);

  // Cookies from "nonpersistent.com" should be deleted.
  store_->DeleteSessionCookies(base::BindRepeating(
      [](const std::string& domain, net::CookieSourceScheme scheme) {
        return domain == "nonpersistent.com";
      }));
  DestroyStore();

  auto entries = net_log_observer_.GetEntries();
  size_t pos = net::ExpectLogContainsSomewhere(
      entries, 0, net::NetLogEventType::COOKIE_PERSISTENT_STORE_ORIGIN_FILTERED,
      net::NetLogEventPhase::NONE);
  EXPECT_EQ("nonpersistent.com",
            net::GetStringValueFromParams(entries[pos], "origin"));
  EXPECT_FALSE(net::GetBooleanValueFromParams(entries[pos], "is_https"));
  pos = net::ExpectLogContainsSomewhere(
      entries, pos, net::NetLogEventType::COOKIE_PERSISTENT_STORE_CLOSED,
      net::NetLogEventPhase::NONE);
  EXPECT_EQ("SessionCleanupCookieStore",
            net::GetStringValueFromParams(entries[pos], "type"));
}

TEST_F(SessionCleanupCookieStoreTest, TestNetLogDoNotIncludeCookies) {
  CanonicalCookieVector cookies = CreateAndLoad();
  base::Time t = base::Time::Now();
  AddCookie("A", "B", "nonpersistent.com", "/", t);

  net_log_observer_.SetObserverCaptureMode(net::NetLogCaptureMode::kDefault);
  // Cookies from "nonpersistent.com" should be deleted.
  store_->DeleteSessionCookies(base::BindRepeating(
      [](const std::string& domain, net::CookieSourceScheme scheme) {
        return domain == "nonpersistent.com";
      }));
  DestroyStore();

  auto entries = net_log_observer_.GetEntries();
  size_t pos = net::ExpectLogContainsSomewhere(
      entries, 0, net::NetLogEventType::COOKIE_PERSISTENT_STORE_ORIGIN_FILTERED,
      net::NetLogEventPhase::NONE);
  EXPECT_FALSE(net::GetOptionalStringValueFromParams(entries[pos], "origin"));
  EXPECT_FALSE(
      net::GetOptionalBooleanValueFromParams(entries[pos], "is_https"));
  pos = net::ExpectLogContainsSomewhere(
      entries, pos, net::NetLogEventType::COOKIE_PERSISTENT_STORE_CLOSED,
      net::NetLogEventPhase::NONE);
  EXPECT_EQ("SessionCleanupCookieStore",
            net::GetStringValueFromParams(entries[pos], "type"));
}

TEST_F(SessionCleanupCookieStoreTest, TestDeleteSessionCookies) {
  CanonicalCookieVector cookies = CreateAndLoad();
  ASSERT_EQ(0u, cookies.size());

  base::Time t = base::Time::Now();
  AddCookie("A", "B", "foo.com", "/", t);
  t += base::Days(10);
  AddCookie("A", "B", "persistent.com", "/", t);
  t += base::Days(10);
  AddCookie("A", "B", "nonpersistent.com", "/", t);

  // Replace the store, which forces the current store to flush data to
  // disk. Then, after reloading the store, confirm that the data was flushed by
  // making sure it loads successfully.  This ensures that all pending commits
  // are made to the store before allowing it to be closed.
  DestroyStore();

  // Reload and test for persistence.
  cookies = CreateAndLoad();
  EXPECT_EQ(3u, cookies.size());

  t += base::Days(10);
  AddCookie("A", "B", "nonpersistent.com", "/second", t);

  // Cookies from "nonpersistent.com" should be deleted.
  store_->DeleteSessionCookies(base::BindRepeating(
      [](const std::string& domain, net::CookieSourceScheme scheme) {
        return domain == "nonpersistent.com";
      }));
  task_environment_.RunUntilIdle();
  DestroyStore();
  cookies = CreateAndLoad();

  EXPECT_EQ(2u, cookies.size());
  for (const auto& cookie : cookies) {
    EXPECT_NE("nonpersistent.com", cookie->Domain());
  }
  cookies.clear();
}

TEST_F(SessionCleanupCookieStoreTest, ForceKeepSessionState) {
  CanonicalCookieVector cookies = CreateAndLoad();
  ASSERT_EQ(0u, cookies.size());

  base::Time t = base::Time::Now();
  AddCookie("A", "B", "foo.com", "/", t);

  // Recreate |store_|, and call DeleteSessionCookies with a function that that
  // makes "nonpersistent.com" session only, but then instruct the store to
  // forcibly keep all cookies.

  // Reload and test for persistence
  DestroyStore();
  cookies = CreateAndLoad();
  EXPECT_EQ(1u, cookies.size());

  t += base::Days(10);
  AddCookie("A", "B", "persistent.com", "/", t);
  t += base::Days(10);
  AddCookie("A", "B", "nonpersistent.com", "/", t);

  store_->SetForceKeepSessionState();
  // Cookies from "nonpersistent.com" should NOT be deleted.
  store_->DeleteSessionCookies(base::BindRepeating(
      [](const std::string& domain, net::CookieSourceScheme scheme) {
        return domain == "nonpersistent.com";
      }));
  task_environment_.RunUntilIdle();
  DestroyStore();
  cookies = CreateAndLoad();

  EXPECT_EQ(3u, cookies.size());
  cookies.clear();
}

}  // namespace
}  // namespace network
