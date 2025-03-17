// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/btm_service.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_file_util.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/browser_context_impl.h"
#include "content/browser/btm/btm_bounce_detector.h"
#include "content/browser/btm/btm_service_impl.h"
#include "content/browser/btm/btm_state.h"
#include "content/browser/btm/btm_test_utils.h"
#include "content/browser/btm/btm_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/btm_redirect_info.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/btm_service_test_utils.h"
#include "content/public/test/mock_browsing_data_remover_delegate.h"
#include "content/public/test/test_browser_context.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/http/http_status_code.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

using testing::AllOf;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Pair;

namespace content {

bool Has3pcException(BrowserContext* browser_context,
                     WebContents* web_contents,
                     const GURL& url,
                     const GURL& initial_url,
                     const GURL& final_url) {
  BtmRedirectInfoPtr redirect = BtmRedirectInfo::CreateForServer(
      UrlAndSourceId(url, ukm::kInvalidSourceId), BtmDataAccessType::kWrite,
      base::Time::Now(), false, net::HTTP_FOUND, base::TimeDelta());
  btm::Populate3PcExceptions(browser_context, web_contents, initial_url,
                             final_url, base::span_from_ref(redirect));
  return redirect->has_3pc_exception.value();
}

class BtmServiceTest : public testing::Test {
 protected:
  base::PassKey<BtmServiceTest> PassKey() { return {}; }

  void RecordBounce(
      BrowserContext* browser_context,
      const GURL& url,
      const GURL& initial_url,
      const GURL& final_url,
      base::Time time,
      bool stateful,
      base::RepeatingCallback<void(const GURL&)> stateful_bounce_callback) {
    BtmServiceImpl::Get(browser_context)
        ->RecordBounceForTesting(url,
                                 Has3pcException(browser_context, nullptr, url,
                                                 initial_url, final_url),
                                 final_url, time, stateful,
                                 stateful_bounce_callback);
  }

 private:
  BrowserTaskEnvironment task_environment_;
};

TEST_F(BtmServiceTest, CreateServiceIfFeatureEnabled) {
  ScopedInitBtmFeature init_dips(true);

  TestBrowserContext profile;
  EXPECT_NE(BtmServiceImpl::Get(&profile), nullptr);
}

TEST_F(BtmServiceTest, DontCreateServiceIfFeatureDisabled) {
  ScopedInitBtmFeature init_dips(false);

  TestBrowserContext profile;
  EXPECT_EQ(BtmServiceImpl::Get(&profile), nullptr);
}

// Verifies that if database persistence is disabled via Finch, then when the
// BTM Service is constructed, it deletes any BTM Database files for the
// associated BrowserContext.
TEST_F(BtmServiceTest, DeleteDbFilesIfPersistenceDisabled) {
  base::FilePath data_path = base::CreateUniqueTempDirectoryScopedToTest();
  BtmServiceImpl* service;
  std::unique_ptr<TestBrowserContext> profile;

  // Ensure the BTM feature is enabled and the database is set to be persisted.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kBtm, {{"persist_database", "true"}});

  profile = std::make_unique<TestBrowserContext>(data_path);
  service = BtmServiceImpl::Get(profile.get());
  ASSERT_NE(service, nullptr);

  // Ensure the database files have been created and are NOT deleted since the
  // BTM feature is enabled.
  WaitOnStorage(service);
  service->WaitForFileDeletionCompleteForTesting();
  ASSERT_TRUE(base::PathExists(GetBtmFilePath(profile.get())));

  // Reset the feature list to set database persistence to false.
  feature_list.Reset();
  feature_list.InitAndEnableFeatureWithParameters(
      features::kBtm, {{"persist_database", "false"}});

  // Reset the TestBrowserContext, then create a new instance with the same user
  // data path.
  profile.reset();
  profile = std::make_unique<TestBrowserContext>(data_path);

  service = BtmServiceImpl::Get(profile.get());
  ASSERT_NE(service, nullptr);

  // Ensure the database files ARE deleted since the BTM feature is disabled.
  WaitOnStorage(service);
  service->WaitForFileDeletionCompleteForTesting();
  EXPECT_FALSE(base::PathExists(GetBtmFilePath(profile.get())));
}

// Verifies that when an OTR profile is opened, the BTM database file for
// the underlying regular profile is NOT deleted.
TEST_F(BtmServiceTest, PreserveRegularProfileDbFiles) {
  base::FilePath data_path = base::CreateUniqueTempDirectoryScopedToTest();

  // Ensure the BTM feature is enabled and the database is set to be persisted.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kBtm, {{"persist_database", "true"}});

  // Build a regular profile.
  std::unique_ptr<TestBrowserContext> profile =
      std::make_unique<TestBrowserContext>(data_path);
  BtmServiceImpl* service = BtmServiceImpl::Get(profile.get());
  ASSERT_NE(service, nullptr);

  // Ensure the regular profile's database files have been created since the
  // BTM feature and persistence are enabled.
  WaitOnStorage(service);
  service->WaitForFileDeletionCompleteForTesting();
  ASSERT_TRUE(base::PathExists(GetBtmFilePath(profile.get())));

  // Build an off-the-record profile based on `profile`.
  std::unique_ptr<TestBrowserContext> otr_profile =
      std::make_unique<TestBrowserContext>(profile->GetPath());
  otr_profile->set_is_off_the_record(true);
  BtmServiceImpl* otr_service = BtmServiceImpl::Get(otr_profile.get());
  ASSERT_NE(otr_service, nullptr);

  // Ensure the OTR profile's database has been initialized and any file
  // deletion tasks have finished (although there shouldn't be any).
  WaitOnStorage(otr_service);
  otr_service->WaitForFileDeletionCompleteForTesting();

  // Ensure the regular profile's database files were NOT deleted.
  EXPECT_TRUE(base::PathExists(GetBtmFilePath(profile.get())));

  // Every TestBrowserContext normally deletes its folder when it's destroyed.
  // But since `otr_profile` is sharing `profile`'s directory, we don't want it
  // to delete that folder (`profile` will).
  otr_profile->TakePath();
}

TEST_F(BtmServiceTest, EmptySiteEventsIgnored) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kBtm);
  std::unique_ptr<TestBrowserContext> profile =
      std::make_unique<TestBrowserContext>();
  BtmServiceImpl* service = BtmServiceImpl::Get(profile.get());

  // Record a bounce for an empty URL.
  GURL url;
  base::Time bounce = base::Time::FromSecondsSinceUnixEpoch(2);
  RecordBounce(profile.get(), url, GURL("https://initial.com"),
               GURL("https://final.com"), bounce, false,
               base::BindRepeating([](const GURL& final_url) {}));
  WaitOnStorage(service);

  // Verify that an entry is not returned when querying for an empty URL,
  StateForURLCallback callback = base::BindLambdaForTesting(
      [&](BtmState state) { EXPECT_FALSE(state.was_loaded()); });
  service->storage()
      ->AsyncCall(&BtmStorage::Read)
      .WithArgs(url)
      .Then(std::move(callback));
  WaitOnStorage(service);
}

class BtmServiceStateRemovalTest : public testing::Test {
 public:
  BtmServiceStateRemovalTest()
      : profile_(std::make_unique<TestBrowserContext>()),
        service_(BtmServiceImpl::Get(GetProfile())) {
    SetBrowserClientForTesting(&browser_client_);
  }

