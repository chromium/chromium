// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sharing/ui_bundled/activity_services/data/chrome_activity_item_thumbnail_generator.h"

#import "base/task/thread_pool.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/snapshots/model/fake_snapshot_generator_delegate.h"
#import "ios/chrome/browser/snapshots/model/snapshot_source_tab_helper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/web/common/uikit_ui_util.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"
#import "ui/base/test/ios/ui_image_test_utils.h"

namespace {

class ChromeActivityItemThumbnailGeneratorTest : public PlatformTest {
 protected:
  ChromeActivityItemThumbnailGeneratorTest() {
    // Add a fake view to the delegate, which is used to capture a snapshot.
    delegate_ = [[FakeSnapshotGeneratorDelegate alloc] init];
    CGRect frame = {CGPointZero, CGSizeMake(400, 300)};
    delegate_.view = [[UIView alloc] initWithFrame:frame];
    delegate_.view.backgroundColor = [UIColor redColor];
    UIWindow* window = GetAnyKeyWindow();
    [window addSubview:delegate_.view];
    [window makeKeyAndVisible];

    // Hack to forcefully render the view to successfully capture a snapshot.
    [NSRunLoop.currentRunLoop
        runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
    [window layoutIfNeeded];

    SnapshotTabHelper::CreateForWebState(&fake_web_state_);
    SnapshotSourceTabHelper::CreateForWebState(&fake_web_state_);
    SnapshotTabHelper::FromWebState(&fake_web_state_)->SetDelegate(delegate_);
  }

  void TearDown() override { [delegate_.view removeFromSuperview]; }

  base::test::TaskEnvironment task_environment_;
  FakeSnapshotGeneratorDelegate* delegate_ = nil;
  web::FakeWebState fake_web_state_;
};

TEST_F(ChromeActivityItemThumbnailGeneratorTest, Thumbnail) {
  CGSize size = CGSizeMake(50, 50);
  ChromeActivityItemThumbnailGenerator* generator =
      [[ChromeActivityItemThumbnailGenerator alloc]
          initWithWebState:&fake_web_state_];
  EXPECT_TRUE(generator);
  UIImage* thumbnail = [generator thumbnailWithSize:size];
  EXPECT_TRUE(thumbnail);
  EXPECT_TRUE(CGSizeEqualToSize(thumbnail.size, size));
}

TEST_F(ChromeActivityItemThumbnailGeneratorTest, DeallocOnBackgroundSequence) {
  // Create a closure that owns a ChromeActivityItemThumbnailGenerator and
  // that will deallocate it when it is run, allowing to deallocate it on
  // a background sequence.
  base::OnceClosure closure;
  @autoreleasepool {
    NS_VALID_UNTIL_END_OF_SCOPE
    ChromeActivityItemThumbnailGenerator* generator =
        [[ChromeActivityItemThumbnailGenerator alloc]
            initWithWebState:&fake_web_state_];

    closure = base::BindOnce(^(id object){/* do nothing */}, generator);
  }

  // Post the closure on a background sequence to force the deallocation
  // of the ChromeActivityItemThumbnailGenerator on another sequence. It
  // should not crash.
  base::RunLoop run_loop;
  base::ThreadPool::PostTaskAndReply(FROM_HERE, {base::MayBlock()},
                                     std::move(closure),
                                     run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace
