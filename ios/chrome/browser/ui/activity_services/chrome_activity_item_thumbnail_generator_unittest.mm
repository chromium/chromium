// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/chrome_activity_item_thumbnail_generator.h"

#include "base/test/task_environment.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/snapshots/fake_snapshot_generator_delegate.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/platform_test.h"
#include "ui/base/test/ios/ui_image_test_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class ChromeActivityItemThumbnailGeneratorTest : public PlatformTest {
 protected:
  ChromeActivityItemThumbnailGeneratorTest() {
    chrome_browser_state_ = TestChromeBrowserState::Builder().Build();

    delegate_ = [[FakeSnapshotGeneratorDelegate alloc] init];
    CGRect frame = {CGPointZero, CGSizeMake(400, 300)};
    delegate_.view = [[UIView alloc] initWithFrame:frame];
    delegate_.view.backgroundColor = [UIColor redColor];
    SnapshotTabHelper::CreateForWebState(&test_web_state_,
                                         [[NSUUID UUID] UUIDString]);
    SnapshotTabHelper::FromWebState(&test_web_state_)->SetDelegate(delegate_);
  }

  void SetIncognito(bool incognito) {
    test_web_state_.SetBrowserState(
        incognito ? chrome_browser_state_->GetOffTheRecordChromeBrowserState()
                  : chrome_browser_state_.get());
  }

  FakeSnapshotGeneratorDelegate* delegate_ = nil;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  web::TestWebState test_web_state_;
};

TEST_F(ChromeActivityItemThumbnailGeneratorTest,
       ThumbnailForNonIncognitoWebState) {
  SetIncognito(/*incognito=*/false);

  CGSize size = CGSizeMake(50, 50);
  ChromeActivityItemThumbnailGenerator* generator =
      [[ChromeActivityItemThumbnailGenerator alloc]
          initWithWebState:&test_web_state_];
  EXPECT_TRUE(generator);
  UIImage* thumbnail = [generator thumbnailWithSize:size];
  EXPECT_TRUE(thumbnail);
  EXPECT_TRUE(CGSizeEqualToSize(thumbnail.size, size));
}

TEST_F(ChromeActivityItemThumbnailGeneratorTest,
       NoThumbnailForIncognitoWebState) {
  SetIncognito(/*incognito=*/true);

  CGSize size = CGSizeMake(50, 50);
  ChromeActivityItemThumbnailGenerator* generator =
      [[ChromeActivityItemThumbnailGenerator alloc]
          initWithWebState:&test_web_state_];
  EXPECT_TRUE(generator);
  UIImage* thumbnail = [generator thumbnailWithSize:size];
  EXPECT_FALSE(thumbnail);
}

}  // namespace
