// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/run_loop.h"
#import "ios/chrome/browser/shared/ui/util/image/image_util.h"
#import "ios/chrome/browser/snapshots/model/fake_snapshot_generator_delegate.h"
#import "ios/chrome/browser/snapshots/model/legacy_snapshot_generator.h"
#import "ios/chrome/browser/snapshots/model/model_swift.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/snapshots/model/web_state_snapshot_info.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "ui/base/test/ios/ui_image_test_utils.h"
#import "ui/gfx/image/image.h"

namespace {

// Dimension of the WebState's view.
constexpr CGSize kWebStateViewSize = {300, 400};

// Returns whether the `image` dominant color is `color`.
bool IsDominantColorForImage(UIImage* image, UIColor* color) {
  return
      [color isEqual:DominantColorForImage(gfx::Image(image), /*opacity=*/1.0)];
}

// Overrides `TakeSnapshot()` to return a placeholder UIImage.
class FakeWebStateWithSnapshot : public web::FakeWebState {
  void TakeSnapshot(const CGRect rect, SnapshotCallback callback) override {
    std::move(callback).Run(
        ui::test::uiimage_utils::UIImageWithSizeAndSolidColor(
            kWebStateViewSize, [UIColor blueColor]));
  }
};

}  // namespace

// TODO(crbug.com/40943236): Remove this test once the new implementation
// written in Swift is used by default.
class LegacySnapshotGeneratorTest : public PlatformTest {
 public:
  LegacySnapshotGeneratorTest() {
    // Create the LegacySnapshotGenerator with a fake delegate.
    delegate_ = [[FakeSnapshotGeneratorDelegate alloc] init];
    generator_ = [[LegacySnapshotGenerator alloc] initWithWebState:&web_state_];
    generator_.delegate = delegate_;

    // Add a base view to the web state.
    CGRect frame = {CGPointZero, kWebStateViewSize};
    UIView* view = [[UIView alloc] initWithFrame:frame];
    view.backgroundColor = [UIColor redColor];
    delegate_.view = view;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  LegacySnapshotGenerator* generator_ = nil;
  FakeSnapshotGeneratorDelegate* delegate_ = nil;
  FakeWebStateWithSnapshot web_state_;
};

// Tests the snapshot taken by UIKit-based API.
TEST_F(LegacySnapshotGeneratorTest, GenerateUIViewSnapshot) {
  UIImage* snapshot = [generator_ generateUIViewSnapshot];

  ASSERT_TRUE(snapshot);
  EXPECT_TRUE(CGSizeEqualToSize(snapshot.size, kWebStateViewSize));
  EXPECT_TRUE(IsDominantColorForImage(snapshot, [UIColor redColor]));
}

// Tests the snapshot taken by WebKit-based API.
TEST_F(LegacySnapshotGeneratorTest, GenerateWebViewSnapshot) {
  // Enable the flag to take a snapshot with WebKit-based API.
  web_state_.SetCanTakeSnapshot(true);

  base::RunLoop run_loop;
  base::RunLoop* run_loop_ptr = &run_loop;

  __block UIImage* snapshot = nil;
  [generator_ generateSnapshotWithCompletion:^(UIImage* image) {
    snapshot = image;
    run_loop_ptr->Quit();
  }];

  run_loop.Run();

  ASSERT_TRUE(snapshot);
  EXPECT_TRUE(CGSizeEqualToSize(snapshot.size, kWebStateViewSize));
  EXPECT_TRUE(IsDominantColorForImage(snapshot, [UIColor blueColor]));
}

class SnapshotGeneratorTest : public PlatformTest {
 public:
  SnapshotGeneratorTest() {
    // Create the SnapshotGenerator with a fake delegate.
    delegate_ = [[FakeSnapshotGeneratorDelegate alloc] init];
    generator_ = [[SnapshotGenerator alloc]
        initWithWebStateInfo:[[WebStateSnapshotInfo alloc]
                                 initWithWebState:&web_state_]];
    generator_.delegate = delegate_;

    // Add a base view to the web state.
    CGRect frame = {CGPointZero, kWebStateViewSize};
    UIView* view = [[UIView alloc] initWithFrame:frame];
    view.backgroundColor = [UIColor redColor];
    delegate_.view = view;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  SnapshotGenerator* generator_ = nil;
  FakeSnapshotGeneratorDelegate* delegate_ = nil;
  FakeWebStateWithSnapshot web_state_;
};

// Tests the snapshot taken by UIKit-based API.
TEST_F(SnapshotGeneratorTest, GenerateUIViewSnapshot) {
  UIImage* snapshot = [generator_ generateUIViewSnapshot];

  ASSERT_TRUE(snapshot);
  EXPECT_TRUE(CGSizeEqualToSize(snapshot.size, kWebStateViewSize));
  EXPECT_TRUE(IsDominantColorForImage(snapshot, [UIColor redColor]));
}

// Tests the snapshot taken by WebKit-based API.
TEST_F(SnapshotGeneratorTest, GenerateWebViewSnapshot) {
  // Enable the flag to take a snapshot with WebKit-based API.
  web_state_.SetCanTakeSnapshot(true);

  base::RunLoop run_loop;
  base::RunLoop* run_loop_ptr = &run_loop;

  __block UIImage* snapshot = nil;
  [generator_ generateSnapshotWithCompletion:^(UIImage* image) {
    snapshot = image;
    run_loop_ptr->Quit();
  }];

  run_loop.Run();

  ASSERT_TRUE(snapshot);
  EXPECT_TRUE(CGSizeEqualToSize(snapshot.size, kWebStateViewSize));
  EXPECT_TRUE(IsDominantColorForImage(snapshot, [UIColor blueColor]));
}

@interface FakeOverlaysSnapshotGeneratorDelegate : FakeSnapshotGeneratorDelegate
@end

@implementation FakeOverlaysSnapshotGeneratorDelegate

#pragma mark - SnapshotGeneratorDelegate

- (NSArray<UIView*>*)snapshotOverlaysWithWebStateInfo:
    (WebStateSnapshotInfo*)webStateInfo {
  CGRect frame = {CGPointZero, kWebStateViewSize};
  UIView* overlay = [[UIView alloc] initWithFrame:frame];
  overlay.backgroundColor = [UIColor greenColor];
  return @[ overlay ];
}

@end

// TODO(crbug.com/40943236): Remove this test once the new implementation
// written in Swift is used by default.
class LegacySnapshotGeneratorWithOverlaysTest : public PlatformTest {
 public:
  LegacySnapshotGeneratorWithOverlaysTest() {
    // Create the LegacySnapshotGenerator with a fake delegate. The fake
    // delegate returns an overlay.
    delegate_ = [[FakeOverlaysSnapshotGeneratorDelegate alloc] init];
    generator_ = [[LegacySnapshotGenerator alloc] initWithWebState:&web_state_];
    generator_.delegate = delegate_;

    // Add a base view to the web state.
    CGRect frame = {CGPointZero, kWebStateViewSize};
    UIView* view = [[UIView alloc] initWithFrame:frame];
    view.backgroundColor = [UIColor redColor];
    delegate_.view = view;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  LegacySnapshotGenerator* generator_ = nil;
  FakeOverlaysSnapshotGeneratorDelegate* delegate_ = nil;
  FakeWebStateWithSnapshot web_state_;
};

// Tests the snapshot taken by UIKit-based API. The page has an overlay.
TEST_F(LegacySnapshotGeneratorWithOverlaysTest, GenerateUIViewSnapshot) {
  UIImage* snapshot = [generator_ generateUIViewSnapshotWithOverlays];

  // The base color of UIView is red, but it's overriden by an overlay.
  ASSERT_TRUE(snapshot);
  EXPECT_TRUE(CGSizeEqualToSize(snapshot.size, kWebStateViewSize));
  EXPECT_TRUE(IsDominantColorForImage(snapshot, [UIColor greenColor]));
}

// Tests the snapshot taken by UIKit-based API. The page has an overlay.
TEST_F(LegacySnapshotGeneratorWithOverlaysTest, GenerateWebViewSnapshot) {
  // Enable the flag to take a snapshot with WebKit-based API.
  web_state_.SetCanTakeSnapshot(true);

  base::RunLoop run_loop;
  base::RunLoop* run_loop_ptr = &run_loop;

  __block UIImage* snapshot = nil;
  [generator_ generateSnapshotWithCompletion:^(UIImage* image) {
    snapshot = image;
    run_loop_ptr->Quit();
  }];

  run_loop.Run();

  // The color should be green because there is an overlay filled with green.
  ASSERT_TRUE(snapshot);
  EXPECT_TRUE(CGSizeEqualToSize(snapshot.size, kWebStateViewSize));
  EXPECT_TRUE(IsDominantColorForImage(snapshot, [UIColor greenColor]));
}

class SnapshotGeneratorWithOverlaysTest : public PlatformTest {
 public:
  SnapshotGeneratorWithOverlaysTest() {
    // Create the SnapshotGenerator with a fake delegate. The fake
    // delegate returns an overlay.
    delegate_ = [[FakeOverlaysSnapshotGeneratorDelegate alloc] init];
    generator_ = [[SnapshotGenerator alloc]
        initWithWebStateInfo:[[WebStateSnapshotInfo alloc]
                                 initWithWebState:&web_state_]];
    generator_.delegate = delegate_;

    // Add a base view to the web state.
    CGRect frame = {CGPointZero, kWebStateViewSize};
    UIView* view = [[UIView alloc] initWithFrame:frame];
    view.backgroundColor = [UIColor redColor];
    delegate_.view = view;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  SnapshotGenerator* generator_ = nil;
  FakeOverlaysSnapshotGeneratorDelegate* delegate_ = nil;
  FakeWebStateWithSnapshot web_state_;
};

// Tests the snapshot taken by UIKit-based API. The page has an overlay.
TEST_F(SnapshotGeneratorWithOverlaysTest, GenerateUIViewSnapshot) {
  UIImage* snapshot = [generator_ generateUIViewSnapshotWithOverlays];

  // The base color of UIView is red, but it's overriden by an overlay.
  ASSERT_TRUE(snapshot);
  EXPECT_TRUE(CGSizeEqualToSize(snapshot.size, kWebStateViewSize));
  EXPECT_TRUE(IsDominantColorForImage(snapshot, [UIColor greenColor]));
}

// Tests the snapshot taken by UIKit-based API. The page has an overlay.
TEST_F(SnapshotGeneratorWithOverlaysTest, GenerateWebViewSnapshot) {
  // Enable the flag to take a snapshot with WebKit-based API.
  web_state_.SetCanTakeSnapshot(true);

  base::RunLoop run_loop;
  base::RunLoop* run_loop_ptr = &run_loop;

  __block UIImage* snapshot = nil;
  [generator_ generateSnapshotWithCompletion:^(UIImage* image) {
    snapshot = image;
    run_loop_ptr->Quit();
  }];

  run_loop.Run();

  // The color should be green because there is an overlay filled with green.
  ASSERT_TRUE(snapshot);
  EXPECT_TRUE(CGSizeEqualToSize(snapshot.size, kWebStateViewSize));
  EXPECT_TRUE(IsDominantColorForImage(snapshot, [UIColor greenColor]));
}
