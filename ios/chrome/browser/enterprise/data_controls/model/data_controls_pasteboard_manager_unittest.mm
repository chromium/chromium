// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_pasteboard_manager.h"

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"
#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_test_utils.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace data_controls {

namespace {

const char kSourceURL[] = "https://chromium.org";
const char kPasteboardString[] = "test";
const char kNewPasteboardString[] = "new content";

}  // namespace

class DataControlsPasteboardManagerTest : public PlatformTest {
 protected:
  DataControlsPasteboardManagerTest() {
    profile_ =
        profile_manager_.AddProfileWithBuilder(TestProfileIOS::Builder());
    manager_ = DataControlsPasteboardManager::GetInstance();
  }

  void TearDown() override {
    manager_->ResetForTesting();
    PlatformTest::TearDown();
  }

  void RestoreItemsToGeneralPasteboard() {
    // Wait for the internal state to be updated. This is necessary as we access
    // the pasteboard async due to bugs in UIPasteboard that block the thread
    // accessing it.
    ASSERT_TRUE(WaitForKnownPasteboardSource());
    base::test::TestFuture<void> future;
    manager_->RestoreItemsToGeneralPasteboardIfNeeded(future.GetCallback());
    EXPECT_TRUE(future.Wait());
  }

  void RestorePlaceholderToGeneralPasteboard() {
    // Wait for the internal state to be updated. This is necessary as we access
    // the pasteboard async due to bugs in UIPasteboard that block the thread
    // accessing it.
    ASSERT_TRUE(WaitForKnownPasteboardSource());

    manager_->RestorePlaceholderToGeneralPasteboardIfNeeded();
  }

  base::test::TaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<ProfileIOS> profile_;

  raw_ptr<DataControlsPasteboardManager> manager_;
};

// Tests that DataControlsPasteboardManager returns an empty pasteboard source
// after initialization.
TEST_F(DataControlsPasteboardManagerTest, InitialState) {
  PasteboardSource pasteboard_source =
      manager_->GetCurrentPasteboardItemsSource();

  EXPECT_TRUE(pasteboard_source.source_url.is_empty());
  EXPECT_FALSE(pasteboard_source.source_profile);
}

// Tests that DataControlsPasteboardManager returns the pasteboard source only
// after setting the source and the pasteboard items change.
TEST_F(DataControlsPasteboardManagerTest, SetAndGetSource) {
  GURL source_url(kSourceURL);
  manager_->SetNextPasteboardItemsSource(source_url, profile_,
                                         /* os_clipboard_allowed= */ true);

  PasteboardSource pasteboard_source =
      manager_->GetCurrentPasteboardItemsSource();

  // Source should not be available yet, as the pasteboard hasn't changed.
  EXPECT_TRUE(pasteboard_source.source_url.is_empty());
  EXPECT_FALSE(pasteboard_source.source_profile);

  // Simulate a pasteboard change.
  UIPasteboard.generalPasteboard.string = @(kPasteboardString);

  EXPECT_TRUE(WaitForKnownPasteboardSource());

  // Now the source should be available.
  pasteboard_source = manager_->GetCurrentPasteboardItemsSource();
  EXPECT_EQ(source_url, pasteboard_source.source_url);
  EXPECT_EQ(profile_, pasteboard_source.source_profile);
}

// Tests that DataControlsPasteboardManager discards the stored pasteboard
// source when the corresponding pasteboard items are replaced with new ones.
TEST_F(DataControlsPasteboardManagerTest,
       SourceIsInvalidatedAfterPasteboardChange) {
  // Set up pasteboard source and items.
  GURL source_url(kSourceURL);
  manager_->SetNextPasteboardItemsSource(source_url, profile_,
                                         /* os_clipboard_allowed= */ true);
  UIPasteboard.generalPasteboard.string = @(kPasteboardString);

  EXPECT_TRUE(WaitForKnownPasteboardSource());
  // There should be a known pasteboard source.
  PasteboardSource pasteboard_source =
      manager_->GetCurrentPasteboardItemsSource();
  EXPECT_EQ(source_url, pasteboard_source.source_url);
  EXPECT_EQ(profile_, pasteboard_source.source_profile);

  // Updating the pasteboard should invalidate the known pasteboard source.
  UIPasteboard.generalPasteboard.string = @(kNewPasteboardString);

  EXPECT_TRUE(WaitForUnknownPasteboardSource());

  // Pasteboard source should be empty now.
  pasteboard_source = manager_->GetCurrentPasteboardItemsSource();
  EXPECT_TRUE(pasteboard_source.source_url.is_empty());
  EXPECT_FALSE(pasteboard_source.source_profile);
}