  base::TimeDelta grace_period;
  base::TimeDelta interaction_ttl;
  base::TimeDelta tiny_delta = base::Milliseconds(1);

  BrowserContext* GetProfile() { return profile_.get(); }
  BtmServiceImpl* GetService() { return service_; }

 protected:
  TpcBlockingBrowserClient browser_client_;
  BrowserTaskEnvironment task_environment_;
  MockBrowsingDataRemoverDelegate delegate_;

  // Test setup.
  void SetUp() override {
    grace_period = features::kBtmGracePeriod.Get();
    interaction_ttl = features::kBtmInteractionTtl.Get();
    ASSERT_LT(tiny_delta, grace_period);

    GetProfile()->GetBrowsingDataRemover()->SetEmbedderDelegate(&delegate_);
    browser_client_.SetBlockThirdPartyCookiesByDefault(true);

    DCHECK(service_);
    service_->SetStorageClockForTesting(&clock_);
    WaitOnStorage(GetService());
  }

  void TearDown() override {
    profile_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void AdvanceTimeTo(base::Time now) {
    ASSERT_GE(now, clock_.Now());
    clock_.SetNow(now);
  }

  base::Time Now() { return clock_.Now(); }
  void SetNow(base::Time now) { clock_.SetNow(now); }

  void AdvanceTimeBy(base::TimeDelta delta) { clock_.Advance(delta); }

  void FireBtmTimer() {
    service_->OnTimerFiredForTesting();
    WaitOnStorage(GetService());
  }

  // Add an exception to the third-party cookie blocking rule for
  // |third_party_url| embedded by |first_party_url|.
  void Add3PCException(const GURL& first_party_url,
                       const GURL& third_party_url) {
    browser_client_.GrantCookieAccessDueToHeuristic(
        profile_.get(), net::SchemefulSite(first_party_url),
        net::SchemefulSite(third_party_url), base::Days(1),
        /*ignore_schemes=*/false);

    auto* client = GetContentClientForTesting()->browser();
    EXPECT_TRUE(client->IsFullCookieAccessAllowed(
        profile_.get(), nullptr, third_party_url,
        blink::StorageKey::CreateFirstParty(
            url::Origin::Create(first_party_url))));
    EXPECT_FALSE(client->IsFullCookieAccessAllowed(
        profile_.get(), nullptr, first_party_url,
        blink::StorageKey::CreateFirstParty(
            url::Origin::Create(third_party_url))));
  }

  void RecordBounce(
      const GURL& url,
      const GURL& initial_url,
      const GURL& final_url,
      base::Time time,
      bool stateful,
      base::RepeatingCallback<void(const GURL&)> stateful_bounce_callback) {
    GetService()->RecordBounceForTesting(
        url,
        Has3pcException(GetProfile(), nullptr, url, initial_url, final_url),
        final_url, time, stateful, stateful_bounce_callback);
  }

 private:
  base::SimpleTestClock clock_;

  std::unique_ptr<TestBrowserContext> profile_;
  raw_ptr<BtmServiceImpl, DanglingUntriaged> service_ = nullptr;
};

namespace {
class RedirectChainCounter : public BtmService::Observer {
 public:
  explicit RedirectChainCounter(BtmService* service) { obs_.Observe(service); }

  size_t count() const { return count_; }

 private:
  void OnChainHandled(const std::vector<BtmRedirectInfoPtr>& redirects,
                      const BtmRedirectChainInfoPtr& chain) override {
    count_++;
  }

  size_t count_ = 0;
  base::ScopedObservation<BtmService, Observer> obs_{this};
};
}  // namespace

TEST_F(BtmServiceStateRemovalTest,
       CompleteChain_NotifiesDipsRedirectChainObservers) {
  GetService()->SetStorageClockForTesting(base::DefaultClock::GetInstance());
  RedirectChainCounter chain_counter(GetService());

  std::vector<BtmRedirectInfoPtr> complete_redirects;
  complete_redirects.push_back(BtmRedirectInfo::CreateForServer(
      /*url=*/MakeUrlAndId("http://b.test/"),
      /*access_type=*/BtmDataAccessType::kNone,
      /*time=*/Now(),
      /*was_response_cached=*/false,
      /*response_code=*/net::HTTP_FOUND,
      /*server_bounce_delay=*/base::TimeDelta()));
  auto complete_chain = std::make_unique<BtmRedirectChainInfo>(
      /*initial_url=*/MakeUrlAndId("http://a.test/"),
      /*final_url=*/MakeUrlAndId("http://c.test/"),
      /*length=*/1, /*is_partial_chain=*/false);

  btm::Populate3PcExceptions(GetProfile(), /*web_contents=*/nullptr,
                             complete_chain->initial_url.url,
                             complete_chain->final_url.url, complete_redirects);
  GetService()->HandleRedirectChain(
      std::move(complete_redirects), std::move(complete_chain),
      base::BindRepeating([](const GURL& final_url) {}));
  WaitOnStorage(GetService());
  // Expect one call to Observer.OnChainHandled when handling a complete chain.
  EXPECT_EQ(chain_counter.count(), 1u);
}

TEST_F(BtmServiceStateRemovalTest,
       PartialChain_DoesNotNotifyDipsRedirectChainObservers) {
  GetService()->SetStorageClockForTesting(base::DefaultClock::GetInstance());
  RedirectChainCounter chain_counter(GetService());

  std::vector<BtmRedirectInfoPtr> partial_redirects;
  partial_redirects.push_back(BtmRedirectInfo::CreateForServer(
      /*url=*/MakeUrlAndId("http://b.test/"),
      /*access_type=*/BtmDataAccessType::kNone,
      /*time=*/Now(),
      /*was_response_cached=*/false,
      /*response_code=*/net::HTTP_FOUND,
      /*server_bounce_delay=*/base::TimeDelta()));
  auto partial_chain = std::make_unique<BtmRedirectChainInfo>(
      /*initial_url=*/MakeUrlAndId("http://a.test/"),
      /*final_url=*/MakeUrlAndId("http://c.test/"),
      /*length=*/1, /*is_partial_chain=*/true);

  btm::Populate3PcExceptions(GetProfile(), /*web_contents=*/nullptr,
                             partial_chain->initial_url.url,
                             partial_chain->final_url.url, partial_redirects);
  GetService()->HandleRedirectChain(
      std::move(partial_redirects), std::move(partial_chain),
      base::BindRepeating([](const GURL& final_url) {}));
  WaitOnStorage(GetService());
  // Expect no calls to Observer.OnChainHandled when handling a partial chain.
  EXPECT_EQ(chain_counter.count(), 0u);
}

// NOTE: The use of a MockBrowsingDataRemoverDelegate in this test fixture
// means that when BTM deletion is enabled, the row for 'url' is not actually
// removed from the BTM db since 'delegate_' doesn't actually carryout the
// removal task.
TEST_F(BtmServiceStateRemovalTest, DISABLED_BrowsingDataDeletion_Enabled) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kBtm, {{"delete", "true"}, {"triggering_action", "bounce"}});

  // Record a bounce.
  GURL url("https://example.com");
  base::Time bounce = base::Time::FromSecondsSinceUnixEpoch(2);
  RecordBounce(url, GURL("https://initial.com"), GURL("https://final.com"),
               bounce, false,
               base::BindRepeating([](const GURL& final_url) {}));
  WaitOnStorage(GetService());
  EXPECT_TRUE(GetBtmState(GetService(), url).has_value());

  // Set the current time to just after the bounce happened.
  AdvanceTimeTo(bounce + tiny_delta);
  FireBtmTimer();
  task_environment_.RunUntilIdle();

  // Verify a removal task was not posted to the BrowsingDataRemover(Delegate).
  delegate_.VerifyAndClearExpectations();

  auto filter_builder = BrowsingDataFilterBuilder::Create(
      BrowsingDataFilterBuilder::Mode::kDelete);
  filter_builder->AddRegisterableDomain(GetSiteForBtm(url));
  filter_builder->SetCookiePartitionKeyCollection(
      net::CookiePartitionKeyCollection());
  delegate_.ExpectCall(
      base::Time::Min(), base::Time::Max(),
      (ContentBrowserClient::kDefaultDipsRemoveMask &
       ~BrowsingDataRemover::DATA_TYPE_PRIVACY_SANDBOX) |
          BrowsingDataRemover::DATA_TYPE_AVOID_CLOSING_CONNECTIONS,
      BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
          BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
      filter_builder.get());
  // We don't test the filter builder for partitioned cookies here because it's
  // messy. The browser tests ensure that it behaves as expected.
  delegate_.ExpectCallDontCareAboutFilterBuilder(
      base::Time::Min(), base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_COOKIES,
      BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
          BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB);

  // Time-travel to after the grace period has ended for the bounce.
  AdvanceTimeTo(bounce + grace_period + tiny_delta);
  FireBtmTimer();
  task_environment_.RunUntilIdle();

  // Verify that a removal task was posted to the BrowsingDataRemover(Delegate)
  // for 'url'.
  delegate_.VerifyAndClearExpectations();
  // Because this test fixture uses a MockBrowsingDataRemoverDelegate the BTM
  // entry should not actually be removed. However, in practice it would be.
  EXPECT_TRUE(GetBtmState(GetService(), url).has_value());

  EXPECT_THAT(ukm_recorder,
              EntryUrlsAre("DIPS.Deletion", {"http://example.com/"}));
}

