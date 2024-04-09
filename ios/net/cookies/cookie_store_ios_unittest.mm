// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/net/cookies/cookie_store_ios.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "ios/net/cookies/cookie_store_ios_client.h"
#import "ios/net/cookies/cookie_store_ios_test_util.h"
#import "ios/net/cookies/ns_http_system_cookie_store.h"
#import "net/base/apple/url_conversions.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_store_change_unittest.h"
#include "net/cookies/cookie_store_unittest.h"
#include "net/cookies/cookie_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

class TestingCookieStoreIOS : public CookieStoreIOS {
 public:
  TestingCookieStoreIOS(std::unique_ptr<SystemCookieStore> system_store)
      : CookieStoreIOS(std::move(system_store), nullptr /* net_log */),
        scoped_cookie_store_ios_client_(
            std::make_unique<TestCookieStoreIOSClient>()) {}

 private:
  ScopedTestingCookieStoreIOSClient scoped_cookie_store_ios_client_;
};

struct CookieStoreIOSTestTraits {
  static std::unique_ptr<net::CookieStore> Create() {
    ClearCookies();
    return std::make_unique<TestingCookieStoreIOS>(
        std::make_unique<NSHTTPSystemCookieStore>());
  }

  static void DeliverChangeNotifications() {
    CookieStoreIOS::NotifySystemCookiesChanged();
    base::RunLoop().RunUntilIdle();
  }

  static const bool supports_http_only = false;
  static const bool supports_non_dotted_domains = false;
  static const bool preserves_trailing_dots = false;
  static const bool filters_schemes = false;
  static const bool has_path_prefix_bug = true;
  static const bool forbids_setting_empty_name = true;
  static const bool supports_global_cookie_tracking = false;
  static const bool supports_url_cookie_tracking = false;
  static const bool supports_named_cookie_tracking = true;
  static const bool supports_multiple_tracking_callbacks = true;
  static const bool has_exact_change_cause = false;
  static const bool has_exact_change_ordering = false;
  static const int creation_time_granularity_in_ms = 1000;
  static const bool supports_cookie_access_semantics = false;
  static const bool supports_partitioned_cookies = false;

  base::test::SingleThreadTaskEnvironment task_environment_;
};

INSTANTIATE_TYPED_TEST_SUITE_P(CookieStoreIOS,
                               CookieStoreTest,
                               CookieStoreIOSTestTraits);
INSTANTIATE_TYPED_TEST_SUITE_P(CookieStoreIOS,
                               CookieStoreChangeGlobalTest,
                               CookieStoreIOSTestTraits);
INSTANTIATE_TYPED_TEST_SUITE_P(CookieStoreIOS,
                               CookieStoreChangeUrlTest,
                               CookieStoreIOSTestTraits);
INSTANTIATE_TYPED_TEST_SUITE_P(CookieStoreIOS,
                               CookieStoreChangeNamedTest,
                               CookieStoreIOSTestTraits);

namespace {

// Helper callback to be passed to CookieStore::GetAllCookiesForURLAsync().
class GetAllCookiesHelperCallback {
 public:
  GetAllCookiesHelperCallback() : did_run_(false) {}

  // Returns true if the callback has been run.
  bool did_run() { return did_run_; }

  // Returns the parameter of the callback.
  const net::CookieList& cookie_list() { return cookie_list_; }

  void Run(const net::CookieAccessResultList& cookie_with_access_result,
           const net::CookieAccessResultList& excluded_cookies) {
    ASSERT_FALSE(did_run_);
    did_run_ = true;
    cookie_list_ =
        net::cookie_util::StripAccessResults(cookie_with_access_result);
  }

 private:
  bool did_run_;
  net::CookieList cookie_list_;
};

void IgnoreList(const net::CookieAccessResultList& ignored,
                const net::CookieAccessResultList& excluded_cookies) {}

}  // namespace

