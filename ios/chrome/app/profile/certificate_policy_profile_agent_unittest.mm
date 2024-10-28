// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/certificate_policy_profile_agent.h"

#import "base/functional/bind.h"
#import "base/memory/scoped_refptr.h"
#import "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_test_utils.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/test/block_cleanup_test.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/security/certificate_policy_cache.h"
#import "ios/web/public/session/session_certificate_policy_cache.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "net/cert/x509_certificate.h"
#import "net/test/cert_test_util.h"
#import "net/test/test_data_directory.h"

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::SpinRunLoopWithMaxDelay;
using base::test::ios::WaitUntilConditionOrTimeout;

// Test fixture for the cert policy profile agent. The APIs under test operate
// on the UI thread (hence the task environment setup). The profile agent
// updates the cert cache based on the contents of multiple browsers, so most
// test cases involve setting up web states with various session caches in
// multiple browsers, and then inducing the profile agent to update the global
// cache.
class CertificatePolicyProfileStateAgentTest : public BlockCleanupTest {
 protected:
  CertificatePolicyProfileStateAgentTest()
      : cert_(net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "ok_cert.pem")),
        status_(net::CERT_STATUS_REVOKED) {
    profile_ =
        profile_manager_.AddProfileWithBuilder(TestProfileIOS::Builder());

    profile_state_ = [[ProfileState alloc] initWithAppState:nil];
    profile_state_.profile = profile_.get();
    SetProfileStateInitStage(profile_state_, ProfileInitStage::kFinal);

    // Create two regular and one OTR browsers.
    regular_browser_1_ = std::make_unique<TestBrowser>(profile_.get());
    regular_browser_2_ = std::make_unique<TestBrowser>(profile_.get());
    incognito_browser_ =
        std::make_unique<TestBrowser>(profile_->GetOffTheRecordProfile());

    BrowserList* browser_list =
        BrowserListFactory::GetForProfile(profile_.get());
    browser_list->AddBrowser(regular_browser_1_.get());
    browser_list->AddBrowser(regular_browser_2_.get());
    browser_list->AddBrowser(incognito_browser_.get());

    // Finally, create the profile agent being tested and attach it to the app
    // state.
    profile_agent_ = [[CertificatePolicyProfileAgent alloc] init];
    profile_agent_.profileState = profile_state_;
  }

  // Adds a web state with `host` as the active URL to `browser`.
  void AddWebStateToBrowser(std::string host, Browser* browser) {
    auto test_web_state = std::make_unique<web::FakeWebStateWithPolicyCache>(
        browser->GetProfile());
    GURL url(host);
    test_web_state->SetCurrentURL(url);
    WebStateList* web_state_list = browser->GetWebStateList();
    web_state_list->InsertWebState(std::move(test_web_state));
  }

  // Adds a web state with `host` as the active URL, and with `host` registered
  // as having a valid certificate to `browser`.
  void AddCertifiedWebStateToBrowser(std::string host, Browser* browser) {
    auto test_web_state = std::make_unique<web::FakeWebStateWithPolicyCache>(
        browser->GetProfile());
    GURL url(host);
    test_web_state->SetCurrentURL(url);
    test_web_state->GetSessionCertificatePolicyCache()
        ->RegisterAllowedCertificate(cert_, host, status_);
    WebStateList* web_state_list = browser->GetWebStateList();
    web_state_list->InsertWebState(std::move(test_web_state));
  }

  // Adds one web state to each browser with no certs.
  void PopulateWebStatesWithNoCerts() {
    AddWebStateToBrowser("x.com", regular_browser_1_.get());
    AddWebStateToBrowser("y.com", regular_browser_2_.get());
    AddWebStateToBrowser("z.com", incognito_browser_.get());
  }

  // Creates the populated fixture for all tests -- one web state in each
  // browser with no allowed certificates, then two more web states, each with
  // one allowed certificate, in each browser. The allowed hosts are
  // {a,b,c,d}.com for the regular browsers, and {a,b}.com for the incognito
  // browser.
  void PopulateWebStates() {
    PopulateWebStatesWithNoCerts();

    // Adds web states with certs
    AddCertifiedWebStateToBrowser("a.com", regular_browser_1_.get());
    AddCertifiedWebStateToBrowser("b.com", regular_browser_1_.get());
    AddCertifiedWebStateToBrowser("c.com", regular_browser_2_.get());
    AddCertifiedWebStateToBrowser("d.com", regular_browser_2_.get());
    AddCertifiedWebStateToBrowser("a.com", incognito_browser_.get());
    AddCertifiedWebStateToBrowser("b.com", incognito_browser_.get());

    // Clear the policy caches after this, since RegisterAllowedCertificate adds
    // to the global cache.
    ClearPolicyCache(RegularPolicyCache());
    ClearPolicyCache(IncognitoPolicyCache());
  }

  // Triggers certificate cache updates in the app agent under test, and wait
  // for updates to complete.
  void TriggerCertCacheUpdate() {
    SceneState* scene_state = [[SceneState alloc] initWithAppState:nil];

    [profile_state_ sceneStateConnected:scene_state];
    scene_state.activationLevel = SceneActivationLevelForegroundInactive;
    scene_state.activationLevel = SceneActivationLevelBackground;

    // Cache clearing is on the IO thread, and cache reconstruction is posted
    // to the main thread, so both a wait and a RunUntilIdle() are needed.
    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
      base::RunLoop().RunUntilIdle();
      return !profile_agent_.working;
    }));
  }

  // Checks `cache` to see if the policy for `host` is "allowed". For the
  // purposes of this test, that's effectively testing if `host` is "in"
  // `cache`. Checking the cache is async, so this method handles synchronous
  // waiting for the result.
  bool IsHostCertAllowed(
      const scoped_refptr<web::CertificatePolicyCache>& cache,
      const std::string& host) {
    __block web::CertPolicy::Judgment judgement =
        web::CertPolicy::Judgment::UNKNOWN;
    __block bool completed = false;
    web::GetIOThreadTaskRunner({})->PostTask(FROM_HERE, base::BindOnce(^{
                                               completed = true;
                                               judgement = cache->QueryPolicy(
                                                   cert_.get(), host, status_);
                                             }));
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
      return completed;
    }));
    return judgement == web::CertPolicy::Judgment::ALLOWED;
  }

  // Clears all entries from `cache`. This is posted to the IO thread and this
  // method sync-waits for this to complete.
  void ClearPolicyCache(
      const scoped_refptr<web::CertificatePolicyCache>& cache) {
    __block bool policies_cleared = false;
    web::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(^{
          cache->ClearCertificatePolicies();
          policies_cleared = true;
        }));
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
      return policies_cleared;
    }));
  }

  // The policy cache for the regular profile used in this test.
  scoped_refptr<web::CertificatePolicyCache> RegularPolicyCache() {
    return web::BrowserState::GetCertificatePolicyCache(profile_.get());
  }

  // The policy cache for the incognito profile used in this test.
  scoped_refptr<web::CertificatePolicyCache> IncognitoPolicyCache() {
    return web::BrowserState::GetCertificatePolicyCache(
        profile_->GetOffTheRecordProfile());
  }

  bool RegularPolicyCacheContainsHost(const std::string& host) {
    return IsHostCertAllowed(RegularPolicyCache(), host);
  }

  bool IncognitoPolicyCacheContainsHost(const std::string& host) {
    return IsHostCertAllowed(IncognitoPolicyCache(), host);
  }

  // Populates `cache` with allowed certs for the hosts in `hosts`. This is done
  // in a single async call, and this method sync-waits on it completing.
  void PopulatePolicyCache(std::vector<std::string> hosts,
                           scoped_refptr<web::CertificatePolicyCache> cache) {
    __block bool hosts_added = false;
    auto populate_cache = ^{
      for (std::string host : hosts) {
        cache->AllowCertForHost(cert_.get(), host, status_);
      }
      hosts_added = true;
    };
    web::GetIOThreadTaskRunner({})->PostTask(FROM_HERE,
                                             base::BindOnce(populate_cache));
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
      return hosts_added;
    }));
  }

 private:
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::IOThreadType::REAL_THREAD};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  CertificatePolicyProfileAgent* profile_agent_;
  raw_ptr<ProfileIOS> profile_;
  ProfileState* profile_state_;
  std::unique_ptr<TestBrowser> regular_browser_1_;
  std::unique_ptr<TestBrowser> regular_browser_2_;
  std::unique_ptr<TestBrowser> incognito_browser_;

  scoped_refptr<net::X509Certificate> cert_;
  net::CertStatus status_;
};