TEST_F(BtmServiceStateRemovalTest, BrowsingDataDeletion_Disabled) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kBtm, {{"delete", "false"}, {"triggering_action", "bounce"}});

  // Record a bounce.
  GURL url("https://example.com");
  base::Time bounce = base::Time::FromSecondsSinceUnixEpoch(2);
  RecordBounce(url, GURL("https://initial.com"), GURL("https://final.com"),
               bounce, false,
               base::BindRepeating([](const GURL& final_url) {}));
  WaitOnStorage(GetService());
  EXPECT_TRUE(GetBtmState(GetService(), url).has_value());

  // Set the current time to just after the bounce happened.
  AdvanceTimeTo(bounce + tiny_delta);
  FireBtmTimer();
  task_environment_.RunUntilIdle();

  // Verify the BTM entry was not removed and a removal task was not posted to
  // the BrowsingDataRemover(Delegate).
  delegate_.VerifyAndClearExpectations();
  EXPECT_TRUE(GetBtmState(GetService(), url).has_value());

  // Time-travel to after the grace period has ended for the bounce.
  AdvanceTimeTo(bounce + grace_period + tiny_delta);
  FireBtmTimer();
  task_environment_.RunUntilIdle();

  // Verify that the site's BTM entry WAS removed, but a removal task was NOT
  // posted to the BrowsingDataRemover(Delegate) since
  // `features::kBtmDeletionEnabled` is false.
  delegate_.VerifyAndClearExpectations();
  EXPECT_FALSE(GetBtmState(GetService(), url).has_value());

  EXPECT_THAT(ukm_recorder,
              EntryUrlsAre("DIPS.Deletion", {"http://example.com/"}));
}

TEST_F(BtmServiceStateRemovalTest,
       BrowsingDataDeletion_Respects3PExceptionsFor3PC) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kBtm, {{"delete", "true"}, {"triggering_action", "bounce"}});

  GURL excepted_3p_url("https://excepted-as-3p.com");
  GURL non_excepted_url("https://not-excepted.com");

  browser_client_.GrantCookieAccessTo3pSite(excepted_3p_url);

  int stateful_bounce_count = 0;
  base::RepeatingCallback<void(const GURL&)> increment_bounce =
      base::BindLambdaForTesting(
          [&](const GURL& final_url) { stateful_bounce_count++; });

  // Bounce through both tracking sites.
  base::Time bounce = base::Time::FromSecondsSinceUnixEpoch(2);
  RecordBounce(excepted_3p_url, GURL("https://initial.com"),
               GURL("https://final.com"), bounce, true, increment_bounce);
  RecordBounce(non_excepted_url, GURL("https://initial.com"),
               GURL("https://final.com"), bounce, true, increment_bounce);
  WaitOnStorage(GetService());

  // Verify that the bounce was not recorded for the excepted 3P URL.
  EXPECT_FALSE(GetBtmState(GetService(), excepted_3p_url).has_value());
  EXPECT_TRUE(GetBtmState(GetService(), non_excepted_url).has_value());

  // Time-travel to after the grace period has ended for the bounce.
  AdvanceTimeTo(bounce + grace_period + tiny_delta);
  FireBtmTimer();
  task_environment_.RunUntilIdle();

  // Only the non-excepted site should be reported to UKM.
  EXPECT_THAT(ukm_recorder,
              EntryUrlsAre("DIPS.Deletion", {"http://not-excepted.com/"}));

  // Expect one recorded bounce, for the stateful redirect through the
  // non-excepted site.
  EXPECT_EQ(stateful_bounce_count, 1);
}

