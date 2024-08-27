// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/safe_browsing/safe_browsing_service_impl.h"

#import "base/files/scoped_temp_dir.h"
#import "base/memory/raw_ptr.h"
#import "base/path_service.h"
#import "base/run_loop.h"
#import "base/test/bind.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/testing_pref_service.h"
#import "components/safe_browsing/core/browser/db/database_manager.h"
#import "components/safe_browsing/core/browser/db/util.h"
#import "components/safe_browsing/core/browser/db/v4_database.h"
#import "components/safe_browsing/core/browser/db/v4_get_hash_protocol_manager.h"
#import "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#import "components/safe_browsing/core/browser/db/v4_test_util.h"
#import "components/safe_browsing/core/browser/realtime/url_lookup_service.h"
#import "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"
#import "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#import "components/safe_browsing/core/browser/verdict_cache_manager.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/safe_browsing/core/common/safebrowsing_constants.h"
#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "components/unified_consent/pref_names.h"
#import "components/unified_consent/unified_consent_service.h"
#import "ios/components/security_interstitials/safe_browsing/fake_safe_browsing_client.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_query_manager.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_unsafe_resource_container.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "services/network/public/cpp/simple_url_loader.h"
#import "testing/platform_test.h"

namespace {

const char kSafePage[] = "https://example.test/safe.html";
const char kMalwarePage[] = "https://unsafe.test/malware.html";

class TestUrlCheckerClient {
 public:
  TestUrlCheckerClient(SafeBrowsingService* safe_browsing_service,
                       web::BrowserState* browser_state,
                       SafeBrowsingClient* safe_browsing_client)
      : safe_browsing_service_(safe_browsing_service),
        safe_browsing_client_(safe_browsing_client) {
    SafeBrowsingQueryManager::CreateForWebState(&web_state_,
                                                safe_browsing_client_);
    SafeBrowsingUrlAllowList::CreateForWebState(&web_state_);
    SafeBrowsingUnsafeResourceContainer::CreateForWebState(&web_state_);
    web_state_.SetBrowserState(browser_state);
  }

  ~TestUrlCheckerClient() = default;

  TestUrlCheckerClient(const TestUrlCheckerClient&) = delete;
  TestUrlCheckerClient& operator=(const TestUrlCheckerClient&) = delete;

  bool url_is_unsafe() const { return url_is_unsafe_; }

  safe_browsing::SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check()
      const {
    return performed_check_;
  }

  void CheckUrl(const GURL& url) {
    result_pending_ = true;
    url_checker_ = safe_browsing_service_->CreateUrlChecker(
        network::mojom::RequestDestination::kDocument, &web_state_,
        safe_browsing_client_);
    CheckUrlOnUIThread(url);
  }

  void CheckUrlWithSyncChecker(const GURL& url) {
    result_pending_ = true;
    url_checker_ = safe_browsing_service_->CreateSyncChecker(
        network::mojom::RequestDestination::kDocument, &web_state_,
        safe_browsing_client_);
    CheckUrlOnUIThread(url);
  }

  void CheckUrlWithAsyncChecker(const GURL& url) {
    result_pending_ = true;
    url_checker_ = safe_browsing_service_->CreateAsyncChecker(
        network::mojom::RequestDestination::kDocument, &web_state_,
        safe_browsing_client_);
    CheckUrlOnUIThread(url);
  }

  bool result_pending() const { return result_pending_; }

  void WaitForResult() {
    while (result_pending()) {
      base::RunLoop().RunUntilIdle();
    }
  }

 private:
  void CheckUrlOnUIThread(const GURL& url) {
    url_checker_->CheckUrl(
        url, "GET",
        base::BindOnce(&TestUrlCheckerClient::OnCheckUrlResult,
                       base::Unretained(this)));
  }

  void OnCheckUrlResult(
      bool proceed,
      bool showed_interstitial,
      bool has_post_commit_interstitial_skipped,
      safe_browsing::SafeBrowsingUrlCheckerImpl::PerformedCheck
          performed_check) {
    url_is_unsafe_ = !proceed;
    result_pending_ = false;
    performed_check_ = performed_check;
    url_checker_.reset();
  }