// Test fixture to exercise net::CookieStoreIOS created without backend and
// synchronized with |[NSHTTPCookieStorage sharedHTTPCookieStorage]|.
class CookieStoreIOSTest : public PlatformTest {
 public:
  CookieStoreIOSTest()
      : kTestCookieURLFooBar("http://foo.google.com/bar"),
        kTestCookieURLFooBaz("http://foo.google.com/baz"),
        kTestCookieURLFoo("http://foo.google.com"),
        kTestCookieURLBarBar("http://bar.google.com/bar"),
        scoped_cookie_store_ios_client_(
            std::make_unique<TestCookieStoreIOSClient>()),
        backend_(new TestPersistentCookieStore) {
    ClearCookies();
    std::unique_ptr<NSHTTPSystemCookieStore> system_store(
        std::make_unique<NSHTTPSystemCookieStore>());
    // |system_store_| doesn't own the NSHTTPSystemCookieStore object, the
    // object is owned  by store_, this will work as we will not use
    // |system_store_| after |store_| is deleted.
    system_store_ = system_store.get();
    store_ = std::make_unique<net::CookieStoreIOS>(std::move(system_store),
                                                   nullptr /* net_log */);
    cookie_change_subscription_ =
        store_->GetChangeDispatcher().AddCallbackForCookie(
            kTestCookieURLFooBar, "abc",
            /*cookie_partition_key=*/std::nullopt,
            base::BindRepeating(&RecordCookieChanges, &cookies_changed_,
                                &cookies_removed_));
  }
  ~CookieStoreIOSTest() override {}

  // Gets the cookies. |callback| will be called on completion.
  void GetCookies(net::CookieStore::GetCookieListCallback callback) {
    net::CookieOptions options;
    options.set_include_httponly();
    store_->GetCookieListWithOptionsAsync(kTestCookieURLFooBar, options,
                                          net::CookiePartitionKeyCollection(),
                                          std::move(callback));
  }

  // Sets a cookie.
  void SetCookie(const std::string& cookie_line) {
    net::SetCookie(cookie_line, kTestCookieURLFooBar, store_.get());
  }

  void SetSystemCookie(const GURL& url,
                       const std::string& name,
                       const std::string& value) {
    system_store_->SetCookieAsync(
        [NSHTTPCookie cookieWithProperties:@{
          NSHTTPCookiePath : base::SysUTF8ToNSString(url.path()),
          NSHTTPCookieName : base::SysUTF8ToNSString(name),
          NSHTTPCookieValue : base::SysUTF8ToNSString(value),
          NSHTTPCookieDomain : base::SysUTF8ToNSString(url.host()),
        }],
        base::BindOnce(&net::CookieStoreIOS::NotifySystemCookiesChanged));
    base::RunLoop().RunUntilIdle();
  }

  void DeleteSystemCookie(const GURL& gurl, const std::string& name) {
    base::WeakPtr<SystemCookieStore> weak_system_store =
        system_store_->GetWeakPtr();
    system_store_->GetCookiesForURLAsync(
        gurl, base::BindOnce(^(NSArray<NSHTTPCookie*>* cookies) {
          for (NSHTTPCookie* cookie in cookies) {
            if ([[cookie name] isEqualToString:base::SysUTF8ToNSString(name)] &&
                weak_system_store) {
              weak_system_store->DeleteCookieAsync(
                  cookie,
                  base::BindOnce(
                      &net::CookieStoreIOS::NotifySystemCookiesChanged));
              break;
            }
          }
        }));
    base::RunLoop().RunUntilIdle();
  }

 protected:
  const GURL kTestCookieURLFooBar;
  const GURL kTestCookieURLFooBaz;
  const GURL kTestCookieURLFoo;
  const GURL kTestCookieURLBarBar;

  base::test::SingleThreadTaskEnvironment task_environment_;
  ScopedTestingCookieStoreIOSClient scoped_cookie_store_ios_client_;
  scoped_refptr<TestPersistentCookieStore> backend_;
  // |system_store_| will point to the NSHTTPSystemCookieStore object owned by
  // |store_|. Once the store_ object is deleted the NSHTTPSystemCookieStore
  // object will be deleted.
  raw_ptr<net::SystemCookieStore> system_store_;
  std::unique_ptr<net::CookieStoreIOS> store_;
  std::unique_ptr<net::CookieChangeSubscription> cookie_change_subscription_;
  std::vector<net::CanonicalCookie> cookies_changed_;
  std::vector<bool> cookies_removed_;
};