TEST_F(BtmServiceStateRemovalTest,
       BrowsingDataDeletion_Respects1PExceptionsFor3PC) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kBtm, {{"delete", "true"}, {"triggering_action", "bounce"}});

  GURL excepted_1p_url("https://excepted-as-1p.com");
  GURL scoped_excepted_1p_url("https://excepted-as-1p-with-3p.com");
  GURL non_excepted_url("https://not-excepted.com");
  GURL redirect_url_1("https://redirect-1.com");
  GURL redirect_url_2("https://redirect-2.com");
  GURL redirect_url_3("https://redirect-3.com");

  browser_client_.AllowThirdPartyCookiesOnSite(excepted_1p_url);
  Add3PCException(scoped_excepted_1p_url, redirect_url_1);

  int stateful_bounce_count = 0;
  base::RepeatingCallback<void(const GURL&)> increment_bounce =
      base::BindLambdaForTesting(
          [&](const GURL& final_url) { stateful_bounce_count++; });

  base::Time bounce = base::Time::FromSecondsSinceUnixEpoch(2);
  // Record a bounce through redirect_url_1 that starts on an excepted
  // URL.
  RecordBounce(redirect_url_1, excepted_1p_url, non_excepted_url, bounce, true,
               increment_bounce);
  // Record a bounce through redirect_url_1 that ends on an excepted
  // URL.
  RecordBounce(redirect_url_1, non_excepted_url, excepted_1p_url, bounce, true,
               increment_bounce);
  // Record a bounce through redirect_url_1 that ends on a URL with an exception
  // scoped to redirect_url_1.
  RecordBounce(redirect_url_1, non_excepted_url, scoped_excepted_1p_url, bounce,
               true, increment_bounce);
  // Record a bounce through redirect_url_2 that does not start or
  // end on an excepted URL.
  RecordBounce(redirect_url_2, non_excepted_url, non_excepted_url, bounce, true,
               increment_bounce);
  // Record a bounce through redirect_url_3 that does not start or
  // end on an excepted URL. Record an interaction on this URL as well.
  RecordBounce(redirect_url_3, non_excepted_url, non_excepted_url, bounce, true,
               increment_bounce);
  GetService()
      ->storage()
      ->AsyncCall(&BtmStorage::RecordUserActivation)
      .WithArgs(redirect_url_3, bounce, GetService()->GetCookieMode());
  WaitOnStorage(GetService());

  // Expect no recorded BtmState for redirect_url_1, since every
  // recorded bounce started or ended on an excepted site.
  EXPECT_FALSE(GetBtmState(GetService(), redirect_url_1).has_value());
  EXPECT_TRUE(GetBtmState(GetService(), redirect_url_2).has_value());
  EXPECT_TRUE(GetBtmState(GetService(), redirect_url_3).has_value());

  // Record a bounce through redirect_url_2 that starts on an
  // excepted URL. This should clear the DB entry for redirect_url_2.
  RecordBounce(redirect_url_2, excepted_1p_url, non_excepted_url, bounce, true,
               increment_bounce);
  EXPECT_FALSE(GetBtmState(GetService(), redirect_url_2).has_value());

  // Record a bounce through redirect_url_3 that starts on an
  // excepted URL. This should not clear the DB entry for redirect_url_3 as it
  // has a recorded interaction.
  RecordBounce(redirect_url_3, excepted_1p_url, non_excepted_url, bounce, true,
               increment_bounce);
  EXPECT_TRUE(GetBtmState(GetService(), redirect_url_3).has_value());

  // Expect two non-excepted stateful redirects: the first bounces through
  // redirect_url_2 and redirect_url_3.
  EXPECT_EQ(stateful_bounce_count, 2);
}

// TODO: crbug.com/376625002 - temporarily disabled for the move to //content,
// where there's no HostContentSettingsMap. Find an appropriate way to implement
// this test in //content or move it back to //chrome.
TEST_F(BtmServiceStateRemovalTest,
       DISABLED_BrowsingDataDeletion_RespectsStorageAccessGrantExceptions) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  std::vector<base::test::FeatureRefAndParams> enabled_features;
  enabled_features.push_back(
      {features::kBtm, {{"delete", "true"}, {"triggering_action", "bounce"}}});
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(enabled_features, {});

  GURL storage_access_grant_url("https://storage-access-granted.com");
  GURL top_level_storage_access_grant_url(
      "https://top-level-storage-access-granted.com");
  GURL no_grant_url("https://no-storage-access-grant.com");
  GURL redirect_url_1("https://redirect-1.com");
  GURL redirect_url_2("https://redirect-2.com");
  GURL redirect_url_3("https://redirect-3.com");

  // Create Storage Access grants for the required sites.
  /*
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromString("[*.]" +
                                         storage_access_grant_url.host()),
      ContentSettingsType::STORAGE_ACCESS, CONTENT_SETTING_ALLOW);
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromString(
          "[*.]" + top_level_storage_access_grant_url.host()),
      ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS, CONTENT_SETTING_ALLOW);
  */
  int stateful_bounce_count = 0;
  base::RepeatingCallback<void(const GURL&)> increment_bounce =
      base::BindLambdaForTesting(
          [&](const GURL& final_url) { stateful_bounce_count++; });

  base::Time bounce = base::Time::FromSecondsSinceUnixEpoch(2);
  // Record a bounce through redirect_url_1 that starts on a URL with an SA
  // grant.
  RecordBounce(redirect_url_1, storage_access_grant_url, no_grant_url, bounce,
               true, increment_bounce);
  // Record a bounce through redirect_url_1 that ends on a URL with a top-level
  // SA grant.
  RecordBounce(redirect_url_1, no_grant_url, top_level_storage_access_grant_url,
               bounce, true, increment_bounce);
  // Record a bounce through redirect_url_2 that does not start or
  // end on a URL with an SA grant.
  RecordBounce(redirect_url_2, no_grant_url, no_grant_url, bounce, true,
               increment_bounce);
  // Record a bounce through redirect_url_3 that does not start or
  // end on a URL with an SA grant. Record an interaction on this URL as well.
  RecordBounce(redirect_url_3, no_grant_url, no_grant_url, bounce, true,
               increment_bounce);
  GetService()
      ->storage()
      ->AsyncCall(&BtmStorage::RecordUserActivation)
      .WithArgs(redirect_url_3, bounce, GetService()->GetCookieMode());
  WaitOnStorage(GetService());

  // Expect no recorded BtmState for redirect_url_1, since every
  // recorded bounce started or ended on a site with an SA grant.
  EXPECT_FALSE(GetBtmState(GetService(), redirect_url_1).has_value());
  EXPECT_TRUE(GetBtmState(GetService(), redirect_url_2).has_value());
  EXPECT_TRUE(GetBtmState(GetService(), redirect_url_3).has_value());

  // Record a bounce through redirect_url_2 that starts on a URL with an SA
  // grant. This should clear the DB entry for redirect_url_2.
  RecordBounce(redirect_url_2, storage_access_grant_url, no_grant_url, bounce,
               true, increment_bounce);
  EXPECT_FALSE(GetBtmState(GetService(), redirect_url_2).has_value());

  // Record a bounce through redirect_url_3 that starts on a URL with an SA
  // grant. This should not clear the DB entry for redirect_url_3 as it has a
  // recorded interaction.
  RecordBounce(redirect_url_3, storage_access_grant_url, no_grant_url, bounce,
               true, increment_bounce);
  EXPECT_TRUE(GetBtmState(GetService(), redirect_url_3).has_value());

  // Expect two non-SA stateful redirects: the first bounces through
  // redirect_url_2 and redirect_url_3.
  EXPECT_EQ(stateful_bounce_count, 2);
}