  void CheckDone() { result_pending_ = false; }

  bool result_pending_ = false;
  bool url_is_unsafe_ = false;
  safe_browsing::SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check_ =
      safe_browsing::SafeBrowsingUrlCheckerImpl::PerformedCheck::kUnknown;
  raw_ptr<SafeBrowsingService> safe_browsing_service_;
  web::FakeWebState web_state_;
  std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl> url_checker_;
  raw_ptr<SafeBrowsingClient> safe_browsing_client_;
};

}  // namespace

class SafeBrowsingServiceTest : public PlatformTest {
 public:
  SafeBrowsingServiceTest() : browser_state_(new web::FakeBrowserState()) {
    store_factory_ = new safe_browsing::TestV4StoreFactory();
    safe_browsing::V4Database::RegisterStoreFactoryForTest(
        base::WrapUnique(store_factory_.get()));

    v4_db_factory_ = new safe_browsing::TestV4DatabaseFactory();
    safe_browsing::V4Database::RegisterDatabaseFactoryForTest(
        base::WrapUnique(v4_db_factory_.get()));

    v4_get_hash_factory_ =
        new safe_browsing::TestV4GetHashProtocolManagerFactory();
    safe_browsing::V4GetHashProtocolManager::RegisterFactory(
        base::WrapUnique(v4_get_hash_factory_.get()));

    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    HostContentSettingsMap::RegisterProfilePrefs(pref_service_->registry());
    safe_browsing::RegisterProfilePrefs(pref_service_->registry());
    unified_consent::UnifiedConsentService::RegisterPrefs(
        pref_service_->registry());

    safe_browsing_service_ = base::MakeRefCounted<SafeBrowsingServiceImpl>();

    CHECK(temp_dir_.CreateUniqueTempDir());
    safe_browsing_service_->Initialize(
        pref_service_.get(), temp_dir_.GetPath(),
        /*safe_browsing_metrics_collector=*/nullptr);
    base::RunLoop().RunUntilIdle();

    SetupUrlLookupService();
  }

  SafeBrowsingServiceTest(const SafeBrowsingServiceTest&) = delete;
  SafeBrowsingServiceTest& operator=(const SafeBrowsingServiceTest&) = delete;

  ~SafeBrowsingServiceTest() override {
    safe_browsing_service_->ShutDown();
    if (host_content_settings_map_) {
      host_content_settings_map_->ShutdownOnUIThread();
    }

    safe_browsing::V4GetHashProtocolManager::RegisterFactory(nullptr);
    safe_browsing::V4Database::RegisterDatabaseFactoryForTest(nullptr);
    safe_browsing::V4Database::RegisterStoreFactoryForTest(nullptr);
  }

  void MarkUrlAsMalware(const GURL& bad_url) {
    MarkUrlAsMalwareOnUIThread(bad_url);
  }

  // Adds the given `safe_url` to the allowlist used by real-time checks.
  void MarkUrlAsRealTimeSafe(const GURL& safe_url) {
    MarkUrlAsSafeOnUIThread(safe_url);
  }

  // Caches the given `bad_url` as unsafe in the VerdictCacheManager used by
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
    verdict_cache_manager_->CacheRealTimeUrlVerdict(response,
                                                    base::Time::Now());
  }

 protected:
  void SetUpVerdict(GURL url, bool is_unsafe) {
    verdict_cache_manager_->CacheArtificialHashRealTimeLookupVerdict(url.spec(),
                                                                     is_unsafe);
  }

  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::MainThreadType::IO};
  scoped_refptr<SafeBrowsingService> safe_browsing_service_;
  std::unique_ptr<web::FakeBrowserState> browser_state_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  FakeSafeBrowsingClient safe_browsing_client_;
  safe_browsing::hash_realtime_utils::GoogleChromeBrandingPretenderForTesting
      apply_branding_;
  base::test::ScopedFeatureList scoped_feature_list_;
  web::FakeWebState web_state_;

 private:
  void MarkUrlAsMalwareOnUIThread(const GURL& bad_url) {
    safe_browsing::FullHashInfo full_hash_info =
        safe_browsing::GetFullHashInfoWithMetadata(
            bad_url, safe_browsing::GetUrlMalwareId(),
            safe_browsing::ThreatMetadata());
    v4_db_factory_->MarkPrefixAsBad(safe_browsing::GetUrlMalwareId(),
                                    full_hash_info.full_hash);
    v4_get_hash_factory_->AddToFullHashCache(full_hash_info);
  }

  void MarkUrlAsSafeOnUIThread(const GURL& bad_url) {
    safe_browsing::FullHashInfo full_hash_info =
        safe_browsing::GetFullHashInfoWithMetadata(
            bad_url, safe_browsing::GetUrlMalwareId(),
            safe_browsing::ThreatMetadata());
    v4_db_factory_->MarkPrefixAsBad(
        safe_browsing::GetUrlHighConfidenceAllowlistId(),
        full_hash_info.full_hash);
    v4_get_hash_factory_->AddToFullHashCache(full_hash_info);
  }

  void SetupUrlLookupService() {
    host_content_settings_map_ = base::MakeRefCounted<HostContentSettingsMap>(
        pref_service_.get(), /*is_off_the_record=*/false,
        /*store_last_modified=*/false, /*restore_session=*/false,
        /*should_record_metrics=*/false);
    verdict_cache_manager_ =
        std::make_unique<safe_browsing::VerdictCacheManager>(
            /*history_service=*/nullptr, host_content_settings_map_.get(),
            pref_service_.get(), /*sync_observer=*/nullptr);
    lookup_service_ = std::make_unique<safe_browsing::RealTimeUrlLookupService>(
        safe_browsing_service_->GetURLLoaderFactory(),
        verdict_cache_manager_.get(), base::BindRepeating([] {
          safe_browsing::ChromeUserPopulation population;
          return population;
        }),
        pref_service_.get(),
        /*token_fetcher=*/nullptr,
        base::BindRepeating([](bool) { return false; }),
        /*is_off_the_record=*/false,
        /*variations_service=*/nullptr,
        /*referrer_chain_provider=*/nullptr,
        /*webui_delegate=*/nullptr);
    safe_browsing_client_.set_real_time_url_lookup_service(
        lookup_service_.get());
  }

  base::ScopedTempDir temp_dir_;

  // Owned by V4Database.
  raw_ptr<safe_browsing::TestV4DatabaseFactory> v4_db_factory_;
  // Owned by V4GetHashProtocolManager.
  raw_ptr<safe_browsing::TestV4GetHashProtocolManagerFactory>
      v4_get_hash_factory_;
  // Owned by V4Database.
  raw_ptr<safe_browsing::TestV4StoreFactory> store_factory_;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  std::unique_ptr<safe_browsing::VerdictCacheManager> verdict_cache_manager_;
  std::unique_ptr<safe_browsing::RealTimeUrlLookupService> lookup_service_;
};

TEST_F(SafeBrowsingServiceTest, SafeAndUnsafePages) {
  // Verify that queries to the Safe Browsing database owned by
  // SafeBrowsingService receive responses.
  TestUrlCheckerClient client(safe_browsing_service_.get(),
                              browser_state_.get(), &safe_browsing_client_);
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
  pref_service_->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  client.CheckUrl(unsafe_url);
  EXPECT_FALSE(client.result_pending());
  EXPECT_FALSE(client.url_is_unsafe());
}

