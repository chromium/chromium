// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_cap_tracker_service.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_cap_tracker_service_factory.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/features.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace contextual_cueing {

class ContextualCueingCapTrackerServiceTest : public PlatformTest {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(ContextualCueingCapTrackerServiceTest, BasicCanShowNudge) {
  ContextualCueingCapTrackerService service;
  EXPECT_EQ(service.CanShowNudge(GURL("https://example.com")),
            ContextualCueingDecision::kSuccess);
}

TEST_F(ContextualCueingCapTrackerServiceTest, InvalidUrlRejected) {
  ContextualCueingCapTrackerService service;
  EXPECT_EQ(service.CanShowNudge(GURL("chrome://version")),
            ContextualCueingDecision::kUrlNotEligible);
  EXPECT_EQ(service.CanShowNudge(GURL("invalid")),
            ContextualCueingDecision::kUrlNotEligible);
}

TEST_F(ContextualCueingCapTrackerServiceTest, PageSpacingEnforced) {
  ContextualCueingCapTrackerService::Config config;
  config.min_page_count_between_nudges = 2;
  config.min_time_between_nudges = base::TimeDelta();
  ContextualCueingCapTrackerService service(config);

  const GURL url("https://example.com");
  EXPECT_EQ(service.CanShowNudge(url), ContextualCueingDecision::kSuccess);

  service.RecordCueShown(url);

  // Immediately after showing a cue, page spacing constraint fails.
  EXPECT_EQ(service.CanShowNudge(url),
            ContextualCueingDecision::kNotEnoughPageLoadsSinceLastCue);

  // Navigate 1 page: 1st quiet page.
  service.RecordPageNavigation();
  EXPECT_EQ(service.CanShowNudge(url),
            ContextualCueingDecision::kNotEnoughPageLoadsSinceLastCue);

  // Navigate 2nd page: 2nd quiet page.
  service.RecordPageNavigation();
  EXPECT_EQ(service.CanShowNudge(url),
            ContextualCueingDecision::kNotEnoughPageLoadsSinceLastCue);

  // Navigate 3rd page: 2 quiet pages have occurred, next page is allowed.
  service.RecordPageNavigation();
  EXPECT_EQ(service.CanShowNudge(GURL("https://other.com")),
            ContextualCueingDecision::kSuccess);
}

TEST_F(ContextualCueingCapTrackerServiceTest, TimeSpacingEnforced) {
  ContextualCueingCapTrackerService::Config config;
  config.min_page_count_between_nudges = 0;
  config.min_time_between_nudges = base::Minutes(10);
  ContextualCueingCapTrackerService service(config);

  const GURL url("https://example.com");
  service.RecordCueShown(url);

  // Blocked because not enough time has passed.
  EXPECT_EQ(service.CanShowNudge(url),
            ContextualCueingDecision::kNotEnoughTimeSinceLastCue);

  // Fast forward 5 minutes.
  task_environment_.FastForwardBy(base::Minutes(5));
  EXPECT_EQ(service.CanShowNudge(url),
            ContextualCueingDecision::kNotEnoughTimeSinceLastCue);

  // Fast forward 6 more minutes (11 total).
  task_environment_.FastForwardBy(base::Minutes(6));
  EXPECT_EQ(service.CanShowNudge(GURL("https://other.com")),
            ContextualCueingDecision::kSuccess);
}

TEST_F(ContextualCueingCapTrackerServiceTest, GlobalCapEnforced) {
  ContextualCueingCapTrackerService::Config config;
  config.global_cap_count = 2;
  config.origin_cap_count = 5;
  config.min_page_count_between_nudges = 0;
  config.min_time_between_nudges = base::TimeDelta();
  config.global_duration = base::Hours(8);
  ContextualCueingCapTrackerService service(config);

  service.RecordCueShown(GURL("https://a.com"));
  task_environment_.FastForwardBy(base::Minutes(1));
  service.RecordCueShown(GURL("https://b.com"));
  task_environment_.FastForwardBy(base::Minutes(1));

  // Reached global limit of 2.
  EXPECT_EQ(service.CanShowNudge(GURL("https://c.com")),
            ContextualCueingDecision::kTooManyCuesShownToTheUser);

  // Fast forward 9 hours.
  task_environment_.FastForwardBy(base::Hours(9));
  EXPECT_EQ(service.CanShowNudge(GURL("https://c.com")),
            ContextualCueingDecision::kSuccess);
}

TEST_F(ContextualCueingCapTrackerServiceTest, OriginCapEnforced) {
  ContextualCueingCapTrackerService::Config config;
  config.global_cap_count = 10;
  config.origin_cap_count = 1;
  config.min_page_count_between_nudges = 0;
  config.min_time_between_nudges = base::TimeDelta();
  config.origin_duration = base::Hours(4);
  ContextualCueingCapTrackerService service(config);

  service.RecordCueShown(GURL("https://example.com/item1"));
  task_environment_.FastForwardBy(base::Minutes(1));

  // example.com is now capped.
  EXPECT_EQ(service.CanShowNudge(GURL("https://example.com/item2")),
            ContextualCueingDecision::kTooManyCuesShownToTheUserForOrigin);

  // A different domain should still be allowed.
  EXPECT_EQ(service.CanShowNudge(GURL("https://other.com")),
            ContextualCueingDecision::kSuccess);

  // Fast forward 5 hours to clear origin cap.
  task_environment_.FastForwardBy(base::Hours(5));
  EXPECT_EQ(service.CanShowNudge(GURL("https://example.com/item2")),
            ContextualCueingDecision::kSuccess);
}

TEST_F(ContextualCueingCapTrackerServiceTest, DismissalBackoffEnforced) {
  ContextualCueingCapTrackerService::Config config;
  config.min_page_count_between_nudges = 0;
  config.min_time_between_nudges = base::TimeDelta();
  config.base_dismiss_backoff_time = base::Hours(12);
  ContextualCueingCapTrackerService service(config);

  const GURL url("https://example.com");
  service.RecordCueDismissed(url);

  // Blocked due to dismissal backoff.
  EXPECT_EQ(service.CanShowNudge(GURL("https://other.com")),
            ContextualCueingDecision::kNotEnoughTimeSinceLastDismissal);

  // Fast forward 6 hours (still in cooldown).
  task_environment_.FastForwardBy(base::Hours(6));
  EXPECT_EQ(service.CanShowNudge(GURL("https://other.com")),
            ContextualCueingDecision::kNotEnoughTimeSinceLastDismissal);

  // Fast forward past the 12 hours mark.
  task_environment_.FastForwardBy(base::Hours(7));
  EXPECT_EQ(service.CanShowNudge(GURL("https://other.com")),
            ContextualCueingDecision::kSuccess);
}

TEST_F(ContextualCueingCapTrackerServiceTest, ClickBackoffEnforced) {
  ContextualCueingCapTrackerService::Config config;
  config.min_page_count_between_nudges = 0;
  config.min_time_between_nudges = base::TimeDelta();
  config.click_backoff_time = base::Hours(1);
  ContextualCueingCapTrackerService service(config);

  const GURL url("https://example.com");
  service.RecordCueClicked(url);

  // Blocked due to click backoff.
  EXPECT_EQ(service.CanShowNudge(GURL("https://other.com")),
            ContextualCueingDecision::kNotEnoughTimeSinceLastClick);

  // Fast forward 65 minutes.
  task_environment_.FastForwardBy(base::Minutes(65));
  EXPECT_EQ(service.CanShowNudge(GURL("https://other.com")),
            ContextualCueingDecision::kSuccess);
}

TEST_F(ContextualCueingCapTrackerServiceTest,
       ExponentialDismissalBackoffPreservedAcrossCueShown) {
  ContextualCueingCapTrackerService::Config config;
  config.min_page_count_between_nudges = 0;
  config.min_time_between_nudges = base::TimeDelta();
  config.base_dismiss_backoff_time = base::Hours(10);
  config.dismiss_backoff_multiplier_base = 2.0;
  ContextualCueingCapTrackerService service(config);

  const GURL url("https://example.com");

  // 1st dismissal: backoff is 10h * 2^0 = 10h.
  service.RecordCueDismissed(url);
  task_environment_.FastForwardBy(base::Hours(11));
  EXPECT_EQ(service.CanShowNudge(url), ContextualCueingDecision::kSuccess);

  // A cue is shown on another page without being clicked.
  // Showing a cue must NOT reset the dismissal backoff tier.
  service.RecordCueShown(GURL("https://other.com"));

  // 2nd dismissal: backoff should be 10h * 2^1 = 20h.
  service.RecordCueDismissed(url);
  task_environment_.FastForwardBy(base::Hours(15));
  // 15 hours passed, which is less than 20 hours, so still in backoff.
  EXPECT_EQ(service.CanShowNudge(url),
            ContextualCueingDecision::kNotEnoughTimeSinceLastDismissal);

  // Fast forward remaining 6 hours (21 total).
  task_environment_.FastForwardBy(base::Hours(6));
  EXPECT_EQ(service.CanShowNudge(url), ContextualCueingDecision::kSuccess);

  // When user clicks a cue, dismissal backoff count resets.
  service.RecordCueClicked(url);
  task_environment_.FastForwardBy(config.click_backoff_time + base::Minutes(1));

  // Next dismissal returns to tier 0 (10h).
  service.RecordCueDismissed(url);
  task_environment_.FastForwardBy(base::Hours(11));
  EXPECT_EQ(service.CanShowNudge(url), ContextualCueingDecision::kSuccess);
}

TEST_F(ContextualCueingCapTrackerServiceTest,
       TimestampQueueRecordsTimeWhenCapCountZero) {
  ContextualCueingCapTrackerService::Config config;
  config.global_cap_count = 0;  // Unlimited cap.
  config.min_page_count_between_nudges = 0;
  config.min_time_between_nudges = base::TimeDelta();
  ContextualCueingCapTrackerService service(config);

  EXPECT_FALSE(service.GetMostRecentNudgeTime().has_value());
  service.RecordCueShown(GURL("https://example.com"));
  EXPECT_TRUE(service.GetMostRecentNudgeTime().has_value());
}

TEST_F(ContextualCueingCapTrackerServiceTest, OriginLRUCacheEviction) {
  ContextualCueingCapTrackerService::Config config;
  config.visited_origins_limit = 2;
  config.origin_cap_count = 1;
  config.origin_duration = base::Hours(4);
  config.global_cap_count = 100;
  config.min_page_count_between_nudges = 0;
  config.min_time_between_nudges = base::TimeDelta();
  ContextualCueingCapTrackerService service(config);

  const GURL url_a("https://a.com");
  const GURL url_b("https://b.com");
  const GURL url_c("https://c.com");

  service.RecordCueShown(url_a);
  task_environment_.FastForwardBy(base::Minutes(1));
  service.RecordCueShown(url_b);
  task_environment_.FastForwardBy(base::Minutes(1));

  // Both a.com and b.com are capped.
  EXPECT_EQ(service.CanShowNudge(url_a),
            ContextualCueingDecision::kTooManyCuesShownToTheUserForOrigin);
  EXPECT_EQ(service.CanShowNudge(url_b),
            ContextualCueingDecision::kTooManyCuesShownToTheUserForOrigin);

  // Record cue on c.com. This evicts a.com (the least recently used origin).
  service.RecordCueShown(url_c);
  task_environment_.FastForwardBy(base::Minutes(1));

  // a.com was evicted from the LRU cache, so it can show a cue again.
  EXPECT_EQ(service.CanShowNudge(url_a), ContextualCueingDecision::kSuccess);
  // b.com and c.com are still in the cache and capped.
  EXPECT_EQ(service.CanShowNudge(url_b),
            ContextualCueingDecision::kTooManyCuesShownToTheUserForOrigin);
  EXPECT_EQ(service.CanShowNudge(url_c),
            ContextualCueingDecision::kTooManyCuesShownToTheUserForOrigin);
}

TEST_F(ContextualCueingCapTrackerServiceTest,
       DisableFrequencyCappingAndBackoff) {
  ContextualCueingCapTrackerService::Config config;
  config.disable_frequency_capping_and_backoff = true;
  config.global_cap_count = 1;
  config.origin_cap_count = 1;
  config.min_page_count_between_nudges = 10;
  config.min_time_between_nudges = base::Hours(1);
  ContextualCueingCapTrackerService service(config);

  const GURL url("https://example.com");
  service.RecordCueShown(url);

  // All constraints are bypassed when disable_frequency_capping_and_backoff is
  // true.
  EXPECT_EQ(service.CanShowNudge(url), ContextualCueingDecision::kSuccess);
}

TEST_F(ContextualCueingCapTrackerServiceTest, DefaultConfigValues) {
  ContextualCueingCapTrackerService service;
  const GURL url1("https://domain1.com");
  const GURL url2("https://domain2.com");
  const GURL url3("https://domain3.com");
  const GURL url4("https://domain4.com");
  const GURL url5("https://domain5.com");

  auto satisfy_page_spacing = [&]() {
    for (size_t i = 0; i <= 10; ++i) {
      service.RecordPageNavigation();
    }
  };

  // 1st cue shown on url1.
  service.RecordCueShown(url1);
  // Default config requires 10 quiet page loads.
  EXPECT_EQ(service.CanShowNudge(url1),
            ContextualCueingDecision::kNotEnoughPageLoadsSinceLastCue);
  satisfy_page_spacing();

  // Page spacing satisfied, but 1 cue per origin & 1st ignore cooldown active.
  EXPECT_EQ(service.CanShowNudge(url1),
            ContextualCueingDecision::kNotEnoughTimeSinceLastCue);

  // Fast forward past 1st ignore cooldown (60 mins).
  task_environment_.FastForwardBy(base::Minutes(61));
  EXPECT_EQ(service.CanShowNudge(url2), ContextualCueingDecision::kSuccess);

  // Global cap of 4 cues per 24 hours: record 3 more cues with page spacing.
  service.RecordCueShown(url2);
  satisfy_page_spacing();
  task_environment_.FastForwardBy(base::Minutes(95));  // 2nd ignore: 90m
  EXPECT_EQ(service.CanShowNudge(url3), ContextualCueingDecision::kSuccess);

  service.RecordCueShown(url3);
  satisfy_page_spacing();
  task_environment_.FastForwardBy(base::Minutes(140));  // 3rd ignore: 135m
  EXPECT_EQ(service.CanShowNudge(url4), ContextualCueingDecision::kSuccess);

  service.RecordCueShown(url4);
  satisfy_page_spacing();

  // 4 cues recorded across 24h: 5th cue is blocked by global cap after 4th
  // ignore cooldown (202.5m).
  task_environment_.FastForwardBy(base::Minutes(210));
  EXPECT_EQ(service.CanShowNudge(url5),
            ContextualCueingDecision::kTooManyCuesShownToTheUser);

  // Fast forward past 24 hours.
  task_environment_.FastForwardBy(base::Hours(24));
  EXPECT_EQ(service.CanShowNudge(url5), ContextualCueingDecision::kSuccess);
}

TEST_F(ContextualCueingCapTrackerServiceTest, ExponentialIgnoreBackoff) {
  ContextualCueingCapTrackerService::Config config;
  config.min_page_count_between_nudges = 0;
  config.min_time_between_nudges = base::Minutes(60);
  config.ignore_backoff_multiplier_base = 1.5;
  config.click_backoff_time = base::Minutes(30);
  config.global_cap_count = 100;
  ContextualCueingCapTrackerService service(config);

  const GURL url1("https://a.com");
  const GURL url2("https://b.com");
  const GURL url3("https://c.com");
  const GURL url4("https://d.com");

  // 1st ignore: 60m * 1.5^0 = 60m.
  service.RecordCueShown(url1);
  task_environment_.FastForwardBy(base::Minutes(45));
  EXPECT_EQ(service.CanShowNudge(url2),
            ContextualCueingDecision::kNotEnoughTimeSinceLastCue);
  task_environment_.FastForwardBy(base::Minutes(20));  // 65m total
  EXPECT_EQ(service.CanShowNudge(url2), ContextualCueingDecision::kSuccess);

  // 2nd ignore: 60m * 1.5^1 = 90m.
  service.RecordCueShown(url2);
  task_environment_.FastForwardBy(base::Minutes(75));
  EXPECT_EQ(service.CanShowNudge(url3),
            ContextualCueingDecision::kNotEnoughTimeSinceLastCue);
  task_environment_.FastForwardBy(base::Minutes(20));  // 95m total
  EXPECT_EQ(service.CanShowNudge(url3), ContextualCueingDecision::kSuccess);

  // 3rd cue: user clicks it. Click backoff is 30 mins, and resets ignore tier.
  service.RecordCueShown(url3);
  service.RecordCueClicked(url3);
  task_environment_.FastForwardBy(base::Minutes(15));
  EXPECT_EQ(service.CanShowNudge(url4),
            ContextualCueingDecision::kNotEnoughTimeSinceLastClick);
  task_environment_.FastForwardBy(base::Minutes(20));  // 35m total
  EXPECT_EQ(service.CanShowNudge(url4), ContextualCueingDecision::kSuccess);

  // Next cue shown: backoff starts from tier 0 again (60m).
  service.RecordCueShown(url4);
  task_environment_.FastForwardBy(base::Minutes(65));
  EXPECT_EQ(service.CanShowNudge(GURL("https://e.com")),
            ContextualCueingDecision::kSuccess);
}

TEST_F(ContextualCueingCapTrackerServiceTest,
       DismissalResetsIgnoreBackoffTier) {
  ContextualCueingCapTrackerService::Config config;
  config.min_page_count_between_nudges = 0;
  config.min_time_between_nudges = base::Minutes(60);
  config.ignore_backoff_multiplier_base = 1.5;
  config.base_dismiss_backoff_time = base::Hours(12);
  config.global_cap_count = 100;
  ContextualCueingCapTrackerService service(config);

  const GURL url1("https://a.com");
  const GURL url2("https://b.com");
  const GURL url3("https://c.com");

  // 1st cue shown: ignore backoff is 60m.
  service.RecordCueShown(url1);
  task_environment_.FastForwardBy(base::Minutes(65));

  // 2nd cue shown and dismissed: dismissal backoff is 12h, and resets ignore
  // tier.
  service.RecordCueShown(url2);
  service.RecordCueDismissed(url2);
  task_environment_.FastForwardBy(base::Hours(13));

  // Next cue shown after dismissal should start at tier 0 ignore backoff (60m).
  service.RecordCueShown(url3);
  task_environment_.FastForwardBy(base::Minutes(45));
  EXPECT_EQ(service.CanShowNudge(GURL("https://d.com")),
            ContextualCueingDecision::kNotEnoughTimeSinceLastCue);
  task_environment_.FastForwardBy(base::Minutes(20));  // 65m total
  EXPECT_EQ(service.CanShowNudge(GURL("https://d.com")),
            ContextualCueingDecision::kSuccess);
}

TEST_F(ContextualCueingCapTrackerServiceTest,
       ZeroVisitedOriginsLimitDoesNotCrash) {
  ContextualCueingCapTrackerService::Config config;
  config.visited_origins_limit = 0;  // Origin tracking disabled.
  config.global_cap_count = 10;
  config.min_page_count_between_nudges = 0;
  config.min_time_between_nudges = base::TimeDelta();
  ContextualCueingCapTrackerService service(config);

  const GURL url("https://example.com");
  EXPECT_EQ(service.CanShowNudge(url), ContextualCueingDecision::kSuccess);
  // Recording a cue shown with limit 0 must safely no-op origin tracking
  // instead of crashing.
  service.RecordCueShown(url);
  EXPECT_EQ(service.CanShowNudge(url), ContextualCueingDecision::kSuccess);
}

TEST_F(ContextualCueingCapTrackerServiceTest,
       DismissalOverridesIgnoreWithDismissalBackoff) {
  ContextualCueingCapTrackerService::Config config;
  config.min_page_count_between_nudges = 0;
  config.min_time_between_nudges = base::Minutes(60);
  config.base_dismiss_backoff_time = base::Minutes(5);
  config.origin_duration = base::Minutes(5);
  ContextualCueingCapTrackerService service(config);

  const GURL url("https://example.com");
  service.RecordCueShown(url);
  // User immediately dismisses.
  service.RecordCueDismissed(url);

  // 3 minutes pass: dismissal backoff (5 mins) is still active.
  task_environment_.FastForwardBy(base::Minutes(3));
  EXPECT_EQ(service.CanShowNudge(url),
            ContextualCueingDecision::kNotEnoughTimeSinceLastDismissal);

  // 3 more minutes pass (6 mins total): dismissal backoff (5 mins) and origin
  // cap (5 mins) have expired.
  task_environment_.FastForwardBy(base::Minutes(3));
  EXPECT_EQ(service.CanShowNudge(url), ContextualCueingDecision::kSuccess);
}

TEST_F(ContextualCueingCapTrackerServiceTest,
       LargeConsecutiveDismissalsNoOverflow) {
  ContextualCueingCapTrackerService::Config config;
  config.min_page_count_between_nudges = 0;
  config.min_time_between_nudges = base::TimeDelta();
  config.base_dismiss_backoff_time = base::Hours(24);
  config.dismiss_backoff_multiplier_base = 2.0;
  ContextualCueingCapTrackerService service(config);

  const GURL url("https://example.com");
  // Simulate 50 consecutive dismissals (which would overflow if uncapped).
  for (int i = 0; i < 50; ++i) {
    service.RecordCueDismissed(url);
  }

  EXPECT_EQ(service.CanShowNudge(url),
            ContextualCueingDecision::kNotEnoughTimeSinceLastDismissal);

  // Clicking a cue resets consecutive dismissals back to 0.
  service.RecordCueClicked(url);
  task_environment_.FastForwardBy(config.click_backoff_time + base::Minutes(1));

  // Next dismissal starts from tier 0 again (24h).
  service.RecordCueDismissed(url);
  task_environment_.FastForwardBy(base::Hours(25));
  EXPECT_EQ(service.CanShowNudge(url), ContextualCueingDecision::kSuccess);
}

TEST_F(ContextualCueingCapTrackerServiceTest, FactoryGetForProfile) {
  TestProfileIOS::Builder builder;
  auto profile = std::move(builder).Build();

  ContextualCueingCapTrackerService* service =
      ContextualCueingCapTrackerServiceFactory::GetForProfile(profile.get());
  EXPECT_NE(service, nullptr);

  // Off-the-record profile should not instantiate the service.
  ProfileIOS* otr_profile =
      profile->CreateOffTheRecordProfileWithTestingFactories();
  ContextualCueingCapTrackerService* otr_service =
      ContextualCueingCapTrackerServiceFactory::GetForProfile(otr_profile);
  EXPECT_EQ(otr_service, nullptr);
}

TEST_F(ContextualCueingCapTrackerServiceTest, FinchParamOverrides) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kGeminiContextualSuggestionsCues,
      {
          {kGlobalCapCount.name, "2"},
          {kGlobalCapDuration.name, "12h"},
          {kOriginCapCount.name, "3"},
          {kOriginCapDuration.name, "2h"},
          {kVisitedOriginsLimit.name, "50"},
          {kMinPageCountBetweenNudges.name, "3"},
          {kMinTimeBetweenNudges.name, "15m"},
          {kIgnoreBackoffMultiplierBase.name, "2.5"},
          {kBaseDismissBackoffTime.name, "6h"},
          {kDismissBackoffMultiplierBase.name, "3.0"},
          {kClickBackoffTime.name, "10m"},
          {kDisableFrequencyCappingAndBackoff.name, "false"},
          {kMaxConsecutiveMessageIgnores.name, "4"},
          {kForceMessageUiOnly.name, "true"},
          {kForceOmniboxChipUiOnly.name, "false"},
      });

  ContextualCueingCapTrackerService service;
  const auto& config = service.config();
  EXPECT_EQ(config.global_cap_count, 2u);
  EXPECT_EQ(config.global_duration, base::Hours(12));
  EXPECT_EQ(config.origin_cap_count, 3u);
  EXPECT_EQ(config.origin_duration, base::Hours(2));
  EXPECT_EQ(config.visited_origins_limit, 50u);
  EXPECT_EQ(config.min_page_count_between_nudges, 3u);
  EXPECT_EQ(config.min_time_between_nudges, base::Minutes(15));
  EXPECT_DOUBLE_EQ(config.ignore_backoff_multiplier_base, 2.5);
  EXPECT_EQ(config.base_dismiss_backoff_time, base::Hours(6));
  EXPECT_DOUBLE_EQ(config.dismiss_backoff_multiplier_base, 3.0);
  EXPECT_EQ(config.click_backoff_time, base::Minutes(10));
  EXPECT_FALSE(config.disable_frequency_capping_and_backoff);
  EXPECT_EQ(config.max_consecutive_message_ignores, 4u);
  EXPECT_TRUE(config.force_message_ui_only);
  EXPECT_FALSE(config.force_omnibox_chip_ui_only);
}

