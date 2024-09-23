// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/network/cookie_manager.h"

#include <algorithm>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/test/spin_wait.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "net/cookies/canonical_cookie_test_helpers.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_store_test_callbacks.h"
#include "net/cookies/cookie_store_test_helpers.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/test_cookie_access_delegate.h"
#include "net/extras/sqlite/sqlite_persistent_cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/cookie_access_delegate_impl.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/session_cleanup_cookie_store.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
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
using testing::IsEmpty;
using testing::UnorderedElementsAre;

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
      : cookie_service_(cookie_service) {}

  SynchronousCookieManager(const SynchronousCookieManager&) = delete;
  SynchronousCookieManager& operator=(const SynchronousCookieManager&) = delete;

  ~SynchronousCookieManager() = default;

  std::vector<net::CanonicalCookie> GetAllCookies() {
    base::test::TestFuture<const std::vector<net::CanonicalCookie>&> future;
    cookie_service_->GetAllCookies(future.GetCallback());
    return future.Take();
  }

  std::vector<net::CanonicalCookie> GetAllCookiesWithAccessSemantics(
      std::vector<net::CookieAccessSemantics>* access_semantics_list_out) {
    base::test::TestFuture<const std::vector<net::CanonicalCookie>&,
                           const std::vector<net::CookieAccessSemantics>&>
        future;
    cookie_service_->GetAllCookiesWithAccessSemantics(future.GetCallback());
    *access_semantics_list_out = std::get<1>(future.Get());
    return std::get<0>(future.Get());
  }

  std::vector<net::CanonicalCookie> GetCookieList(
      const GURL& url,
      net::CookieOptions options,
      const net::CookiePartitionKeyCollection&
          cookie_partition_key_collection) {
    base::test::TestFuture<const net::CookieAccessResultList&,
                           const net::CookieAccessResultList&>
        future;
    cookie_service_->GetCookieList(
        url, options, cookie_partition_key_collection, future.GetCallback());
    return net::cookie_util::StripAccessResults(std::get<0>(future.Take()));
  }

  // TODO(crbug.com/40188414): CookieManager should be able to see which cookies
  // are excluded because their partition key is not contained in the
  // key collection.
  net::CookieAccessResultList GetExcludedCookieList(
      const GURL& url,
      net::CookieOptions options) {
    base::test::TestFuture<const net::CookieAccessResultList&,
                           const net::CookieAccessResultList&>
        future;
    cookie_service_->GetCookieList(url, options,
                                   net::CookiePartitionKeyCollection::Todo(),
                                   future.GetCallback());
    return std::get<1>(future.Take());
  }

  bool SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          std::string source_scheme,
                          bool modify_http_only) {
    net::CookieOptions options;
    options.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext::MakeInclusive());
    if (modify_http_only)
      options.set_include_httponly();
    base::test::TestFuture<net::CookieAccessResult> future;
    cookie_service_->SetCanonicalCookie(
        cookie, net::cookie_util::SimulatedCookieSource(cookie, source_scheme),
        options, future.GetCallback());

    return future.Take().status.IsInclude();
  }

  net::CookieAccessResult SetCanonicalCookieWithAccessResult(
      const net::CanonicalCookie& cookie,
      std::string source_scheme,
      bool modify_http_only) {
    net::CookieOptions options;
    options.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext::MakeInclusive());
    if (modify_http_only)
      options.set_include_httponly();
    base::test::TestFuture<net::CookieAccessResult> future;
    cookie_service_->SetCanonicalCookie(
        cookie, net::cookie_util::SimulatedCookieSource(cookie, source_scheme),
        options, future.GetCallback());

    return future.Take();
  }

  // TODO(chlily): Clean up these Set*() methods to all use proper source_url.
  net::CookieInclusionStatus SetCanonicalCookieFromUrlWithStatus(
      const net::CanonicalCookie& cookie,
      const GURL& source_url,
      bool modify_http_only) {
    net::CookieOptions options;
    options.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext::MakeInclusive());
    if (modify_http_only)
      options.set_include_httponly();
    net::CookieInclusionStatus result_out(
        net::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR);
    base::test::TestFuture<net::CookieAccessResult> future;
    cookie_service_->SetCanonicalCookie(cookie, source_url, options,
                                        future.GetCallback());

    return future.Take().status;
  }

  bool DeleteCanonicalCookie(const net::CanonicalCookie& cookie) {
    base::test::TestFuture<bool> future;
    cookie_service_->DeleteCanonicalCookie(cookie, future.GetCallback());
    return future.Get();
  }

  uint32_t DeleteCookies(mojom::CookieDeletionFilter filter) {
    base::test::TestFuture<uint32_t> future;
    cookie_service_->DeleteCookies(mojom::CookieDeletionFilter::New(filter),
                                   future.GetCallback());
    return future.Get();
  }

  uint32_t DeleteSessionOnlyCookies() {
    base::test::TestFuture<uint32_t> future;
    cookie_service_->DeleteSessionOnlyCookies(future.GetCallback());
    return future.Get();
  }

  uint32_t DeleteStaleSessionOnlyCookies() {
    base::test::TestFuture<uint32_t> future;
    cookie_service_->DeleteStaleSessionOnlyCookies(future.GetCallback());
    return future.Get();
  }

  void FlushCookieStore() {
    base::RunLoop run_loop;
    cookie_service_->FlushCookieStore(base::BindLambdaForTesting([&]() {
      ++callback_counter_;
      run_loop.Quit();
    }));
    run_loop.Run();
  }

  void SetStorageAccessGrantSettings() {
    std::vector<ContentSettingPatternSource> settings;
    base::RunLoop run_loop;
    cookie_service_->SetContentSettings(ContentSettingsType::STORAGE_ACCESS,
                                        std::move(settings),
                                        base::BindLambdaForTesting([&]() {
                                          ++callback_counter_;
                                          run_loop.Quit();
                                        }));
    run_loop.Run();
  }

  uint32_t callback_count() const { return callback_counter_; }

  // No need to wrap Add*Listener and CloneInterface, since their use
  // is purely async.
 private:
  raw_ptr<mojom::CookieManager> cookie_service_;
  uint32_t callback_counter_ = 0;
};

class CookieManagerTest : public testing::Test {
 public:
  CookieManagerTest() = default;

  void SetUp() override { InitializeCookieService(nullptr, nullptr); }

  CookieManagerTest(const CookieManagerTest&) = delete;
  CookieManagerTest& operator=(const CookieManagerTest&) = delete;

  ~CookieManagerTest() override = default;

  // Tear down the remote service.
  void NukeService() { cookie_service_.reset(); }