TEST_F(SafeBrowsingServiceTest, SafeAndUnsafePagesWithSyncChecker) {
  // Verify that queries to the Safe Browsing database owned by
  // SafeBrowsingService receive responses.
  TestUrlCheckerClient client(safe_browsing_service_.get(),
                              browser_state_.get(), &safe_browsing_client_);
  GURL safe_url = GURL(kSafePage);
  client.CheckUrlWithSyncChecker(safe_url);
  EXPECT_TRUE(client.result_pending());
  client.WaitForResult();
  EXPECT_FALSE(client.result_pending());
  EXPECT_FALSE(client.url_is_unsafe());

  GURL unsafe_url = GURL(kMalwarePage);
  MarkUrlAsMalware(unsafe_url);
  client.CheckUrlWithSyncChecker(unsafe_url);
  EXPECT_TRUE(client.result_pending());
  client.WaitForResult();
  EXPECT_FALSE(client.result_pending());
  EXPECT_TRUE(client.url_is_unsafe());

  // Disable Safe Browsing, and ensure that unsafe URLs are no longer flagged.
  pref_service_->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  client.CheckUrlWithSyncChecker(unsafe_url);
  EXPECT_FALSE(client.result_pending());
  EXPECT_FALSE(client.url_is_unsafe());
}

TEST_F(SafeBrowsingServiceTest, SafeAndUnsafePagesWithAsyncChecker) {
  // Verify that queries to the Safe Browsing database owned by
  // SafeBrowsingService receive responses.
  TestUrlCheckerClient client(safe_browsing_service_.get(),
                              browser_state_.get(), &safe_browsing_client_);
  GURL safe_url = GURL(kSafePage);
  client.CheckUrlWithAsyncChecker(safe_url);
  EXPECT_TRUE(client.result_pending());
  client.WaitForResult();
  EXPECT_FALSE(client.result_pending());
  EXPECT_FALSE(client.url_is_unsafe());

  GURL unsafe_url = GURL(kMalwarePage);
  MarkUrlAsMalware(unsafe_url);
  client.CheckUrlWithAsyncChecker(unsafe_url);
  EXPECT_TRUE(client.result_pending());
  client.WaitForResult();
  EXPECT_FALSE(client.result_pending());
  EXPECT_TRUE(client.url_is_unsafe());

  // Disable Safe Browsing, and ensure that unsafe URLs are no longer flagged.
  pref_service_->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  client.CheckUrlWithAsyncChecker(unsafe_url);
  EXPECT_FALSE(client.result_pending());
  EXPECT_FALSE(client.url_is_unsafe());
}

// Verifies that safe and unsafe URLs are identified correctly when real-time
// lookups are enabled, and that opting out of real-time checks works as
// expected.
TEST_F(SafeBrowsingServiceTest, RealTimeSafeAndUnsafePages) {
  TestUrlCheckerClient client(safe_browsing_service_.get(),
                              browser_state_.get(), &safe_browsing_client_);

  // Wait for an initial result to make sure the Safe Browsing database has
  // been initialized, before calling into functions that mark URLs as safe
  // or unsafe in the database.
  GURL safe_url(kSafePage);
  client.CheckUrl(safe_url);
  client.WaitForResult();

  // Opt into real-time checks.
  pref_service_->SetBoolean(
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
  pref_service_->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);
  client.CheckUrl(unsafe_url);
  EXPECT_TRUE(client.result_pending());
  client.WaitForResult();
  EXPECT_FALSE(client.result_pending());
  EXPECT_FALSE(client.url_is_unsafe());
}

// Verifies that safe and unsafe URLs are identified correctly for when a sync
// checker is used. A sync checker shouldn't be able to detect unsafe pages
// related to real time checks.
TEST_F(SafeBrowsingServiceTest, RealTimeSafeAndUnsafePagesWithSyncChecker) {
  TestUrlCheckerClient client(safe_browsing_service_.get(),
                              browser_state_.get(), &safe_browsing_client_);

  // Wait for an initial result to make sure the Safe Browsing database has
  // been initialized, before calling into functions that mark URLs as safe
  // or unsafe in the database.
  GURL safe_url(kSafePage);
  client.CheckUrlWithSyncChecker(safe_url);
  client.WaitForResult();

  // Opt into real-time checks.
  pref_service_->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  MarkUrlAsRealTimeSafe(safe_url);
  client.CheckUrlWithSyncChecker(safe_url);
  EXPECT_TRUE(client.result_pending());
  client.WaitForResult();
  EXPECT_FALSE(client.result_pending());
  EXPECT_FALSE(client.url_is_unsafe());

  GURL unsafe_url(kMalwarePage);
  MarkUrlAsRealTimeUnsafe(unsafe_url);
  client.CheckUrlWithSyncChecker(unsafe_url);
  EXPECT_TRUE(client.result_pending());
  client.WaitForResult();
  EXPECT_FALSE(client.result_pending());
  EXPECT_FALSE(client.url_is_unsafe());

  // Opt out of real-time checks, and ensure that unsafe URLs continue to be
  // unflagged.
  pref_service_->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);
  client.CheckUrlWithSyncChecker(unsafe_url);
  EXPECT_TRUE(client.result_pending());
  client.WaitForResult();
  EXPECT_FALSE(client.result_pending());
  EXPECT_FALSE(client.url_is_unsafe());
}

