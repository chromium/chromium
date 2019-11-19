// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/quota_policy_cookie_store.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/time/time.h"
#include "content/public/test/browser_task_environment.h"
#include "net/cookies/cookie_util.h"
#include "net/log/net_log_with_source.h"
#include "net/ssl/ssl_client_cert_type.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "sql/statement.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
const base::FilePath::CharType kTestCookiesFilename[] =
    FILE_PATH_LITERAL("Cookies");
}

namespace content {
namespace {

using CanonicalCookieVector =
    std::vector<std::unique_ptr<net::CanonicalCookie>>;

class QuotaPolicyCookieStoreTest : public testing::Test {
 public:
  QuotaPolicyCookieStoreTest()
      : loaded_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                      base::WaitableEvent::InitialState::NOT_SIGNALED),
        destroy_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                       base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  void OnLoaded(CanonicalCookieVector cookies) {
    cookies_.swap(cookies);
    loaded_event_.Signal();
  }

  void Load(CanonicalCookieVector* cookies) {
    EXPECT_FALSE(loaded_event_.IsSignaled());
    store_->Load(base::Bind(&QuotaPolicyCookieStoreTest::OnLoaded,
                            base::Unretained(this)),
                 net::NetLogWithSource());
    loaded_event_.Wait();
    cookies->swap(cookies_);
  }

  void ReleaseStore() {
    EXPECT_TRUE(background_task_runner_->RunsTasksInCurrentSequence());
    store_ = nullptr;
    destroy_event_.Signal();
  }

  void DestroyStoreOnBackgroundThread() {
    background_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&QuotaPolicyCookieStoreTest::ReleaseStore,
                                  base::Unretained(this)));
    destroy_event_.Wait();
    DestroyStore();
  }

 protected:
  void CreateAndLoad(storage::SpecialStoragePolicy* storage_policy,
                     CanonicalCookieVector* cookies) {
    scoped_refptr<net::SQLitePersistentCookieStore> sqlite_store(
        new net::SQLitePersistentCookieStore(
            temp_dir_.GetPath().Append(kTestCookiesFilename),
            base::CreateSequencedTaskRunner(
                {base::ThreadPool(), base::MayBlock()}),
            background_task_runner_, true, nullptr));
    store_ = new QuotaPolicyCookieStore(sqlite_store.get(), storage_policy);
    Load(cookies);
  }

  // Adds a persistent cookie to store_.
  void AddCookie(const std::string& name,
                 const std::string& value,
                 const std::string& domain,
                 const std::string& path,
                 const base::Time& creation) {
    store_->AddCookie(net::CanonicalCookie(name, value, domain, path, creation,
                                           creation, base::Time(), false, false,
                                           net::CookieSameSite::NO_RESTRICTION,
                                           net::COOKIE_PRIORITY_DEFAULT));
  }

  void DestroyStore() {
    store_ = nullptr;
    // Ensure that |store_|'s destructor has run by flushing ThreadPool.
    base::ThreadPoolInstance::Get()->FlushForTesting();
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void TearDown() override {
    DestroyStore();
  }

  BrowserTaskEnvironment task_environment_;
  const scoped_refptr<base::SequencedTaskRunner> background_task_runner_ =
      base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock()});
  base::WaitableEvent loaded_event_;
  base::WaitableEvent destroy_event_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<QuotaPolicyCookieStore> store_;
  CanonicalCookieVector cookies_;
};

// Test if data is stored as expected in the QuotaPolicy database.
TEST_F(QuotaPolicyCookieStoreTest, TestPersistence) {
  CanonicalCookieVector cookies;
  CreateAndLoad(nullptr, &cookies);
  ASSERT_EQ(0U, cookies.size());

  base::Time t = base::Time::Now();
  AddCookie("A", "B", "foo.com", "/", t);
  t += base::TimeDelta::FromInternalValue(10);
  AddCookie("A", "B", "persistent.com", "/", t);

  // Replace the store, which forces the current store to flush data to
  // disk. Then, after reloading the store, confirm that the data was flushed by
  // making sure it loads successfully.  This ensures that all pending commits
  // are made to the store before allowing it to be closed.
  DestroyStore();

  // Reload and test for persistence.
  cookies.clear();
  CreateAndLoad(nullptr, &cookies);
  EXPECT_EQ(2U, cookies.size());
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
  cookies.clear();
  CreateAndLoad(nullptr, &cookies);
  EXPECT_EQ(0U, cookies.size());
  cookies.clear();
}