  // Set a canonical cookie directly into the store.
  bool SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          std::string source_scheme,
                          bool can_modify_httponly) {
    net::ResultSavingCookieCallback<net::CookieAccessResult> callback;
    net::CookieOptions options;
    options.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext::MakeInclusive());
    if (can_modify_httponly)
      options.set_include_httponly();

    cookie_store()->SetCanonicalCookieAsync(
        std::make_unique<net::CanonicalCookie>(cookie),
        net::cookie_util::SimulatedCookieSource(cookie, source_scheme), options,
        callback.MakeCallback());
    callback.WaitUntilDone();
    return callback.result().status.IsInclude();
  }

  std::string DumpAllCookies() {
    std::string result = "Cookies in store:\n";
    std::vector<net::CanonicalCookie> cookies =
        service_wrapper()->GetAllCookies();
    for (const auto& cookie : cookies) {
      result += "\t";
      result += cookie.DebugString();
      result += "\n";
    }
    return result;
  }

  ContentSettingPatternSource CreateDefaultSetting(ContentSetting setting) {
    return ContentSettingPatternSource(
        ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
        base::Value(setting), content_settings::ProviderType::kNone, false);
  }

  ContentSettingPatternSource CreateSetting(ContentSetting setting,
                                            const std::string& url_str) {
    const GURL url(url_str);
    EXPECT_TRUE(url.is_valid());
    return ContentSettingPatternSource(
        ContentSettingsPattern::FromURL(url),
        ContentSettingsPattern::Wildcard(), base::Value(setting),
        content_settings::ProviderType::kNone, false);
  }

  void SetContentSettings(ContentSettingsForOneType settings) {
    base::RunLoop runloop;
    cookie_service_client()->SetContentSettings(
        ContentSettingsType::COOKIES, settings, runloop.QuitClosure());
    runloop.Run();
  }

  net::CookieStore* cookie_store() {
    return url_request_context_->cookie_store();
  }

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
      // |cookie_store()|.
      cookie_service_ = nullptr;
      net::NoResultCookieCallback callback;
      cookie_store()->FlushStore(callback.MakeCallback());
      callback.WaitUntilDone();
    }
    // Reset |cookie_service_remote_| to allow re-initialize with params
    // for FlushableCookieManagerTest and SessionCleanupCookieManagerTest.
    service_wrapper_.reset();
    cookie_service_remote_.reset();

    connection_error_seen_ = false;
    auto cookie_monster = std::make_unique<net::CookieMonster>(
        std::move(store), nullptr /* netlog */);
    auto context_builder = net::CreateTestURLRequestContextBuilder();
    context_builder->SetCookieStore(std::move(cookie_monster));
    url_request_context_ = context_builder->Build();
    cookie_service_ = std::make_unique<CookieManager>(
        url_request_context_.get(),
        /*first_party_sets_access_delegate=*/nullptr, std::move(cleanup_store),
        /*params=*/nullptr,
        /*tpcd_metadata_manager=*/nullptr);
    cookie_service_->AddReceiver(
        cookie_service_remote_.BindNewPipeAndPassReceiver());
    service_wrapper_ = std::make_unique<SynchronousCookieManager>(
        cookie_service_remote_.get());
    cookie_service_remote_.set_disconnect_handler(base::BindOnce(
        &CookieManagerTest::OnConnectionError, base::Unretained(this)));
  }

  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  void OnConnectionError() { connection_error_seen_ = true; }

  bool connection_error_seen_;

  std::unique_ptr<net::URLRequestContext> url_request_context_;
  std::unique_ptr<CookieManager> cookie_service_;
  mojo::Remote<mojom::CookieManager> cookie_service_remote_;
  std::unique_ptr<SynchronousCookieManager> service_wrapper_;
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
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A", "B", kCookieDomain, "/", base::Time(), base::Time(),
          base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "C", "D", "foo_host2", "/with/path", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "Secure", "E", kCookieDomain, "/with/path", base::Time(),
          base::Time(), base::Time(), base::Time(), /*secure=*/true,
          /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "HttpOnly", "F", kCookieDomain, "/with/path", base::Time(),
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
  EXPECT_LE(before_creation, cookies[0].CreationDate());
  EXPECT_LE(cookies[0].CreationDate(), after_creation);
  EXPECT_EQ(cookies[0].LastAccessDate(), base::Time());
  EXPECT_EQ(cookies[0].ExpiryDate(), base::Time());
  EXPECT_FALSE(cookies[0].IsPersistent());
  EXPECT_FALSE(cookies[0].SecureAttribute());
  EXPECT_FALSE(cookies[0].IsHttpOnly());
  EXPECT_EQ(net::CookieSameSite::LAX_MODE, cookies[0].SameSite());
  EXPECT_EQ(net::COOKIE_PRIORITY_MEDIUM, cookies[0].Priority());

  EXPECT_EQ("C", cookies[1].Name());
  EXPECT_EQ("D", cookies[1].Value());
  EXPECT_EQ("foo_host2", cookies[1].Domain());
  EXPECT_EQ("/with/path", cookies[1].Path());
  EXPECT_LE(before_creation, cookies[1].CreationDate());
  EXPECT_LE(cookies[1].CreationDate(), after_creation);
  EXPECT_EQ(cookies[1].LastAccessDate(), base::Time());
  EXPECT_EQ(cookies[1].ExpiryDate(), base::Time());
  EXPECT_FALSE(cookies[1].IsPersistent());
  EXPECT_FALSE(cookies[1].SecureAttribute());
  EXPECT_FALSE(cookies[1].IsHttpOnly());
  EXPECT_EQ(net::CookieSameSite::LAX_MODE, cookies[1].SameSite());
  EXPECT_EQ(net::COOKIE_PRIORITY_MEDIUM, cookies[1].Priority());

  EXPECT_EQ("HttpOnly", cookies[2].Name());
  EXPECT_EQ("F", cookies[2].Value());
  EXPECT_EQ(kCookieDomain, cookies[2].Domain());
  EXPECT_EQ("/with/path", cookies[2].Path());
  EXPECT_LE(before_creation, cookies[2].CreationDate());
  EXPECT_LE(cookies[2].CreationDate(), after_creation);
  EXPECT_EQ(cookies[2].LastAccessDate(), base::Time());
  EXPECT_EQ(cookies[2].ExpiryDate(), base::Time());
  EXPECT_FALSE(cookies[2].IsPersistent());
  EXPECT_FALSE(cookies[2].SecureAttribute());
  EXPECT_TRUE(cookies[2].IsHttpOnly());
  EXPECT_EQ(net::CookieSameSite::LAX_MODE, cookies[2].SameSite());
  EXPECT_EQ(net::COOKIE_PRIORITY_MEDIUM, cookies[2].Priority());

  EXPECT_EQ("Secure", cookies[3].Name());
  EXPECT_EQ("E", cookies[3].Value());
  EXPECT_EQ(kCookieDomain, cookies[3].Domain());
  EXPECT_EQ("/with/path", cookies[3].Path());
  EXPECT_LE(before_creation, cookies[3].CreationDate());
  EXPECT_LE(cookies[3].CreationDate(), after_creation);
  EXPECT_EQ(cookies[3].LastAccessDate(), base::Time());
  EXPECT_EQ(cookies[3].ExpiryDate(), base::Time());
  EXPECT_FALSE(cookies[3].IsPersistent());
  EXPECT_TRUE(cookies[3].SecureAttribute());
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
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A", "B", "domain1.test",
          "/this/path/is/the/longest/for/sorting/purposes", base::Time(),
          base::Time(), base::Time(), base::Time(),
          /*secure=*/true, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  // LEGACY
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "C", "D", "domain2.test", "/with/longer/path", base::Time(),
          base::Time(), base::Time(), base::Time(),
          /*secure=*/true, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  // not set (UNKNOWN)
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "HttpOnly", "F", "domain3.test", "/with/path", base::Time(),
          base::Time(), base::Time(), base::Time(), /*secure=*/true,
          /*httponly=*/true, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  // NONLEGACY
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "Secure", "E", ".domainwithdot.test", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/true,
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
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A", "B", kCookieDomain, "/", base::Time(), base::Time(),
          base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "C", "D", "foo_host2", "/with/path", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "Secure", "E", kCookieDomain, "/with/path", base::Time(),
          base::Time(), base::Time(), base::Time(),
          /*secure=*/true,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "HttpOnly", "F", kCookieDomain, "/with/path", base::Time(),
          base::Time(), base::Time(), base::Time(),
          /*secure=*/false,
          /*httponly=*/true, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  // Want the SameSite=lax cookies, but not httponly ones.
  net::CookieOptions options;
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());
  std::vector<net::CanonicalCookie> cookies = service_wrapper()->GetCookieList(
      GURL("https://foo_host.com/with/path"), options,
      net::CookiePartitionKeyCollection());

  ASSERT_EQ(2u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);

  EXPECT_EQ("A", cookies[0].Name());
  EXPECT_EQ("B", cookies[0].Value());

  EXPECT_EQ("Secure", cookies[1].Name());
  EXPECT_EQ("E", cookies[1].Value());

  net::CookieOptions excluded_options = options;
  excluded_options.set_return_excluded_cookies();
  net::CookieAccessResultList excluded_cookies =
      service_wrapper()->GetExcludedCookieList(
          GURL("https://foo_host.com/with/path"), excluded_options);

  ASSERT_EQ(1u, excluded_cookies.size());

  EXPECT_EQ("HttpOnly", excluded_cookies[0].cookie.Name());
  EXPECT_EQ("F", excluded_cookies[0].cookie.Value());
  EXPECT_TRUE(excluded_cookies[0]
                  .access_result.status.HasExactlyExclusionReasonsForTesting(
                      {net::CookieInclusionStatus::EXCLUDE_HTTP_ONLY}));
}