// Verifies that safe and unsafe URLs are identified correctly for when an async
// checker is used. An async checker should detect unsafe pages related to real
// time checks.
TEST_F(SafeBrowsingServiceTest, RealTimeSafeAndUnsafePagesWithAsyncChecker) {
  TestUrlCheckerClient client(safe_browsing_service_.get(),
                              browser_state_.get(), &safe_browsing_client_);

  // Wait for an initial result to make sure the Safe Browsing database has
  // been initialized, before calling into functions that mark URLs as safe
  // or unsafe in the database.
  GURL safe_url(kSafePage);
  client.CheckUrlWithAsyncChecker(safe_url);
  client.WaitForResult();

  // Opt into real-time checks.
  pref_service_->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);

  MarkUrlAsRealTimeSafe(safe_url);
  client.CheckUrlWithAsyncChecker(safe_url);
  EXPECT_TRUE(client.result_pending());
  client.WaitForResult();
  EXPECT_FALSE(client.result_pending());
  EXPECT_FALSE(client.url_is_unsafe());

  GURL unsafe_url(kMalwarePage);
  MarkUrlAsRealTimeUnsafe(unsafe_url);
  client.CheckUrlWithAsyncChecker(unsafe_url);
  EXPECT_TRUE(client.result_pending());
  client.WaitForResult();
  EXPECT_FALSE(client.result_pending());
  EXPECT_TRUE(client.url_is_unsafe());

  // Opt out of real-time checks, and ensure that unsafe URLs are no longer
  // flagged.
  pref_service_->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);
  client.CheckUrlWithAsyncChecker(unsafe_url);
  EXPECT_TRUE(client.result_pending());
  client.WaitForResult();
  EXPECT_FALSE(client.result_pending());
  EXPECT_FALSE(client.url_is_unsafe());
}

