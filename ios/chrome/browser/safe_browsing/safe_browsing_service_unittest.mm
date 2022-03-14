// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/safe_browsing/safe_browsing_service_impl.h"

#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/metadata.pb.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/db/v4_database.h"
#include "components/safe_browsing/core/browser/db/v4_get_hash_protocol_manager.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/v4_test_util.h"
#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/unified_consent/pref_names.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/prerender/fake_prerender_service.h"
#import "ios/chrome/browser/prerender/prerender_service_factory.h"
#import "ios/chrome/browser/safe_browsing/safe_browsing_metrics_collector_factory.h"
#import "ios/chrome/browser/safe_browsing/safe_browsing_query_manager.h"
#import "ios/chrome/browser/safe_browsing/safe_browsing_unsafe_resource_container.h"
#import "ios/chrome/browser/safe_browsing/verdict_cache_manager_factory.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kSafePage[] = "https://example.test/safe.html";
const char kMalwarePage[] = "https://unsafe.test/malware.html";

class TestUrlCheckerClient {
 public:
  TestUrlCheckerClient(SafeBrowsingService* safe_browsing_service,
                       web::BrowserState* browser_state)
      : safe_browsing_service_(safe_browsing_service) {
    SafeBrowsingQueryManager::CreateForWebState(&web_state_);
    SafeBrowsingUrlAllowList::CreateForWebState(&web_state_);
    SafeBrowsingUnsafeResourceContainer::CreateForWebState(&web_state_);
    web_state_.SetBrowserState(browser_state);
    PrerenderServiceFactory::GetInstance()->SetTestingFactory(
        browser_state,
        base::BindRepeating(
            [](web::BrowserState*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<FakePrerenderService>();
            }));
  }

  ~TestUrlCheckerClient() = default;

  TestUrlCheckerClient(const TestUrlCheckerClient&) = delete;
  TestUrlCheckerClient& operator=(const TestUrlCheckerClient&) = delete;

  bool url_is_unsafe() const { return url_is_unsafe_; }

  void CheckUrl(const GURL& url) {
    result_pending_ = true;
    url_checker_ = safe_browsing_service_->CreateUrlChecker(
        network::mojom::RequestDestination::kDocument, &web_state_);
    web::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&TestUrlCheckerClient::CheckUrlOnIOThread,
                                  base::Unretained(this), url));
  }

  void CheckSubFrameUrl(const GURL& url) {
    result_pending_ = true;
    url_checker_ = safe_browsing_service_->CreateUrlChecker(
        network::mojom::RequestDestination::kIframe, &web_state_);
    web::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&TestUrlCheckerClient::CheckUrlOnIOThread,
                                  base::Unretained(this), url));
  }

  bool result_pending() const { return result_pending_; }

  void WaitForResult() {
    while (result_pending()) {
      base::RunLoop().RunUntilIdle();
    }
  }

 private:
  void CheckUrlOnIOThread(const GURL& url) {
    url_checker_->CheckUrl(
        url, "GET",
        base::BindOnce(&TestUrlCheckerClient::OnCheckUrlResult,
                       base::Unretained(this)));
  }

  void OnCheckUrlResult(
      safe_browsing::SafeBrowsingUrlCheckerImpl::NativeUrlCheckNotifier*
          slow_check_notifier,
      bool proceed,
      bool showed_interstitial) {
    if (slow_check_notifier) {
      *slow_check_notifier =
          base::BindOnce(&TestUrlCheckerClient::OnCheckUrlResult,
                         base::Unretained(this), nullptr);
      return;
    }
    url_is_unsafe_ = !proceed;
    result_pending_ = false;
    url_checker_.reset();
  }

  void CheckDone() { result_pending_ = false; }

  bool result_pending_ = false;
  bool url_is_unsafe_ = false;
  SafeBrowsingService* safe_browsing_service_;
  web::FakeWebState web_state_;
  std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl> url_checker_;
};

}  // namespace

class SafeBrowsingServiceTest : public PlatformTest {
 public:
  SafeBrowsingServiceTest()
      : task_environment_(web::WebTaskEnvironment::IO_MAINLOOP),
        browser_state_(TestChromeBrowserState::Builder().Build()) {
    store_factory_ = new safe_browsing::TestV4StoreFactory();
    safe_browsing::V4Database::RegisterStoreFactoryForTest(
        base::WrapUnique(store_factory_));

    v4_db_factory_ = new safe_browsing::TestV4DatabaseFactory();
    safe_browsing::V4Database::RegisterDatabaseFactoryForTest(
        base::WrapUnique(v4_db_factory_));

    v4_get_hash_factory_ =
        new safe_browsing::TestV4GetHashProtocolManagerFactory();
    safe_browsing::V4GetHashProtocolManager::RegisterFactory(
        base::WrapUnique(v4_get_hash_factory_));

    safe_browsing_service_ = base::MakeRefCounted<SafeBrowsingServiceImpl>();

    CHECK(temp_dir_.CreateUniqueTempDir());
    safe_browsing::SafeBrowsingMetricsCollector*
        safe_browsing_metrics_collector =
            SafeBrowsingMetricsCollectorFactory::GetForBrowserState(
                browser_state_.get());
    safe_browsing_service_->Initialize(browser_state_->GetPrefs(),
                                       temp_dir_.GetPath(),
                                       safe_browsing_metrics_collector);
    base::RunLoop().RunUntilIdle();
  }

  SafeBrowsingServiceTest(const SafeBrowsingServiceTest&) = delete;
  SafeBrowsingServiceTest& operator=(const SafeBrowsingServiceTest&) = delete;

  ~SafeBrowsingServiceTest() override {
    safe_browsing_service_->ShutDown();

    safe_browsing::V4GetHashProtocolManager::RegisterFactory(nullptr);
    safe_browsing::V4Database::RegisterDatabaseFactoryForTest(nullptr);
    safe_browsing::V4Database::RegisterStoreFactoryForTest(nullptr);
  }

  void MarkUrlAsMalware(const GURL& bad_url) {
    web::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&SafeBrowsingServiceTest::MarkUrlAsMalwareOnIOThread,
                       base::Unretained(this), bad_url));
  }

  // Adds the given |safe_url| to the allowlist used by real-time checks.
  void MarkUrlAsRealTimeSafe(const GURL& safe_url) {
    web::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&SafeBrowsingServiceTest::MarkUrlAsSafeOnIOThread,
                       base::Unretained(this), safe_url));
  }

  // Caches the given |bad_url| as unsafe in the VerdictCacheManager used by
  // real-time checks.
  void MarkUrlAsRealTimeUnsafe(const GURL& bad_url) {
    safe_browsing::RTLookupResponse response;
    safe_browsing::RTLookupResponse::ThreatInfo* threat_info =
        response.add_threat_info();
    threat_info->set_verdict_type(
        safe_browsing::RTLookupResponse::ThreatInfo::DANGEROUS);
    threat_info->set_threat_type(
        safe_browsing::RTLookupResponse::ThreatInfo::SOCIAL_ENGINEERING);
    threat_info->set_cache_duration_sec(100);
    threat_info->set_cache_expression_using_match_type(bad_url.host() + "/");
    threat_info->set_cache_expression_match_type(
        safe_browsing::RTLookupResponse::ThreatInfo::COVERING_MATCH);
    VerdictCacheManagerFactory::GetForBrowserState(browser_state_.get())
        ->CacheRealTimeUrlVerdict(bad_url, response, base::Time::Now());
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  scoped_refptr<SafeBrowsingService> safe_browsing_service_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;

 private:
  void MarkUrlAsMalwareOnIOThread(const GURL& bad_url) {
    safe_browsing::FullHashInfo full_hash_info =
        safe_browsing::GetFullHashInfoWithMetadata(
            bad_url, safe_browsing::GetUrlMalwareId(),
            safe_browsing::ThreatMetadata());
    v4_db_factory_->MarkPrefixAsBad(safe_browsing::GetUrlMalwareId(),
                                    full_hash_info.full_hash);
    v4_get_hash_factory_->AddToFullHashCache(full_hash_info);
  }

  void MarkUrlAsSafeOnIOThread(const GURL& bad_url) {
    safe_browsing::FullHashInfo full_hash_info =
        safe_browsing::GetFullHashInfoWithMetadata(
            bad_url, safe_browsing::GetUrlMalwareId(),
            safe_browsing::ThreatMetadata());
    v4_db_factory_->MarkPrefixAsBad(
        safe_browsing::GetUrlHighConfidenceAllowlistId(),
        full_hash_info.full_hash);
    v4_get_hash_factory_->AddToFullHashCache(full_hash_info);
  }

  base::ScopedTempDir temp_dir_;

  // Owned by V4Database.
  safe_browsing::TestV4DatabaseFactory* v4_db_factory_;
  // Owned by V4GetHashProtocolManager.
  safe_browsing::TestV4GetHashProtocolManagerFactory* v4_get_hash_factory_;
  // Owned by V4Database.
  safe_browsing::TestV4StoreFactory* store_factory_;
};