TEST_F(CookieManagerTest, GetCookieListHttpOnly) {
  // Create an httponly and a non-httponly cookie.
  bool result;
  result = SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A", "B", kCookieDomain, "/", base::Time(), base::Time(),
          base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/true, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true);
  ASSERT_TRUE(result);
  result = SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "C", "D", kCookieDomain, "/", base::Time(), base::Time(),
          base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true);
  ASSERT_TRUE(result);

  // Retrieve without httponly cookies (default)
  net::CookieOptions options;
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());

  EXPECT_TRUE(options.exclude_httponly());
  std::vector<net::CanonicalCookie> cookies = service_wrapper()->GetCookieList(
      GURL("https://foo_host.com/with/path"), options,
      net::CookiePartitionKeyCollection());
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ("C", cookies[0].Name());

  options.set_return_excluded_cookies();

  net::CookieAccessResultList excluded_cookies =
      service_wrapper()->GetExcludedCookieList(
          GURL("https://foo_host.com/with/path"), options);
  ASSERT_EQ(1u, excluded_cookies.size());
  EXPECT_EQ("A", excluded_cookies[0].cookie.Name());

  // Retrieve with httponly cookies.
  options.set_include_httponly();
  cookies = service_wrapper()->GetCookieList(
      GURL("https://foo_host.com/with/path"), options,
      net::CookiePartitionKeyCollection());
  ASSERT_EQ(2u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);

  EXPECT_EQ("A", cookies[0].Name());
  EXPECT_EQ("C", cookies[1].Name());
}

TEST_F(CookieManagerTest, GetCookieListSameSite) {
  // Create an unrestricted, a lax, and a strict cookie.
  bool result;
  result = SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A", "B", kCookieDomain, "/", base::Time(), base::Time(),
          base::Time(), base::Time(),
          /*secure=*/true, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      "https", true);
  ASSERT_TRUE(result);
  result = SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "C", "D", kCookieDomain, "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true);
  ASSERT_TRUE(result);
  result = SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "E", "F", kCookieDomain, "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::STRICT_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true);
  ASSERT_TRUE(result);

  // Retrieve only unrestricted cookies.
  net::CookieOptions options;
  EXPECT_EQ(
      net::CookieOptions::SameSiteCookieContext(
          net::CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
      options.same_site_cookie_context());
  std::vector<net::CanonicalCookie> cookies = service_wrapper()->GetCookieList(
      GURL("https://foo_host.com/with/path"), options,
      net::CookiePartitionKeyCollection());
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ("A", cookies[0].Name());

  options.set_return_excluded_cookies();

  net::CookieAccessResultList excluded_cookies =
      service_wrapper()->GetExcludedCookieList(
          GURL("https://foo_host.com/with/path"), options);
  ASSERT_EQ(2u, excluded_cookies.size());

  // Retrieve unrestricted and lax cookies.
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext(
          net::CookieOptions::SameSiteCookieContext::ContextType::
              SAME_SITE_LAX));
  cookies = service_wrapper()->GetCookieList(
      GURL("https://foo_host.com/with/path"), options,
      net::CookiePartitionKeyCollection());
  ASSERT_EQ(2u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);
  EXPECT_EQ("A", cookies[0].Name());
  EXPECT_EQ("C", cookies[1].Name());

  excluded_cookies = service_wrapper()->GetExcludedCookieList(
      GURL("https://foo_host.com/with/path"), options);
  ASSERT_EQ(1u, excluded_cookies.size());

  // Retrieve everything.
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());
  cookies = service_wrapper()->GetCookieList(
      GURL("https://foo_host.com/with/path"), options,
      net::CookiePartitionKeyCollection());
  ASSERT_EQ(3u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);
  EXPECT_EQ("A", cookies[0].Name());
  EXPECT_EQ("C", cookies[1].Name());
  EXPECT_EQ("E", cookies[2].Name());

  excluded_cookies = service_wrapper()->GetExcludedCookieList(
      GURL("https://foo_host.com/with/path"), options);
  ASSERT_EQ(0u, excluded_cookies.size());
}

TEST_F(CookieManagerTest, GetCookieListCookiePartitionKeyCollection) {
  // Add unpartitioned cookie.
  ASSERT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "__Host-A", "1", kCookieDomain, "/", base::Time(), base::Time(),
          base::Time(), base::Time(),
          /*secure=*/true, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM,
          /*partition_key=*/std::nullopt),
      "https", true));
  // Add partitioned cookies.
  ASSERT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "__Host-B", "2", kCookieDomain, "/", base::Time(), base::Time(),
          base::Time(), base::Time(),
          /*secure=*/true, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM,
          net::CookiePartitionKey::FromURLForTesting(
              GURL("https://www.a.com"))),
      "https", true));
  ASSERT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "__Host-C", "3", kCookieDomain, "/", base::Time(), base::Time(),
          base::Time(), base::Time(),
          /*secure=*/true, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM,
          net::CookiePartitionKey::FromURLForTesting(
              GURL("https://www.b.com"))),
      "https", true));
  ASSERT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "__Host-D", "4", kCookieDomain, "/", base::Time(), base::Time(),
          base::Time(), base::Time(),
          /*secure=*/true, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM,
          net::CookiePartitionKey::FromURLForTesting(
              GURL("https://www.c.com"))),
      "https", true));
  // Set a partitioned cookie that has the same filed value as the unpartitioned
  // cookie, except for the partition_key field.
  ASSERT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "__Host-A", "2", kCookieDomain, "/", base::Time(), base::Time(),
          base::Time(), base::Time(),
          /*secure=*/true, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM,
          net::CookiePartitionKey::FromURLForTesting(GURL(kCookieHttpsURL))),
      "https", true));

  // Test empty key_collection.
  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetCookieList(GURL("https://foo_host.com/with/path"),
                                       net::CookieOptions::MakeAllInclusive(),
                                       net::CookiePartitionKeyCollection());
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ("__Host-A", cookies[0].Name());

  // Test key_collection with single partition key.
  cookies = service_wrapper()->GetCookieList(
      GURL("https://foo_host.com/with/path"),
      net::CookieOptions::MakeAllInclusive(),
      net::CookiePartitionKeyCollection(
          net::CookiePartitionKey::FromURLForTesting(
              GURL("https://subdomain.a.com"))));
  ASSERT_EQ(2u, cookies.size());
  EXPECT_EQ("__Host-A", cookies[0].Name());
  EXPECT_EQ("__Host-B", cookies[1].Name());

  // Test key_collection with multiple partition keys.
  cookies = service_wrapper()->GetCookieList(
      GURL("https://foo_host.com/with/path"),
      net::CookieOptions::MakeAllInclusive(),
      net::CookiePartitionKeyCollection({
          net::CookiePartitionKey::FromURLForTesting(
              GURL("https://subdomain.a.com")),
          net::CookiePartitionKey::FromURLForTesting(
              GURL("https://subdomain.b.com")),
      }));
  ASSERT_EQ(3u, cookies.size());
  EXPECT_EQ("__Host-A", cookies[0].Name());
  EXPECT_EQ("__Host-B", cookies[1].Name());
  EXPECT_EQ("__Host-C", cookies[2].Name());

  // Test key_collection with all partition keys.
  cookies = service_wrapper()->GetCookieList(
      GURL("https://foo_host.com/with/path"),
      net::CookieOptions::MakeAllInclusive(),
      net::CookiePartitionKeyCollection::ContainsAll());
  ASSERT_EQ(5u, cookies.size());
  EXPECT_EQ("__Host-A", cookies[0].Name());
  EXPECT_EQ("__Host-B", cookies[1].Name());
  EXPECT_EQ("__Host-C", cookies[2].Name());
  EXPECT_EQ("__Host-D", cookies[3].Name());
  EXPECT_EQ("__Host-A", cookies[4].Name());

  // Test key_collection with single partition key that's same-site to the
  // cookie domain.
  cookies = service_wrapper()->GetCookieList(
      GURL("https://foo_host.com/with/path"),
      net::CookieOptions::MakeAllInclusive(),
      net::CookiePartitionKeyCollection(
          net::CookiePartitionKey::FromURLForTesting(
              GURL("https://foo_host.com"))));
  ASSERT_EQ(2u, cookies.size());
  EXPECT_EQ("__Host-A", cookies[0].Name());
  EXPECT_EQ("__Host-A", cookies[1].Name());
  EXPECT_EQ(
      net::CookiePartitionKey::FromURLForTesting(GURL("https://foo_host.com")),
      cookies[1].PartitionKey());
}