TEST_F(SafeBrowsingServiceTest,
       RealTimeSafeAndUnsafePagesWithEnhancedProtection) {
  TestUrlCheckerClient client(safe_browsing_service_.get(),
                              browser_state_.get(), &safe_browsing_client_);

  // Wait for an initial result to make sure the Safe Browsing database has
  // been initialized, before calling into functions that mark URLs as safe
  // or unsafe in the database.
  GURL safe_url(kSafePage);
  client.CheckUrl(safe_url);
  client.WaitForResult();

  // Opt into real-time checks.
  pref_service_->SetBoolean(prefs::kSafeBrowsingEnhanced, true);

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
  pref_service_->SetBoolean(prefs::kSafeBrowsingEnhanced, false);
  client.CheckUrl(unsafe_url);
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

// Verifies that Safe Browsing hash prefix metrics are correctly recorded and
// the performed check is correct when the hash prefix feature is enabled.
TEST_F(SafeBrowsingServiceTest, HashPrefixEnabled) {
  scoped_feature_list_.InitAndEnableFeature(
      safe_browsing::kHashPrefixRealTimeLookups);
  TestUrlCheckerClient client(safe_browsing_service_.get(),
                              browser_state_.get(), &safe_browsing_client_);
  pref_service_->SetBoolean(prefs::kSafeBrowsingEnabled, true);

  base::HistogramTester histogram_tester;
  GURL url = GURL(kMalwarePage);
  SetUpVerdict(url, /*is_unsafe=*/true);
  client.CheckUrl(url);

  EXPECT_TRUE(client.result_pending());
  client.WaitForResult();
  EXPECT_FALSE(client.result_pending());
  EXPECT_EQ(safe_browsing::SafeBrowsingUrlCheckerImpl::PerformedCheck::
                kHashRealTimeCheck,
            client.performed_check());
  histogram_tester.ExpectBucketCount(
      "SafeBrowsing.HPRT.Ineligible.IneligibleForSessionOrLocation",
      /*sample=*/false,
      /*expected_bucket_count=*/1);

  task_environment_.RunUntilIdle();
}

// Verifies that Safe Browsing hash prefix metrics are correctly recorded and
// the performed check is correct when the hash prefix feature is disabled.
TEST_F(SafeBrowsingServiceTest, HashPrefixDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      safe_browsing::kHashPrefixRealTimeLookups);
  TestUrlCheckerClient client(safe_browsing_service_.get(),
                              browser_state_.get(), &safe_browsing_client_);

  pref_service_->SetBoolean(prefs::kSafeBrowsingEnabled, true);

  base::HistogramTester histogram_tester;
  GURL url = GURL(kMalwarePage);
  SetUpVerdict(url, /*is_unsafe=*/true);
  client.CheckUrl(url);

  EXPECT_TRUE(client.result_pending());
  client.WaitForResult();
  EXPECT_FALSE(client.result_pending());
  EXPECT_EQ(safe_browsing::SafeBrowsingUrlCheckerImpl::PerformedCheck::
                kHashDatabaseCheck,
            client.performed_check());
  histogram_tester.ExpectBucketCount(
      "SafeBrowsing.HPRT.Ineligible.IneligibleForSessionOrLocation",
      /*sample=*/true,
      /*expected_bucket_count=*/1);

  task_environment_.RunUntilIdle();
}

// Verifies that Safe Browsing preference metrics are correctly recorded when
// Safe Browsing is disabled.
TEST_F(SafeBrowsingServiceTest, TestShouldCreateAsyncChecker) {
  scoped_feature_list_.InitAndEnableFeature(
      safe_browsing::kSafeBrowsingAsyncRealTimeCheck);
  TestUrlCheckerClient client(safe_browsing_service_.get(),
                              browser_state_.get(), &safe_browsing_client_);
  web_state_.SetBrowserState(browser_state_.get());
  EXPECT_TRUE(safe_browsing_service_->ShouldCreateAsyncChecker(
      &web_state_, &safe_browsing_client_));

  pref_service_->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  pref_service_->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);
  EXPECT_FALSE(safe_browsing_service_->ShouldCreateAsyncChecker(
      &web_state_, &safe_browsing_client_));

  safe_browsing_service_->ShutDown();
}

using SafeBrowsingServiceInitializationTest = PlatformTest;

// Verifies that GetURLLoaderFactory() has a non-null return value when called
// immediately after initialization.
TEST_F(SafeBrowsingServiceInitializationTest, GetURLLoaderFactory) {
  web::WebTaskEnvironment task_environment;

  std::unique_ptr<web::FakeBrowserState> browser_state =
      std::make_unique<web::FakeBrowserState>();
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs =
      std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
  safe_browsing::RegisterProfilePrefs(prefs->registry());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  scoped_refptr<SafeBrowsingService> safe_browsing_service =
      base::MakeRefCounted<SafeBrowsingServiceImpl>();
  safe_browsing_service->Initialize(
      prefs.get(), temp_dir.GetPath(),
      /*safe_browsing_metrics_collector=*/nullptr);

  EXPECT_TRUE(safe_browsing_service->GetURLLoaderFactory());

  safe_browsing_service->ShutDown();
  task_environment.RunUntilIdle();
}