// Test if data is stored as expected in the QuotaPolicy database.
TEST_F(QuotaPolicyCookieStoreTest, TestPolicy) {
  CanonicalCookieVector cookies;
  CreateAndLoad(nullptr, &cookies);
  ASSERT_EQ(0U, cookies.size());

  base::Time t = base::Time::Now();
  AddCookie("A", "B", "foo.com", "/", t);
  t += base::TimeDelta::FromInternalValue(10);
  AddCookie("A", "B", "persistent.com", "/", t);
  t += base::TimeDelta::FromInternalValue(10);
  AddCookie("A", "B", "nonpersistent.com", "/", t);

  // Replace the store, which forces the current store to flush data to
  // disk. Then, after reloading the store, confirm that the data was flushed by
  // making sure it loads successfully.  This ensures that all pending commits
  // are made to the store before allowing it to be closed.
  DestroyStore();
  // Specify storage policy that makes "nonpersistent.com" session only.
  scoped_refptr<content::MockSpecialStoragePolicy> storage_policy =
      new content::MockSpecialStoragePolicy();
  storage_policy->AddSessionOnly(
      net::cookie_util::CookieOriginToURL("nonpersistent.com", false));

  // Reload and test for persistence.
  cookies.clear();
  CreateAndLoad(storage_policy.get(), &cookies);
  EXPECT_EQ(3U, cookies.size());

  t += base::TimeDelta::FromInternalValue(10);
  AddCookie("A", "B", "nonpersistent.com", "/second", t);

  // Now close the store, and "nonpersistent.com" should be deleted according to
  // policy.
  DestroyStore();
  cookies.clear();
  CreateAndLoad(nullptr, &cookies);

  EXPECT_EQ(2U, cookies.size());
  for (const auto& cookie : cookies) {
    EXPECT_NE("nonpersistent.com", cookie->Domain());
  }
  cookies.clear();
}

TEST_F(QuotaPolicyCookieStoreTest, ForceKeepSessionState) {
  CanonicalCookieVector cookies;
  CreateAndLoad(nullptr, &cookies);
  ASSERT_EQ(0U, cookies.size());

  base::Time t = base::Time::Now();
  AddCookie("A", "B", "foo.com", "/", t);

  // Recreate |store_| with a storage policy that makes "nonpersistent.com"
  // session only, but then instruct the store to forcibly keep all cookies.
  DestroyStore();
  scoped_refptr<content::MockSpecialStoragePolicy> storage_policy =
      new content::MockSpecialStoragePolicy();
  storage_policy->AddSessionOnly(
      net::cookie_util::CookieOriginToURL("nonpersistent.com", false));

  // Reload and test for persistence
  cookies.clear();
  CreateAndLoad(storage_policy.get(), &cookies);
  EXPECT_EQ(1U, cookies.size());

  t += base::TimeDelta::FromInternalValue(10);
  AddCookie("A", "B", "persistent.com", "/", t);
  t += base::TimeDelta::FromInternalValue(10);
  AddCookie("A", "B", "nonpersistent.com", "/", t);

  // Now close the store, but the "nonpersistent.com" cookie should not be
  // deleted.
  store_->SetForceKeepSessionState();
  DestroyStore();
  cookies.clear();
  CreateAndLoad(nullptr, &cookies);

  EXPECT_EQ(3U, cookies.size());
  cookies.clear();
}

// Tests that the special storage policy is properly applied even when the store
// is destroyed on a background thread.
TEST_F(QuotaPolicyCookieStoreTest, TestDestroyOnBackgroundThread) {
  // Specify storage policy that makes "nonpersistent.com" session only.
  scoped_refptr<content::MockSpecialStoragePolicy> storage_policy =
      new content::MockSpecialStoragePolicy();
  storage_policy->AddSessionOnly(
      net::cookie_util::CookieOriginToURL("nonpersistent.com", false));

  CanonicalCookieVector cookies;
  CreateAndLoad(storage_policy.get(), &cookies);
  ASSERT_EQ(0U, cookies.size());

  base::Time t = base::Time::Now();
  AddCookie("A", "B", "nonpersistent.com", "/", t);

  // Replace the store, which forces the current store to flush data to
  // disk. Then, after reloading the store, confirm that the data was flushed by
  // making sure it loads successfully.  This ensures that all pending commits
  // are made to the store before allowing it to be closed.
  DestroyStoreOnBackgroundThread();

  // Reload and test for persistence.
  cookies.clear();
  CreateAndLoad(storage_policy.get(), &cookies);
  EXPECT_EQ(0U, cookies.size());

  cookies.clear();
}

}  // namespace
}  // namespace content
