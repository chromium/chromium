// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cookie_manager.h"

#include <algorithm>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_store_test_callbacks.h"
#include "net/cookies/cookie_store_test_helpers.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/test_cookie_access_delegate.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/session_cleanup_cookie_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test infrastructure outline:
//      # Classes
//      * SynchronousMojoCookieWrapper: Takes a mojom::CookieManager at
//        construction; exposes synchronous interfaces that wrap the
//        mojom::CookieManager async interfaces to make testing easier.
//      * CookieChangeListener: Test class implementing the CookieChangeListener
//        interface and recording incoming messages on it.
//      * CookieManagerTest: Test base class.  Automatically sets up
//        a cookie store, a cookie service wrapping it, a mojo pipe
//        connected to the server, and the cookie service implemented
//        by the other end of the pipe.
//
//      # Functions
//      * CompareCanonicalCookies: Comparison function to make it easy to
//        sort cookie list responses from the mojom::CookieManager.
//      * CompareCookiesByValue: As above, but only by value.

namespace network {
namespace {
using base::StrCat;

using CookieDeletionInfo = net::CookieDeletionInfo;

const base::FilePath::CharType kTestCookiesFilename[] =
    FILE_PATH_LITERAL("Cookies");

constexpr char kCookieDomain[] = "foo_host.com";
constexpr char kCookieURL[] = "http://foo_host.com";
constexpr char kCookieHttpsURL[] = "https://foo_host.com";

// Wraps a mojom::CookieManager in synchronous, blocking calls to make
// it easier to test.
class SynchronousCookieManager {
 public:
  // Caller must guarantee that |*cookie_service| outlives the
  // SynchronousCookieManager.
  explicit SynchronousCookieManager(mojom::CookieManager* cookie_service)
      : cookie_service_(cookie_service), flush_callback_counter_(0) {}

  ~SynchronousCookieManager() {}

  std::vector<net::CanonicalCookie> GetAllCookies() {
    base::RunLoop run_loop;
    std::vector<net::CanonicalCookie> cookies_out;
    cookie_service_->GetAllCookies(base::BindLambdaForTesting(
        [&run_loop,
         &cookies_out](const std::vector<net::CanonicalCookie>& cookies) {
          cookies_out = cookies;
          run_loop.Quit();
        }));
    run_loop.Run();
    return cookies_out;
  }

  std::vector<net::CanonicalCookie> GetAllCookiesWithAccessSemantics(
      std::vector<net::CookieAccessSemantics>* access_semantics_list_out) {
    base::RunLoop run_loop;
    std::vector<net::CanonicalCookie> cookies_out;
    cookie_service_->GetAllCookiesWithAccessSemantics(
        base::BindLambdaForTesting(
            [&run_loop, &cookies_out, access_semantics_list_out](
                const std::vector<net::CanonicalCookie>& cookies,
                const std::vector<net::CookieAccessSemantics>&
                    access_semantics_list) {
              cookies_out = cookies;
              *access_semantics_list_out = access_semantics_list;
              run_loop.Quit();
            }));
    run_loop.Run();
    return cookies_out;
  }

  std::vector<net::CanonicalCookie> GetCookieList(const GURL& url,
                                                  net::CookieOptions options) {
    base::RunLoop run_loop;
    std::vector<net::CanonicalCookie> cookies_out;
    cookie_service_->GetCookieList(
        url, options,
        base::BindLambdaForTesting(
            [&run_loop, &cookies_out](
                const net::CookieStatusList& cookies,
                const net::CookieStatusList& excluded_cookies) {
              cookies_out = net::cookie_util::StripStatuses(cookies);
              run_loop.Quit();
            }));
    run_loop.Run();
    return cookies_out;
  }

  net::CookieStatusList GetExcludedCookieList(const GURL& url,
                                              net::CookieOptions options) {
    base::RunLoop run_loop;
    net::CookieStatusList cookies_out;
    cookie_service_->GetCookieList(
        url, options,
        base::BindLambdaForTesting(
            [&run_loop, &cookies_out](
                const net::CookieStatusList& cookies,
                const net::CookieStatusList& excluded_cookies) {
              cookies_out = excluded_cookies;
              run_loop.Quit();
            }));
    run_loop.Run();
    return cookies_out;
  }

  bool SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          std::string source_scheme,
                          bool modify_http_only) {
    base::RunLoop run_loop;
    net::CanonicalCookie::CookieInclusionStatus result_out(
        net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR);
    net::CookieOptions options;
    options.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT);
    if (modify_http_only)
      options.set_include_httponly();
    cookie_service_->SetCanonicalCookie(
        cookie, std::move(source_scheme), options,
        base::BindLambdaForTesting(
            [&run_loop,
             &result_out](net::CanonicalCookie::CookieInclusionStatus result) {
              result_out = result;
              run_loop.Quit();
            }));

    run_loop.Run();
    return result_out.IsInclude();
  }

  net::CanonicalCookie::CookieInclusionStatus SetCanonicalCookieWithStatus(
      const net::CanonicalCookie& cookie,
      std::string source_scheme,
      bool modify_http_only) {
    base::RunLoop run_loop;
    net::CookieOptions options;
    options.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT);
    if (modify_http_only)
      options.set_include_httponly();
    net::CanonicalCookie::CookieInclusionStatus result_out(
        net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR);
    cookie_service_->SetCanonicalCookie(
        cookie, std::move(source_scheme), options,
        base::BindLambdaForTesting(
            [&run_loop,
             &result_out](net::CanonicalCookie::CookieInclusionStatus result) {
              result_out = result;
              run_loop.Quit();
            }));

    run_loop.Run();
    return result_out;
  }

  bool DeleteCanonicalCookie(const net::CanonicalCookie& cookie) {
    base::RunLoop run_loop;
    bool result_out;
    cookie_service_->DeleteCanonicalCookie(
        cookie,
        base::BindLambdaForTesting([&run_loop, &result_out](bool result) {
          result_out = result;
          run_loop.Quit();
        }));

    run_loop.Run();
    return result_out;
  }

  uint32_t DeleteCookies(mojom::CookieDeletionFilter filter) {
    base::RunLoop run_loop;
    uint32_t result_out = 0u;
    mojom::CookieDeletionFilterPtr filter_ptr =
        mojom::CookieDeletionFilter::New(filter);

    cookie_service_->DeleteCookies(
        std::move(filter_ptr),
        base::BindLambdaForTesting([&run_loop, &result_out](uint32_t result) {
          result_out = result;
          run_loop.Quit();
        }));

    run_loop.Run();
    return result_out;
  }

  void FlushCookieStore() {
    base::RunLoop run_loop;
    cookie_service_->FlushCookieStore(base::BindLambdaForTesting([&]() {
      ++flush_callback_counter_;
      run_loop.Quit();
    }));
    run_loop.Run();
  }

  uint32_t callback_count() const { return flush_callback_counter_; }

  // No need to wrap Add*Listener and CloneInterface, since their use
  // is purely async.
 private:

  mojom::CookieManager* cookie_service_;
  uint32_t flush_callback_counter_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousCookieManager);
};

class CookieManagerTest : public testing::Test {
 public:
  CookieManagerTest() { InitializeCookieService(nullptr, nullptr); }

  ~CookieManagerTest() override {}

  // Tear down the remote service.
  void NukeService() { cookie_service_.reset(); }

  // Set a canonical cookie directly into the store.
  bool SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          std::string source_scheme,
                          bool can_modify_httponly) {
    net::ResultSavingCookieCallback<net::CanonicalCookie::CookieInclusionStatus>
        callback;
    net::CookieOptions options;
    options.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT);
    if (can_modify_httponly)
      options.set_include_httponly();

    cookie_monster_->SetCanonicalCookieAsync(
        std::make_unique<net::CanonicalCookie>(cookie),
        std::move(source_scheme), options,
        base::BindOnce(&net::ResultSavingCookieCallback<
                           net::CanonicalCookie::CookieInclusionStatus>::Run,
                       base::Unretained(&callback)));
    callback.WaitUntilDone();
    return callback.result().IsInclude();
  }

  std::string DumpAllCookies() {
    std::string result = "Cookies in store:\n";
    std::vector<net::CanonicalCookie> cookies =
        service_wrapper()->GetAllCookies();
    for (int i = 0; i < static_cast<int>(cookies.size()); ++i) {
      result += "\t";
      result += cookies[i].DebugString();
      result += "\n";
    }
    return result;
  }

  ContentSettingPatternSource CreateDefaultSetting(ContentSetting setting) {
    return ContentSettingPatternSource(
        ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
        base::Value(setting), std::string(), false);
  }

  ContentSettingPatternSource CreateSetting(ContentSetting setting,
                                            const std::string& url_str) {
    const GURL url(url_str);
    EXPECT_TRUE(url.is_valid());
    return ContentSettingPatternSource(ContentSettingsPattern::FromURL(url),
                                       ContentSettingsPattern::Wildcard(),
                                       base::Value(setting), std::string(),
                                       false);
  }

  net::CookieStore* cookie_store() { return cookie_monster_.get(); }

  CookieManager* service() const { return cookie_service_.get(); }

  // Return the cookie service at the client end of the mojo pipe.
  mojom::CookieManager* cookie_service_client() {
    return cookie_service_remote_.get();
  }

  // Synchronous wrapper
  SynchronousCookieManager* service_wrapper() { return service_wrapper_.get(); }

  bool connection_error_seen() const { return connection_error_seen_; }

 protected:
  void InitializeCookieService(
      scoped_refptr<net::CookieMonster::PersistentCookieStore> store,
      scoped_refptr<SessionCleanupCookieStore> cleanup_store) {
    if (cookie_service_) {
      // Make sure that data from any previous store is fully saved.
      // |cookie_service_| destroyed first since it may issue some writes to the
      // |cookie_monster_|.
      cookie_service_ = nullptr;
      net::NoResultCookieCallback callback;
      cookie_monster_->FlushStore(callback.MakeCallback());
      callback.WaitUntilDone();
    }
    // Reset |cookie_service_remote_| to allow re-initialize with params
    // for FlushableCookieManagerTest and SessionCleanupCookieManagerTest.
    cookie_service_remote_.reset();

    connection_error_seen_ = false;
    cookie_monster_ = std::make_unique<net::CookieMonster>(
        std::move(store), nullptr /* netlog */);
    cookie_service_ = std::make_unique<CookieManager>(
        cookie_monster_.get(), std::move(cleanup_store), nullptr);
    cookie_service_->AddReceiver(
        cookie_service_remote_.BindNewPipeAndPassReceiver());
    service_wrapper_ = std::make_unique<SynchronousCookieManager>(
        cookie_service_remote_.get());
    cookie_service_remote_.set_disconnect_handler(base::BindOnce(
        &CookieManagerTest::OnConnectionError, base::Unretained(this)));
  }

  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;

 private:
  void OnConnectionError() { connection_error_seen_ = true; }

  bool connection_error_seen_;

  std::unique_ptr<net::CookieMonster> cookie_monster_;
  std::unique_ptr<CookieManager> cookie_service_;
  mojo::Remote<mojom::CookieManager> cookie_service_remote_;
  std::unique_ptr<SynchronousCookieManager> service_wrapper_;

  DISALLOW_COPY_AND_ASSIGN(CookieManagerTest);
};