// When third-party cookies are globally allowed, bounces should be recorded for
// sites which have an exception to block 3PC, but not by default.
TEST_F(
    BtmServiceStateRemovalTest,
    BrowsingDataDeletion_Respects1PExceptionsForBlocking3PCWhenDefaultAllowed) {
  browser_client_.SetBlockThirdPartyCookiesByDefault(false);

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kBtm, {{"delete", "true"}, {"triggering_action", "bounce"}});

  GURL blocked_1p_url("https://excepted-as-1p.com");
  GURL scoped_blocked_1p_url("https://excepted-as-1p-with-3p.com");
  GURL non_blocked_url("https://not-excepted.com");
  GURL redirect_url_1("https://redirect-1.com");
  GURL redirect_url_2("https://redirect-2.com");
  GURL redirect_url_3("https://redirect-3.com");
  GURL redirect_url_4("https://redirect-4.com");

  // Exceptions to block third-party cookies.
  browser_client_.BlockThirdPartyCookiesOnSite(blocked_1p_url);
  browser_client_.BlockThirdPartyCookies(redirect_url_1, scoped_blocked_1p_url);

  int stateful_bounce_count = 0;
  base::RepeatingCallback<void(const GURL&)> increment_bounce =
      base::BindLambdaForTesting(
          [&](const GURL& final_url) { stateful_bounce_count++; });

  base::Time bounce = base::Time::FromSecondsSinceUnixEpoch(2);
  // Record a bounce through redirect_url_1 that starts and ends on blocked
  // URLs.
  RecordBounce(redirect_url_1, blocked_1p_url, scoped_blocked_1p_url, bounce,
               true, increment_bounce);
  // Record a bounce through redirect_url_2 that starts and ends on blocked
  // URLs. Record an interaction on this URL as well.
  RecordBounce(redirect_url_2, blocked_1p_url, blocked_1p_url, bounce, true,
               increment_bounce);
  GetService()
      ->storage()
      ->AsyncCall(&BtmStorage::RecordUserActivation)
      .WithArgs(redirect_url_2, bounce, GetService()->GetCookieMode());
  WaitOnStorage(GetService());
  // Record a bounce through redirect_url_3 that starts on a non-blocked URL.
  RecordBounce(redirect_url_3, non_blocked_url, blocked_1p_url, bounce, true,
               increment_bounce);
  // Record a bounce through redirect_url_4 that ends on a non-blocked URL.
  RecordBounce(redirect_url_4, blocked_1p_url, non_blocked_url, bounce, true,
               increment_bounce);

  // Expect a recorded BtmState for redirect_url_1 and redirect_url_2, since
  // they were bounced through with blocking exceptions on both the initial and
  // final URL. The other two trackers were only bounced through from
  // default-allowed sites.
  EXPECT_TRUE(GetBtmState(GetService(), redirect_url_1).has_value());
  EXPECT_TRUE(GetBtmState(GetService(), redirect_url_2).has_value());
  EXPECT_FALSE(GetBtmState(GetService(), redirect_url_3).has_value());
  EXPECT_FALSE(GetBtmState(GetService(), redirect_url_4).has_value());

  // Record a bounce through redirect_url_1 that starts on a non-blocked URL.
  // This should clear the DB entry for redirect_url_1.
  RecordBounce(redirect_url_1, non_blocked_url, blocked_1p_url, bounce, true,
               increment_bounce);
  EXPECT_FALSE(GetBtmState(GetService(), redirect_url_1).has_value());

  // Record a bounce through redirect_url_2 that starts on a
  // blocked URL. This should not clear the DB entry for redirect_url_2 as it
  // has a recorded interaction.
  RecordBounce(redirect_url_2, non_blocked_url, blocked_1p_url, bounce, true,
               increment_bounce);
  EXPECT_TRUE(GetBtmState(GetService(), redirect_url_2).has_value());

  // Expect two recorded stateful redirects: the first bounces through
  // redirect_url_1 and redirect_url_2.
  EXPECT_EQ(stateful_bounce_count, 2);
}

TEST_F(BtmServiceStateRemovalTest, ImmediateEnforcement) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kBtm, {{"delete", "true"}, {"triggering_action", "bounce"}});
  SetNow(base::Time::FromSecondsSinceUnixEpoch(2));

  // Record a bounce.
  GURL url("https://example.com");
  base::Time bounce = Now();
  RecordBounce(url, GURL("https://initial.com"), GURL("https://final.com"),
               bounce, false,
               base::BindRepeating([](const GURL& final_url) {}));
  WaitOnStorage(GetService());
  EXPECT_TRUE(GetBtmState(GetService(), url).has_value());

  // Set the current time to just after the bounce happened and simulate firing
  // the BTM timer.
  AdvanceTimeTo(bounce + tiny_delta);
  FireBtmTimer();
  task_environment_.RunUntilIdle();

  // Verify a removal task was not posted to the BrowsingDataRemover(Delegate).
  delegate_.VerifyAndClearExpectations();

  auto filter_builder = BrowsingDataFilterBuilder::Create(
      BrowsingDataFilterBuilder::Mode::kDelete);
  filter_builder->AddRegisterableDomain(GetSiteForBtm(url));
  filter_builder->SetCookiePartitionKeyCollection(
      net::CookiePartitionKeyCollection());
  delegate_.ExpectCall(
      base::Time::Min(), base::Time::Max(),
      (ContentBrowserClient::kDefaultDipsRemoveMask &
       ~BrowsingDataRemover::DATA_TYPE_PRIVACY_SANDBOX) |
          BrowsingDataRemover::DATA_TYPE_AVOID_CLOSING_CONNECTIONS,
      BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
          BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
      filter_builder.get());
  // We don't test the filter builder for partitioned cookies here because it's
  // messy. The browser tests ensure that it behaves as expected.
  delegate_.ExpectCallDontCareAboutFilterBuilder(
      base::Time::Min(), base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_COOKIES,
      BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
          BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB);

  // Perform immediate enforcement of deletion, without regard for grace period
  // and verify `url` is returned the `DeletedSitesCallback`.
  base::RunLoop run_loop;
  base::OnceCallback<void(const std::vector<std::string>& sites)> callback =
      base::BindLambdaForTesting(
          [&](const std::vector<std::string>& deleted_sites) {
            EXPECT_THAT(deleted_sites,
                        testing::UnorderedElementsAre(GetSiteForBtm(url)));
            run_loop.Quit();
          });
  GetService()->DeleteEligibleSitesImmediately(std::move(callback));
  task_environment_.RunUntilIdle();
  run_loop.Run();

  // Verify that a removal task was posted to the BrowsingDataRemover(Delegate)
  // for 'url'.
  delegate_.VerifyAndClearExpectations();
}

// A test class that verifies BtmService state deletion metrics collection
// behavior.
class BtmServiceHistogramTest : public BtmServiceStateRemovalTest {
 public:
  BtmServiceHistogramTest() = default;

  const base::HistogramTester& histograms() const { return histogram_tester_; }

 protected:
  const std::string kBlock3PC = "Block3PC";
  const std::string kUmaHistogramDeletionPrefix = "Privacy.DIPS.Deletion.";
  const std::string kServerRedirectsDelayHist =
      "Privacy.DIPS.ServerBounceDelay";
  const std::string kServerRedirectsChainDelayHist =
      "Privacy.DIPS.ServerBounceChainDelay";
  const std::string kServerRedirectsStatusCodePrefix =
      "Privacy.DIPS.BounceStatusCode.";
  const std::string kNoCache = "NoCache";
  const std::string kCached = "Cached";

  base::HistogramTester histogram_tester_;
};

TEST_F(BtmServiceHistogramTest, DeletionLatency) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kBtm, {{"delete", "false"}, {"triggering_action", "bounce"}});

  // Verify the histogram starts empty
  histograms().ExpectTotalCount("Privacy.DIPS.DeletionLatency2", 0);

  // Record a bounce.
  GURL url("https://example.com");
  base::Time bounce = base::Time::FromSecondsSinceUnixEpoch(2);
  RecordBounce(url, GURL("https://initial.com"), GURL("https://final.com"),
               bounce, false,
               base::BindRepeating([](const GURL& final_url) {}));
  WaitOnStorage(GetService());

  // Set the current time to just after the bounce happened.
  AdvanceTimeTo(bounce + tiny_delta);
  FireBtmTimer();
  task_environment_.RunUntilIdle();

  // Verify deletion latency metrics were NOT emitted and the BTM entry was NOT
  // removed.
  histograms().ExpectTotalCount("Privacy.DIPS.DeletionLatency2", 0);
  EXPECT_TRUE(GetBtmState(GetService(), url).has_value());

  // Time-travel to after the grace period has ended for the bounce.
  AdvanceTimeTo(bounce + grace_period + tiny_delta);
  FireBtmTimer();
  task_environment_.RunUntilIdle();

  // Verify a deletion latency metric was emitted and the BTM entry was
  // removed.
  histograms().ExpectTotalCount("Privacy.DIPS.DeletionLatency2", 1);
  EXPECT_FALSE(GetBtmState(GetService(), url).has_value());
}