// Verifies that Safe Browsing preference metrics are correctly recorded when
// Safe Browsing is enabled but Enhanced Safe Browsing is not.
TEST_F(SafeBrowsingServiceInitializationTest,
       PreferenceMetricsStandardSafeBrowsing) {
  web::WebTaskEnvironment task_environment;

  std::unique_ptr<web::FakeBrowserState> browser_state =
      std::make_unique<web::FakeBrowserState>();
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs =
      std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
  safe_browsing::RegisterProfilePrefs(prefs->registry());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  scoped_refptr<SafeBrowsingService> safe_browsing_service =
      base::MakeRefCounted<SafeBrowsingServiceImpl>();
  prefs->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  prefs->SetBoolean(prefs::kSafeBrowsingEnhanced, false);
  base::HistogramTester histogram_tester;
  safe_browsing_service->Initialize(
      prefs.get(), temp_dir.GetPath(),
      /*safe_browsing_metrics_collector=*/nullptr);
  histogram_tester.ExpectUniqueSample(
      safe_browsing::kSafeBrowsingEnabledHistogramName, /*sample=*/1,
      /*count=*/1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.Pref.Enhanced",
                                      /*sample=*/0, /*count=*/1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.Pref.Enhanced.RegularProfile",
      /*sample=*/0, /*count=*/1);

  safe_browsing_service->ShutDown();
  task_environment.RunUntilIdle();
}

// Verifies that Safe Browsing preference metrics are correctly recorded when
// Enhanced Safe Browsing is enabled.
TEST_F(SafeBrowsingServiceInitializationTest,
       PreferenceMetricsEnhancedSafeBrowsing) {
  web::WebTaskEnvironment task_environment;

  std::unique_ptr<web::FakeBrowserState> browser_state =
      std::make_unique<web::FakeBrowserState>();
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs =
      std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
  safe_browsing::RegisterProfilePrefs(prefs->registry());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  scoped_refptr<SafeBrowsingService> safe_browsing_service =
      base::MakeRefCounted<SafeBrowsingServiceImpl>();
  prefs->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  prefs->SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  base::HistogramTester histogram_tester;
  safe_browsing_service->Initialize(
      prefs.get(), temp_dir.GetPath(),
      /*safe_browsing_metrics_collector=*/nullptr);
  histogram_tester.ExpectUniqueSample(
      safe_browsing::kSafeBrowsingEnabledHistogramName, /*sample=*/1,
      /*count=*/1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.Pref.Enhanced",
                                      /*sample=*/1, /*count=*/1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.Pref.Enhanced.RegularProfile",
      /*sample=*/1, /*count=*/1);

  safe_browsing_service->ShutDown();
  task_environment.RunUntilIdle();
}

// Verifies that Safe Browsing preference metrics are correctly recorded when
// Safe Browsing is disabled.
TEST_F(SafeBrowsingServiceInitializationTest, PreferenceMetricsNoSafeBrowsing) {
  web::WebTaskEnvironment task_environment;

  std::unique_ptr<web::FakeBrowserState> browser_state =
      std::make_unique<web::FakeBrowserState>();
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs =
      std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
  safe_browsing::RegisterProfilePrefs(prefs->registry());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  scoped_refptr<SafeBrowsingService> safe_browsing_service =
      base::MakeRefCounted<SafeBrowsingServiceImpl>();
  prefs->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  prefs->SetBoolean(prefs::kSafeBrowsingEnhanced, false);
  base::HistogramTester histogram_tester;
  safe_browsing_service->Initialize(
      prefs.get(), temp_dir.GetPath(),
      /*safe_browsing_metrics_collector=*/nullptr);
  histogram_tester.ExpectUniqueSample(
      safe_browsing::kSafeBrowsingEnabledHistogramName, /*sample=*/0,
      /*count=*/1);
  histogram_tester.ExpectUniqueSample("SafeBrowsing.Pref.Enhanced",
                                      /*sample=*/0, /*count=*/1);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.Pref.Enhanced.RegularProfile",
      /*sample=*/0, /*count=*/1);

  safe_browsing_service->ShutDown();
  task_environment.RunUntilIdle();
}
