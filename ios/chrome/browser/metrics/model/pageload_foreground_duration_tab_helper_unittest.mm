// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/pageload_foreground_duration_tab_helper.h"

#import <UIKit/UIKit.h>

#import "components/ukm/ios/ukm_url_recorder.h"
#import "components/ukm/test_ukm_recorder.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {
const char kPageNavigationUkmEvent[] = "MainFrameNavigation";
const char kPageNavigationUkmMetric[] = "DidCommit";
const char kPageForegroundSessionUkmSearchMatchesEvent[] =
    "PageForegroundSession";
}

class PageloadForegroundDurationTabHelperTest : public PlatformTest {
 protected:
  PageloadForegroundDurationTabHelperTest() {
    // This must be initialized first so that it is the first receiver of
    // WebStateObserver calls to set the navigation id.
    ukm::InitializeSourceUrlRecorderForWebState(&web_state_);
    PageloadForegroundDurationTabHelper::CreateForWebState(&web_state_);
  }

  base::test::TaskEnvironment environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::FakeWebState web_state_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
};

// Tests that a navigation UKM is logged as true after a successful navigation.
TEST_F(PageloadForegroundDurationTabHelperTest,
       VerifyUKMLoggedAfterNavigation) {
  test_ukm_recorder_.Purge();
  web::FakeNavigationContext context_with_zero_nav_id;

  web_state_.WasShown();
  // No entry should be recorded for the interaction above.
  const auto& navigation_entries =
      test_ukm_recorder_.GetEntriesByName(kPageNavigationUkmEvent);
  ASSERT_EQ(0u, navigation_entries.size());

  web::FakeNavigationContext context;
  context.SetIsSameDocument(false);
  context.SetHasCommitted(true);
  web_state_.OnNavigationStarted(&context);
  // No navigation logging have occurred yet.
  const auto& navigation_start_navigation_entries =
      test_ukm_recorder_.GetEntriesByName(kPageNavigationUkmEvent);
  ASSERT_EQ(0u, navigation_start_navigation_entries.size());

  web_state_.OnNavigationFinished(&context);
  // A successful navigation should be recorded
  const auto& post_navigation_entry =
      test_ukm_recorder_.GetEntriesByName(kPageNavigationUkmEvent);
  ASSERT_EQ(1u, post_navigation_entry.size());
  const ukm::mojom::UkmEntry* entry = post_navigation_entry[0];
  ASSERT_TRUE(entry);
  EXPECT_NE(ukm::kInvalidSourceId, entry->source_id);
  test_ukm_recorder_.ExpectEntryMetric(entry, kPageNavigationUkmMetric, true);
}

// Tests that a navigation UKM is not logged if the navigation is within the
// same document.
TEST_F(PageloadForegroundDurationTabHelperTest,
       VerifyUKMNotLoggedAfterSameDocumentNavigation) {
  test_ukm_recorder_.Purge();
  web::FakeNavigationContext context_with_zero_nav_id;

  web_state_.WasShown();
  // No entry should be recorded for the interaction above.
  const auto& navigation_entries =
      test_ukm_recorder_.GetEntriesByName(kPageNavigationUkmEvent);
  ASSERT_EQ(0u, navigation_entries.size());

  web::FakeNavigationContext context;
  context.SetIsSameDocument(true);
  context.SetHasCommitted(true);
  web_state_.OnNavigationStarted(&context);
  // No navigation logging have occurred yet.
  const auto& navigation_start_navigation_entries =
      test_ukm_recorder_.GetEntriesByName(kPageNavigationUkmEvent);
  ASSERT_EQ(0u, navigation_start_navigation_entries.size());

  web_state_.OnNavigationFinished(&context);
  // A same-document navigation should result in no logging.
  const auto& post_navigation_entry =
      test_ukm_recorder_.GetEntriesByName(kPageNavigationUkmEvent);
  EXPECT_EQ(0u, post_navigation_entry.size());
}

// Tests that a UKM is logged after a page that is being shown is hidden.
TEST_F(PageloadForegroundDurationTabHelperTest,
       VerifyUKMLoggedAfterShowAndHide) {
  web::FakeNavigationContext context;
  context.SetIsSameDocument(false);
  context.SetHasCommitted(true);
  web_state_.OnNavigationStarted(&context);
  // Mark WebState as visible without logging a page session.
  web_state_.WasShown();
  web_state_.OnNavigationFinished(&context);
  // There should be no entries yet.
  const auto& page_session_entries = test_ukm_recorder_.GetEntriesByName(
      kPageForegroundSessionUkmSearchMatchesEvent);
  ASSERT_EQ(0u, page_session_entries.size());

  web_state_.WasHidden();
  const auto& after_hidden_page_session_entries =
      test_ukm_recorder_.GetEntriesByName(
          kPageForegroundSessionUkmSearchMatchesEvent);
  EXPECT_EQ(1u, after_hidden_page_session_entries.size());
}