TEST_F(SafeBrowsingServiceTest, SafeAndUnsafePages) {
  // Verify that queries to the Safe Browsing database owned by
  // SafeBrowsingService receive responses.
  TestUrlCheckerClient client(safe_browsing_service_.get(),
                              browser_state_.get());
  GURL safe_url = GURL(kSafePage);
  client.CheckUrl(safe_url);
  EXPECT_TRUE(client.result_pending());
  client.WaitForResult();
  EXPECT_FALSE(client.result_pending());
  EXPECT_FALSE(client.url_is_unsafe());

  GURL unsafe_url = GURL(kMalwarePage);
  MarkUrlAsMalware(unsafe_url);
  client.CheckUrl(unsafe_url);
  EXPECT_TRUE(client.result_pending());
  client.WaitForResult();
  EXPECT_FALSE(client.result_pending());
  EXPECT_TRUE(client.url_is_unsafe());

  // Disable Safe Browsing, and ensure that unsafe URLs are no longer flagged.
  browser_state_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  client.CheckUrl(unsafe_url);
  EXPECT_TRUE(client.result_pending());
  client.WaitForResult();
  EXPECT_FALSE(client.result_pending());
  EXPECT_FALSE(client.url_is_unsafe());
}

// Verifies that safe and unsafe URLs are identified correctly when real-time
// lookups are enabled, and that opting out of real-time checks works as
// expected.
TEST_F(SafeBrowsingServiceTest, RealTimeSafeAndUnsafePages) {
  TestUrlCheckerClient client(safe_browsing_service_.get(),
                              browser_state_.get());

  // Wait for an initial result to make sure the Safe Browsing database has
  // been initialized, before calling into functions that mark URLs as safe
  // or unsafe in the database.
  GURL safe_url(kSafePage);
  client.CheckUrl(safe_url);
  client.WaitForResult();

  // Opt into real-time checks.
  browser_state_->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  MarkUrlAsRealTimeSafe(safe_url);
  client.CheckUrl(safe_url);
  EXPECT_TRUE(client.result_pending());
  client.WaitForResult();
  EXPECT_FALSE(client.result_pending());
  EXPECT_FALSE(client.url_is_unsafe());

  GURL unsafe_url(kMalwarePage);
  MarkUrlAsRealTimeUnsafe(unsafe_url);
  client.CheckUrl(unsafe_url);
  EXPECT_TRUE(client.result_pending());
  client.WaitForResult();
  EXPECT_FALSE(client.result_pending());
  EXPECT_TRUE(client.url_is_unsafe());

  // Opt out of real-time checks, and ensure that unsafe URLs are no longer
  // flagged.
  browser_state_->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);
  client.CheckUrl(unsafe_url);
  EXPECT_TRUE(client.result_pending());
  client.WaitForResult();
  EXPECT_FALSE(client.result_pending());
  EXPECT_FALSE(client.url_is_unsafe());
}

