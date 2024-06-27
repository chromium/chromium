// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface FakeSnapshotConsumer : NSObject <LensOverlaySnapshotConsumer>
@property(nonatomic, copy) void (^onSnapshotLoaded)();
@end
@implementation FakeSnapshotConsumer
- (void)loadSnapshot:(UIImage*)snapshot {
  self.onSnapshotLoaded();
}

@end

@interface FakeResultConsumer : NSObject <LensOverlayResultConsumer>
@property(nonatomic, assign) GURL lastPushedURL;
@end

@implementation FakeResultConsumer
- (void)loadResultsURL:(GURL)url {
  self.lastPushedURL = url;
}
@end

namespace {

class LensOverlayMediatorTest : public PlatformTest {
 public:
  LensOverlayMediatorTest() {
    mediator_ = [[LensOverlayMediator alloc] init];
    mock_result_consumer_ = [[FakeResultConsumer alloc] init];
    fake_snapshot_consumer_ = [[FakeSnapshotConsumer alloc] init];
    mediator_.resultConsumer = mock_result_consumer_;
    mediator_.snapshotConsumer = fake_snapshot_consumer_;
  }

 protected:
  LensOverlayMediator* mediator_;

  FakeResultConsumer* mock_result_consumer_;
  FakeSnapshotConsumer* fake_snapshot_consumer_;
};

TEST_F(LensOverlayMediatorTest, ShouldPushURLToConsumerOnSelection) {
  GURL testURL = GURL("http://test.com");

  [mediator_ selectionUI:nil
             performedSelection:nil
      constructedResultsPageURL:testURL
                 suggestSignals:@"test_iil"];

  EXPECT_EQ(mock_result_consumer_.lastPushedURL, testURL);
}

TEST_F(LensOverlayMediatorTest, ShouldRouteTheImageToTheConsumerWhenStarted) {
  __block BOOL didReceiveSnapshot = false;

  // Given a test snapshot image.
  UIImage* testSnapshot = [[UIImage alloc] init];
  fake_snapshot_consumer_.onSnapshotLoaded = ^void() {
    didReceiveSnapshot = true;
  };

  // When the mediator starts the flow with the snapshot image.
  [mediator_ startWithSnapshot:testSnapshot];

  // Then the consumer should receive the snapshot.
  EXPECT_TRUE(didReceiveSnapshot);
}

}  // namespace