TEST_F(BtmServiceHistogramTest, Deletion_Disallowed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kBtm,
      {{"delete", "false"}, {"triggering_action", "stateful_bounce"}});

  // Verify the histogram is initially empty.
  EXPECT_TRUE(histograms()
                  .GetTotalCountsForPrefix(kUmaHistogramDeletionPrefix)
                  .empty());

  // Record a bounce.
  GURL url("https://example.com");
  base::Time bounce_time = base::Time::FromSecondsSinceUnixEpoch(2);
  RecordBounce(url, GURL("https://initial.com"), GURL("https://final.com"),
               bounce_time, true,
               base::BindRepeating([](const GURL& final_url) {}));
  WaitOnStorage(GetService());

  // Time-travel to after the grace period has ended for the bounce.
  AdvanceTimeTo(bounce_time + grace_period + tiny_delta);
  FireBtmTimer();
  task_environment_.RunUntilIdle();

  // Verify a deletion metric was emitted and the BTM entry was removed.
  base::HistogramTester::CountsMap expected_counts;
  expected_counts[kUmaHistogramDeletionPrefix + kBlock3PC] = 1;
  EXPECT_THAT(histograms().GetTotalCountsForPrefix(kUmaHistogramDeletionPrefix),
              testing::ContainerEq(expected_counts));
  histograms().ExpectUniqueSample(kUmaHistogramDeletionPrefix + kBlock3PC,
                                  BtmDeletionAction::kDisallowed, 1);
  EXPECT_FALSE(GetBtmState(GetService(), url).has_value());
}

TEST_F(BtmServiceHistogramTest, Deletion_ExceptedAs1P) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kBtm,
      {{"delete", "true"}, {"triggering_action", "stateful_bounce"}});

  // Verify the histogram is initially empty.
  EXPECT_TRUE(histograms()
                  .GetTotalCountsForPrefix(kUmaHistogramDeletionPrefix)
                  .empty());

  // Record a bounce.
  GURL url("https://example.com");
  GURL excepted_1p_url("https://initial.com");
  browser_client_.AllowThirdPartyCookiesOnSite(excepted_1p_url);
  base::Time bounce_time = base::Time::FromSecondsSinceUnixEpoch(2);
  RecordBounce(url, excepted_1p_url, GURL("https://final.com"), bounce_time,
               true, base::BindRepeating([](const GURL& final_url) {}));
  WaitOnStorage(GetService());

  // Time-travel to after the grace period has ended for the bounce.
  AdvanceTimeTo(bounce_time + grace_period + tiny_delta);
  FireBtmTimer();
  task_environment_.RunUntilIdle();

  // Verify a deletion metric was emitted and the BTM entry was removed.
  base::HistogramTester::CountsMap expected_counts;
  expected_counts[kUmaHistogramDeletionPrefix + kBlock3PC] = 1;
  EXPECT_THAT(histograms().GetTotalCountsForPrefix(kUmaHistogramDeletionPrefix),
              testing::ContainerEq(expected_counts));
  histograms().ExpectUniqueSample(kUmaHistogramDeletionPrefix + kBlock3PC,
                                  BtmDeletionAction::kExcepted, 1);
  EXPECT_FALSE(GetBtmState(GetService(), url).has_value());
}

TEST_F(BtmServiceHistogramTest, Deletion_ExceptedAs3P) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kBtm,
      {{"delete", "true"}, {"triggering_action", "stateful_bounce"}});

  // Verify the histogram is initially empty.
  EXPECT_TRUE(histograms()
                  .GetTotalCountsForPrefix(kUmaHistogramDeletionPrefix)
                  .empty());

  // Record a bounce.
  GURL excepted_3p_url("https://example.com");
  browser_client_.GrantCookieAccessTo3pSite(excepted_3p_url);
  base::Time bounce_time = base::Time::FromSecondsSinceUnixEpoch(2);
  RecordBounce(excepted_3p_url, GURL("https://initial.com"),
               GURL("https://final.com"), bounce_time, true,
               base::BindRepeating([](const GURL& final_url) {}));
  WaitOnStorage(GetService());

  // Time-travel to after the grace period has ended for the bounce.
  AdvanceTimeTo(bounce_time + grace_period + tiny_delta);
  FireBtmTimer();
  task_environment_.RunUntilIdle();

  // Verify a deletion metric was emitted and the BTM entry was removed.
  base::HistogramTester::CountsMap expected_counts;
  expected_counts[kUmaHistogramDeletionPrefix + kBlock3PC] = 1;
  EXPECT_THAT(histograms().GetTotalCountsForPrefix(kUmaHistogramDeletionPrefix),
              testing::ContainerEq(expected_counts));
  histograms().ExpectUniqueSample(kUmaHistogramDeletionPrefix + kBlock3PC,
                                  BtmDeletionAction::kExcepted, 1);
  EXPECT_FALSE(GetBtmState(GetService(), excepted_3p_url).has_value());
}

TEST_F(BtmServiceHistogramTest, DISABLED_Deletion_Enforced) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kBtm,
      {{"delete", "true"}, {"triggering_action", "stateful_bounce"}});

  // Verify the histogram is initially empty.
  EXPECT_TRUE(histograms()
                  .GetTotalCountsForPrefix(kUmaHistogramDeletionPrefix)
                  .empty());

  // Record a bounce.
  GURL url("https://example.com");
  base::Time bounce_time = base::Time::FromSecondsSinceUnixEpoch(2);
  RecordBounce(url, GURL("https://initial.com"), GURL("https://final.com"),
               bounce_time, true,
               base::BindRepeating([](const GURL& final_url) {}));
  WaitOnStorage(GetService());

  // Time-travel to after the grace period has ended for the bounce.
  AdvanceTimeTo(bounce_time + grace_period + tiny_delta);
  FireBtmTimer();
  task_environment_.RunUntilIdle();

  // Verify a deletion metric was emitted and the BTM entry was not removed.
  base::HistogramTester::CountsMap expected_counts;
  expected_counts[kUmaHistogramDeletionPrefix + kBlock3PC] = 1;
  EXPECT_THAT(histograms().GetTotalCountsForPrefix(kUmaHistogramDeletionPrefix),
              testing::ContainerEq(expected_counts));
  histograms().ExpectUniqueSample(kUmaHistogramDeletionPrefix + kBlock3PC,
                                  BtmDeletionAction::kEnforced, 1);
  EXPECT_TRUE(GetBtmState(GetService(), url).has_value());
}