TEST_F(CookieStoreIOSTest, SetCookieCallsHookWhenSynchronized) {
  GetCookies(base::BindOnce(&IgnoreList));
  ClearCookies();
  SetCookie("abc=def");
  EXPECT_EQ(1U, cookies_changed_.size());
  EXPECT_EQ(1U, cookies_removed_.size());
  EXPECT_EQ("abc", cookies_changed_[0].Name());
  EXPECT_EQ("def", cookies_changed_[0].Value());
  EXPECT_FALSE(cookies_removed_[0]);

  SetCookie("abc=ghi");
  EXPECT_EQ(3U, cookies_changed_.size());
  EXPECT_EQ(3U, cookies_removed_.size());
  EXPECT_EQ("abc", cookies_changed_[1].Name());
  EXPECT_EQ("def", cookies_changed_[1].Value());
  EXPECT_TRUE(cookies_removed_[1]);
  EXPECT_EQ("abc", cookies_changed_[2].Name());
  EXPECT_EQ("ghi", cookies_changed_[2].Value());
  EXPECT_FALSE(cookies_removed_[2]);
  DeleteSystemCookie(kTestCookieURLFooBar, "abc");
}

TEST_F(CookieStoreIOSTest, DeleteCanonicalCookie) {
  // Make sure DeleteCanonicalCookieAsync does not rely on creation time.
  GetCookies(base::BindOnce(&IgnoreList));
  ClearCookies();
  SetCookie("abc=def");

  // Time is different, though.
  base::Time not_now = base::Time::Now() - base::Days(30);

  // Semantics for CookieMonster::DeleteCanonicalCookieAsync don't match deletes
  // for same key if cookie value changed.  Document CookieStoreIOS compat.
  std::unique_ptr<CanonicalCookie> non_equiv_cookie =
      CanonicalCookie::CreateForTesting(kTestCookieURLFooBar, "abc=wfg",
                                        not_now);
  base::RunLoop run_loop;
  store_->DeleteCanonicalCookieAsync(
      *non_equiv_cookie, base::BindLambdaForTesting([&](uint32_t deleted) {
        EXPECT_EQ(0U, deleted);
        run_loop.Quit();
      }));
  run_loop.Run();

  // Cookie should still exist.
  base::RunLoop run_loop2;
  GetCookies(base::BindLambdaForTesting(
      [&](const CookieAccessResultList& cookies,
          const CookieAccessResultList& excluded_list) {
        ASSERT_EQ(1u, cookies.size());
        EXPECT_EQ("abc", cookies[0].cookie.Name());
        EXPECT_EQ("def", cookies[0].cookie.Value());
        run_loop2.Quit();
      }));
  run_loop2.Run();

  // Now delete equivalent one with non-matching ctime.
  std::unique_ptr<CanonicalCookie> equiv_cookie =
      CanonicalCookie::CreateForTesting(kTestCookieURLFooBar, "abc=def",
                                        not_now);

  base::RunLoop run_loop3;
  store_->DeleteCanonicalCookieAsync(
      *equiv_cookie, base::BindLambdaForTesting([&](uint32_t deleted) {
        EXPECT_EQ(1U, deleted);
        run_loop3.Quit();
      }));
  run_loop3.Run();

  // Cookie should no longer exist.
  base::RunLoop run_loop4;
  GetCookies(base::BindLambdaForTesting(
      [&](const CookieAccessResultList& cookies,
          const CookieAccessResultList& excluded_list) {
        EXPECT_EQ(0u, cookies.size());
        run_loop4.Quit();
      }));
  run_loop4.Run();
}

TEST_F(CookieStoreIOSTest, DeleteCallsHook) {
  GetCookies(base::BindOnce(&IgnoreList));
  ClearCookies();
  SetCookie("abc=def");
  ASSERT_EQ(1U, cookies_changed_.size());
  ASSERT_EQ(1U, cookies_removed_.size());
  EXPECT_EQ("abc", cookies_changed_[0].Name());
  EXPECT_EQ("def", cookies_changed_[0].Value());
  EXPECT_FALSE(cookies_removed_[0]);

  base::RunLoop run_loop;
  store_->DeleteAllAsync(base::BindLambdaForTesting([&](uint32_t num_deleted) {
    EXPECT_EQ(1U, num_deleted);
    run_loop.Quit();
  }));
  run_loop.Run();
  CookieStoreIOS::NotifySystemCookiesChanged();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2U, cookies_changed_.size());
  ASSERT_EQ(2U, cookies_removed_.size());
  EXPECT_EQ("abc", cookies_changed_[1].Name());
  EXPECT_EQ("def", cookies_changed_[1].Value());
  EXPECT_TRUE(cookies_removed_[1]);
}

TEST_F(CookieStoreIOSTest, SameValueDoesNotCallHook) {
  GetCookies(base::BindOnce(&IgnoreList));
  ClearCookies();
  SetCookie("abc=def");
  EXPECT_EQ(1U, cookies_changed_.size());
  SetCookie("abc=def");
  EXPECT_EQ(1U, cookies_changed_.size());
}