TEST_F(SafeBrowsingServiceTest,
       RealTimeSafeAndUnsafePagesWithEnhancedProtection) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(safe_browsing::kEnhancedProtection);

  TestUrlCheckerClient client(safe_browsing_service_.get(),
                              browser_state_.get());

  // Wait for an initial result to make sure the Safe Browsing database has
  // been initialized, before calling into functions that mark URLs as safe
  // or unsafe in the database.
  GURL safe_url(kSafePage);
  client.CheckUrl(safe_url);
  client.WaitForResult();

  // Opt into real-time checks and also does real-time checks for subframe url.
  browser_state_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);

  MarkUrlAsRealTimeSafe(safe_url);
  client.CheckUrl(safe_url);
  EXPECT_TRUE(client.result_pending());
  client.WaitForResult();
  EXPECT_FALSE(client.result_pending());
  EXPECT_FALSE(client.url_is_unsafe());

  MarkUrlAsRealTimeSafe(safe_url);
  client.CheckSubFrameUrl(safe_url);
  EXPECT_TRUE(client.result_pending());
  client.WaitForResult();
  EXPECT_FALSE(client.result_pending());
  EXPECT_FALSE(client.url_is_unsafe());

  GURL unsafe_url(kMalwarePage);
  MarkUrlAsRealTimeUnsafe(unsafe_url);
  client.CheckUrl(unsafe_url);
  EXPECT_TRUE(client.result_pending());
  client.WaitForResult();
  EXPECT_FALSE(client.result_pending());
  EXPECT_TRUE(client.url_is_unsafe());

  MarkUrlAsRealTimeUnsafe(unsafe_url);
  client.CheckSubFrameUrl(unsafe_url);
  EXPECT_TRUE(client.result_pending());
  client.WaitForResult();
  EXPECT_FALSE(client.result_pending());
  EXPECT_TRUE(client.url_is_unsafe());

  // Opt out of real-time checks, and ensure that unsafe URLs are no longer
  // flagged.
  browser_state_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, false);
  client.CheckUrl(unsafe_url);
  EXPECT_TRUE(client.result_pending());
  client.WaitForResult();
  EXPECT_FALSE(client.result_pending());
  EXPECT_FALSE(client.url_is_unsafe());

  client.CheckSubFrameUrl(unsafe_url);
  EXPECT_TRUE(client.result_pending());
  client.WaitForResult();
  EXPECT_FALSE(client.result_pending());
  EXPECT_FALSE(client.url_is_unsafe());
}

// Verifies that cookies are persisted across calls to
// SafeBrowsingServiceImpl::GetURLLoaderFactory.
TEST_F(SafeBrowsingServiceTest, PersistentCookies) {
  net::EmbeddedTestServer server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::test_server::RegisterDefaultHandlers(&server);
  ASSERT_TRUE(server.Start());
  std::string cookie = "test=123";
  std::unique_ptr<network::ResourceRequest> resource_request =
      std::make_unique<network::ResourceRequest>();

  // Set a cookie that expires in an hour.
  resource_request->url = server.GetURL("/set-cookie?" + cookie +
                                        ";max-age=3600;SameSite=None;Secure");
  std::unique_ptr<network::SimpleURLLoader> url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);
  base::RunLoop run_loop1;
  url_loader->DownloadHeadersOnly(
      safe_browsing_service_->GetURLLoaderFactory().get(),
      base::BindLambdaForTesting(
          [&](scoped_refptr<net::HttpResponseHeaders> headers) {
            run_loop1.Quit();
          }));
  run_loop1.Run();

  // Make another request to the same site, and verify that the cookie is still
  // set.
  resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = server.GetURL("/echoheader?Cookie");
  url_loader = network::SimpleURLLoader::Create(std::move(resource_request),
                                                TRAFFIC_ANNOTATION_FOR_TESTS);
  base::RunLoop run_loop2;
  url_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      safe_browsing_service_->GetURLLoaderFactory().get(),
      base::BindLambdaForTesting([&](std::unique_ptr<std::string> body) {
        EXPECT_NE(std::string::npos, body->find(cookie));
        run_loop2.Quit();
      }));
  run_loop2.Run();
}