TEST_F(BtmServiceHistogramTest, ServerBounceDelay) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kBtm, {{"delete", "false"}, {"triggering_action", "bounce"}});

  // Verify that the histograms start empty.
  histograms().ExpectTotalCount(kServerRedirectsDelayHist, 0);
  histograms().ExpectTotalCount(kServerRedirectsChainDelayHist, 0);
  EXPECT_TRUE(histograms()
                  .GetTotalCountsForPrefix(kServerRedirectsStatusCodePrefix)
                  .empty());

  TestBrowserContext profile;
  BtmServiceImpl* service = BtmServiceImpl::Get(&profile);

  UrlAndSourceId initial_url = MakeUrlAndId("http://a.test/");
  UrlAndSourceId first_redirect_url = MakeUrlAndId("http://b.test/");
  UrlAndSourceId second_redirect_url = MakeUrlAndId("http://c.test/");

  DipsRedirectChainObserver observer(service, GURL());
  std::vector<BtmRedirectInfoPtr> redirects;
  redirects.push_back(BtmRedirectInfo::CreateForServer(
      first_redirect_url,
      /*access_type=*/BtmDataAccessType::kNone,
      /*time=*/base::Time::Now(),
      /*was_response_cached=*/true,
      /*response_code=*/net::HTTP_MOVED_PERMANENTLY,
      /*server_bounce_delay=*/base::Milliseconds(100)));
  redirects.push_back(BtmRedirectInfo::CreateForServer(
      second_redirect_url,
      /*access_type=*/BtmDataAccessType::kNone,
      /*time=*/base::Time::Now(),
      /*was_response_cached=*/false,
      /*response_code=*/net::HTTP_FOUND,
      /*server_bounce_delay=*/base::Milliseconds(100)));
  BtmRedirectChainInfoPtr chain = std::make_unique<BtmRedirectChainInfo>(
      initial_url, UrlAndSourceId(), redirects.size(),
      /*is_partial_chain=*/false);
  btm::Populate3PcExceptions(&profile, /*web_contents=*/nullptr,
                             chain->initial_url.url, chain->final_url.url,
                             redirects);
  service->HandleRedirectChain(std::move(redirects), std::move(chain),
                               base::DoNothing());
  observer.Wait();

  histograms().ExpectTotalCount(kServerRedirectsDelayHist, 2);
  histograms().ExpectTotalCount(kServerRedirectsChainDelayHist, 1);
  base::HistogramTester::CountsMap expected_counts = {
      {kServerRedirectsStatusCodePrefix + kNoCache, 1},
      {kServerRedirectsStatusCodePrefix + kCached, 1},
  };
  EXPECT_THAT(
      histograms().GetTotalCountsForPrefix(kServerRedirectsStatusCodePrefix),
      testing::ContainerEq(expected_counts));

  histograms().ExpectUniqueSample(kServerRedirectsStatusCodePrefix + kNoCache,
                                  net::HTTP_FOUND, 1);
  histograms().ExpectUniqueSample(kServerRedirectsStatusCodePrefix + kCached,
                                  net::HTTP_MOVED_PERMANENTLY, 1);
  histograms().ExpectUniqueSample(kServerRedirectsDelayHist, 100, 2);
  histograms().ExpectUniqueSample(kServerRedirectsChainDelayHist, 200, 1);
}

MATCHER_P(HasSourceId, id, "") {
  *result_listener << "where the source id is " << arg.source_id;
  return arg.source_id == id;
}

MATCHER_P(HasMetrics, matcher, "") {
  return ExplainMatchResult(matcher, arg.metrics, result_listener);
}

using BtmServiceUkmTest = BtmServiceTest;

TEST_F(BtmServiceUkmTest, BothChainBeginAndChainEnd) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  TestBrowserContext profile;
  BtmServiceImpl* service = BtmServiceImpl::Get(&profile);

  UrlAndSourceId initial_url = MakeUrlAndId("http://a.test/");
  UrlAndSourceId redirect_url1 = MakeUrlAndId("http://b.test/");
  UrlAndSourceId redirect_url2 = MakeUrlAndId("http://c.test/first");
  UrlAndSourceId final_url = MakeUrlAndId("http://c.test/second");

  DipsRedirectChainObserver observer(service, final_url.url);
  std::vector<BtmRedirectInfoPtr> redirects;
  redirects.push_back(BtmRedirectInfo::CreateForServer(
      redirect_url1,
      /*access_type=*/BtmDataAccessType::kNone,
      /*time=*/base::Time::Now(),
      /*was_response_cached=*/false,
      /*response_code=*/net::HTTP_FOUND,
      /*server_bounce_delay=*/base::TimeDelta()));
  redirects.push_back(BtmRedirectInfo::CreateForServer(
      redirect_url2,
      /*access_type=*/BtmDataAccessType::kNone,
      /*time=*/base::Time::Now(),
      /*was_response_cached=*/false,
      /*response_code=*/net::HTTP_FOUND,
      /*server_bounce_delay=*/base::TimeDelta()));
  BtmRedirectChainInfoPtr chain = std::make_unique<BtmRedirectChainInfo>(
      initial_url, final_url,
      /*length=*/2, /*is_partial_chain=*/false);
  const int32_t chain_id = chain->chain_id;
  btm::Populate3PcExceptions(&profile, /*web_contents=*/nullptr,
                             initial_url.url, final_url.url, redirects);
  service->HandleRedirectChain(std::move(redirects), std::move(chain),
                               base::DoNothing());
  observer.Wait();

  EXPECT_THAT(ukm_recorder.GetEntries("DIPS.ChainBegin",
                                      {"ChainId", "InitialAndFinalSitesSame"}),
              ElementsAre(AllOf(HasSourceId(initial_url.source_id),
                                HasMetrics(ElementsAre(
                                    Pair("ChainId", chain_id),
                                    Pair("InitialAndFinalSitesSame", 0))))));

  EXPECT_THAT(
      ukm_recorder.GetEntries("DIPS.Redirect",
                              {"ChainId", "InitialAndFinalSitesSame"}),
      ElementsAre(
          AllOf(HasSourceId(redirect_url1.source_id),
                HasMetrics(ElementsAre(Pair("ChainId", chain_id),
                                       Pair("InitialAndFinalSitesSame", 0)))),
          AllOf(HasSourceId(redirect_url2.source_id),
                HasMetrics(ElementsAre(Pair("ChainId", chain_id),
                                       Pair("InitialAndFinalSitesSame", 0))))));

  EXPECT_THAT(ukm_recorder.GetEntries("DIPS.ChainEnd",
                                      {"ChainId", "InitialAndFinalSitesSame"}),
              ElementsAre(AllOf(HasSourceId(final_url.source_id),
                                HasMetrics(ElementsAre(
                                    Pair("ChainId", chain_id),
                                    Pair("InitialAndFinalSitesSame", 0))))));
}

TEST_F(BtmServiceUkmTest, InitialAndFinalSitesSame_True) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  TestBrowserContext profile;
  BtmServiceImpl* service = BtmServiceImpl::Get(&profile);

  UrlAndSourceId initial_url = MakeUrlAndId("http://a.test/");
  UrlAndSourceId redirect_url = MakeUrlAndId("http://b.test/");
  UrlAndSourceId final_url = MakeUrlAndId("http://a.test/different-path");

  DipsRedirectChainObserver observer(service, final_url.url);
  std::vector<BtmRedirectInfoPtr> redirects;
  redirects.push_back(BtmRedirectInfo::CreateForServer(
      redirect_url,
      /*access_type=*/BtmDataAccessType::kNone,
      /*time=*/base::Time::Now(),
      /*was_response_cached=*/false,
      /*response_code=*/net::HTTP_FOUND,
      /*server_bounce_delay=*/base::TimeDelta()));
  BtmRedirectChainInfoPtr chain = std::make_unique<BtmRedirectChainInfo>(
      initial_url, final_url,
      /*length=*/1, /*is_partial_chain=*/false);
  btm::Populate3PcExceptions(&profile, /*web_contents=*/nullptr,
                             chain->initial_url.url, chain->final_url.url,
                             redirects);
  service->HandleRedirectChain(std::move(redirects), std::move(chain),
                               base::DoNothing());
  observer.Wait();

  EXPECT_THAT(
      ukm_recorder.GetEntries("DIPS.ChainBegin", {"InitialAndFinalSitesSame"}),
      ElementsAre(
          AllOf(HasSourceId(initial_url.source_id),
                HasMetrics(ElementsAre(Pair("InitialAndFinalSitesSame", 1))))));

  EXPECT_THAT(
      ukm_recorder.GetEntries("DIPS.Redirect", {"InitialAndFinalSitesSame"}),
      ElementsAre(
          AllOf(HasSourceId(redirect_url.source_id),
                HasMetrics(ElementsAre(Pair("InitialAndFinalSitesSame", 1))))));

  EXPECT_THAT(
      ukm_recorder.GetEntries("DIPS.ChainEnd", {"InitialAndFinalSitesSame"}),
      ElementsAre(
          AllOf(HasSourceId(final_url.source_id),
                HasMetrics(ElementsAre(Pair("InitialAndFinalSitesSame", 1))))));
}

