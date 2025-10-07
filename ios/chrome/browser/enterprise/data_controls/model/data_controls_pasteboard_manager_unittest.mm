// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_pasteboard_manager.h"

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
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

  // Waits until DataControlsPasteboardManagerTest returns a non-empty
  // pasteboard source.
  void WaitForPasteboardSource() {
    EXPECT_TRUE(WaitUntilConditionOrTimeout(
        kWaitForUIElementTimeout, /* run_message_loop= */ true, ^bool {
          return manager_->GetCurrentPasteboardItemsSource().source_profile;
        }));
  }

  void WaitForEmptyPasteboardSource() {
    EXPECT_TRUE(WaitUntilConditionOrTimeout(
        kWaitForUIElementTimeout, /* run_message_loop= */ true, ^bool {
          return manager_->GetCurrentPasteboardItemsSource()
              .source_url.is_empty();
        }));
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
  manager_->SetNextPasteboardItemsSource(source_url, profile_);

  PasteboardSource pasteboard_source =
      manager_->GetCurrentPasteboardItemsSource();

  // Source should not be available yet, as the pasteboard hasn't changed.
  EXPECT_TRUE(pasteboard_source.source_url.is_empty());
  EXPECT_FALSE(pasteboard_source.source_profile);

  // Simulate a pasteboard change.
  UIPasteboard.generalPasteboard.string = @(kPasteboardString);

  WaitForPasteboardSource();

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
  manager_->SetNextPasteboardItemsSource(source_url, profile_);
  UIPasteboard.generalPasteboard.string = @(kPasteboardString);

  WaitForPasteboardSource();
  // There should be a known pasteboard source.
  PasteboardSource pasteboard_source =
      manager_->GetCurrentPasteboardItemsSource();
  EXPECT_EQ(source_url, pasteboard_source.source_url);
  EXPECT_EQ(profile_, pasteboard_source.source_profile);

  // Updating the pasteboard should invalidate the known pasteboard source.
  UIPasteboard.generalPasteboard.string = @(kNewPasteboardString);

  WaitForEmptyPasteboardSource();

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
  manager_->SetNextPasteboardItemsSource(source_url, profile_);
  UIPasteboard.generalPasteboard.string = @(kPasteboardString);

  WaitForPasteboardSource();
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
  WaitForEmptyPasteboardSource();
  pasteboard_source = manager_->GetCurrentPasteboardItemsSource();
  EXPECT_TRUE(pasteboard_source.source_url.is_empty());
  EXPECT_FALSE(pasteboard_source.source_profile);
}

}  // namespace data_controls