TEST_F(CookieManagerTest, GetCookieListAccessTime) {
  bool result = SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A", "B", kCookieDomain, "/", base::Time(), base::Time(),
          base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true);
  ASSERT_TRUE(result);

  // Get the cookie without updating the access time and check
  // the access time is null.
  net::CookieOptions options;
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());

  options.set_do_not_update_access_time();
  std::vector<net::CanonicalCookie> cookies = service_wrapper()->GetCookieList(
      GURL("https://foo_host.com/with/path"), options,
      net::CookiePartitionKeyCollection());
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ("A", cookies[0].Name());
  EXPECT_TRUE(cookies[0].LastAccessDate().is_null());

  // Get the cookie updating the access time and check
  // that it's a valid value.
  base::Time start(base::Time::Now());
  options.set_update_access_time();
  cookies = service_wrapper()->GetCookieList(
      GURL("https://foo_host.com/with/path"), options,
      net::CookiePartitionKeyCollection());
  ASSERT_EQ(1u, cookies.size());
  EXPECT_EQ("A", cookies[0].Name());
  EXPECT_FALSE(cookies[0].LastAccessDate().is_null());
  EXPECT_GE(cookies[0].LastAccessDate(), start);
  EXPECT_LE(cookies[0].LastAccessDate(), base::Time::Now());
}