TEST_F(BtmServiceUkmTest, DontReportEmptyChainsAtAll) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  TestBrowserContext profile;
  BtmServiceImpl* service = BtmServiceImpl::Get(&profile);

  UrlAndSourceId initial_url = MakeUrlAndId("http://a.test/");
  UrlAndSourceId final_url = MakeUrlAndId("http://b.test/");

  DipsRedirectChainObserver observer(service, final_url.url);
  BtmRedirectChainInfoPtr chain = std::make_unique<BtmRedirectChainInfo>(
      initial_url, final_url,
      /*length=*/0, /*is_partial_chain=*/false);

  service->HandleRedirectChain({}, std::move(chain), base::DoNothing());
  observer.Wait();

  EXPECT_THAT(ukm_recorder.GetEntries("DIPS.ChainBegin", {}), IsEmpty());
  EXPECT_THAT(ukm_recorder.GetEntries("DIPS.Redirect", {}), IsEmpty());
  EXPECT_THAT(ukm_recorder.GetEntries("DIPS.ChainEnd", {}), IsEmpty());
}

TEST_F(BtmServiceUkmTest, DontReportChainBeginIfInvalidSourceId) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  TestBrowserContext profile;
  BtmServiceImpl* service = BtmServiceImpl::Get(&profile);

  UrlAndSourceId redirect_url = MakeUrlAndId("http://b.test/");
  UrlAndSourceId final_url = MakeUrlAndId("http://c.test/");

  DipsRedirectChainObserver observer(service, final_url.url);
  std::vector<BtmRedirectInfoPtr> redirects;
  redirects.push_back(BtmRedirectInfo::CreateForServer(
      redirect_url,
      /*access_type=*/BtmDataAccessType::kNone,
      /*time=*/base::Time::Now(),
      /*was_response_cached=*/false,
      /*response_code=*/net::HTTP_FOUND,
      /*server_bounce_delay=*/base::TimeDelta()));
  BtmRedirectChainInfoPtr chain = std::make_unique<BtmRedirectChainInfo>(
      UrlAndSourceId(), final_url,
      /*length=*/1, /*is_partial_chain=*/false);
  btm::Populate3PcExceptions(&profile, /*web_contents=*/nullptr,
                             chain->initial_url.url, chain->final_url.url,
                             redirects);
  service->HandleRedirectChain(std::move(redirects), std::move(chain),
                               base::DoNothing());
  observer.Wait();

  EXPECT_THAT(ukm_recorder.GetEntries("DIPS.ChainBegin", {}), IsEmpty());

  EXPECT_THAT(ukm_recorder.GetEntries("DIPS.Redirect", {}),
              ElementsAre(AllOf(HasSourceId(redirect_url.source_id))));

  EXPECT_THAT(ukm_recorder.GetEntries("DIPS.ChainEnd", {}),
              ElementsAre(AllOf(HasSourceId(final_url.source_id))));
}

TEST_F(BtmServiceUkmTest, DontReportChainEndIfInvalidSourceId) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  TestBrowserContext profile;
  BtmServiceImpl* service = BtmServiceImpl::Get(&profile);

  UrlAndSourceId initial_url = MakeUrlAndId("http://a.test/");
  UrlAndSourceId redirect_url = MakeUrlAndId("http://b.test/");

  DipsRedirectChainObserver observer(service, GURL());
  std::vector<BtmRedirectInfoPtr> redirects;
  redirects.push_back(BtmRedirectInfo::CreateForServer(
      redirect_url,
      /*access_type=*/BtmDataAccessType::kNone,
      /*time=*/base::Time::Now(),
      /*was_response_cached=*/false,
      /*response_code=*/net::HTTP_FOUND,
      /*server_bounce_delay=*/base::TimeDelta()));
  BtmRedirectChainInfoPtr chain = std::make_unique<BtmRedirectChainInfo>(
      initial_url, UrlAndSourceId(),
      /*length=*/1, /*is_partial_chain=*/false);
  btm::Populate3PcExceptions(&profile, /*web_contents=*/nullptr,
                             chain->initial_url.url, chain->final_url.url,
                             redirects);
  service->HandleRedirectChain(std::move(redirects), std::move(chain),
                               base::DoNothing());
  observer.Wait();

  EXPECT_THAT(ukm_recorder.GetEntries("DIPS.ChainBegin", {}),
              ElementsAre(AllOf(HasSourceId(initial_url.source_id))));

  EXPECT_THAT(ukm_recorder.GetEntries("DIPS.Redirect", {}),
              ElementsAre(AllOf(HasSourceId(redirect_url.source_id))));

  EXPECT_THAT(ukm_recorder.GetEntries("DIPS.ChainEnd", {}), IsEmpty());
}

TEST(BtmCleanupTest, DatabaseFileIsDeletedIfFeatureIsDisabled) {
  BrowserTaskEnvironment task_environment;

  base::FilePath user_data_dir;
  base::FilePath db_path;

  // First, create a browser context while DIPS is enabled, and confirm a
  // database file is created.
  {
    TestBrowserContext browser_context;
    db_path = GetBtmFilePath(&browser_context);
    // Wait for the database to be created.
    BrowserContextImpl::From(&browser_context)
        ->GetDipsService()
        ->storage()
        ->FlushPostedTasksForTesting();
    ASSERT_TRUE(base::PathExists(db_path));

    // Take ownership of the browser context's directory so we can reuse it.
    user_data_dir = browser_context.TakePath();

    // Confirm that WaitForDipsCleanupForTesting() returns even if the file is
    // not deleted.
    BrowserContextImpl::From(&browser_context)->WaitForDipsCleanupForTesting();
    ASSERT_TRUE(base::PathExists(db_path));
  }

  // Confirm the file still exists after the browser context is destroyed.
  ASSERT_TRUE(base::PathExists(db_path));

  // Create another browser context for the same directory, while DIPS is
  // disabled. Confirm the database file is deleted.
  {
    ScopedInitBtmFeature disable_dips(false);
    TestBrowserContext browser_context(user_data_dir);
    ASSERT_FALSE(BrowserContextImpl::From(&browser_context)->GetDipsService());
    BrowserContextImpl::From(&browser_context)->WaitForDipsCleanupForTesting();
    ASSERT_FALSE(base::PathExists(db_path));
  }
}

}  // namespace content