// Tests that DataControlsPasteboardManager discards the stored pasteboard
// source when the pasteboard content changes outside the app.
TEST_F(DataControlsPasteboardManagerTest,
       SourceIsInvalidatedAfterExternalPasteboardChange) {
  // Set up pasteboard source and items.
  GURL source_url(kSourceURL);
  manager_->SetNextPasteboardItemsSource(source_url, profile_,
                                         /* os_clipboard_allowed= */ true);
  UIPasteboard.generalPasteboard.string = @(kPasteboardString);

  EXPECT_TRUE(WaitForKnownPasteboardSource());
  // There should be a known pasteboard source.
  PasteboardSource pasteboard_source =
      manager_->GetCurrentPasteboardItemsSource();
  EXPECT_EQ(source_url, pasteboard_source.source_url);
  EXPECT_EQ(profile_, pasteboard_source.source_profile);

  // Create a partial mock of the pasteboard to simulate the pasteboard contents
  // changing outside of the app.
  NSInteger original_change_count = UIPasteboard.generalPasteboard.changeCount;
  __block NSInteger change_count = original_change_count;
  id mock_pasteboard = OCMPartialMock(UIPasteboard.generalPasteboard);

  // Increase the changeCount value of the Pasteboard whenever it is queried to
  // simulate a Pasteboard change.
  OCMStub([mock_pasteboard changeCount]).andDo(^(NSInvocation* invocation) {
    NSInteger new_change_count = change_count++;
    [invocation setReturnValue:&new_change_count];
  });

  // Simulate the app going to the background.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationWillResignActiveNotification
                    object:nil
                  userInfo:nil];

  // The pasteboard observer should query and cache the pasteboard count when
  // the app is going to the background. We compare the cached value with the
  // current one when the app is active again and use a change as a signal that
  // the pasteboard source should be invalidated.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(
      kWaitForUIElementTimeout, /* run_message_loop= */ true, ^bool {
        return change_count > original_change_count;
      }));

  // Simulate the app becoming active again.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidBecomeActiveNotification
                    object:nil
                  userInfo:nil];

  // The pasteboard source should be invalidated because the pasteboard was
  // updated outside the app.
  EXPECT_TRUE(WaitForUnknownPasteboardSource());
  pasteboard_source = manager_->GetCurrentPasteboardItemsSource();
  EXPECT_TRUE(pasteboard_source.source_url.is_empty());
  EXPECT_FALSE(pasteboard_source.source_profile);
}

// Tests that protected items are replaced with a placeholder after being set.
TEST_F(DataControlsPasteboardManagerTest, ReplaceItems) {
  GURL source_url(kSourceURL);
  manager_->SetNextPasteboardItemsSource(source_url, profile_,
                                         /* os_clipboard_allowed= */ false);
  // Setting something in the pasteboard should trigger a replacement.
  UIPasteboard.generalPasteboard.string = @(kPasteboardString);

  EXPECT_TRUE(WaitForStringInPasteboard(l10n_util::GetNSString(
      IDS_ENTERPRISE_DATA_CONTROLS_COPY_PREVENTION_WARNING_MESSAGE)));
}

// Tests that protected items are restored to the pasteboard.
TEST_F(DataControlsPasteboardManagerTest,
       RestoreItemsToGeneralPasteboardIfNeededRestoresProtectedItems) {
  GURL source_url(kSourceURL);
  manager_->SetNextPasteboardItemsSource(source_url, profile_,
                                         /* os_clipboard_allowed= */ false);
  UIPasteboard.generalPasteboard.string = @(kPasteboardString);

  EXPECT_TRUE(WaitForStringInPasteboard(l10n_util::GetNSString(
      IDS_ENTERPRISE_DATA_CONTROLS_COPY_PREVENTION_WARNING_MESSAGE)));

  // Restore the original items.
  RestoreItemsToGeneralPasteboard();

  EXPECT_TRUE(WaitForStringInPasteboard(@(kPasteboardString)));
}

// Tests that nothing is restored to the pasteboard if the items are not
// protected.
TEST_F(DataControlsPasteboardManagerTest,
       RestoreItemsToGeneralPasteboardIfNeededDoesNothingForUnprotectedItems) {
  // Set up pasteboard source and items with os_clipboard_allowed=true.
  GURL source_url(kSourceURL);
  manager_->SetNextPasteboardItemsSource(source_url, profile_,
                                         /* os_clipboard_allowed= */ true);
  UIPasteboard.generalPasteboard.string = @(kPasteboardString);

  RestoreItemsToGeneralPasteboard();

  EXPECT_TRUE(WaitForKnownPasteboardSource());

  // The pasteboard should still contain the original string.
  EXPECT_NSEQ(@(kPasteboardString), UIPasteboard.generalPasteboard.string);
}

// Tests that the placeholder is restored to the pasteboard.
TEST_F(DataControlsPasteboardManagerTest,
       RestorePlaceholderToGeneralPasteboardIfNeededRestoresPlaceholder) {
  GURL source_url(kSourceURL);
  manager_->SetNextPasteboardItemsSource(source_url, profile_,
                                         /* os_clipboard_allowed= */ false);
  UIPasteboard.generalPasteboard.string = @(kPasteboardString);

  EXPECT_TRUE(WaitForKnownPasteboardSource());

  EXPECT_NSNE(@(kPasteboardString), UIPasteboard.generalPasteboard.string);

  // Restore the original items and then the placeholder.
  RestoreItemsToGeneralPasteboard();
  RestorePlaceholderToGeneralPasteboard();

  EXPECT_TRUE(WaitForStringInPasteboard(l10n_util::GetNSString(
      IDS_ENTERPRISE_DATA_CONTROLS_COPY_PREVENTION_WARNING_MESSAGE)));
}

// Tests that nothing is restored to the pasteboard if the items are not
// protected.
TEST_F(
    DataControlsPasteboardManagerTest,
    RestorePlaceholderToGeneralPasteboardIfNeededDoesNothingForUnprotectedItems) {
  GURL source_url(kSourceURL);
  manager_->SetNextPasteboardItemsSource(source_url, profile_,
                                         /* os_clipboard_allowed= */ true);
  UIPasteboard.generalPasteboard.string = @(kPasteboardString);

  // Try to restore the placeholder.
  RestorePlaceholderToGeneralPasteboard();

  // The original string should be in the pasteboard.
  EXPECT_TRUE(WaitForStringInPasteboard(@(kPasteboardString)));
}

}  // namespace data_controls