TEST_F(CookieManagerTest, DeleteCanonicalCookie) {
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A", "B", "foo_host", "/", base::Time(), base::Time(), base::Time(),
          base::Time(),
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
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A", "B", kCookieDomain, "/", base::Time(), base::Time(),
          base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "C", "D", "foo_host2", "/with/path", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "Secure", "E", kCookieDomain, "/with/path", base::Time(),
          base::Time(), base::Time(), base::Time(),
          /*secure=*/true,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "HttpOnly", "F", kCookieDomain, "/with/path", base::Time(),
          base::Time(), base::Time(), base::Time(),
          /*secure=*/false,
          /*httponly=*/true, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  base::Time yesterday = base::Time::Now() - base::Days(1);
  EXPECT_TRUE(service_wrapper()->SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A", "E", kCookieDomain, "/", base::Time(), yesterday, base::Time(),
          base::Time(),
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
  net::CookieAccessResult access_result =
      service_wrapper()->SetCanonicalCookieWithAccessResult(
          *net::CanonicalCookie::CreateUnsafeCookieForTesting(
              "N", "O", kCookieDomain, "/", base::Time(), base::Time(),
              base::Time(), base::Time(),
              /*secure=*/true, /*httponly=*/false,
              net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
          "http", false);

  EXPECT_TRUE(access_result.status.HasExactlyExclusionReasonsForTesting(
      {net::CookieInclusionStatus::EXCLUDE_SECURE_ONLY}));
  EXPECT_EQ(access_result.effective_same_site,
            net::CookieEffectiveSameSite::NO_RESTRICTION);
  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();

  ASSERT_EQ(0u, cookies.size());
}

// Test CookieAccessDelegateImpl functionality for allowing Secure cookie access
// from potentially trustworthy origins, even if non-cryptographic.
TEST_F(CookieManagerTest, SecureCookieNonCryptographicPotentiallyTrustworthy) {
  GURL http_localhost_url("http://localhost/path");
  auto http_localhost_cookie = net::CanonicalCookie::CreateForTesting(
      http_localhost_url, "http_localhost=1; Secure", base::Time::Now());

  // Secure cookie can be set from non-cryptographic localhost URL.
  EXPECT_TRUE(service_wrapper()
                  ->SetCanonicalCookieFromUrlWithStatus(
                      *http_localhost_cookie, http_localhost_url,
                      false /* modify_http_only */)
                  .IsInclude());
  // And can be retrieved from such.
  std::vector<net::CanonicalCookie> http_localhost_cookies =
      service_wrapper()->GetCookieList(http_localhost_url,
                                       net::CookieOptions::MakeAllInclusive(),
                                       net::CookiePartitionKeyCollection());
  ASSERT_EQ(1u, http_localhost_cookies.size());
  EXPECT_EQ("http_localhost", http_localhost_cookies[0].Name());
  EXPECT_EQ(net::CookieSourceScheme::kSecure,
            http_localhost_cookies[0].SourceScheme());
  EXPECT_TRUE(http_localhost_cookies[0].SecureAttribute());

  GURL http_other_url("http://other.test/path");
  auto http_other_cookie = net::CanonicalCookie::CreateForTesting(
      http_other_url, "http_other=1; Secure", base::Time::Now());

  // Secure cookie cannot be set from another non-cryptographic URL if there is
  // no CookieAccessDelegate.
  EXPECT_TRUE(
      service_wrapper()
          ->SetCanonicalCookieFromUrlWithStatus(
              *http_other_cookie, http_other_url, false /* modify_http_only */)
          .HasExactlyExclusionReasonsForTesting(
              {net::CookieInclusionStatus::EXCLUDE_SECURE_ONLY}));

  // Set a CookieAccessDelegateImpl which allows other origins registered
  // as trustworthy to set a Secure cookie.
  auto delegate = std::make_unique<CookieAccessDelegateImpl>(
      mojom::CookieAccessDelegateType::ALWAYS_LEGACY, nullptr);
  cookie_store()->SetCookieAccessDelegate(std::move(delegate));
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(
      switches::kUnsafelyTreatInsecureOriginAsSecure, "http://other.test");
  SecureOriginAllowlist::GetInstance().ResetForTesting();
  ASSERT_TRUE(IsUrlPotentiallyTrustworthy(http_other_url));

  // Secure cookie can be set from non-cryptographic but potentially trustworthy
  // origin, if CookieAccessDelegate allows it.
  EXPECT_TRUE(
      service_wrapper()
          ->SetCanonicalCookieFromUrlWithStatus(
              *http_other_cookie, http_other_url, false /* modify_http_only */)
          .IsInclude());
  // And can be retrieved from such.
  std::vector<net::CanonicalCookie> http_other_cookies =
      service_wrapper()->GetCookieList(http_other_url,
                                       net::CookieOptions::MakeAllInclusive(),
                                       net::CookiePartitionKeyCollection());
  ASSERT_EQ(1u, http_other_cookies.size());
  EXPECT_EQ("http_other", http_other_cookies[0].Name());
  EXPECT_EQ(net::CookieSourceScheme::kSecure,
            http_other_cookies[0].SourceScheme());
  EXPECT_TRUE(http_other_cookies[0].SecureAttribute());
}

TEST_F(CookieManagerTest, ConfirmHttpOnlySetFails) {
  net::CookieAccessResult access_result =
      service_wrapper()->SetCanonicalCookieWithAccessResult(
          *net::CanonicalCookie::CreateUnsafeCookieForTesting(
              "N", "O", kCookieDomain, "/", base::Time(), base::Time(),
              base::Time(), base::Time(),
              /*secure=*/false, /*httponly=*/true,
              net::CookieSameSite::LAX_MODE, net::COOKIE_PRIORITY_MEDIUM),
          "http", false);

  EXPECT_TRUE(access_result.status.HasExactlyExclusionReasonsForTesting(
      {net::CookieInclusionStatus::EXCLUDE_HTTP_ONLY}));
  EXPECT_EQ(access_result.effective_same_site,
            net::CookieEffectiveSameSite::LAX_MODE);
  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();

  ASSERT_EQ(0u, cookies.size());
}

TEST_F(CookieManagerTest, ConfirmSecureOverwriteFails) {
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "Secure", "F", kCookieDomain, "/with/path", base::Time(),
          base::Time(), base::Time(), base::Time(),
          /*secure=*/true,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  net::CookieAccessResult access_result =
      service_wrapper()->SetCanonicalCookieWithAccessResult(
          *net::CanonicalCookie::CreateUnsafeCookieForTesting(
              "Secure", "Nope", kCookieDomain, "/with/path", base::Time(),
              base::Time(), base::Time(), base::Time(), /*secure=*/false,
              /*httponly=*/false, net::CookieSameSite::LAX_MODE,
              net::COOKIE_PRIORITY_MEDIUM),
          "http", false);

  EXPECT_TRUE(access_result.status.HasExactlyExclusionReasonsForTesting(
      {net::CookieInclusionStatus::EXCLUDE_OVERWRITE_SECURE}));
  EXPECT_EQ(access_result.effective_same_site,
            net::CookieEffectiveSameSite::LAX_MODE);

  std::vector<net::CanonicalCookie> cookies =
      service_wrapper()->GetAllCookies();
  ASSERT_EQ(1u, cookies.size());
  std::sort(cookies.begin(), cookies.end(), &CompareCanonicalCookies);
  EXPECT_EQ("Secure", cookies[0].Name());
  EXPECT_EQ("F", cookies[0].Value());
}

TEST_F(CookieManagerTest, ConfirmHttpOnlyOverwriteFails) {
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "HttpOnly", "F", kCookieDomain, "/with/path", base::Time(),
          base::Time(), base::Time(), base::Time(),
          /*secure=*/false,
          /*httponly=*/true, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "http", true));

  net::CookieAccessResult access_result =
      service_wrapper()->SetCanonicalCookieWithAccessResult(
          *net::CanonicalCookie::CreateUnsafeCookieForTesting(
              "HttpOnly", "Nope", kCookieDomain, "/with/path", base::Time(),
              base::Time(), base::Time(), base::Time(), /*secure=*/false,
              /*httponly=*/false, net::CookieSameSite::LAX_MODE,
              net::COOKIE_PRIORITY_MEDIUM),
          "https", false);

  EXPECT_TRUE(access_result.status.HasExactlyExclusionReasonsForTesting(
      {net::CookieInclusionStatus::EXCLUDE_OVERWRITE_HTTP_ONLY}));
  EXPECT_EQ(access_result.effective_same_site,
            net::CookieEffectiveSameSite::LAX_MODE);

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
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A", "B", kCookieDomain, "/", base::Time(), base::Time(),
          base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "C", "D", "foo_host2", "/with/path", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "Secure", "E", kCookieDomain, "/with/path", base::Time(),
          base::Time(), base::Time(), base::Time(), /*secure=*/true,
          /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "HttpOnly", "F", kCookieDomain, "/with/path", base::Time(),
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
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A1", "val", kCookieDomain, "/", now - base::Minutes(60),
          base::Time(), base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A2", "val", kCookieDomain, "/", now - base::Minutes(120),
          base::Time(), base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A3", "val", kCookieDomain, "/", now - base::Minutes(180),
          base::Time(), base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  mojom::CookieDeletionFilter filter;
  filter.created_after_time = now - base::Minutes(150);
  filter.created_before_time = now - base::Minutes(90);
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
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A1", "val", "foo_host1", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A2", "val", "foo_host2", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A3", "val", "foo_host3", "/", base::Time(), base::Time(),
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
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A1", "val", "foo_host1", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A2", "val", "foo_host2", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A3", "val", "foo_host3", "/", base::Time(), base::Time(),
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

TEST_F(CookieManagerTest, DeleteByEmptyIncludingDomains) {
  // Create one cookies and keep it.
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A1", "val", "foo_host1", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  ASSERT_EQ(1u, service_wrapper()->GetAllCookies().size());

  mojom::CookieDeletionFilter filter;
  filter.including_domains = std::vector<std::string>();
  EXPECT_EQ(0u, service_wrapper()->DeleteCookies(filter));
  ASSERT_EQ(1u, service_wrapper()->GetAllCookies().size());
}

TEST_F(CookieManagerTest, DeleteByEmptyExcludingDomains) {
  // Create one cookies and delete it.
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A1", "val", "foo_host1", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  ASSERT_EQ(1u, service_wrapper()->GetAllCookies().size());

  mojom::CookieDeletionFilter filter;
  filter.excluding_domains = std::vector<std::string>();
  EXPECT_EQ(1u, service_wrapper()->DeleteCookies(filter));
  ASSERT_EQ(0u, service_wrapper()->GetAllCookies().size());
}

// Confirm deletion is based on eTLD+1
TEST_F(CookieManagerTest, DeleteDetails_eTLD) {
  // Two domains on different levels of the same eTLD both get deleted.
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A1", "val", "example.com", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A2", "val", "www.example.com", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A3", "val", "www.nonexample.com", "/", base::Time(), base::Time(),
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
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A1", "val", "example.co.uk", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A2", "val", "www.example.co.uk", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A3", "val", "www.nonexample.co.uk", "/", base::Time(), base::Time(),
          base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
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
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A1", "val", "example.co.uk", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A2", "val", "www.example.co.uk", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A3", "val", "www.nonexample.co.uk", "/", base::Time(), base::Time(),
          base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
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
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A1", "val", "foo_host.com", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A2", "val", ".foo_host.com", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A3", "val", "bar.host.com", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A4", "val", ".bar.host.com", "/", base::Time(), base::Time(),
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
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A1", "val", "random.co.uk", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A2", "val", "sub.domain.random.co.uk", "/", base::Time(),
          base::Time(), base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A3", "val", "random.com", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A4", "val", "random", "/", base::Time(), base::Time(), base::Time(),
          base::Time(),
          /*secure=*/false, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A5", "val", "normal.co.uk", "/", base::Time(), base::Time(),
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
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A1", "val", "privatedomain", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          // Will not actually be treated as a private domain as it's under
          // .com.
          "A2", "val", "privatedomain.com", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false, /*httponly=*/false,
          net::CookieSameSite::LAX_MODE, net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          // Will not actually be treated as a private domain as it's two
          // level
          "A3", "val", "subdomain.privatedomain", "/", base::Time(),
          base::Time(), base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
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
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A01", "RandomValue", "example.com", "/", base::Time(), base::Time(),
          base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A02", "RandomValue", "canonical.com", "/", base::Time(),
          base::Time(), base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  // Path
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A03", "val", "example.com", "/this/is/a/long/path", base::Time(),
          base::Time(), base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A04", "val", "canonical.com", "/this/is/a/long/path", base::Time(),
          base::Time(), base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  // Last_access
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A05", "val", "example.com", "/", base::Time::Now() - base::Days(3),
          base::Time(), base::Time::Now() - base::Days(3), base::Time(),
          /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A06", "val", "canonical.com", "/", base::Time::Now() - base::Days(3),
          base::Time(), base::Time::Now() - base::Days(3), base::Time(),
          /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  // Same_site
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A07", "val", "example.com", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::STRICT_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A08", "val", "canonical.com", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::STRICT_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  // Priority
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A09", "val", "example.com", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_HIGH),
      "https", true));
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A10", "val", "canonical.com", "/", base::Time(), base::Time(),
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
  test_filter.including_domains->insert(test_filter.including_domains->end(),
                                        std::begin(filter_domains),
                                        std::end(filter_domains));

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
  for (int i = 0; i < static_cast<int>(std::size(test_cases)); ++i) {
    TestCase& test_case(test_cases[i]);

    // Clear store.
    service_wrapper()->DeleteCookies(clear_filter);

    // IP addresses and internal hostnames can only be host cookies.
    bool exclude_domain_cookie =
        (GURL("http://" + test_case.domain).HostIsIPAddress() ||
         test_case.domain.find(".") == std::string::npos);

    // Standard cookie
    EXPECT_TRUE(SetCanonicalCookie(
        *net::CanonicalCookie::CreateUnsafeCookieForTesting(
            "A1", "val", test_cases[i].domain, test_cases[i].path, base::Time(),
            base::Time(), base::Time(), base::Time(), /*secure=*/false,
            /*httponly=*/false, net::CookieSameSite::LAX_MODE,
            net::COOKIE_PRIORITY_MEDIUM),
        "https", true));

    if (!exclude_domain_cookie) {
      // Host cookie
      EXPECT_TRUE(SetCanonicalCookie(
          *net::CanonicalCookie::CreateUnsafeCookieForTesting(
              "A2", "val", "." + test_cases[i].domain, test_cases[i].path,
              base::Time(), base::Time(), base::Time(), base::Time(),
              /*secure=*/false,
              /*httponly=*/false, net::CookieSameSite::LAX_MODE,
              net::COOKIE_PRIORITY_MEDIUM),
          "https", true));
    }

    // Httponly cookie
    EXPECT_TRUE(SetCanonicalCookie(
        *net::CanonicalCookie::CreateUnsafeCookieForTesting(
            "A3", "val", test_cases[i].domain, test_cases[i].path, base::Time(),
            base::Time(), base::Time(), base::Time(), /*secure=*/false,
            /*httponly=*/true, net::CookieSameSite::LAX_MODE,
            net::COOKIE_PRIORITY_MEDIUM),
        "https", true));

    // Httponly and secure cookie
    EXPECT_TRUE(SetCanonicalCookie(
        *net::CanonicalCookie::CreateUnsafeCookieForTesting(
            "A4", "val", test_cases[i].domain, test_cases[i].path, base::Time(),
            base::Time(), base::Time(), base::Time(), /*secure=*/false,
            /*httponly=*/true, net::CookieSameSite::LAX_MODE,
            net::COOKIE_PRIORITY_MEDIUM),
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
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A1", "val", kCookieDomain, "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true /*modify_httponly*/));

  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A1", "val", "bar_host", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true /*modify_httponly*/));

  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A2", "val", kCookieDomain, "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true /*modify_httponly*/));

  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A3", "val", "bar_host", "/", base::Time(), base::Time(),
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
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A01", "val", "www.example.com", "/path", base::Time(), base::Time(),
          base::Time(), base::Time(),
          /*secure=*/true, /*httponly=*/false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM),
      "https", true /*modify_httponly*/));

  // Cookie that should not be deleted because it's a host cookie in a
  // subdomain that doesn't exactly match the passed URL.
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A02", "val", "sub.www.example.com", "/path", base::Time(),
          base::Time(), base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true /*modify_httponly*/));

  // Cookie that shouldn't be deleted because the path doesn't match.
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A03", "val", "www.example.com", "/otherpath", base::Time(),
          base::Time(), base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true /*modify_httponly*/));

  // Cookie that shouldn't be deleted because the path is more specific
  // than the URL.
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A04", "val", "www.example.com", "/path/path2", base::Time(),
          base::Time(), base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true /*modify_httponly*/));

  // Cookie that shouldn't be deleted because it's at a host cookie domain that
  // doesn't exactly match the url's host.
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A05", "val", "example.com", "/path", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true /*modify_httponly*/));

  // Cookie that should not be deleted because it's not a host cookie and
  // has a domain that's more specific than the URL
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A06", "val", ".sub.www.example.com", "/path", base::Time(),
          base::Time(), base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true /*modify_httponly*/));

  // Cookie that should be deleted because it's not a host cookie and has a
  // domain that matches the URL
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A07", "val", ".www.example.com", "/path", base::Time(), base::Time(),
          base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true /*modify_httponly*/));

  // Cookie that should be deleted because it's not a host cookie and has a
  // domain that domain matches the URL.
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A08", "val", ".example.com", "/path", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true /*modify_httponly*/));

  // Cookie that should be deleted because it matches exactly.
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A09", "val", "www.example.com", "/path", base::Time(), base::Time(),
          base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true /*modify_httponly*/));

  // Cookie that should be deleted because it applies to a larger set
  // of paths than the URL path.
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A10", "val", "www.example.com", "/", base::Time(), base::Time(),
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
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A1", "val", kCookieDomain, "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A2", "val", kCookieDomain, "/", base::Time(), now + base::Days(1),
          base::Time(), base::Time(),
          /*secure=*/false, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A3", "val", kCookieDomain, "/", base::Time(), base::Time(),
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
  filter.created_after_time = now - base::Days(4);
  filter.created_before_time = now - base::Days(2);
  filter.including_domains = std::vector<std::string>();
  filter.including_domains->push_back("no.com");
  filter.including_domains->push_back("nope.com");
  filter.excluding_domains = std::vector<std::string>();
  filter.excluding_domains->push_back("no.com");
  filter.excluding_domains->push_back("yes.com");
  filter.url = GURL("http://nope.com/path");
  filter.session_control =
      mojom::CookieDeletionSessionControl::PERSISTENT_COOKIES;

  // Archetypal cookie:
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A1", "val0", "nope.com", "/path", now - base::Days(3),
          now + base::Days(3), base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  // Too old cookie.
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A2", "val1", "nope.com", "/path", now - base::Days(5),
          now + base::Days(3), base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  // Too young cookie.
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A3", "val2", "nope.com", "/path", now - base::Days(1),
          now + base::Days(3), base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  // Not in domains_and_ips_to_delete.
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A4", "val3", "other.com", "/path", now - base::Days(3),
          now + base::Days(3), base::Time(), base::Time(),
          /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  // In domains_and_ips_to_ignore.
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A5", "val4", "no.com", "/path", now - base::Days(3),
          now + base::Days(3), base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  // Doesn't match URL (by path).
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A6", "val6", "nope.com", "/otherpath", now - base::Days(3),
          now + base::Days(3), base::Time(), base::Time(),
          /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true));

  // Session
  EXPECT_TRUE(SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "A7", "val7", "nope.com", "/path", now - base::Days(3), base::Time(),
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
  explicit CookieChangeListener(
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
  raw_ptr<base::RunLoop> run_loop_;

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
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "DifferentName", "val", listener_url_host, "/", base::Time(),
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
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          listener_cookie_name, "val", "www.other.host", "/", base::Time(),
          base::Time(), base::Time(), base::Time(),
          /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, listener.observed_changes().size());

  // Insert a cookie that does match.
  service_wrapper()->SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          listener_cookie_name, "val", listener_url_host, "/", base::Time(),
          base::Time(), base::Time(), base::Time(),
          /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true);

  // Expect synchronous notifications.
  EXPECT_EQ(1u, listener.observed_changes().size());
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
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "Thing1", "val", kExampleHost, "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true);

  // Expect synchronous notifications.
  EXPECT_EQ(1u, listener.observed_changes().size());
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
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "Thing1", "val", kThisHost, "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true);
  service_wrapper()->SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "Thing2", "val", kThatHost, "/", base::Time(), base::Time(),
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
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          listener_cookie_name, "val", listener_url_host, "/", base::Time(),
          base::Time(), base::Time(), base::Time(),
          /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_MEDIUM),
      "https", true);

  EXPECT_EQ(1u, listener1->observed_changes().size());
  listener1->ClearObservedChanges();

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

  {
    // Can't outlive `new_remote`.
    SynchronousCookieManager new_wrapper(new_remote.get());

    // Set a cookie on the new interface and make sure it's visible on the
    // old one.
    EXPECT_TRUE(new_wrapper.SetCanonicalCookie(
        *net::CanonicalCookie::CreateUnsafeCookieForTesting(
            "X", "Y", "www.other.host", "/", base::Time(), base::Time(),
            base::Time(), base::Time(), /*secure=*/false,
            /*httponly=*/false, net::CookieSameSite::LAX_MODE,
            net::COOKIE_PRIORITY_MEDIUM),
        "https", true));

    std::vector<net::CanonicalCookie> cookies =
        service_wrapper()->GetCookieList(GURL("http://www.other.host/"),
                                         net::CookieOptions::MakeAllInclusive(),
                                         net::CookiePartitionKeyCollection());
    ASSERT_EQ(1u, cookies.size());
    EXPECT_EQ("X", cookies[0].Name());
    EXPECT_EQ("Y", cookies[0].Value());

    // After a synchronous round trip through the new client pointer, it
    // should be reflected in the bindings seen on the server.
    EXPECT_EQ(2u, service()->GetClientsBoundForTesting());
  }

  new_remote.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, service()->GetClientsBoundForTesting());
}

