// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"

#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"

namespace {

using DefaultBrowserInterestSignalsTest = PlatformTest;

// Tests that pasting and navigating to a URL records the promo signal.
TEST_F(DefaultBrowserInterestSignalsTest,
       URLCopyPasteAndNavigateNotifiesTracker) {
  feature_engagement::test::MockTracker tracker;
  EXPECT_CALL(tracker,
              NotifyEvent(feature_engagement::events::
                              kGenericDefaultBrowserPromoConditionsMet));

  default_browser::NotifyOmniboxURLCopyPasteAndNavigate(
      /*is_off_record=*/false, &tracker, /*scene_state=*/nil);
}

// Tests that pasting and navigating to a URL in Incognito records nothing.
TEST_F(DefaultBrowserInterestSignalsTest,
       URLCopyPasteAndNavigateInIncognitoDoesNotNotifyTracker) {
  feature_engagement::test::MockTracker tracker;
  EXPECT_CALL(tracker, NotifyEvent(testing::_)).Times(0);

  default_browser::NotifyOmniboxURLCopyPasteAndNavigate(
      /*is_off_record=*/true, &tracker, /*scene_state=*/nil);
}

// Tests that pasting and searching a text records the promo signal.
TEST_F(DefaultBrowserInterestSignalsTest,
       TextCopyPasteAndNavigateNotifiesTracker) {
  feature_engagement::test::MockTracker tracker;
  EXPECT_CALL(tracker,
              NotifyEvent(feature_engagement::events::
                              kGenericDefaultBrowserPromoConditionsMet));

  default_browser::NotifyOmniboxTextCopyPasteAndNavigate(
      /*is_off_record=*/false, &tracker);
}

// Tests that pasting and searching a text in Incognito records nothing.
TEST_F(DefaultBrowserInterestSignalsTest,
       TextCopyPasteAndNavigateInIncognitoDoesNotNotifyTracker) {
  feature_engagement::test::MockTracker tracker;
  EXPECT_CALL(tracker, NotifyEvent(testing::_)).Times(0);

  default_browser::NotifyOmniboxTextCopyPasteAndNavigate(
      /*is_off_record=*/true, &tracker);
}

}  // namespace