TEST_F(ContextualCueingCapTrackerServiceTest,
       ThreeConsecutiveMessageIgnoresSwitchesToOmniboxPerVertical) {
  ContextualCueingCapTrackerService::Config config;
  config.disable_frequency_capping_and_backoff = true;
  ContextualCueingCapTrackerService service(config);

  const GURL url("https://example.com");
  const auto kEdu = page_content_annotations::CategoryType::kEducation;
  const auto kShop = page_content_annotations::CategoryType::kShopping;

  // Initially both verticals start on Message UI.
  EXPECT_EQ(service.GetCueUiTypeForCategory(kEdu),
            ContextualCueUiType::kMessage);
  EXPECT_EQ(service.GetCueUiTypeForCategory(kShop),
            ContextualCueUiType::kMessage);

  // 1st Message shown and ignored for Education.
  service.RecordCueShown(url, kEdu);
  EXPECT_EQ(service.GetCueUiTypeForCategory(kEdu),
            ContextualCueUiType::kMessage);

  // 2nd Message shown and ignored for Education.
  service.RecordCueShown(url, kEdu);
  EXPECT_EQ(service.GetCueUiTypeForCategory(kEdu),
            ContextualCueUiType::kMessage);

  // 3rd Message shown and ignored for Education -> switches to Omnibox Chip.
  service.RecordCueShown(url, kEdu);
  EXPECT_EQ(service.GetCueUiTypeForCategory(kEdu),
            ContextualCueUiType::kOmniboxChip);

  // Shopping vertical is unaffected and still uses Message UI.
  EXPECT_EQ(service.GetCueUiTypeForCategory(kShop),
            ContextualCueUiType::kMessage);

  // Once Education has switched to Omnibox Chip, even clicking the Omnibox
  // Chip on the 4th cue does not switch back to Message UI.
  service.RecordCueShown(url, kEdu);
  service.RecordCueClicked(url, kEdu);
  EXPECT_EQ(service.GetCueUiTypeForCategory(kEdu),
            ContextualCueUiType::kOmniboxChip);
}

TEST_F(ContextualCueingCapTrackerServiceTest,
       AcceptingMessageKeepsShowingMessageUntilDismissedOrIgnoredThreeTimes) {
  ContextualCueingCapTrackerService::Config config;
  config.disable_frequency_capping_and_backoff = true;
  ContextualCueingCapTrackerService service(config);

  const GURL url("https://example.com");
  const auto kEdu = page_content_annotations::CategoryType::kEducation;

  // Ignore 1st and 2nd Messages.
  service.RecordCueShown(url, kEdu);
  service.RecordCueShown(url, kEdu);
  EXPECT_EQ(service.GetCueUiTypeForCategory(kEdu),
            ContextualCueUiType::kMessage);

  // 3rd Message is shown, and the user accepts (clicks) it.
  service.RecordCueShown(url, kEdu);
  service.RecordCueClicked(url, kEdu);

  // Because the user accepted the 3rd Message, Message UI is shown on the 4th
  // cue.
  EXPECT_EQ(service.GetCueUiTypeForCategory(kEdu),
            ContextualCueUiType::kMessage);
  service.RecordCueShown(url, kEdu);

  // Accepting the 4th Message continues to show Message UI on subsequent cues.
  service.RecordCueClicked(url, kEdu);
  EXPECT_EQ(service.GetCueUiTypeForCategory(kEdu),
            ContextualCueUiType::kMessage);

  // 5th Message is shown: 1st consecutive ignore.
  service.RecordCueShown(url, kEdu);
  EXPECT_EQ(service.GetCueUiTypeForCategory(kEdu),
            ContextualCueUiType::kMessage);

  // 6th Message is shown: 2nd consecutive ignore.
  service.RecordCueShown(url, kEdu);
  EXPECT_EQ(service.GetCueUiTypeForCategory(kEdu),
            ContextualCueUiType::kMessage);

  // 7th Message is shown: 3rd consecutive ignore -> switches to Omnibox Chip.
  service.RecordCueShown(url, kEdu);
  EXPECT_EQ(service.GetCueUiTypeForCategory(kEdu),
            ContextualCueUiType::kOmniboxChip);

  // Once switched to Omnibox Chip, clicking the chip never switches back to
  // Message UI.
  service.RecordCueShown(url, kEdu);
  service.RecordCueClicked(url, kEdu);
  EXPECT_EQ(service.GetCueUiTypeForCategory(kEdu),
            ContextualCueUiType::kOmniboxChip);
}

TEST_F(ContextualCueingCapTrackerServiceTest,
       DismissingMessageOnceSwitchesToOmniboxPermanently) {
  ContextualCueingCapTrackerService::Config config;
  config.disable_frequency_capping_and_backoff = true;
  ContextualCueingCapTrackerService service(config);

  const GURL url("https://example.com");
  const auto kShop = page_content_annotations::CategoryType::kShopping;

  EXPECT_EQ(service.GetCueUiTypeForCategory(kShop),
            ContextualCueUiType::kMessage);

  service.RecordCueShown(url, kShop);
  service.RecordCueDismissed(url, kShop);

  // Immediately switches to Omnibox Chip UI.
  EXPECT_EQ(service.GetCueUiTypeForCategory(kShop),
            ContextualCueUiType::kOmniboxChip);

  // Clicking subsequent Omnibox Chips never switches back to Message UI.
  service.RecordCueShown(url, kShop);
  service.RecordCueClicked(url, kShop);
  EXPECT_EQ(service.GetCueUiTypeForCategory(kShop),
            ContextualCueUiType::kOmniboxChip);

  // Calling RecordCueDismissed when an Omnibox Chip is shown does not apply the
  // 24-hour dismissal backoff.
  ContextualCueingCapTrackerService::Config backoff_config;
  backoff_config.min_page_count_between_nudges = 0;
  backoff_config.min_time_between_nudges = base::Minutes(60);
  backoff_config.base_dismiss_backoff_time = base::Hours(24);
  backoff_config.force_omnibox_chip_ui_only = true;
  ContextualCueingCapTrackerService chip_service(backoff_config);
  chip_service.RecordCueShown(GURL("https://a.com"), kShop);
  chip_service.RecordCueDismissed(GURL("https://a.com"), kShop);
  task_environment_.FastForwardBy(base::Minutes(61));
  EXPECT_EQ(chip_service.CanShowNudge(GURL("https://b.com")),
            ContextualCueingDecision::kSuccess);
}