// Verifies that cookies are cleared when ClearCookies() is called with a
// time range of all-time, but not otherwise.
TEST_F(SafeBrowsingServiceTest, ClearCookies) {
  net::EmbeddedTestServer server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::test_server::RegisterDefaultHandlers(&server);
  ASSERT_TRUE(server.Start());
  std::string cookie = "test=123";
  std::unique_ptr<network::ResourceRequest> resource_request =
      std::make_unique<network::ResourceRequest>();

  // Set a cookie that expires in an hour.
  resource_request->url = server.GetURL("/set-cookie?" + cookie +
                                        ";max-age=3600;SameSite=None;Secure");
  std::unique_ptr<network::SimpleURLLoader> url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);
  base::RunLoop run_loop1;
  url_loader->DownloadHeadersOnly(
      safe_browsing_service_->GetURLLoaderFactory().get(),
      base::BindLambdaForTesting(
          [&](scoped_refptr<net::HttpResponseHeaders> headers) {
            run_loop1.Quit();
          }));
  run_loop1.Run();

  // Call ClearCookies() with a non-all-time time range, and verify that the
  // cookie is still set.
  base::RunLoop run_loop2;
  safe_browsing_service_->ClearCookies(
      net::CookieDeletionInfo::TimeRange(base::Time(), base::Time::Now()),
      base::BindLambdaForTesting([&]() { run_loop2.Quit(); }));
  run_loop2.Run();

  resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = server.GetURL("/echoheader?Cookie");
  url_loader = network::SimpleURLLoader::Create(std::move(resource_request),
                                                TRAFFIC_ANNOTATION_FOR_TESTS);
  base::RunLoop run_loop3;
  url_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      safe_browsing_service_->GetURLLoaderFactory().get(),
      base::BindLambdaForTesting([&](std::unique_ptr<std::string> body) {
        EXPECT_NE(std::string::npos, body->find(cookie));
        run_loop3.Quit();
      }));
  run_loop3.Run();

  // Call ClearCookies() with a time range of all-time, and verify that the
  // cookie is no longer set.
  base::RunLoop run_loop4;
  safe_browsing_service_->ClearCookies(
      net::CookieDeletionInfo::TimeRange(base::Time(), base::Time::Max()),
      base::BindLambdaForTesting([&]() { run_loop4.Quit(); }));
  run_loop4.Run();

  resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = server.GetURL("/echoheader?Cookie");
  url_loader = network::SimpleURLLoader::Create(std::move(resource_request),
                                                TRAFFIC_ANNOTATION_FOR_TESTS);
  base::RunLoop run_loop5;
  url_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      safe_browsing_service_->GetURLLoaderFactory().get(),
      base::BindLambdaForTesting([&](std::unique_ptr<std::string> body) {
        EXPECT_EQ(std::string::npos, body->find(cookie));
        run_loop5.Quit();
      }));
  run_loop5.Run();
}

// Verfies that http requests sent by SafeBrowsingServiceImpl's network context
// have a non-empty User-Agent header.
TEST_F(SafeBrowsingServiceTest, NonEmptyUserAgent) {
  net::EmbeddedTestServer server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::test_server::RegisterDefaultHandlers(&server);
  ASSERT_TRUE(server.Start());
  std::unique_ptr<network::ResourceRequest> resource_request =
      std::make_unique<network::ResourceRequest>();

  // Ask the server to echo the User-Agent header and verify that the echoed
  // value is non-empty.
  resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = server.GetURL("/echoheader?User-Agent");
  std::unique_ptr<network::SimpleURLLoader> url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);
  base::RunLoop run_loop;
  url_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      safe_browsing_service_->GetURLLoaderFactory().get(),
      base::BindLambdaForTesting([&](std::unique_ptr<std::string> body) {
        EXPECT_FALSE(body->empty());
        run_loop.Quit();
      }));
  run_loop.Run();
}

using SafeBrowsingServiceInitializationTest = PlatformTest;

// Verifies that GetURLLoaderFactory() has a non-null return value when called
// immediately after initialization.
TEST_F(SafeBrowsingServiceInitializationTest, GetURLLoaderFactory) {
  web::WebTaskEnvironment task_environment;

  std::unique_ptr<TestChromeBrowserState> browser_state =
      TestChromeBrowserState::Builder().Build();

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  scoped_refptr<SafeBrowsingService> safe_browsing_service =
      base::MakeRefCounted<SafeBrowsingServiceImpl>();
  safe_browsing::SafeBrowsingMetricsCollector* safe_browsing_metrics_collector =
      SafeBrowsingMetricsCollectorFactory::GetForBrowserState(
          browser_state.get());
  safe_browsing_service->Initialize(browser_state->GetPrefs(),
                                    temp_dir.GetPath(),
                                    safe_browsing_metrics_collector);

  EXPECT_TRUE(safe_browsing_service->GetURLLoaderFactory());

  safe_browsing_service->ShutDown();
  task_environment.RunUntilIdle();
}