// Test that updating an empty cache with no webstates results in an empty
// cache.
TEST_F(CertificatePolicyProfileStateAgentTest, EmptyCacheNoWebstates) {
  // Empty cache, no webstates.
  TriggerCertCacheUpdate();
  // Expect nothing is in the cache.
  EXPECT_FALSE(RegularPolicyCacheContainsHost("a.com"));
  EXPECT_FALSE(RegularPolicyCacheContainsHost("b.com"));
  EXPECT_FALSE(RegularPolicyCacheContainsHost("c.com"));
  EXPECT_FALSE(RegularPolicyCacheContainsHost("d.com"));
  EXPECT_FALSE(IncognitoPolicyCacheContainsHost("a.com"));
  EXPECT_FALSE(IncognitoPolicyCacheContainsHost("b.com"));
}

// Test that updating an populated cache with no webstates results in an empty
// cache.
TEST_F(CertificatePolicyProfileStateAgentTest, PopulatedCacheNoWebstates) {
  // Populated caches.
  PopulatePolicyCache({"a.com", "b.com"}, RegularPolicyCache());
  PopulatePolicyCache({"a.com", "b.com"}, IncognitoPolicyCache());
  // No webstates.
  TriggerCertCacheUpdate();

  // Expect nothing in caches.
  EXPECT_FALSE(RegularPolicyCacheContainsHost("a.com"));
  EXPECT_FALSE(RegularPolicyCacheContainsHost("b.com"));
  EXPECT_FALSE(IncognitoPolicyCacheContainsHost("a.com"));
  EXPECT_FALSE(IncognitoPolicyCacheContainsHost("b.com"));
}