TEST_F(CookieStoreIOSTest, GetAllCookies) {
  const GURL kTestCookieURLFooBar("http://foo.google.com/bar");
  ScopedTestingCookieStoreIOSClient scoped_cookie_store_ios_client(
      std::make_unique<TestCookieStoreIOSClient>());
  ClearCookies();
  auto cookie_store = std::make_unique<CookieStoreIOS>(
      std::make_unique<NSHTTPSystemCookieStore>(), nullptr /* net_log */);

  // Add a cookie.
  auto canonical_cookie = net::CanonicalCookie::CreateForTesting(
      kTestCookieURLFooBar, "a=b", base::Time::Now());
  cookie_store->SetCanonicalCookieAsync(std::move(canonical_cookie),
                                        kTestCookieURLFooBar,
                                        net::CookieOptions::MakeAllInclusive(),
                                        net::CookieStore::SetCookiesCallback());
  // Check we can get the cookie.
  GetAllCookiesHelperCallback callback;
  cookie_store->GetCookieListWithOptionsAsync(
      kTestCookieURLFooBar, net::CookieOptions::MakeAllInclusive(),
      net::CookiePartitionKeyCollection(),
      base::BindOnce(&GetAllCookiesHelperCallback::Run,
                     base::Unretained(&callback)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback.did_run());
  EXPECT_EQ(1u, callback.cookie_list().size());
  net::CanonicalCookie cookie = callback.cookie_list()[0];
  EXPECT_EQ("a", cookie.Name());
  EXPECT_EQ("b", cookie.Value());
}

TEST_F(CookieStoreIOSTest, NoInitialNotifyWithNoCookie) {
  std::vector<net::CanonicalCookie> cookies;
  std::unique_ptr<net::CookieChangeSubscription> handle =
      store_->GetChangeDispatcher().AddCallbackForCookie(
          kTestCookieURLFooBar, "abc",
          /*cookie_partition_key=*/std::nullopt,
          base::BindRepeating(&RecordCookieChanges, &cookies, nullptr));
  EXPECT_EQ(0U, cookies.size());
}

TEST_F(CookieStoreIOSTest, NoInitialNotifyWithSystemCookie) {
  SetSystemCookie(kTestCookieURLFooBar, "abc", "def");
  std::vector<net::CanonicalCookie> cookies;
  std::unique_ptr<net::CookieChangeSubscription> handle =
      store_->GetChangeDispatcher().AddCallbackForCookie(
          kTestCookieURLFooBar, "abc",
          /*cookie_partition_key=*/std::nullopt,
          base::BindRepeating(&RecordCookieChanges, &cookies, nullptr));
  EXPECT_EQ(0U, cookies.size());
  DeleteSystemCookie(kTestCookieURLFooBar, "abc");
}

TEST_F(CookieStoreIOSTest, NotifyOnAdd) {
  std::vector<net::CanonicalCookie> cookies;
  std::vector<bool> removes;
  std::unique_ptr<net::CookieChangeSubscription> handle =
      store_->GetChangeDispatcher().AddCallbackForCookie(
          kTestCookieURLFooBar, "abc",
          /*cookie_partition_key=*/std::nullopt,
          base::BindRepeating(&RecordCookieChanges, &cookies, &removes));
  EXPECT_EQ(0U, cookies.size());
  EXPECT_EQ(0U, removes.size());
  SetSystemCookie(kTestCookieURLFooBar, "abc", "def");
  EXPECT_EQ(1U, cookies.size());
  EXPECT_EQ(1U, removes.size());
  EXPECT_EQ("abc", cookies[0].Name());
  EXPECT_EQ("def", cookies[0].Value());
  EXPECT_FALSE(removes[0]);

  SetSystemCookie(kTestCookieURLFooBar, "ghi", "jkl");
  EXPECT_EQ(1U, cookies.size());
  EXPECT_EQ(1U, removes.size());

  DeleteSystemCookie(kTestCookieURLFooBar, "abc");
  DeleteSystemCookie(kTestCookieURLFooBar, "ghi");
}

TEST_F(CookieStoreIOSTest, NotifyOnChange) {
  std::vector<net::CanonicalCookie> cookies;
  std::vector<bool> removes;
  std::unique_ptr<net::CookieChangeSubscription> handle =
      store_->GetChangeDispatcher().AddCallbackForCookie(
          kTestCookieURLFooBar, "abc",
          /*cookie_partition_key=*/std::nullopt,
          base::BindRepeating(&RecordCookieChanges, &cookies, &removes));
  EXPECT_EQ(0U, cookies.size());
  SetSystemCookie(kTestCookieURLFooBar, "abc", "def");
  EXPECT_EQ(1U, cookies.size());
  SetSystemCookie(kTestCookieURLFooBar, "abc", "ghi");
  EXPECT_EQ(3U, cookies.size());
  EXPECT_EQ(3U, removes.size());
  EXPECT_EQ("abc", cookies[1].Name());
  EXPECT_EQ("def", cookies[1].Value());
  EXPECT_TRUE(removes[1]);
  EXPECT_EQ("abc", cookies[2].Name());
  EXPECT_EQ("ghi", cookies[2].Value());
  EXPECT_FALSE(removes[2]);

  DeleteSystemCookie(kTestCookieURLFooBar, "abc");
}

TEST_F(CookieStoreIOSTest, NotifyOnDelete) {
  std::vector<net::CanonicalCookie> cookies;
  std::vector<bool> removes;
  SetSystemCookie(kTestCookieURLFooBar, "abc", "def");
  std::unique_ptr<net::CookieChangeSubscription> handle =
      store_->GetChangeDispatcher().AddCallbackForCookie(
          kTestCookieURLFooBar, "abc",
          /*cookie_partition_key=*/std::nullopt,
          base::BindRepeating(&RecordCookieChanges, &cookies, &removes));
  EXPECT_EQ(0U, cookies.size());
  DeleteSystemCookie(kTestCookieURLFooBar, "abc");
  EXPECT_EQ(1U, cookies.size());
  EXPECT_EQ(1U, removes.size());
  EXPECT_TRUE(removes[0]);
  SetSystemCookie(kTestCookieURLFooBar, "abc", "def");
  EXPECT_EQ(2U, cookies.size());
  EXPECT_EQ(2U, removes.size());
  EXPECT_FALSE(removes[1]);
  DeleteSystemCookie(kTestCookieURLFooBar, "abc");
}

TEST_F(CookieStoreIOSTest, NoNotifyOnNoChange) {
  std::vector<net::CanonicalCookie> cookies;
  std::unique_ptr<net::CookieChangeSubscription> handle =
      store_->GetChangeDispatcher().AddCallbackForCookie(
          kTestCookieURLFooBar, "abc",
          /*cookie_partition_key=*/std::nullopt,
          base::BindRepeating(&RecordCookieChanges, &cookies, nullptr));
  EXPECT_EQ(0U, cookies.size());
  SetSystemCookie(kTestCookieURLFooBar, "abc", "def");
  EXPECT_EQ(1U, cookies.size());
  SetSystemCookie(kTestCookieURLFooBar, "abc", "def");
  EXPECT_EQ(1U, cookies.size());
  DeleteSystemCookie(kTestCookieURLFooBar, "abc");
}

TEST_F(CookieStoreIOSTest, MultipleNotifies) {
  std::vector<net::CanonicalCookie> cookies;
  std::vector<net::CanonicalCookie> cookies2;
  std::vector<net::CanonicalCookie> cookies3;
  std::vector<net::CanonicalCookie> cookies4;
  std::unique_ptr<net::CookieChangeSubscription> handle =
      store_->GetChangeDispatcher().AddCallbackForCookie(
          kTestCookieURLFooBar, "abc",
          /*cookie_partition_key=*/std::nullopt,
          base::BindRepeating(&RecordCookieChanges, &cookies, nullptr));
  std::unique_ptr<net::CookieChangeSubscription> handle2 =
      store_->GetChangeDispatcher().AddCallbackForCookie(
          kTestCookieURLFooBaz, "abc",
          /*cookie_partition_key=*/std::nullopt,
          base::BindRepeating(&RecordCookieChanges, &cookies2, nullptr));
  std::unique_ptr<net::CookieChangeSubscription> handle3 =
      store_->GetChangeDispatcher().AddCallbackForCookie(
          kTestCookieURLFoo, "abc",
          /*cookie_partition_key=*/std::nullopt,
          base::BindRepeating(&RecordCookieChanges, &cookies3, nullptr));
  std::unique_ptr<net::CookieChangeSubscription> handle4 =
      store_->GetChangeDispatcher().AddCallbackForCookie(
          kTestCookieURLBarBar, "abc",
          /*cookie_partition_key=*/std::nullopt,
          base::BindRepeating(&RecordCookieChanges, &cookies4, nullptr));
  SetSystemCookie(kTestCookieURLFooBar, "abc", "def");
  SetSystemCookie(kTestCookieURLFooBaz, "abc", "def");
  SetSystemCookie(kTestCookieURLFoo, "abc", "def");
  SetSystemCookie(kTestCookieURLBarBar, "abc", "def");
  EXPECT_EQ(2U, cookies.size());
  EXPECT_EQ(2U, cookies2.size());
  EXPECT_EQ(1U, cookies3.size());
  EXPECT_EQ(1U, cookies4.size());
  DeleteSystemCookie(kTestCookieURLFooBar, "abc");
  DeleteSystemCookie(kTestCookieURLFooBaz, "abc");
  DeleteSystemCookie(kTestCookieURLFoo, "abc");
  DeleteSystemCookie(kTestCookieURLBarBar, "abc");
}

TEST_F(CookieStoreIOSTest, LessSpecificNestedCookie) {
  std::vector<net::CanonicalCookie> cookies;
  SetSystemCookie(kTestCookieURLFooBaz, "abc", "def");
  std::unique_ptr<net::CookieChangeSubscription> handle =
      store_->GetChangeDispatcher().AddCallbackForCookie(
          kTestCookieURLFooBaz, "abc",
          /*cookie_partition_key=*/std::nullopt,
          base::BindRepeating(&RecordCookieChanges, &cookies, nullptr));
  EXPECT_EQ(0U, cookies.size());
  SetSystemCookie(kTestCookieURLFoo, "abc", "ghi");
  EXPECT_EQ(1U, cookies.size());
  DeleteSystemCookie(kTestCookieURLFooBar, "abc");
}

TEST_F(CookieStoreIOSTest, MoreSpecificNestedCookie) {
  std::vector<net::CanonicalCookie> cookies;
  SetSystemCookie(kTestCookieURLFoo, "abc", "def");
  std::unique_ptr<net::CookieChangeSubscription> handle =
      store_->GetChangeDispatcher().AddCallbackForCookie(
          kTestCookieURLFooBaz, "abc",
          /*cookie_partition_key=*/std::nullopt,
          base::BindRepeating(&RecordCookieChanges, &cookies, nullptr));
  EXPECT_EQ(0U, cookies.size());
  SetSystemCookie(kTestCookieURLFooBaz, "abc", "ghi");
  EXPECT_EQ(1U, cookies.size());
  DeleteSystemCookie(kTestCookieURLFooBar, "abc");
}

TEST_F(CookieStoreIOSTest, MoreSpecificNestedCookieWithSameValue) {
  std::vector<net::CanonicalCookie> cookies;
  SetSystemCookie(kTestCookieURLFoo, "abc", "def");
  std::unique_ptr<net::CookieChangeSubscription> handle =
      store_->GetChangeDispatcher().AddCallbackForCookie(
          kTestCookieURLFooBaz, "abc",
          /*cookie_partition_key=*/std::nullopt,
          base::BindRepeating(&RecordCookieChanges, &cookies, nullptr));
  EXPECT_EQ(0U, cookies.size());
  SetSystemCookie(kTestCookieURLFooBaz, "abc", "def");
  EXPECT_EQ(1U, cookies.size());
  DeleteSystemCookie(kTestCookieURLFooBar, "abc");
}

TEST_F(CookieStoreIOSTest, RemoveCallback) {
  std::vector<net::CanonicalCookie> cookies;
  SetSystemCookie(kTestCookieURLFooBar, "abc", "def");
  std::unique_ptr<net::CookieChangeSubscription> handle =
      store_->GetChangeDispatcher().AddCallbackForCookie(
          kTestCookieURLFooBar, "abc",
          /*cookie_partition_key=*/std::nullopt,
          base::BindRepeating(&RecordCookieChanges, &cookies, nullptr));
  EXPECT_EQ(0U, cookies.size());
  SetSystemCookie(kTestCookieURLFooBar, "abc", "ghi");
  EXPECT_EQ(2U, cookies.size());
  // this deletes the callback
  handle.reset();
  SetSystemCookie(kTestCookieURLFooBar, "abc", "jkl");
  EXPECT_EQ(2U, cookies.size());
  DeleteSystemCookie(kTestCookieURLFooBar, "abc");
}

}  // namespace net