TEST_F(ContextualCueingCapTrackerServiceTest,
       VerticalUiTransitionPersistsAcrossServiceRestartsViaPrefs) {
  TestProfileIOS::Builder builder;
  auto profile = std::move(builder).Build();
  PrefService* prefs = profile->GetPrefs();

  const GURL url("https://example.com");
  const auto kEdu = page_content_annotations::CategoryType::kEducation;
  const auto kShop = page_content_annotations::CategoryType::kShopping;

  {
    ContextualCueingCapTrackerService::Config config;
    config.disable_frequency_capping_and_backoff = true;
    ContextualCueingCapTrackerService service1(prefs, config);

    // Dismiss Shopping Message once -> switches to Omnibox.
    service1.RecordCueShown(url, kShop);
    service1.RecordCueDismissed(url, kShop);

    // Accept Education Message once -> keeps Message UI after restart.
    service1.RecordCueShown(url, kEdu);
    service1.RecordCueClicked(url, kEdu);
  }

  // Recreate service with the same PrefService (simulating app relaunch).
  ContextualCueingCapTrackerService::Config config;
  config.disable_frequency_capping_and_backoff = true;
  ContextualCueingCapTrackerService service2(prefs, config);

  EXPECT_EQ(service2.GetCueUiTypeForCategory(kShop),
            ContextualCueUiType::kOmniboxChip);
  EXPECT_EQ(service2.GetCueUiTypeForCategory(kEdu),
            ContextualCueUiType::kMessage);

  // Dismissing Education Message switches Education to Omnibox.
  service2.RecordCueShown(url, kEdu);
  service2.RecordCueDismissed(url, kEdu);
  EXPECT_EQ(service2.GetCueUiTypeForCategory(kEdu),
            ContextualCueUiType::kOmniboxChip);
}