// Test that updating an empty cache with webstates having no certs results in
// an empty cache.
TEST_F(CertificatePolicyProfileStateAgentTest, EmptyCacheNoCertedWebstates) {
  // Empty cache.
  // Webstates without certs:
  PopulateWebStatesWithNoCerts();

  TriggerCertCacheUpdate();

  // Expect nothing is in the cache.
  EXPECT_FALSE(RegularPolicyCacheContainsHost("a.com"));
  EXPECT_FALSE(RegularPolicyCacheContainsHost("b.com"));
  EXPECT_FALSE(RegularPolicyCacheContainsHost("c.com"));
  EXPECT_FALSE(RegularPolicyCacheContainsHost("d.com"));
  EXPECT_FALSE(IncognitoPolicyCacheContainsHost("a.com"));
  EXPECT_FALSE(IncognitoPolicyCacheContainsHost("b.com"));
}

// Test that updating an empty cache with webstates having certs results in all
// webstate entries being in the cache. (Also incidentally tests that populating
// test fixture web states doesn't also populate the cache).
TEST_F(CertificatePolicyProfileStateAgentTest, EmptyCacheCertedWebstates) {
  // Fully populated webstates, empty cache.
  PopulateWebStates();

  // Expect the cache is actually empty.
  EXPECT_FALSE(RegularPolicyCacheContainsHost("a.com"));
  EXPECT_FALSE(RegularPolicyCacheContainsHost("b.com"));
  EXPECT_FALSE(RegularPolicyCacheContainsHost("c.com"));
  EXPECT_FALSE(RegularPolicyCacheContainsHost("d.com"));
  EXPECT_FALSE(IncognitoPolicyCacheContainsHost("a.com"));
  EXPECT_FALSE(IncognitoPolicyCacheContainsHost("b.com"));

  TriggerCertCacheUpdate();

  // Expect that entries for all web states are now in the cache.
  EXPECT_TRUE(RegularPolicyCacheContainsHost("a.com"));
  EXPECT_TRUE(RegularPolicyCacheContainsHost("b.com"));
  EXPECT_TRUE(RegularPolicyCacheContainsHost("c.com"));
  EXPECT_TRUE(RegularPolicyCacheContainsHost("d.com"));
  EXPECT_TRUE(IncognitoPolicyCacheContainsHost("a.com"));
  EXPECT_TRUE(IncognitoPolicyCacheContainsHost("b.com"));
}