// Tests that a UKM is logged after OnRenderProcessGone is called for a page
// that is being shown.
TEST_F(PageloadForegroundDurationTabHelperTest,
       VerifyUKMLoggedAfterRenderProcessGone) {
  web::FakeNavigationContext context;
  context.SetIsSameDocument(false);
  context.SetHasCommitted(true);
  web_state_.OnNavigationStarted(&context);
  // Mark WebState as visible without logging a page session.
  web_state_.WasShown();
  web_state_.OnNavigationFinished(&context);
  // There should be no entries yet.
  const auto& page_session_entries = test_ukm_recorder_.GetEntriesByName(
      kPageForegroundSessionUkmSearchMatchesEvent);
  ASSERT_EQ(0u, page_session_entries.size());

  web_state_.OnRenderProcessGone();
  const auto& after_render_gone_entries = test_ukm_recorder_.GetEntriesByName(
      kPageForegroundSessionUkmSearchMatchesEvent);
  EXPECT_EQ(1u, after_render_gone_entries.size());
}

// Tests that a UKM is logged after a
// UIApplicationDidEnterBackgroundNotification notification and after a
// successive UIApplicationWillEnterForegroundNotification notification that is
// followed by the Webstate being hidden.
TEST_F(PageloadForegroundDurationTabHelperTest,
       VerifyUKMLoggedAfterBackgroundingAndForegrounding) {
  web::FakeNavigationContext context;
  context.SetIsSameDocument(false);
  context.SetHasCommitted(true);
  web_state_.OnNavigationStarted(&context);
  // Mark WebState as visible without logging a page session.
  web_state_.WasShown();
  web_state_.OnNavigationFinished(&context);

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidEnterBackgroundNotification
                    object:nil];
  const auto& after_background_entries = test_ukm_recorder_.GetEntriesByName(
      kPageForegroundSessionUkmSearchMatchesEvent);
  ASSERT_EQ(1u, after_background_entries.size());

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationWillEnterForegroundNotification
                    object:nil];
  const auto& after_foreground_entries = test_ukm_recorder_.GetEntriesByName(
      kPageForegroundSessionUkmSearchMatchesEvent);
  ASSERT_EQ(1u, after_foreground_entries.size());

  web_state_.WasHidden();
  const auto& after_hidden_page_session_entries =
      test_ukm_recorder_.GetEntriesByName(
          kPageForegroundSessionUkmSearchMatchesEvent);
  EXPECT_EQ(2u, after_hidden_page_session_entries.size());
}

// Tests that no UKMs are logged as long as the WebState is not visible.
TEST_F(PageloadForegroundDurationTabHelperTest,
       VerifyNoUKMLoggedIfWebStateNotVisible) {
  // Mark WebState as not visible.
  web_state_.WasHidden();

  web::FakeNavigationContext context;
  context.SetIsSameDocument(false);
  context.SetHasCommitted(true);
  web_state_.OnNavigationStarted(&context);
  web_state_.OnNavigationFinished(&context);
  const auto& post_navigation_entries =
      test_ukm_recorder_.GetEntriesByName(kPageNavigationUkmEvent);
  EXPECT_EQ(0u, post_navigation_entries.size());

  web_state_.OnNavigationStarted(&context);
  web_state_.OnNavigationFinished(&context);
  const auto& second_post_navigation_entries =
      test_ukm_recorder_.GetEntriesByName(kPageNavigationUkmEvent);
  EXPECT_EQ(0u, second_post_navigation_entries.size());

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidEnterBackgroundNotification
                    object:nil];
  const auto& after_background_entries = test_ukm_recorder_.GetEntriesByName(
      kPageForegroundSessionUkmSearchMatchesEvent);
  ASSERT_EQ(0u, after_background_entries.size());

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationWillEnterForegroundNotification
                    object:nil];
  const auto& after_foreground_entries = test_ukm_recorder_.GetEntriesByName(
      kPageForegroundSessionUkmSearchMatchesEvent);
  ASSERT_EQ(0u, after_foreground_entries.size());

  web_state_.WasHidden();
  const auto& after_hidden_page_session_entries =
      test_ukm_recorder_.GetEntriesByName(
          kPageForegroundSessionUkmSearchMatchesEvent);
  EXPECT_EQ(0u, after_hidden_page_session_entries.size());
}