TEST_F(CookieManagerTest, BlockThirdPartyCookies) {
  const GURL kThisURL = GURL("http://www.this.com");
  const GURL kThatURL = GURL("http://www.that.com");
  const url::Origin kThisOrigin = url::Origin::Create(kThisURL);
  const net::SiteForCookies kThisSiteForCookies =
      net::SiteForCookies::FromOrigin(kThisOrigin);
  const net::SiteForCookies kThatSiteForCookies =
      net::SiteForCookies::FromUrl(kThatURL);
  EXPECT_TRUE(service()->cookie_settings().IsFullCookieAccessAllowed(
      kThisURL, kThatSiteForCookies, kThisOrigin,
      net::CookieSettingOverrides()));

  // Set block third party cookies to true, cookie should now be blocked.
  cookie_service_client()->BlockThirdPartyCookies(true);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(service()->cookie_settings().IsFullCookieAccessAllowed(
      kThisURL, kThatSiteForCookies, kThisOrigin,
      net::CookieSettingOverrides()));
  EXPECT_TRUE(service()->cookie_settings().IsFullCookieAccessAllowed(
      kThisURL, kThisSiteForCookies, kThisOrigin,
      net::CookieSettingOverrides()));

  // Set block third party cookies back to false, cookie should no longer be
  // blocked.
  cookie_service_client()->BlockThirdPartyCookies(false);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service()->cookie_settings().IsFullCookieAccessAllowed(
      kThisURL, kThatSiteForCookies, kThisOrigin,
      net::CookieSettingOverrides()));
}