// Tests that entries in a cache that aren't in the webstates are removed.
TEST_F(CertificatePolicyProfileStateAgentTest, CacheHasExtraCerts) {
  // Fully populated web states.
  PopulateWebStates();

  // Populate the caches with entries for all webstates, and some extras --
  // {e,f}.com in the regular cache, and c.com in the incognito cache.
  PopulatePolicyCache({"a.com", "b.com", "c.com", "d.com", "e.com", "f.com"},
                      RegularPolicyCache());
  PopulatePolicyCache({"a.com", "b.com", "c.com"}, IncognitoPolicyCache());

  TriggerCertCacheUpdate();

  // Expect that the entries corresponding to the webstates are in the cache.
  EXPECT_TRUE(RegularPolicyCacheContainsHost("a.com"));
  EXPECT_TRUE(RegularPolicyCacheContainsHost("b.com"));
  EXPECT_TRUE(RegularPolicyCacheContainsHost("c.com"));
  EXPECT_TRUE(RegularPolicyCacheContainsHost("d.com"));
  EXPECT_TRUE(IncognitoPolicyCacheContainsHost("a.com"));
  EXPECT_TRUE(IncognitoPolicyCacheContainsHost("b.com"));

  // Expect the extra entries are not in the cache.
  EXPECT_FALSE(RegularPolicyCacheContainsHost("e.com"));
  EXPECT_FALSE(RegularPolicyCacheContainsHost("f.com"));
  EXPECT_FALSE(IncognitoPolicyCacheContainsHost("c.com"));
}

// Tests that a cache containing some (but not all) of the entries in the web
// states, and some extra entries, is properly updates to contain all and only
// the entries in the web states.
TEST_F(CertificatePolicyProfileStateAgentTest, CacheAndWebstatesDiffer) {
  // Fully populated web states.
  PopulateWebStates();

  // Populate the caches with entries for some webstates ({a,b}.com), and some
  // extras -- ({e,f}.com in the regular cache, and c.com in the incognito
  // cache).
  PopulatePolicyCache({"a.com", "b.com", "e.com", "f.com"},
                      RegularPolicyCache());
  PopulatePolicyCache({"a.com", "c.com"}, IncognitoPolicyCache());

  TriggerCertCacheUpdate();

  // Expect that entries for all of the webstates are in the cache.
  EXPECT_TRUE(RegularPolicyCacheContainsHost("a.com"));
  EXPECT_TRUE(RegularPolicyCacheContainsHost("b.com"));
  EXPECT_TRUE(RegularPolicyCacheContainsHost("c.com"));
  EXPECT_TRUE(RegularPolicyCacheContainsHost("d.com"));
  EXPECT_TRUE(IncognitoPolicyCacheContainsHost("a.com"));
  EXPECT_TRUE(IncognitoPolicyCacheContainsHost("b.com"));

  // Expect that none of the "extra" entries are in the cache.
  EXPECT_FALSE(RegularPolicyCacheContainsHost("e.com"));
  EXPECT_FALSE(RegularPolicyCacheContainsHost("f.com"));
  EXPECT_FALSE(IncognitoPolicyCacheContainsHost("c.com"));
}