bool CompareCanonicalCookies(const net::CanonicalCookie& c1,
                             const net::CanonicalCookie& c2) {
  return c1.PartialCompare(c2);
}

// Test the GetAllCookies accessor.  Also tests that canonical
// cookies come out of the store unchanged.
TEST_F(CookieManagerTest, GetAllCookies) {
  base::Time before_creation(base::Time::Now());

  // Set some cookies for the test to play with.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A", "B", kCookieDomain, "/", base::Time(),
                           base::Time(), base::Time(),
                           /*secure=*/false, /*httponly=*/false,
                           net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("C", "D", "foo_host2", "/with/path", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "Secure", "E", kCookieDomain, "/with/path", base::Time(),
          base::Time(), base::Time(), /*secure=*/true,
          /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("HttpOnly", "F", kCookieDomain, "/with/path",
                           base::Time(), base::Time(), base::Time(),
                           /*secure=*/false,
                           /*httponly=*/true, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  base::Time after_creation(base::Time::Now());

  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();

  ASSERT_EQ(4u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);

  EXPECT_EQ("A", cookies[0].Name());
  EXPECT_EQ("B", cookies[0].Value());
  EXPECT_EQ(kCookieDomain, cookies[0].Domain());
  EXPECT_EQ("/", cookies[0].Path());
  EXPECT_LT(before_creation, cookies[0].CreationDate());
  EXPECT_LE(cookies[0].CreationDate(), after_creation);
  EXPECT_EQ(cookies[0].LastAccessDate(), base::Time());
  EXPECT_EQ(cookies[0].ExpiryDate(), base::Time());
  EXPECT_FALSE(cookies[0].IsPersistent());
  EXPECT_FALSE(cookies[0].IsSecure());
  EXPECT_FALSE(cookies[0].IsHttpOnly());
  EXPECT_EQ(net::CookieSameSite::LAX_MODE, cookies[0].SameSite());
  EXPECT_EQ(net::COOKIE_PRIORITY_MEDIUM, cookies[0].Priority());

  EXPECT_EQ("C", cookies[1].Name());
  EXPECT_EQ("D", cookies[1].Value());
  EXPECT_EQ("foo_host2", cookies[1].Domain());
  EXPECT_EQ("/with/path", cookies[1].Path());
  EXPECT_LT(before_creation, cookies[1].CreationDate());
  EXPECT_LE(cookies[1].CreationDate(), after_creation);
  EXPECT_EQ(cookies[1].LastAccessDate(), base::Time());
  EXPECT_EQ(cookies[1].ExpiryDate(), base::Time());
  EXPECT_FALSE(cookies[1].IsPersistent());
  EXPECT_FALSE(cookies[1].IsSecure());
  EXPECT_FALSE(cookies[1].IsHttpOnly());
  EXPECT_EQ(net::CookieSameSite::LAX_MODE, cookies[1].SameSite());
  EXPECT_EQ(net::COOKIE_PRIORITY_MEDIUM, cookies[1].Priority());

  EXPECT_EQ("HttpOnly", cookies[2].Name());
  EXPECT_EQ("F", cookies[2].Value());
  EXPECT_EQ(kCookieDomain, cookies[2].Domain());
  EXPECT_EQ("/with/path", cookies[2].Path());
  EXPECT_LT(before_creation, cookies[2].CreationDate());
  EXPECT_LE(cookies[2].CreationDate(), after_creation);
  EXPECT_EQ(cookies[2].LastAccessDate(), base::Time());
  EXPECT_EQ(cookies[2].ExpiryDate(), base::Time());
  EXPECT_FALSE(cookies[2].IsPersistent());
  EXPECT_FALSE(cookies[2].IsSecure());
  EXPECT_TRUE(cookies[2].IsHttpOnly());
  EXPECT_EQ(net::CookieSameSite::LAX_MODE, cookies[2].SameSite());
  EXPECT_EQ(net::COOKIE_PRIORITY_MEDIUM, cookies[2].Priority());

  EXPECT_EQ("Secure", cookies[3].Name());
  EXPECT_EQ("E", cookies[3].Value());
  EXPECT_EQ(kCookieDomain, cookies[3].Domain());
  EXPECT_EQ("/with/path", cookies[3].Path());
  EXPECT_LT(before_creation, cookies[3].CreationDate());
  EXPECT_LE(cookies[3].CreationDate(), after_creation);
  EXPECT_EQ(cookies[3].LastAccessDate(), base::Time());
  EXPECT_EQ(cookies[3].ExpiryDate(), base::Time());
  EXPECT_FALSE(cookies[3].IsPersistent());
  EXPECT_TRUE(cookies[3].IsSecure());
  EXPECT_FALSE(cookies[3].IsHttpOnly());
  EXPECT_EQ(net::CookieSameSite::NO_RESTRICTION, cookies[3].SameSite());
  EXPECT_EQ(net::COOKIE_PRIORITY_MEDIUM, cookies[3].Priority());
}

TEST_F(CookieManagerTest, GetAllCookiesWithAccessSemantics) {
  auto delegate = std::make_unique<net::TestCookieAccessDelegate>();
  delegate->SetExpectationForCookieDomain("domain1.test",
                                          net::CookieAccessSemantics::UNKNOWN);
  delegate->SetExpectationForCookieDomain("domain2.test",
                                          net::CookieAccessSemantics::LEGACY);
  delegate->SetExpectationForCookieDomain(
      ".domainwithdot.test", net::CookieAccessSemantics::NONLEGACY);
  cookie_store()->SetCookieAccessDelegate(std::move(delegate));

  // Set some cookies for the test to play with.
  // TODO(chlily): Because the order of the cookies with respect to the access
  // semantics entries should match up, for the purposes of this test we need
  // the cookies to sort in a predictable order. Since the longest path is
  // first, we can manipulate the path attribute of each cookie set below to get
  // them in the right order. This will have to change if CookieSorter ever
  // starts sorting the cookies differently.

  // UNKNOWN
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A", "B", "domain1.test",
                           "/this/path/is/the/longest/for/sorting/purposes",
                           base::Time(), base::Time(), base::Time(),
                           /*secure=*/true, /*httponly=*/false,
                           net::CookieSameSite::NO_RESTRICTION,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  // LEGACY
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "C", "D", "domain2.test", "/with/longer/path", base::Time(),
          base::Time(), base::Time(), /*secure=*/true, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  // not set (UNKNOWN)
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "HttpOnly", "F", "domain3.test", "/with/path", base::Time(),
          base::Time(), base::Time(), /*secure=*/true,
          /*httponly=*/true, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  // NONLEGACY
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "Secure", "E", ".domainwithdot.test", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/true,
          /*httponly=*/true, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  std::vector<net::CookieAccessSemantics> access_semantics_list;
  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookiesWithAccessSemantics(
          &access_semantics_list);

  ASSERT_EQ(4u, cookies.size());
  EXPECT_EQ(cookies.size(), access_semantics_list.size());
  EXPECT_EQ("domain1.test", cookies[0].Domain());
  EXPECT_EQ("domain2.test", cookies[1].Domain());
  EXPECT_EQ("domain3.test", cookies[2].Domain());
  EXPECT_EQ(".domainwithdot.test", cookies[3].Domain());

  EXPECT_EQ(net::CookieAccessSemantics::UNKNOWN, access_semantics_list[0]);
  EXPECT_EQ(net::CookieAccessSemantics::LEGACY, access_semantics_list[1]);
  EXPECT_EQ(net::CookieAccessSemantics::UNKNOWN, access_semantics_list[2]);
  EXPECT_EQ(net::CookieAccessSemantics::NONLEGACY, access_semantics_list[3]);
}

TEST_F(CookieManagerTest, GetCookieList) {
  // Set some cookies for the test to play with.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A", "B", kCookieDomain, "/", base::Time(),
                           base::Time(), base::Time(),
                           /*secure=*/false, /*httponly=*/false,
                           net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("C", "D", "foo_host2", "/with/path", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("Secure", "E", kCookieDomain, "/with/path",
                           base::Time(), base::Time(), base::Time(),
                           /*secure=*/true,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("HttpOnly", "F", kCookieDomain, "/with/path",
                           base::Time(), base::Time(), base::Time(),
                           /*secure=*/false,
                           /*httponly=*/true, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  // Want the SameSite=lax cookies, but not httponly ones.
  net::CookieOptions options;
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT);
  std::vector<net::CanonicalCookie> cookies = service_wrapper()->GetCookieList(
      GURL("https://foo_host.com/with/path"), options);

  ASSERT_EQ(2u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);

  EXPECT_EQ("A", cookies[0].Name());
  EXPECT_EQ("B", cookies[0].Value());

  EXPECT_EQ("Secure", cookies[1].Name());
  EXPECT_EQ("E", cookies[1].Value());

  net::CookieOptions excluded_options = options;
  excluded_options.set_return_excluded_cookies();
  net::CookieStatusList excluded_cookies =
      service_wrapper()->GetExcludedCookieList(
          GURL("https://foo_host.com/with/path"), excluded_options);

  ASSERT_EQ(1u, excluded_cookies.size());

  EXPECT_EQ("HttpOnly", excluded_cookies[0].cookie.Name());
  EXPECT_EQ("F", excluded_cookies[0].cookie.Value());
  EXPECT_TRUE(excluded_cookies[0].status.HasExactlyExclusionReasonsForTesting(
      {net::CanonicalCookie::CookieInclusionStatus::EXCLUDE_HTTP_ONLY}));
}

TEST_F(CookieManagerTest, GetCookieListHttpOnly) {
  // Create an httponly and a non-httponly cookie.
  bool result;
  result = SetCanonicalCookie(
      net::CanonicalCookie("A", "B", kCookieDomain, "/", base::Time(),
                           base::Time(), base::Time(),
                           /*secure=*/false, /*httponly=*/true,
                           net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true);
  ASSERT_TRUE(result);
  result = SetCanonicalCookie(
      net::CanonicalCookie("C", "D", kCookieDomain, "/", base::Time(),
                           base::Time(), base::Time(),
                           /*secure=*/false, /*httponly=*/false,
                           net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true);
  ASSERT_TRUE(result);

  // Retrieve without httponly cookies (default)
  net::CookieOptions options;
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT);

  EXPECT_TRUE(options.exclude_httponly());
  std::vector<net::CanonicalCookie> cookies = service_wrapper()->GetCookieList(
      GURL("https://foo_host.com/with/path"), options);
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ("C", cookies[0].Name());

  options.set_return_excluded_cookies();

  net::CookieStatusList excluded_cookies =
      service_wrapper()->GetExcludedCookieList(
          GURL("https://foo_host.com/with/path"), options);
  ASSERT_EQ(1u, excluded_cookies.size());
  EXPECT_EQ("A", excluded_cookies[0].cookie.Name());

  // Retrieve with httponly cookies.
  options.set_include_httponly();
  cookies = service_wrapper()->GetCookieList(
      GURL("https://foo_host.com/with/path"), options);
  ASSERT_EQ(2u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);

  EXPECT_EQ("A", cookies[0].Name());
  EXPECT_EQ("C", cookies[1].Name());
}

TEST_F(CookieManagerTest, GetCookieListSameSite) {
  // Create an unrestricted, a lax, and a strict cookie.
  bool result;
  result = SetCanonicalCookie(
      net::CanonicalCookie("A", "B", kCookieDomain, "/", base::Time(),
                           base::Time(), base::Time(),
                           /*secure=*/true, /*httponly=*/false,
                           net::CookieSameSite::NO_RESTRICTION,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true);
  ASSERT_TRUE(result);
  result = SetCanonicalCookie(
      net::CanonicalCookie("C", "D", kCookieDomain, "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true);
  ASSERT_TRUE(result);
  result = SetCanonicalCookie(
      net::CanonicalCookie("E", "F", kCookieDomain, "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::STRICT_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true);
  ASSERT_TRUE(result);

  // Retrieve only unrestricted cookies.
  net::CookieOptions options;
  EXPECT_EQ(net::CookieOptions::SameSiteCookieContext::CROSS_SITE,
            options.same_site_cookie_context());
  std::vector<net::CanonicalCookie> cookies = service_wrapper()->GetCookieList(
      GURL("https://foo_host.com/with/path"), options);
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ("A", cookies[0].Name());

  options.set_return_excluded_cookies();

  net::CookieStatusList excluded_cookies =
      service_wrapper()->GetExcludedCookieList(
          GURL("https://foo_host.com/with/path"), options);
  ASSERT_EQ(2u, excluded_cookies.size());

  // Retrieve unrestricted and lax cookies.
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::SAME_SITE_LAX);
  cookies = service_wrapper()->GetCookieList(
      GURL("https://foo_host.com/with/path"), options);
  ASSERT_EQ(2u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);
  EXPECT_EQ("A", cookies[0].Name());
  EXPECT_EQ("C", cookies[1].Name());

  excluded_cookies = service_wrapper()->GetExcludedCookieList(
      GURL("https://foo_host.com/with/path"), options);
  ASSERT_EQ(1u, excluded_cookies.size());

  // Retrieve everything.
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT);
  cookies = service_wrapper()->GetCookieList(
      GURL("https://foo_host.com/with/path"), options);
  ASSERT_EQ(3u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);
  EXPECT_EQ("A", cookies[0].Name());
  EXPECT_EQ("C", cookies[1].Name());
  EXPECT_EQ("E", cookies[2].Name());

  excluded_cookies = service_wrapper()->GetExcludedCookieList(
      GURL("https://foo_host.com/with/path"), options);
  ASSERT_EQ(0u, excluded_cookies.size());
}

TEST_F(CookieManagerTest, GetCookieListAccessTime) {
  bool result = SetCanonicalCookie(
      net::CanonicalCookie("A", "B", kCookieDomain, "/", base::Time(),
                           base::Time(), base::Time(),
                           /*secure=*/false, /*httponly=*/false,
                           net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true);
  ASSERT_TRUE(result);

  // Get the cookie without updating the access time and check
  // the access time is null.
  net::CookieOptions options;
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT);

  options.set_do_not_update_access_time();
  std::vector<net::CanonicalCookie> cookies = service_wrapper()->GetCookieList(
      GURL("https://foo_host.com/with/path"), options);
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ("A", cookies[0].Name());
  EXPECT_TRUE(cookies[0].LastAccessDate().is_null());

  // Get the cookie updating the access time and check
  // that it's a valid value.
  base::Time start(base::Time::Now());
  options.set_update_access_time();
  cookies = service_wrapper()->GetCookieList(
      GURL("https://foo_host.com/with/path"), options);
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ("A", cookies[0].Name());
  EXPECT_FALSE(cookies[0].LastAccessDate().is_null());
  EXPECT_GE(cookies[0].LastAccessDate(), start);
  EXPECT_LE(cookies[0].LastAccessDate(), base::Time::Now());
}

TEST_F(CookieManagerTest, DeleteCanonicalCookie) {
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A", "B", "foo_host", "/", base::Time(), base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();
  ASSERT_EQ(1U, cookies.size());
  EXPECT_TRUE(service_wrapper()->DeleteCanonicalCookie(cookies[0]));
  EXPECT_EQ(0U, service_wrapper()->GetAllCookies().size());
}

TEST_F(CookieManagerTest, DeleteThroughSet) {
  // Set some cookies for the test to play with.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A", "B", kCookieDomain, "/", base::Time(),
                           base::Time(), base::Time(),
                           /*secure=*/false, /*httponly=*/false,
                           net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("C", "D", "foo_host2", "/with/path", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("Secure", "E", kCookieDomain, "/with/path",
                           base::Time(), base::Time(), base::Time(),
                           /*secure=*/true,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("HttpOnly", "F", kCookieDomain, "/with/path",
                           base::Time(), base::Time(), base::Time(),
                           /*secure=*/false,
                           /*httponly=*/true, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  base::Time yesterday = base::Time::Now() - base::TimeDelta::FromDays(1);
  EXPECT_TRUE(service_wrapper()->SetCanonicalCookie(
      net::CanonicalCookie(
          "A", "E", kCookieDomain, "/", base::Time(), yesterday, base::Time(),
          /*secure=*/false, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "http", false));

  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();

  ASSERT_EQ(3u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);

  EXPECT_EQ("C", cookies[0].Name());
  EXPECT_EQ("D", cookies[0].Value());

  EXPECT_EQ("HttpOnly", cookies[1].Name());
  EXPECT_EQ("F", cookies[1].Value());

  EXPECT_EQ("Secure", cookies[2].Name());
  EXPECT_EQ("E", cookies[2].Value());
}

TEST_F(CookieManagerTest, ConfirmSecureSetFails) {
  EXPECT_TRUE(
      service_wrapper()
          ->SetCanonicalCookieWithStatus(
              net::CanonicalCookie("N", "O", kCookieDomain, "/", base::Time(),
                                   base::Time(), base::Time(),
                                   /*secure=*/true, /*httponly=*/false,
                                   net::CookieSameSite::NO_RESTRICTION,
                                   net::COOKIE_PRIORITY_MEDIUM),
              "http", false)
          .HasExactlyExclusionReasonsForTesting(
              {net::CanonicalCookie::CookieInclusionStatus::
                   EXCLUDE_SECURE_ONLY}));
  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();

  ASSERT_EQ(0u, cookies.size());
}

TEST_F(CookieManagerTest, ConfirmHttpOnlySetFails) {
  EXPECT_TRUE(
      service_wrapper()
          ->SetCanonicalCookieWithStatus(
              net::CanonicalCookie("N", "O", kCookieDomain, "/", base::Time(),
                                   base::Time(), base::Time(),
                                   /*secure=*/false, /*httponly=*/true,
                                   net::CookieSameSite::LAX_MODE,
                                   net::COOKIE_PRIORITY_MEDIUM),
              "http", false)
          .HasExactlyExclusionReasonsForTesting(
              {net::CanonicalCookie::CookieInclusionStatus::
                   EXCLUDE_HTTP_ONLY}));
  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();

  ASSERT_EQ(0u, cookies.size());
}

TEST_F(CookieManagerTest, ConfirmSecureOverwriteFails) {
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("Secure", "F", kCookieDomain, "/with/path",
                           base::Time(), base::Time(), base::Time(),
                           /*secure=*/true,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  EXPECT_TRUE(
      service_wrapper()
          ->SetCanonicalCookieWithStatus(
              net::CanonicalCookie(
                  "Secure", "Nope", kCookieDomain, "/with/path", base::Time(),
                  base::Time(), base::Time(), /*secure=*/false,
                  /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                  net::COOKIE_PRIORITY_MEDIUM),
              "http", false)
          .HasExactlyExclusionReasonsForTesting(
              {net::CanonicalCookie::CookieInclusionStatus::
                   EXCLUDE_OVERWRITE_SECURE}));

  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();
  ASSERT_EQ(1u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);
  EXPECT_EQ("Secure", cookies[0].Name());
  EXPECT_EQ("F", cookies[0].Value());
}

TEST_F(CookieManagerTest, ConfirmHttpOnlyOverwriteFails) {
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("HttpOnly", "F", kCookieDomain, "/with/path",
                           base::Time(), base::Time(), base::Time(),
                           /*secure=*/false,
                           /*httponly=*/true, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "http", true));

  EXPECT_TRUE(
      service_wrapper()
          ->SetCanonicalCookieWithStatus(
              net::CanonicalCookie(
                  "HttpOnly", "Nope", kCookieDomain, "/with/path", base::Time(),
                  base::Time(), base::Time(), /*secure=*/false,
                  /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                  net::COOKIE_PRIORITY_MEDIUM),
              "https", false)
          .HasExactlyExclusionReasonsForTesting(
              {net::CanonicalCookie::CookieInclusionStatus::
                   EXCLUDE_OVERWRITE_HTTP_ONLY}));

  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();
  ASSERT_EQ(1u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);
  EXPECT_EQ("HttpOnly", cookies[0].Name());
  EXPECT_EQ("F", cookies[0].Value());
}

TEST_F(CookieManagerTest, DeleteEverything) {
  // Set some cookies for the test to play with.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A", "B", kCookieDomain, "/", base::Time(),
                           base::Time(), base::Time(),
                           /*secure=*/false, /*httponly=*/false,
                           net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("C", "D", "foo_host2", "/with/path", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "Secure", "E", kCookieDomain, "/with/path", base::Time(),
          base::Time(), base::Time(), /*secure=*/true,
          /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("HttpOnly", "F", kCookieDomain, "/with/path",
                           base::Time(), base::Time(), base::Time(),
                           /*secure=*/false,
                           /*httponly=*/true, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  mojom::CookieDeletionFilter filter;
  EXPECT_EQ(4u, service_wrapper()->DeleteCookies(filter));

  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();
  ASSERT_EQ(0u, cookies.size());
}

TEST_F(CookieManagerTest, DeleteByTime) {
  base::Time now(base::Time::Now());

  // Create three cookies and delete the middle one.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A1", "val", kCookieDomain, "/",
                           now - base::TimeDelta::FromMinutes(60), base::Time(),
                           base::Time(), /*secure=*/false, /*httponly=*/false,
                           net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A2", "val", kCookieDomain, "/",
          now - base::TimeDelta::FromMinutes(120), base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A3", "val", kCookieDomain, "/",
          now - base::TimeDelta::FromMinutes(180), base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  mojom::CookieDeletionFilter filter;
  filter.created_after_time = now - base::TimeDelta::FromMinutes(150);
  filter.created_before_time = now - base::TimeDelta::FromMinutes(90);
  EXPECT_EQ(1u, service_wrapper()->DeleteCookies(filter));
  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();
  ASSERT_EQ(2u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);
  EXPECT_EQ("A1", cookies[0].Name());
  EXPECT_EQ("A3", cookies[1].Name());
}

TEST_F(CookieManagerTest, DeleteByExcludingDomains) {
  // Create three cookies and delete the middle one.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A1", "val", "foo_host1", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A2", "val", "foo_host2", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A3", "val", "foo_host3", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  mojom::CookieDeletionFilter filter;
  filter.excluding_domains = std::vector<std::string>();
  filter.excluding_domains->push_back("foo_host2");
  EXPECT_EQ(2u, service_wrapper()->DeleteCookies(filter));
  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ("A2", cookies[0].Name());
}

TEST_F(CookieManagerTest, DeleteByIncludingDomains) {
  // Create three cookies and delete the middle one.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A1", "val", "foo_host1", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A2", "val", "foo_host2", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A3", "val", "foo_host3", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  mojom::CookieDeletionFilter filter;
  filter.including_domains = std::vector<std::string>();
  filter.including_domains->push_back("foo_host1");
  filter.including_domains->push_back("foo_host3");
  EXPECT_EQ(2u, service_wrapper()->DeleteCookies(filter));
  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ("A2", cookies[0].Name());
}

// Confirm deletion is based on eTLD+1
TEST_F(CookieManagerTest, DeleteDetails_eTLD) {
  // Two domains on diferent levels of the same eTLD both get deleted.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A1", "val", "example.com", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A2", "val", "www.example.com", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A3", "val", "www.nonexample.com", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  mojom::CookieDeletionFilter filter;
  filter.including_domains = std::vector<std::string>();
  filter.including_domains->push_back("example.com");
  EXPECT_EQ(2u, service_wrapper()->DeleteCookies(filter));
  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ("A3", cookies[0].Name());
  filter = mojom::CookieDeletionFilter();
  EXPECT_EQ(1u, service_wrapper()->DeleteCookies(filter));

  // Same thing happens on an eTLD+1 which isn't a TLD+1.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A1", "val", "example.co.uk", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A2", "val", "www.example.co.uk", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A3", "val", "www.nonexample.co.uk", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::LAX_MODE, net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  filter.including_domains = std::vector<std::string>();
  filter.including_domains->push_back("example.co.uk");
  EXPECT_EQ(2u, service_wrapper()->DeleteCookies(filter));
  cookies = service_wrapper()->GetAllCookies();
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ("A3", cookies[0].Name());
  filter = mojom::CookieDeletionFilter();
  EXPECT_EQ(1u, service_wrapper()->DeleteCookies(filter));

  // Deletion of a second level domain that's an eTLD doesn't delete any
  // subdomains.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A1", "val", "example.co.uk", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A2", "val", "www.example.co.uk", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A3", "val", "www.nonexample.co.uk", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::LAX_MODE, net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  filter.including_domains = std::vector<std::string>();
  filter.including_domains->push_back("co.uk");
  EXPECT_EQ(0u, service_wrapper()->DeleteCookies(filter));
  cookies = service_wrapper()->GetAllCookies();
  ASSERT_EQ(3u, cookies.size());
  EXPECT_EQ("A1", cookies[0].Name());
  EXPECT_EQ("A2", cookies[1].Name());
  EXPECT_EQ("A3", cookies[2].Name());
}

// Confirm deletion ignores host/domain distinction.
TEST_F(CookieManagerTest, DeleteDetails_HostDomain) {
  // Create four cookies: A host (no leading .) and domain cookie
  // (leading .) for each of two separate domains.  Confirm that the
  // filter deletes both of one domain and leaves the other alone.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A1", "val", "foo_host.com", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A2", "val", ".foo_host.com", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A3", "val", "bar.host.com", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A4", "val", ".bar.host.com", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  mojom::CookieDeletionFilter filter;
  filter.including_domains = std::vector<std::string>();
  filter.including_domains->push_back("foo_host.com");
  EXPECT_EQ(2u, service_wrapper()->DeleteCookies(filter));
  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();
  ASSERT_EQ(2u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);
  EXPECT_EQ("A3", cookies[0].Name());
  EXPECT_EQ("A4", cookies[1].Name());
}

TEST_F(CookieManagerTest, DeleteDetails_eTLDvsPrivateRegistry) {
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A1", "val", "random.co.uk", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A2", "val", "sub.domain.random.co.uk", "/", base::Time(),
          base::Time(), base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::LAX_MODE, net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A3", "val", "random.com", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A4", "val", "random", "/", base::Time(), base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A5", "val", "normal.co.uk", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  mojom::CookieDeletionFilter filter;
  filter.including_domains = std::vector<std::string>();
  filter.including_domains->push_back("random.co.uk");
  EXPECT_EQ(2u, service_wrapper()->DeleteCookies(filter));
  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();
  ASSERT_EQ(3u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);
  EXPECT_EQ("A3", cookies[0].Name());
  EXPECT_EQ("A4", cookies[1].Name());
  EXPECT_EQ("A5", cookies[2].Name());
}

TEST_F(CookieManagerTest, DeleteDetails_PrivateRegistry) {
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A1", "val", "privatedomain", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          // Will not actually be treated as a private domain as it's under
          // .com.
          "A2", "val", "privatedomain.com", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::LAX_MODE, net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          // Will not actually be treated as a private domain as it's two
          // level
          "A3", "val", "subdomain.privatedomain", "/", base::Time(),
          base::Time(), base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::LAX_MODE, net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  mojom::CookieDeletionFilter filter;
  filter.including_domains = std::vector<std::string>();
  filter.including_domains->push_back("privatedomain");
  EXPECT_EQ(1u, service_wrapper()->DeleteCookies(filter));
  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();
  ASSERT_EQ(2u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);
  EXPECT_EQ("A2", cookies[0].Name());
  EXPECT_EQ("A3", cookies[1].Name());
}

// Test to probe and make sure the attributes that deletion should ignore
// don't affect the results.
TEST_F(CookieManagerTest, DeleteDetails_IgnoredFields) {
  // Simple deletion filter
  mojom::CookieDeletionFilter filter;
  filter.including_domains = std::vector<std::string>();
  filter.including_domains->push_back("example.com");

  // Set two cookies for each ignored field, one that will be deleted by the
  // above filter, and one that won't, with values unused in other tests
  // for the ignored field.  Secure & httponly are tested in
  // DeleteDetails_Consumer below, and expiration does affect deletion
  // through the persistence distinction.

  // Value
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A01", "RandomValue", "example.com", "/", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::LAX_MODE, net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A02", "RandomValue", "canonical.com", "/", base::Time(),
          base::Time(), base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::LAX_MODE, net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  // Path
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A03", "val", "example.com", "/this/is/a/long/path", base::Time(),
          base::Time(), base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::LAX_MODE, net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A04", "val", "canonical.com", "/this/is/a/long/path", base::Time(),
          base::Time(), base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::LAX_MODE, net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  // Last_access
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A05", "val", "example.com", "/",
          base::Time::Now() - base::TimeDelta::FromDays(3), base::Time(),
          base::Time::Now() - base::TimeDelta::FromDays(3), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A06", "val", "canonical.com", "/",
          base::Time::Now() - base::TimeDelta::FromDays(3), base::Time(),
          base::Time::Now() - base::TimeDelta::FromDays(3), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  // Same_site
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A07", "val", "example.com", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::STRICT_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A08", "val", "canonical.com", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::STRICT_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  // Priority
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A09", "val", "example.com", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_HIGH),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A10", "val", "canonical.com", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_HIGH),
      "https", true));

  // Use the filter and make sure the result is the expected set.
  EXPECT_EQ(5u, service_wrapper()->DeleteCookies(filter));
  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();
  ASSERT_EQ(5u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);
  EXPECT_EQ("A02", cookies[0].Name());
  EXPECT_EQ("A04", cookies[1].Name());
  EXPECT_EQ("A06", cookies[2].Name());
  EXPECT_EQ("A08", cookies[3].Name());
  EXPECT_EQ("A10", cookies[4].Name());
}

// A set of tests specified by the only consumer of this interface
// (BrowsingDataFilterBuilderImpl).
TEST_F(CookieManagerTest, DeleteDetails_Consumer) {
  const char* filter_domains[] = {
      "google.com",

      // sp.nom.br is an eTLD, so this is a regular valid registrable domain,
      // just like google.com.
      "website.sp.nom.br",

      // This domain will also not be found in registries, and since it has only
      // one component, it will not be recognized as a valid registrable domain.
      "fileserver",

      // This domain will not be found in registries. It will be assumed that
      // it belongs to an unknown registry, and since it has two components,
      // they will be treated as the second level domain and TLD. Most
      // importantly, it will NOT be treated as a subdomain of "fileserver".
      "second-level-domain.fileserver",

      // IP addresses are supported.
      "192.168.1.1",
  };
  mojom::CookieDeletionFilter test_filter;
  test_filter.including_domains = std::vector<std::string>();
  for (int i = 0; i < static_cast<int>(base::size(filter_domains)); ++i)
    test_filter.including_domains->push_back(filter_domains[i]);

  struct TestCase {
    std::string domain;
    std::string path;
    bool expect_delete;
  } test_cases[] = {
      // We match any URL on the specified domains.
      {"www.google.com", "/foo/bar", true},
      {"www.sub.google.com", "/foo/bar", true},
      {"sub.google.com", "/", true},
      {"www.sub.google.com", "/foo/bar", true},
      {"website.sp.nom.br", "/", true},
      {"www.website.sp.nom.br", "/", true},
      {"192.168.1.1", "/", true},

      // Internal hostnames do not have subdomains.
      {"fileserver", "/", true},
      {"fileserver", "/foo/bar", true},
      {"website.fileserver", "/foo/bar", false},

      // This is a valid registrable domain with the TLD "fileserver", which
      // is unrelated to the internal hostname "fileserver".
      {"second-level-domain.fileserver", "/foo", true},
      {"www.second-level-domain.fileserver", "/index.html", true},

      // Different domains.
      {"www.youtube.com", "/", false},
      {"www.google.net", "/", false},
      {"192.168.1.2", "/", false},

      // Check both a bare eTLD.
      {"sp.nom.br", "/", false},
  };

  mojom::CookieDeletionFilter clear_filter;
  for (int i = 0; i < static_cast<int>(base::size(test_cases)); ++i) {
    TestCase& test_case(test_cases[i]);

    // Clear store.
    service_wrapper()->DeleteCookies(clear_filter);

    // IP addresses and internal hostnames can only be host cookies.
    bool exclude_domain_cookie =
        (GURL("http://" + test_case.domain).HostIsIPAddress() ||
         test_case.domain.find(".") == std::string::npos);

    // Standard cookie
    EXPECT_TRUE(SetCanonicalCookie(
        net::CanonicalCookie(
            "A1", "val", test_cases[i].domain, test_cases[i].path, base::Time(),
            base::Time(), base::Time(), /*secure=*/false, /*httponly=*/false,
            net::CookieSameSite::LAX_MODE, net::COOKIE_PRIORITY_MEDIUM),
        "https", true));

    if (!exclude_domain_cookie) {
      // Host cookie
      EXPECT_TRUE(SetCanonicalCookie(
          net::CanonicalCookie(
              "A2", "val", "." + test_cases[i].domain, test_cases[i].path,
              base::Time(), base::Time(), base::Time(), /*secure=*/false,
              /*httponly=*/false, net::CookieSameSite::LAX_MODE,
              net::COOKIE_PRIORITY_MEDIUM),
          "https", true));
    }

    // Httponly cookie
    EXPECT_TRUE(SetCanonicalCookie(
        net::CanonicalCookie(
            "A3", "val", test_cases[i].domain, test_cases[i].path, base::Time(),
            base::Time(), base::Time(), /*secure=*/false, /*httponly=*/true,
            net::CookieSameSite::LAX_MODE, net::COOKIE_PRIORITY_MEDIUM),
        "https", true));

    // Httponly and secure cookie
    EXPECT_TRUE(SetCanonicalCookie(
        net::CanonicalCookie(
            "A4", "val", test_cases[i].domain, test_cases[i].path, base::Time(),
            base::Time(), base::Time(), /*secure=*/false, /*httponly=*/true,
            net::CookieSameSite::LAX_MODE, net::COOKIE_PRIORITY_MEDIUM),
        "https", true));

    const uint32_t number_cookies = exclude_domain_cookie ? 3u : 4u;
    EXPECT_EQ(number_cookies, service_wrapper()->GetAllCookies().size());
    EXPECT_EQ(test_cases[i].expect_delete ? number_cookies : 0u,
              service_wrapper()->DeleteCookies(test_filter))
        << "Case " << i << " domain " << test_cases[i].domain << " path "
        << test_cases[i].path << " expect "
        << (test_cases[i].expect_delete ? "delete" : "keep");
    EXPECT_EQ(test_cases[i].expect_delete ? 0u : number_cookies,
              service_wrapper()->GetAllCookies().size());
  }
}

TEST_F(CookieManagerTest, DeleteByName) {
  // Create cookies with varying (name, host)
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A1", "val", kCookieDomain, "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true /*modify_httponly*/));

  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A1", "val", "bar_host", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true /*modify_httponly*/));

  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A2", "val", kCookieDomain, "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true /*modify_httponly*/));

  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A3", "val", "bar_host", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true /*modify_httponly*/));

  mojom::CookieDeletionFilter filter;
  filter.cookie_name = std::string("A1");
  EXPECT_EQ(2u, service_wrapper()->DeleteCookies(filter));

  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();
  ASSERT_EQ(2u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);
  EXPECT_EQ("A2", cookies[0].Name());
  EXPECT_EQ("A3", cookies[1].Name());
}

TEST_F(CookieManagerTest, DeleteByURL) {
  GURL filter_url("http://www.example.com/path");

  // Cookie that shouldn't be deleted because it's secure.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A01", "val", "www.example.com", "/path", base::Time(), base::Time(),
          base::Time(), /*secure=*/true, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      "https", true /*modify_httponly*/));

  // Cookie that should not be deleted because it's a host cookie in a
  // subdomain that doesn't exactly match the passed URL.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A02", "val", "sub.www.example.com", "/path", base::Time(),
          base::Time(), base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::LAX_MODE, net::COOKIE_PRIORITY_MEDIUM),
      "https", true /*modify_httponly*/));

  // Cookie that shouldn't be deleted because the path doesn't match.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A03", "val", "www.example.com", "/otherpath", base::Time(),
          base::Time(), base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::LAX_MODE, net::COOKIE_PRIORITY_MEDIUM),
      "https", true /*modify_httponly*/));

  // Cookie that shouldn't be deleted because the path is more specific
  // than the URL.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A04", "val", "www.example.com", "/path/path2", base::Time(),
          base::Time(), base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::LAX_MODE, net::COOKIE_PRIORITY_MEDIUM),
      "https", true /*modify_httponly*/));

  // Cookie that shouldn't be deleted because it's at a host cookie domain that
  // doesn't exactly match the url's host.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A05", "val", "example.com", "/path", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true /*modify_httponly*/));

  // Cookie that should not be deleted because it's not a host cookie and
  // has a domain that's more specific than the URL
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A06", "val", ".sub.www.example.com", "/path", base::Time(),
          base::Time(), base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::LAX_MODE, net::COOKIE_PRIORITY_MEDIUM),
      "https", true /*modify_httponly*/));

  // Cookie that should be deleted because it's not a host cookie and has a
  // domain that matches the URL
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A07", "val", ".www.example.com", "/path", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::LAX_MODE, net::COOKIE_PRIORITY_MEDIUM),
      "https", true /*modify_httponly*/));

  // Cookie that should be deleted because it's not a host cookie and has a
  // domain that domain matches the URL.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A08", "val", ".example.com", "/path", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true /*modify_httponly*/));

  // Cookie that should be deleted because it matches exactly.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A09", "val", "www.example.com", "/path", base::Time(), base::Time(),
          base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::LAX_MODE, net::COOKIE_PRIORITY_MEDIUM),
      "https", true /*modify_httponly*/));

  // Cookie that should be deleted because it applies to a larger set
  // of paths than the URL path.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A10", "val", "www.example.com", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true /*modify_httponly*/));

  mojom::CookieDeletionFilter filter;
  filter.url = filter_url;
  EXPECT_EQ(4u, service_wrapper()->DeleteCookies(filter));

  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();
  ASSERT_EQ(6u, cookies.size()) << DumpAllCookies();
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);
  EXPECT_EQ("A01", cookies[0].Name());
  EXPECT_EQ("A02", cookies[1].Name());
  EXPECT_EQ("A03", cookies[2].Name());
  EXPECT_EQ("A04", cookies[3].Name());
  EXPECT_EQ("A05", cookies[4].Name());
  EXPECT_EQ("A06", cookies[5].Name());
}