// A test class having cookie store with a persistent backing store.
class FlushableCookieManagerTest : public CookieManagerTest {
 public:
  FlushableCookieManagerTest()
      : store_(base::MakeRefCounted<net::FlushablePersistentStore>()) {}

  void SetUp() override { InitializeCookieService(store_, nullptr); }

  ~FlushableCookieManagerTest() override = default;

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
  EXPECT_FALSE(delete_info.domains_and_ips_to_delete.has_value());
  EXPECT_FALSE(delete_info.domains_and_ips_to_ignore.has_value());
  EXPECT_FALSE(delete_info.value_for_testing.has_value());
  EXPECT_TRUE(delete_info.cookie_partition_key_collection.ContainsAllKeys());

  // Then test all with non-default values.
  const double kTestStartEpoch = 1000;
  const double kTestEndEpoch = 10000000;
  filter_ptr = mojom::CookieDeletionFilter::New();
  filter_ptr->created_after_time =
      base::Time::FromSecondsSinceUnixEpoch(kTestStartEpoch);
  filter_ptr->created_before_time =
      base::Time::FromSecondsSinceUnixEpoch(kTestEndEpoch);
  filter_ptr->cookie_name = "cookie-name";
  filter_ptr->host_name = "cookie-host";
  filter_ptr->including_domains =
      std::vector<std::string>({"first.com", "second.com", "third.com"});
  filter_ptr->excluding_domains =
      std::vector<std::string>({"ten.com", "twelve.com"});
  filter_ptr->url = GURL("https://www.example.com");
  filter_ptr->session_control =
      mojom::CookieDeletionSessionControl::PERSISTENT_COOKIES;
  filter_ptr->cookie_partition_key_collection =
      net::CookiePartitionKeyCollection(
          net::CookiePartitionKey::FromURLForTesting(
              GURL("https://www.foo.com")));
  filter_ptr->partitioned_state_only = true;

  delete_info = DeletionFilterToInfo(std::move(filter_ptr));
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(kTestStartEpoch),
            delete_info.creation_range.start());
  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(kTestEndEpoch),
            delete_info.creation_range.end());

  EXPECT_EQ(CookieDeletionInfo::SessionControl::PERSISTENT_COOKIES,
            delete_info.session_control);
  EXPECT_EQ("cookie-name", delete_info.name.value());
  EXPECT_EQ("cookie-host", delete_info.host.value());
  EXPECT_EQ(GURL("https://www.example.com"), delete_info.url.value());
  ASSERT_TRUE(delete_info.domains_and_ips_to_delete.has_value());
  EXPECT_EQ(3u, delete_info.domains_and_ips_to_delete->size());
  EXPECT_NE(delete_info.domains_and_ips_to_delete->find("first.com"),
            delete_info.domains_and_ips_to_delete->end());
  EXPECT_NE(delete_info.domains_and_ips_to_delete->find("second.com"),
            delete_info.domains_and_ips_to_delete->end());
  EXPECT_NE(delete_info.domains_and_ips_to_delete->find("third.com"),
            delete_info.domains_and_ips_to_delete->end());
  EXPECT_EQ(2u, delete_info.domains_and_ips_to_ignore->size());
  EXPECT_NE(delete_info.domains_and_ips_to_ignore->find("ten.com"),
            delete_info.domains_and_ips_to_ignore->end());
  EXPECT_NE(delete_info.domains_and_ips_to_ignore->find("twelve.com"),
            delete_info.domains_and_ips_to_ignore->end());
  EXPECT_FALSE(delete_info.value_for_testing.has_value());
  EXPECT_FALSE(delete_info.cookie_partition_key_collection.ContainsAllKeys());
  EXPECT_THAT(
      delete_info.cookie_partition_key_collection.PartitionKeys(),
      testing::UnorderedElementsAre(net::CookiePartitionKey::FromURLForTesting(
          GURL("https://www.foo.com"))));
  EXPECT_TRUE(delete_info.partitioned_state_only);
}

