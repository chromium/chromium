// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_in_page/model/find_tab_helper.h"

#import "base/test/scoped_feature_list.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "components/ukm/test_ukm_recorder.h"
#import "ios/chrome/browser/find_in_page/model/find_in_page_model.h"
#import "ios/chrome/browser/shared/public/commands/fullscreen_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/fakes/crw_fake_find_interaction.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/metrics/public/cpp/ukm_builders.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using FindInPageUkmEntry = ukm::builders::IOS_FindInPageSearchMatches;

}  // namespace

// Fake FullscreenCommands handler for FindTabHelper tests. This is necessary as
// when Find in Page opens on iOS, Chrome expands the webpage to fullscreen and
// thus expects a Handler to be registered.
@interface FakeFullscreenCommandsHandler : NSObject <FullscreenCommands>
@end

@implementation FakeFullscreenCommandsHandler

- (void)enterFullscreenWithTrigger:(FullscreenModeTransitionTrigger)trigger
                          animated:(BOOL)animated {
}
- (void)exitFullscreenWithTrigger:(FullscreenModeTransitionTrigger)trigger
                         animated:(BOOL)animated {
}
- (void)disableFullscreenAnimated:(BOOL)animated {
}
- (void)reenableFullscreen {
}
- (void)exitForceFullscreen {
}
- (void)forceFullscreen:(BOOL)enable feature:(ForceFullscreenFeature)feature {
}

@end

// Test fixture for FindTabHelper.
class FindTabHelperTest : public PlatformTest {
 protected:
  FindTabHelperTest() {
    scoped_feature_list_.InitAndEnableFeature(kFullscreenRefactoring);
    ukm::InitializeSourceUrlRecorderForWebState(&web_state_);
    web_state_.SetFindInteractionEnabled(true);
    web_state_.SetFindInteraction([[CRWFakeFindInteraction alloc] init]);
    FindTabHelper::CreateForWebState(&web_state_);
    fullscreen_handler_ = [[FakeFullscreenCommandsHandler alloc] init];
    find_tab_helper()->SetFullscreenHandler(fullscreen_handler_);
  }

  void NavigateToPage() {
    web::FakeNavigationContext context;
    context.SetHasCommitted(true);
    context.SetIsSameDocument(false);
    web_state_.OnNavigationStarted(&context);
    web_state_.OnNavigationFinished(&context);
  }

  FindTabHelper* find_tab_helper() {
    return FindTabHelper::FromWebState(&web_state_);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
  FakeFullscreenCommandsHandler* fullscreen_handler_ = nil;
  web::FakeWebState web_state_;
};

// Tests that calling DismissFindNavigator() or StopFinding() when the Find UI
// is not active does not record IOS.FindInPageSearchMatches UKM entries.
TEST_F(FindTabHelperTest, DismissWhenInactiveDoesNotRecordUKM) {
  NavigateToPage();
  ASSERT_FALSE(find_tab_helper()->IsFindUIActive());

  web_state_.WasHidden();
  find_tab_helper()->DismissFindNavigator();
  find_tab_helper()->StopFinding();

  const auto entries =
      test_ukm_recorder_.GetEntriesByName(FindInPageUkmEntry::kEntryName);
  EXPECT_TRUE(entries.empty());
}

// Tests that calling DismissFindNavigator() and StopFinding() when the Find UI
// is active records IOS.FindInPageSearchMatches UKM entries, and that
// DismissFindNavigator() is a no-op once StopFinding() marks the UI inactive.
TEST_F(FindTabHelperTest, DismissAndStopWhenActiveRecordUKM) {
  NavigateToPage();
  find_tab_helper()->SetFindUIActive(true);
  ASSERT_TRUE(find_tab_helper()->IsFindUIActive());

  // Hiding the WebState (e.g. on tab switch) dismisses the navigator and
  // records UKM while keeping IsFindUIActive() true so the UI can be restored
  // when returning to the tab.
  web_state_.WasHidden();
  EXPECT_TRUE(find_tab_helper()->IsFindUIActive());

  auto entries =
      test_ukm_recorder_.GetEntriesByName(FindInPageUkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());
  test_ukm_recorder_.ExpectEntryMetric(
      entries[0], FindInPageUkmEntry::kHasMatchesName, false);

  // Stopping Find in Page marks the UI inactive and records UKM.
  find_tab_helper()->StopFinding();
  EXPECT_FALSE(find_tab_helper()->IsFindUIActive());

  entries = test_ukm_recorder_.GetEntriesByName(FindInPageUkmEntry::kEntryName);
  ASSERT_EQ(2u, entries.size());
  test_ukm_recorder_.ExpectEntryMetric(
      entries[1], FindInPageUkmEntry::kHasMatchesName, false);

  // Subsequent DismissFindNavigator() calls after StopFinding() should not
  // record additional UKM entries.
  find_tab_helper()->DismissFindNavigator();
  entries = test_ukm_recorder_.GetEntriesByName(FindInPageUkmEntry::kEntryName);
  EXPECT_EQ(2u, entries.size());
}