TEST_F(CookieManagerTest, DeleteBySessionStatus) {
  base::Time now(base::Time::Now());

  // Create three cookies and delete the middle one.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A1", "val", kCookieDomain, "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A2", "val", kCookieDomain, "/", base::Time(),
                           now + base::TimeDelta::FromDays(1), base::Time(),
                           /*secure=*/false, /*httponly=*/false,
                           net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A3", "val", kCookieDomain, "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  mojom::CookieDeletionFilter filter;
  filter.session_control =
      mojom::CookieDeletionSessionControl::PERSISTENT_COOKIES;
  EXPECT_EQ(1u, service_wrapper()->DeleteCookies(filter));
  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();
  ASSERT_EQ(2u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);
  EXPECT_EQ("A1", cookies[0].Name());
  EXPECT_EQ("A3", cookies[1].Name());
}

TEST_F(CookieManagerTest, DeleteByAll) {
  base::Time now(base::Time::Now());

  // Add a lot of cookies, only one of which will be deleted by the filter.
  // Filter will be:
  //    * Between two and four days ago.
  //    * Including domains: no.com and nope.com
  //    * Excluding domains: no.com and yes.com (excluding wins on no.com
  //      because of ANDing)
  //    * Matching a specific URL.
  //    * Persistent cookies.
  // The archetypal cookie (which will be deleted) will satisfy all of
  // these filters (2 days old, nope.com, persistent).
  // Each of the other four cookies will vary in a way that will take it out
  // of being deleted by one of the filter.

  // Cookie name is not included as a filter because the cookies are made
  // unique by name.

  mojom::CookieDeletionFilter filter;
  filter.created_after_time = now - base::TimeDelta::FromDays(4);
  filter.created_before_time = now - base::TimeDelta::FromDays(2);
  filter.including_domains = std::vector<std::string>();
  filter.including_domains->push_back("no.com");
  filter.including_domains->push_back("nope.com");
  filter.excluding_domains = std::vector<std::string>();
  filter.excluding_domains->push_back("no.com");
  filter.excluding_domains->push_back("yes.com");
  filter.url = GURL("http://nope.com/path");
  filter.session_control =
      mojom::CookieDeletionSessionControl::PERSISTENT_COOKIES;

  // Architectypal cookie:
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A1", "val0", "nope.com", "/path", now - base::TimeDelta::FromDays(3),
          now + base::TimeDelta::FromDays(3), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  // Too old cookie.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A2", "val1", "nope.com", "/path", now - base::TimeDelta::FromDays(5),
          now + base::TimeDelta::FromDays(3), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  // Too young cookie.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A3", "val2", "nope.com", "/path", now - base::TimeDelta::FromDays(1),
          now + base::TimeDelta::FromDays(3), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  // Not in domains_and_ips_to_delete.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A4", "val3", "other.com", "/path",
                           now - base::TimeDelta::FromDays(3),
                           now + base::TimeDelta::FromDays(3), base::Time(),
                           /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  // In domains_and_ips_to_ignore.
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A5", "val4", "no.com", "/path", now - base::TimeDelta::FromDays(3),
          now + base::TimeDelta::FromDays(3), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  // Doesn't match URL (by path).
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie("A6", "val6", "nope.com", "/otherpath",
                           now - base::TimeDelta::FromDays(3),
                           now + base::TimeDelta::FromDays(3), base::Time(),
                           /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  // Session
  EXPECT_TRUE(SetCanonicalCookie(
      net::CanonicalCookie(
          "A7", "val7", "nope.com", "/path", now - base::TimeDelta::FromDays(3),
          base::Time(), base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::LAX_MODE, net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  EXPECT_EQ(1u, service_wrapper()->DeleteCookies(filter));
  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();
  ASSERT_EQ(6u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);
  EXPECT_EQ("A2", cookies[0].Name());
  EXPECT_EQ("A3", cookies[1].Name());
  EXPECT_EQ("A4", cookies[2].Name());
  EXPECT_EQ("A5", cookies[3].Name());
  EXPECT_EQ("A6", cookies[4].Name());
  EXPECT_EQ("A7", cookies[5].Name());
}

// Receives and records notifications from the mojom::CookieManager.
class CookieChangeListener : public mojom::CookieChangeListener {
 public:
  CookieChangeListener(
      mojo::PendingReceiver<mojom::CookieChangeListener> receiver)
      : run_loop_(nullptr), receiver_(this, std::move(receiver)) {}

  // Blocks until the listener observes a cookie change.
  void WaitForChange() {
    if (!observed_changes_.empty())
      return;
    base::RunLoop loop;
    run_loop_ = &loop;
    loop.Run();
    run_loop_ = nullptr;
  }

  void ClearObservedChanges() { observed_changes_.clear(); }

  const std::vector<net::CookieChangeInfo>& observed_changes() const {
    return observed_changes_;
  }

  // mojom::CookieChangeListener
  void OnCookieChange(const net::CookieChangeInfo& change) override {
    observed_changes_.push_back(change);
    if (run_loop_)
      run_loop_->Quit();
  }

 private:
  std::vector<net::CookieChangeInfo> observed_changes_;

  // Loop to signal on receiving a notification if not null.
  base::RunLoop* run_loop_;

  mojo::Receiver<mojom::CookieChangeListener> receiver_;
};

TEST_F(CookieManagerTest, AddCookieChangeListener) {
  const GURL listener_url("http://www.testing.com/pathele");
  const std::string listener_url_host("www.testing.com");
  const std::string listener_url_domain("testing.com");
  const std::string listener_cookie_name("Cookie_Name");
  ASSERT_EQ(listener_url.host(), listener_url_host);

  mojo::PendingRemote<mojom::CookieChangeListener> listener_remote;
  CookieChangeListener listener(
      listener_remote.InitWithNewPipeAndPassReceiver());

  cookie_service_client()->AddCookieChangeListener(
      listener_url, listener_cookie_name, std::move(listener_remote));

  EXPECT_EQ(0u, listener.observed_changes().size());

  // Set a cookie that doesn't match the above notification request in name
  // and confirm it doesn't produce a notification.
  service_wrapper()->SetCanonicalCookie(
      net::CanonicalCookie("DifferentName", "val", listener_url_host, "/",
                           base::Time(), base::Time(), base::Time(),
                           /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, listener.observed_changes().size());

  // Set a cookie that doesn't match the above notification request in url
  // and confirm it doesn't produce a notification.
  service_wrapper()->SetCanonicalCookie(
      net::CanonicalCookie(listener_cookie_name, "val", "www.other.host", "/",
                           base::Time(), base::Time(), base::Time(),
                           /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, listener.observed_changes().size());

  // Insert a cookie that does match.
  service_wrapper()->SetCanonicalCookie(
      net::CanonicalCookie(listener_cookie_name, "val", listener_url_host, "/",
                           base::Time(), base::Time(), base::Time(),
                           /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true);

  // Expect asynchrony
  EXPECT_EQ(0u, listener.observed_changes().size());

  // Expect to observe a cookie change.
  listener.WaitForChange();
  std::vector<net::CookieChangeInfo> observed_changes =
      listener.observed_changes();
  ASSERT_EQ(1u, observed_changes.size());
  EXPECT_EQ(listener_cookie_name, observed_changes[0].cookie.Name());
  EXPECT_EQ(listener_url_host, observed_changes[0].cookie.Domain());
  EXPECT_EQ(net::CookieChangeCause::INSERTED, observed_changes[0].cause);
  listener.ClearObservedChanges();

  // Delete all cookies matching the domain.  This includes one for which
  // a notification will be generated, and one for which a notification
  // will not be generated.
  mojom::CookieDeletionFilter filter;
  filter.including_domains = std::vector<std::string>();
  filter.including_domains->push_back(listener_url_domain);
  // If this test fails, it may indicate a problem which will lead to
  // no notifications being generated and the test hanging, so assert.
  ASSERT_EQ(2u, service_wrapper()->DeleteCookies(filter));

  // The notification may already have arrived, or it may arrive in the future.
  listener.WaitForChange();
  observed_changes = listener.observed_changes();
  ASSERT_EQ(1u, observed_changes.size());
  EXPECT_EQ(listener_cookie_name, observed_changes[0].cookie.Name());
  EXPECT_EQ(listener_url_host, observed_changes[0].cookie.Domain());
  EXPECT_EQ(net::CookieChangeCause::EXPLICIT, observed_changes[0].cause);
}

TEST_F(CookieManagerTest, AddGlobalChangeListener) {
  const std::string kExampleHost = "www.example.com";
  const std::string kThisHost = "www.this.com";
  const std::string kThisETLDP1 = "this.com";
  const std::string kThatHost = "www.that.com";

  mojo::PendingRemote<mojom::CookieChangeListener> listener_remote;
  CookieChangeListener listener(
      listener_remote.InitWithNewPipeAndPassReceiver());

  cookie_service_client()->AddGlobalChangeListener(std::move(listener_remote));

  EXPECT_EQ(0u, listener.observed_changes().size());

  // Confirm the right change is observed after setting a cookie.
  service_wrapper()->SetCanonicalCookie(
      net::CanonicalCookie("Thing1", "val", kExampleHost, "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true);

  // Expect asynchrony
  EXPECT_EQ(0u, listener.observed_changes().size());

  base::RunLoop().RunUntilIdle();
  std::vector<net::CookieChangeInfo> observed_changes =
      listener.observed_changes();
  ASSERT_EQ(1u, observed_changes.size());
  EXPECT_EQ("Thing1", observed_changes[0].cookie.Name());
  EXPECT_EQ("val", observed_changes[0].cookie.Value());
  EXPECT_EQ(kExampleHost, observed_changes[0].cookie.Domain());
  EXPECT_EQ("/", observed_changes[0].cookie.Path());
  EXPECT_EQ(net::CookieChangeCause::INSERTED, observed_changes[0].cause);
  listener.ClearObservedChanges();

  // Set two cookies in a row on different domains and confirm they are both
  // signalled.
  service_wrapper()->SetCanonicalCookie(
      net::CanonicalCookie("Thing1", "val", kThisHost, "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true);
  service_wrapper()->SetCanonicalCookie(
      net::CanonicalCookie("Thing2", "val", kThatHost, "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true);

  base::RunLoop().RunUntilIdle();
  observed_changes = listener.observed_changes();
  ASSERT_EQ(2u, observed_changes.size());
  EXPECT_EQ("Thing1", observed_changes[0].cookie.Name());
  EXPECT_EQ(net::CookieChangeCause::INSERTED, observed_changes[0].cause);
  EXPECT_EQ("Thing2", observed_changes[1].cookie.Name());
  EXPECT_EQ(net::CookieChangeCause::INSERTED, observed_changes[1].cause);
  listener.ClearObservedChanges();

  // Delete cookies matching one domain; should produce one notification.
  mojom::CookieDeletionFilter filter;
  filter.including_domains = std::vector<std::string>();
  filter.including_domains->push_back(kThisETLDP1);
  // If this test fails, it may indicate a problem which will lead to
  // no notifications being generated and the test hanging, so assert.
  ASSERT_EQ(1u, service_wrapper()->DeleteCookies(filter));

  // The notification may already have arrived, or it may arrive in the future.
  listener.WaitForChange();
  observed_changes = listener.observed_changes();
  ASSERT_EQ(1u, observed_changes.size());
  EXPECT_EQ("Thing1", observed_changes[0].cookie.Name());
  EXPECT_EQ(kThisHost, observed_changes[0].cookie.Domain());
  EXPECT_EQ(net::CookieChangeCause::EXPLICIT, observed_changes[0].cause);
}

// Confirm the service operates properly if a returned notification interface
// is destroyed.
TEST_F(CookieManagerTest, ListenerDestroyed) {
  // Create two identical listeners.
  const GURL listener_url("http://www.testing.com/pathele");
  const std::string listener_url_host("www.testing.com");
  ASSERT_EQ(listener_url.host(), listener_url_host);
  const std::string listener_cookie_name("Cookie_Name");

  mojo::PendingRemote<mojom::CookieChangeListener> listener1_remote;
  auto listener1 = std::make_unique<CookieChangeListener>(
      listener1_remote.InitWithNewPipeAndPassReceiver());
  cookie_service_client()->AddCookieChangeListener(
      listener_url, listener_cookie_name, std::move(listener1_remote));

  mojo::PendingRemote<mojom::CookieChangeListener> listener2_remote;
  auto listener2 = std::make_unique<CookieChangeListener>(
      listener2_remote.InitWithNewPipeAndPassReceiver());
  cookie_service_client()->AddCookieChangeListener(
      listener_url, listener_cookie_name, std::move(listener2_remote));

  // Add a cookie and receive a notification on both interfaces.
  service_wrapper()->SetCanonicalCookie(
      net::CanonicalCookie(listener_cookie_name, "val", listener_url_host, "/",
                           base::Time(), base::Time(), base::Time(),
                           /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true);

  EXPECT_EQ(0u, listener1->observed_changes().size());
  EXPECT_EQ(0u, listener2->observed_changes().size());

  listener1->WaitForChange();
  EXPECT_EQ(1u, listener1->observed_changes().size());
  listener1->ClearObservedChanges();
  listener2->WaitForChange();
  EXPECT_EQ(1u, listener2->observed_changes().size());
  listener2->ClearObservedChanges();
  EXPECT_EQ(2u, service()->GetListenersRegisteredForTesting());

  // Destroy the first listener.
  listener1.reset();

  // Confirm the second interface can still receive notifications.
  mojom::CookieDeletionFilter filter;
  EXPECT_EQ(1u, service_wrapper()->DeleteCookies(filter));

  listener2->WaitForChange();
  EXPECT_EQ(1u, listener2->observed_changes().size());

  EXPECT_EQ(1u, service()->GetListenersRegisteredForTesting());
}

// Confirm we get a connection error notification if the service dies.
TEST_F(CookieManagerTest, ServiceDestructVisible) {
  EXPECT_FALSE(connection_error_seen());
  NukeService();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(connection_error_seen());
}

// Test service cloning.  Also confirm that the service notices if a client
// dies.
TEST_F(CookieManagerTest, CloningAndClientDestructVisible) {
  EXPECT_EQ(1u, service()->GetClientsBoundForTesting());

  // Clone the interface.
  mojo::Remote<mojom::CookieManager> new_remote;
  cookie_service_client()->CloneInterface(
      new_remote.BindNewPipeAndPassReceiver());

  SynchronousCookieManager new_wrapper(new_remote.get());

  // Set a cookie on the new interface and make sure it's visible on the
  // old one.
  EXPECT_TRUE(new_wrapper.SetCanonicalCookie(
      net::CanonicalCookie("X", "Y", "www.other.host", "/", base::Time(),
                           base::Time(), base::Time(), /*secure=*/false,
                           /*httponly=*/false, net::CookieSameSite::LAX_MODE,
                           net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  std::vector<net::CanonicalCookie> cookies = service_wrapper()->GetCookieList(
      GURL("http://www.other.host/"), net::CookieOptions::MakeAllInclusive());
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ("X", cookies[0].Name());
  EXPECT_EQ("Y", cookies[0].Value());

  // After a synchronous round trip through the new client pointer, it
  // should be reflected in the bindings seen on the server.
  EXPECT_EQ(2u, service()->GetClientsBoundForTesting());

  new_remote.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, service()->GetClientsBoundForTesting());
}

TEST_F(CookieManagerTest, BlockThirdPartyCookies) {
  const GURL kThisURL = GURL("http://www.this.com");
  const GURL kThatURL = GURL("http://www.that.com");
  EXPECT_TRUE(
      service()->cookie_settings().IsCookieAccessAllowed(kThisURL, kThatURL));

  // Set block third party cookies to true, cookie should now be blocked.
  cookie_service_client()->BlockThirdPartyCookies(true);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(
      service()->cookie_settings().IsCookieAccessAllowed(kThisURL, kThatURL));
  EXPECT_TRUE(
      service()->cookie_settings().IsCookieAccessAllowed(kThisURL, kThisURL));

  // Set block third party cookies back to false, cookie should no longer be
  // blocked.
  cookie_service_client()->BlockThirdPartyCookies(false);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(
      service()->cookie_settings().IsCookieAccessAllowed(kThisURL, kThatURL));
}

// A test class having cookie store with a persistent backing store.
class FlushableCookieManagerTest : public CookieManagerTest {
 public:
  FlushableCookieManagerTest()
      : store_(base::MakeRefCounted<net::FlushablePersistentStore>()) {
    InitializeCookieService(store_, nullptr);
  }

  ~FlushableCookieManagerTest() override {}

  net::FlushablePersistentStore* store() { return store_.get(); }

 private:
  scoped_refptr<net::FlushablePersistentStore> store_;
};

// Tests that the cookie's backing store (if available) gets flush to disk.
TEST_F(FlushableCookieManagerTest, FlushCookieStore) {
  ASSERT_EQ(0, store()->flush_count());
  ASSERT_EQ(0U, service_wrapper()->callback_count());

  // Before initialization, FlushCookieStore() should just run the callback.
  service_wrapper()->FlushCookieStore();

  ASSERT_EQ(0, store()->flush_count());
  ASSERT_EQ(1U, service_wrapper()->callback_count());

  // After initialization, FlushCookieStore() should delegate to the store.
  service_wrapper()->GetAllCookies();  // force init
  service_wrapper()->FlushCookieStore();

  ASSERT_EQ(1, store()->flush_count());
  ASSERT_EQ(2U, service_wrapper()->callback_count());
}

TEST_F(FlushableCookieManagerTest, DeletionFilterToInfo) {
  mojom::CookieDeletionFilterPtr filter_ptr =
      mojom::CookieDeletionFilter::New();

  // First test the default values.
  CookieDeletionInfo delete_info = DeletionFilterToInfo(std::move(filter_ptr));
  EXPECT_TRUE(delete_info.creation_range.start().is_null());
  EXPECT_TRUE(delete_info.creation_range.end().is_null());
  EXPECT_EQ(CookieDeletionInfo::SessionControl::IGNORE_CONTROL,
            delete_info.session_control);
  EXPECT_FALSE(delete_info.host.has_value());
  EXPECT_FALSE(delete_info.name.has_value());
  EXPECT_FALSE(delete_info.url.has_value());
  EXPECT_TRUE(delete_info.domains_and_ips_to_delete.empty());
  EXPECT_TRUE(delete_info.domains_and_ips_to_ignore.empty());
  EXPECT_FALSE(delete_info.value_for_testing.has_value());

  // Then test all with non-default values.
  const double kTestStartEpoch = 1000;
  const double kTestEndEpoch = 10000000;
  filter_ptr = mojom::CookieDeletionFilter::New();
  filter_ptr->created_after_time = base::Time::FromDoubleT(kTestStartEpoch);
  filter_ptr->created_before_time = base::Time::FromDoubleT(kTestEndEpoch);
  filter_ptr->cookie_name = "cookie-name";
  filter_ptr->host_name = "cookie-host";
  filter_ptr->including_domains =
      std::vector<std::string>({"first.com", "second.com", "third.com"});
  filter_ptr->excluding_domains =
      std::vector<std::string>({"ten.com", "twelve.com"});
  filter_ptr->url = GURL("https://www.example.com");
  filter_ptr->session_control =
      mojom::CookieDeletionSessionControl::PERSISTENT_COOKIES;

  delete_info = DeletionFilterToInfo(std::move(filter_ptr));
  EXPECT_EQ(base::Time::FromDoubleT(kTestStartEpoch),
            delete_info.creation_range.start());
  EXPECT_EQ(base::Time::FromDoubleT(kTestEndEpoch),
            delete_info.creation_range.end());

  EXPECT_EQ(CookieDeletionInfo::SessionControl::PERSISTENT_COOKIES,
            delete_info.session_control);
  EXPECT_EQ("cookie-name", delete_info.name.value());
  EXPECT_EQ("cookie-host", delete_info.host.value());
  EXPECT_EQ(GURL("https://www.example.com"), delete_info.url.value());
  EXPECT_EQ(3u, delete_info.domains_and_ips_to_delete.size());
  EXPECT_NE(delete_info.domains_and_ips_to_delete.find("first.com"),
            delete_info.domains_and_ips_to_delete.end());
  EXPECT_NE(delete_info.domains_and_ips_to_delete.find("second.com"),
            delete_info.domains_and_ips_to_delete.end());
  EXPECT_NE(delete_info.domains_and_ips_to_delete.find("third.com"),
            delete_info.domains_and_ips_to_delete.end());
  EXPECT_EQ(2u, delete_info.domains_and_ips_to_ignore.size());
  EXPECT_NE(delete_info.domains_and_ips_to_ignore.find("ten.com"),
            delete_info.domains_and_ips_to_ignore.end());
  EXPECT_NE(delete_info.domains_and_ips_to_ignore.find("twelve.com"),
            delete_info.domains_and_ips_to_ignore.end());
  EXPECT_FALSE(delete_info.value_for_testing.has_value());
}

// A test class having cookie store with a persistent backing store. The cookie
// store can be destroyed and recreated by calling InitializeCookieService
// again.
class SessionCleanupCookieManagerTest : public CookieManagerTest {
 public:
  ~SessionCleanupCookieManagerTest() override {}

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    auto store = CreateCookieStore();
    InitializeCookieService(store, store);
  }

  scoped_refptr<SessionCleanupCookieStore> CreateCookieStore() {
    auto sqlite_store = base::MakeRefCounted<net::SQLitePersistentCookieStore>(
        temp_dir_.GetPath().Append(kTestCookiesFilename),
        task_environment_.GetMainThreadTaskRunner(), background_task_runner_,
        true, nullptr);
    return base::MakeRefCounted<SessionCleanupCookieStore>(sqlite_store.get());
  }

  net::CanonicalCookie CreateCookie() { return CreateCookie(kCookieDomain); }

  net::CanonicalCookie CreateCookie(const std::string& domain) {
    base::Time t = base::Time::Now();
    return net::CanonicalCookie("A", "B", domain, "/", t,
                                t + base::TimeDelta::FromDays(1), base::Time(),
                                /*secure=*/false, /*httponly=*/false,
                                net::CookieSameSite::LAX_MODE,
                                net::COOKIE_PRIORITY_MEDIUM);
  }

 private:
  const scoped_refptr<base::SequencedTaskRunner> background_task_runner_ =
      base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock()});
};

TEST_F(SessionCleanupCookieManagerTest, PersistSessionCookies) {
  EXPECT_TRUE(SetCanonicalCookie(CreateCookie(), "https", true));

  EXPECT_EQ(1u, service_wrapper()->GetAllCookies().size());

  // Re-create the cookie store to make sure cookies are persisted.
  auto store = CreateCookieStore();
  InitializeCookieService(store, store);

  EXPECT_EQ(1u, service_wrapper()->GetAllCookies().size());
}

TEST_F(SessionCleanupCookieManagerTest, DeleteSessionCookies) {
  EXPECT_TRUE(SetCanonicalCookie(CreateCookie(), "https", true));

  EXPECT_EQ(1u, service_wrapper()->GetAllCookies().size());

  cookie_service_client()->SetContentSettings(
      {CreateSetting(CONTENT_SETTING_SESSION_ONLY, kCookieURL)});
  base::RunLoop().RunUntilIdle();

  auto store = CreateCookieStore();
  InitializeCookieService(store, store);

  EXPECT_EQ(0u, service_wrapper()->GetAllCookies().size());
}

TEST_F(SessionCleanupCookieManagerTest, SettingMustMatchDomain) {
  EXPECT_TRUE(SetCanonicalCookie(CreateCookie(), "https", true));

  EXPECT_EQ(1u, service_wrapper()->GetAllCookies().size());

  cookie_service_client()->SetContentSettings(
      {CreateSetting(CONTENT_SETTING_SESSION_ONLY, "http://other.com")});
  base::RunLoop().RunUntilIdle();

  auto store = CreateCookieStore();
  InitializeCookieService(store, store);

  EXPECT_EQ(1u, service_wrapper()->GetAllCookies().size());
}

TEST_F(SessionCleanupCookieManagerTest, FirstSettingTakesPrecedence) {
  EXPECT_TRUE(SetCanonicalCookie(CreateCookie(), "https", true));

  EXPECT_EQ(1u, service_wrapper()->GetAllCookies().size());

  // If a rule with ALLOW is before a SESSION_ONLY rule, the cookie should not
  // be deleted.
  cookie_service_client()->SetContentSettings(
      {CreateSetting(CONTENT_SETTING_ALLOW, kCookieURL),
       CreateSetting(CONTENT_SETTING_SESSION_ONLY, kCookieURL)});
  base::RunLoop().RunUntilIdle();

  auto store = CreateCookieStore();
  InitializeCookieService(store, store);

  EXPECT_EQ(1u, service_wrapper()->GetAllCookies().size());
}

TEST_F(SessionCleanupCookieManagerTest, ForceKeepSessionState) {
  EXPECT_TRUE(SetCanonicalCookie(CreateCookie(), "https", true));

  EXPECT_EQ(1u, service_wrapper()->GetAllCookies().size());

  cookie_service_client()->SetContentSettings(
      {CreateSetting(CONTENT_SETTING_SESSION_ONLY, kCookieURL)});
  cookie_service_client()->SetForceKeepSessionState();
  base::RunLoop().RunUntilIdle();

  auto store = CreateCookieStore();
  InitializeCookieService(store, store);

  EXPECT_EQ(1u, service_wrapper()->GetAllCookies().size());
}

TEST_F(SessionCleanupCookieManagerTest, HttpCookieAllowedOnHttps) {
  EXPECT_TRUE(SetCanonicalCookie(CreateCookie(StrCat({"www.", kCookieDomain})),
                                 "https", true));

  EXPECT_EQ(1u, service_wrapper()->GetAllCookies().size());

  cookie_service_client()->SetContentSettings({
      CreateSetting(CONTENT_SETTING_ALLOW, kCookieHttpsURL),
      CreateDefaultSetting(CONTENT_SETTING_SESSION_ONLY),
  });
  base::RunLoop().RunUntilIdle();

  auto store = CreateCookieStore();
  InitializeCookieService(store, store);

  EXPECT_EQ(1u, service_wrapper()->GetAllCookies().size());
}

}  // namespace
}  // namespace network