// A test class having cookie store with a persistent backing store. The cookie
// store can be destroyed and recreated by calling InitializeCookieService
// again.
class SessionCleanupCookieManagerTest : public CookieManagerTest {
 public:
  ~SessionCleanupCookieManagerTest() override = default;

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
        /*restore_old_session_cookies=*/true, /*crypto_delegate=*/nullptr,
        /*enable_exclusive_access=*/false);
    return base::MakeRefCounted<SessionCleanupCookieStore>(sqlite_store.get());
  }

  net::CanonicalCookie CreateCookie() { return CreateCookie(kCookieDomain); }

  net::CanonicalCookie CreateCookie(const std::string& domain) {
    base::Time t = base::Time::Now();
    return *net::CanonicalCookie::CreateUnsafeCookieForTesting(
        "A", "B", domain, "/", t, t + base::Days(1), base::Time(), base::Time(),
        /*secure=*/false, /*httponly=*/false, net::CookieSameSite::LAX_MODE,
        net::COOKIE_PRIORITY_MEDIUM);
  }

  bool CreateAndTryToClearStaleCookie(base::Time expiration,
                                      base::Time last_access,
                                      base::Time last_update) {
    EXPECT_TRUE(SetCanonicalCookie(
        *net::CanonicalCookie::CreateUnsafeCookieForTesting(
            "A", "B", kCookieDomain, "/",
            /*creation=*/base::Time::Now() - base::Days(100), expiration,
            last_access, last_update,
            /*secure=*/true, /*httponly=*/false,
            net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_MEDIUM,
            std::nullopt, net::CookieSourceScheme::kSecure),
        "https", false));
    EXPECT_EQ(1u, service_wrapper()->GetAllCookies().size());
    uint32_t cleared = service_wrapper()->DeleteStaleSessionOnlyCookies();
    uint32_t remaining = service_wrapper()->GetAllCookies().size();
    EXPECT_EQ(1u, cleared + remaining);
    return cleared == 1u;
  }

 private:
  const scoped_refptr<base::SequencedTaskRunner> background_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
};

TEST_F(SessionCleanupCookieManagerTest, PersistSessionCookies) {
  EXPECT_TRUE(SetCanonicalCookie(CreateCookie(), "https", true));

  EXPECT_EQ(1u, service_wrapper()->GetAllCookies().size());

  // Re-create the cookie store to make sure cookies are persisted.
  auto store = CreateCookieStore();
  InitializeCookieService(store, store);

  EXPECT_EQ(1u, service_wrapper()->GetAllCookies().size());
}

TEST_F(SessionCleanupCookieManagerTest, DeleteSessionCookiesOnShutdown) {
  EXPECT_TRUE(SetCanonicalCookie(CreateCookie(), "https", true));

  EXPECT_EQ(1u, service_wrapper()->GetAllCookies().size());

  SetContentSettings({CreateSetting(CONTENT_SETTING_SESSION_ONLY, kCookieURL)});

  auto store = CreateCookieStore();
  InitializeCookieService(store, store);

  EXPECT_EQ(0u, service_wrapper()->GetAllCookies().size());
}

TEST_F(SessionCleanupCookieManagerTest, SettingMustMatchDomainOnShutdown) {
  EXPECT_TRUE(SetCanonicalCookie(CreateCookie(), "https", true));

  EXPECT_EQ(1u, service_wrapper()->GetAllCookies().size());

  SetContentSettings(
      {CreateSetting(CONTENT_SETTING_SESSION_ONLY, "http://other.com")});

  auto store = CreateCookieStore();
  InitializeCookieService(store, store);

  EXPECT_EQ(1u, service_wrapper()->GetAllCookies().size());
}

TEST_F(SessionCleanupCookieManagerTest, DeleteSessionOnlyCookies) {
  EXPECT_TRUE(SetCanonicalCookie(CreateCookie(), "https", true));

  EXPECT_EQ(1u, service_wrapper()->GetAllCookies().size());

  SetContentSettings({CreateSetting(CONTENT_SETTING_SESSION_ONLY, kCookieURL)});

  EXPECT_EQ(1u, service_wrapper()->DeleteSessionOnlyCookies());
  EXPECT_EQ(0u, service_wrapper()->GetAllCookies().size());
}

TEST_F(SessionCleanupCookieManagerTest, SettingMustMatchDomain) {
  EXPECT_TRUE(SetCanonicalCookie(CreateCookie(), "https", true));

  EXPECT_EQ(1u, service_wrapper()->GetAllCookies().size());

  SetContentSettings(
      {CreateSetting(CONTENT_SETTING_SESSION_ONLY, "http://other.com")});

  EXPECT_EQ(0u, service_wrapper()->DeleteSessionOnlyCookies());
  EXPECT_EQ(1u, service_wrapper()->GetAllCookies().size());

  SetContentSettings({CreateSetting(CONTENT_SETTING_SESSION_ONLY, kCookieURL)});

  EXPECT_EQ(1u, service_wrapper()->DeleteSessionOnlyCookies());
  EXPECT_EQ(0u, service_wrapper()->GetAllCookies().size());
}

TEST_F(SessionCleanupCookieManagerTest, DeleteStaleSessionOnlyCookies) {
  base::Time now = base::Time::Now();
  base::Time eight_days_ago = now - base::Days(8);
  for (base::Time expiration : {base::Time(), now + base::Days(8)}) {
    for (base::Time last_access : {base::Time(), eight_days_ago, now}) {
      for (base::Time last_update : {base::Time(), eight_days_ago, now}) {
        EXPECT_EQ(CreateAndTryToClearStaleCookie(expiration, last_access,
                                                 last_update),
                  expiration.is_null() && last_access < now &&
                      last_update < now &&
                      !(last_access.is_null() && last_update.is_null()));
      }
    }
  }
}

TEST_F(SessionCleanupCookieManagerTest, MorePreciseSettingTakesPrecedence) {
  EXPECT_TRUE(SetCanonicalCookie(CreateCookie(), "https", true));

  EXPECT_EQ(1u, service_wrapper()->GetAllCookies().size());

  // If a rule with ALLOW is before a SESSION_ONLY rule, the cookie should not
  // be deleted.
  SetContentSettings(
      {ContentSettingPatternSource(
           ContentSettingsPattern::FromURLNoWildcard(GURL(kCookieURL)),
           ContentSettingsPattern::Wildcard(),
           base::Value(CONTENT_SETTING_SESSION_ONLY),
           content_settings::ProviderType::kNone, false),
       ContentSettingPatternSource(
           ContentSettingsPattern::FromURL(GURL(kCookieURL)),
           ContentSettingsPattern::Wildcard(),
           base::Value(CONTENT_SETTING_ALLOW),
           content_settings::ProviderType::kNone, false)});

  auto store = CreateCookieStore();
  InitializeCookieService(store, store);

  EXPECT_EQ(1u, service_wrapper()->GetAllCookies().size());
}

TEST_F(SessionCleanupCookieManagerTest, ForceKeepSessionState) {
  EXPECT_TRUE(SetCanonicalCookie(CreateCookie(), "https", true));

  EXPECT_EQ(1u, service_wrapper()->GetAllCookies().size());

  SetContentSettings({CreateSetting(CONTENT_SETTING_SESSION_ONLY, kCookieURL)});
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

  SetContentSettings({
      CreateSetting(CONTENT_SETTING_ALLOW, kCookieHttpsURL),
      CreateDefaultSetting(CONTENT_SETTING_SESSION_ONLY),
  });

  auto store = CreateCookieStore();
  InitializeCookieService(store, store);

  EXPECT_EQ(1u, service_wrapper()->GetAllCookies().size());
}

// Each call to SetStorageAccessGrantSettings should run the provided callback
// when complete.
TEST_F(CookieManagerTest, SetStorageAccessGrantSettingsRunsCallback) {
  service_wrapper()->SetStorageAccessGrantSettings();
  ASSERT_EQ(1U, service_wrapper()->callback_count());
}

}  // namespace
}  // namespace network