TEST_F(ContextualCueingCapTrackerServiceTest, ForceUiTypeFinchFlags) {
  const GURL url("https://example.com");
  const auto kEdu = page_content_annotations::CategoryType::kEducation;

  // Force Message UI Only: stays on Message UI even after dismissal.
  {
    ContextualCueingCapTrackerService::Config config;
    config.force_message_ui_only = true;
    ContextualCueingCapTrackerService service(config);
    service.RecordCueShown(url, kEdu);
    service.RecordCueDismissed(url, kEdu);
    EXPECT_EQ(service.GetCueUiTypeForCategory(kEdu),
              ContextualCueUiType::kMessage);
  }

  // Force Omnibox Chip UI Only: starts on Omnibox Chip UI immediately.
  {
    ContextualCueingCapTrackerService::Config config;
    config.force_omnibox_chip_ui_only = true;
    ContextualCueingCapTrackerService service(config);
    EXPECT_EQ(service.GetCueUiTypeForCategory(kEdu),
              ContextualCueUiType::kOmniboxChip);
  }
}

TEST_F(ContextualCueingCapTrackerServiceTest,
       IgnoreContextualCueingThresholdsFlagBypassesAllCapsAndForcesMessageUi) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{kPageActionMenu, {}},
       {kGeminiContextualSuggestionsCues,
        {{kGeminiContextualSuggestionsCuesIgnoreThresholdsParam, "true"}}}},
      {});

  ContextualCueingCapTrackerService service;
  const GURL url("https://example.com");
  const auto kEdu = page_content_annotations::CategoryType::kEducation;
  const auto kShop = page_content_annotations::CategoryType::kShopping;

  EXPECT_TRUE(service.config().disable_frequency_capping_and_backoff);
  EXPECT_TRUE(service.config().force_message_ui_only);

  // Even after repeated impressions, ignores, and explicit dismissals on the
  // same origin without page navigations or time advancing, CanShowNudge
  // succeeds and GetCueUiTypeForCategory always returns kMessage.
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(service.CanShowNudge(url), ContextualCueingDecision::kSuccess);
    service.RecordCueShown(url, kEdu);
    service.RecordCueDismissed(url, kEdu);
    EXPECT_EQ(service.GetCueUiTypeForCategory(kEdu),
              ContextualCueUiType::kMessage);
    EXPECT_EQ(service.GetCueUiTypeForCategory(kShop),
              ContextualCueUiType::kMessage);
  }
}

}  // namespace contextual_cueing
