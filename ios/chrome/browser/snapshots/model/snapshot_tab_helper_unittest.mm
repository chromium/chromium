// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"

#import "base/files/scoped_temp_dir.h"
#import "base/run_loop.h"
#import "ios/chrome/browser/shared/ui/util/image/image_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/snapshots/model/fake_snapshot_generator_delegate.h"
#import "ios/chrome/browser/snapshots/model/model_swift.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id.h"
#import "ios/chrome/browser/snapshots/model/snapshot_kind.h"
#import "ios/chrome/browser/snapshots/model/snapshot_source_tab_helper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_storage_util.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/test/ios/ui_image_test_utils.h"
#import "ui/gfx/image/image.h"

using ui::test::uiimage_utils::UIImagesAreEqual;
using ui::test::uiimage_utils::UIImageWithSizeAndSolidColor;

@class WebStateSnapshotInfo;

// SnapshotGeneratorDelegate used to test SnapshotTabHelper by allowing to
// count the number of snapshot generated and control whether capturing a
// snapshot is possible.
@interface TabHelperSnapshotGeneratorDelegate : FakeSnapshotGeneratorDelegate

// Returns the number of times a snapshot was captured (count the number of
// calls to -willUpdateSnapshotForWebState:).
@property(nonatomic, readonly) NSUInteger snapshotTakenCount;

// This property controls the value returned by -canTakeSnapshotForWebState:
// method of the SnapshotGeneratorDelegate protocol.
@property(nonatomic, assign) BOOL canTakeSnapshot;

@end

@implementation TabHelperSnapshotGeneratorDelegate

@synthesize snapshotTakenCount = _snapshotTakenCount;
@synthesize canTakeSnapshot = _canTakeSnapshot;

- (instancetype)init {
  if ((self = [super init])) {
    _canTakeSnapshot = YES;
  }
  return self;
}

#pragma mark - SnapshotGeneratorDelegate

- (BOOL)canTakeSnapshotWithWebStateInfo:(WebStateSnapshotInfo*)webStateInfo {
  return _canTakeSnapshot;
}

- (void)willUpdateSnapshotWithWebStateInfo:(WebStateSnapshotInfo*)webStateInfo {
  ++_snapshotTakenCount;
}

@end

namespace {

// Returns whether the `image` dominant color is `color`.
bool IsDominantColorForImage(UIImage* image, UIColor* color) {
  UIColor* dominant_color =
      DominantColorForImage(gfx::Image(image), /*opacity=*/1.0);
  return [color isEqual:dominant_color];
}

// Converts `snapshot_id` to a SnapshotIDWrapper.
SnapshotIDWrapper* ToWrapper(SnapshotID snapshot_id) {
  return [[SnapshotIDWrapper alloc] initWithSnapshotID:snapshot_id];
}

// Dimension of the WebState's view (if defined).
constexpr CGSize kWebStateViewSize = {300, 400};

// Dimension of the cached snapshot images.
constexpr CGSize kCachedSnapshotSize = {15, 20};

// Dimension of the default snapshot image.
constexpr CGSize kDefaultSnapshotSize = {150, 200};

}  // namespace

class SnapshotTabHelperTest : public PlatformTest {
 public:
  SnapshotTabHelperTest() {
    // Create the SnapshotTabHelper with a fake delegate.
    delegate_ = [[TabHelperSnapshotGeneratorDelegate alloc] init];
    SnapshotTabHelper::CreateForWebState(&web_state_);
    SnapshotSourceTabHelper::CreateForWebState(&web_state_);
    SnapshotTabHelper::FromWebState(&web_state_)->SetDelegate(delegate_);

    // Set custom snapshot storage.
    EXPECT_TRUE(scoped_temp_directory_.CreateUniqueTempDir());
    base::FilePath directory_name = scoped_temp_directory_.GetPath();
    snapshot_storage_ = CreateSnapshotStorage(directory_name);
    SnapshotTabHelper::FromWebState(&web_state_)
        ->SetSnapshotStorage(snapshot_storage_);

    // Add a fake view to the FakeWebState. This will be used to capture the
    // snapshot. By default the WebState is not ready for taking snapshot.
    CGRect frame = {CGPointZero, kWebStateViewSize};
    UIView* view = [[UIView alloc] initWithFrame:frame];
    view.backgroundColor = [UIColor redColor];
    delegate_.view = view;

    UIWindow* window = GetAnyKeyWindow();
    [window addSubview:delegate_.view];
    [window makeKeyAndVisible];

    // Hack to forcefully render the view to successfully capture a snapshot.
    [NSRunLoop.currentRunLoop
        runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
    [window layoutIfNeeded];
  }

  SnapshotTabHelperTest(const SnapshotTabHelperTest&) = delete;
  SnapshotTabHelperTest& operator=(const SnapshotTabHelperTest&) = delete;

  ~SnapshotTabHelperTest() override { [snapshot_storage_ shutdown]; }

  void TearDown() override { [delegate_.view removeFromSuperview]; }

  void SetCachedSnapshot(UIImage* image) {
    const SnapshotID snapshot_id(web_state_.GetUniqueIdentifier());
    [snapshot_storage_ setImage:image withSnapshotID:ToWrapper(snapshot_id)];
  }

  UIImage* GetCachedSnapshot() {
    base::RunLoop run_loop;
    base::RunLoop* run_loop_ptr = &run_loop;

    __block UIImage* snapshot = nil;
    const SnapshotID snapshot_id(web_state_.GetUniqueIdentifier());
    [snapshot_storage_ retrieveImageWithSnapshotID:ToWrapper(snapshot_id)
                                      snapshotKind:SnapshotKindColor
                                        completion:^(UIImage* cached_snapshot) {
                                          snapshot = cached_snapshot;
                                          run_loop_ptr->Quit();
                                        }];

    run_loop.Run();
    return snapshot;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  base::ScopedTempDir scoped_temp_directory_;
  TabHelperSnapshotGeneratorDelegate* delegate_ = nil;
  id<SnapshotStorage> snapshot_storage_ = nil;
  web::FakeWebState web_state_;
};

// Tests that RetrieveColorSnapshot uses the image from the cache if
// there is one present.
TEST_F(SnapshotTabHelperTest, RetrieveColorSnapshotStoragedSnapshot) {
  SetCachedSnapshot(
      UIImageWithSizeAndSolidColor(kCachedSnapshotSize, [UIColor greenColor]));

  base::RunLoop run_loop;
  base::RunLoop* run_loop_ptr = &run_loop;

  __block UIImage* snapshot = nil;
  SnapshotTabHelper::FromWebState(&web_state_)
      ->RetrieveColorSnapshot(^(UIImage* image) {
        snapshot = image;
        run_loop_ptr->Quit();
      });

  run_loop.Run();

  ASSERT_TRUE(snapshot);
  EXPECT_TRUE(UIImagesAreEqual(snapshot, GetCachedSnapshot()));
  EXPECT_EQ(delegate_.snapshotTakenCount, 0u);
}

// Tests that RetrieveColorSnapshot returns nil when there is no cached snapshot
// and the WebState web usage is disabled.
TEST_F(SnapshotTabHelperTest, RetrieveColorSnapshotWebUsageDisabled) {
  web_state_.SetWebUsageEnabled(false);

  base::RunLoop run_loop;
  base::RunLoop* run_loop_ptr = &run_loop;

  __block UIImage* snapshot = nil;
  SnapshotTabHelper::FromWebState(&web_state_)
      ->RetrieveColorSnapshot(^(UIImage* image) {
        snapshot = image;
        run_loop_ptr->Quit();
      });

  run_loop.Run();

  EXPECT_FALSE(snapshot);
  EXPECT_EQ(delegate_.snapshotTakenCount, 0u);
}

// Tests that RetrieveColorSnapshot returns nil when there is no cached snapshot
// and the delegate says it is not possible to take a snapshot.
TEST_F(SnapshotTabHelperTest, RetrieveColorSnapshotCannotTakeSnapshot) {
  delegate_.canTakeSnapshot = NO;

  base::RunLoop run_loop;
  base::RunLoop* run_loop_ptr = &run_loop;

  __block UIImage* snapshot = nil;
  SnapshotTabHelper::FromWebState(&web_state_)
      ->RetrieveColorSnapshot(^(UIImage* image) {
        snapshot = image;
        run_loop_ptr->Quit();
      });

  run_loop.Run();

  EXPECT_FALSE(snapshot);
  EXPECT_EQ(delegate_.snapshotTakenCount, 0u);
}

// Tests that RetrieveGreySnapshot uses the image from the cache if
// there is one present, and that it is greyscale.
TEST_F(SnapshotTabHelperTest, RetrieveGreySnapshotStoragedSnapshot) {
  SetCachedSnapshot(
      UIImageWithSizeAndSolidColor(kCachedSnapshotSize, [UIColor greenColor]));

  base::RunLoop run_loop;
  base::RunLoop* run_loop_ptr = &run_loop;

  __block UIImage* snapshot = nil;
  SnapshotTabHelper::FromWebState(&web_state_)
      ->RetrieveGreySnapshot(^(UIImage* image) {
        snapshot = image;
        run_loop_ptr->Quit();
      });

  run_loop.Run();

  ASSERT_TRUE(snapshot);
  EXPECT_TRUE(UIImagesAreEqual(snapshot, GreyImage(GetCachedSnapshot())));
  EXPECT_EQ(delegate_.snapshotTakenCount, 0u);
}

// Tests that RetrieveGreySnapshot returns nil when there is no cached snapshot
// and the WebState web usage is disabled.
TEST_F(SnapshotTabHelperTest, RetrieveGreySnapshotWebUsageDisabled) {
  web_state_.SetWebUsageEnabled(false);

  base::RunLoop run_loop;
  base::RunLoop* run_loop_ptr = &run_loop;

  __block UIImage* snapshot = nil;
  SnapshotTabHelper::FromWebState(&web_state_)
      ->RetrieveGreySnapshot(^(UIImage* image) {
        snapshot = image;
        run_loop_ptr->Quit();
      });

  run_loop.Run();

  EXPECT_FALSE(snapshot);
  EXPECT_EQ(delegate_.snapshotTakenCount, 0u);
}

// Tests that RetrieveGreySnapshot returns nil when there is no cached snapshot
// and the WebState web usage is disabled.
TEST_F(SnapshotTabHelperTest, RetrieveGreySnapshotCannotTakeSnapshot) {
  delegate_.canTakeSnapshot = NO;
  base::RunLoop run_loop;
  base::RunLoop* run_loop_ptr = &run_loop;

  __block UIImage* snapshot = nil;
  SnapshotTabHelper::FromWebState(&web_state_)
      ->RetrieveGreySnapshot(^(UIImage* image) {
        snapshot = image;
        run_loop_ptr->Quit();
      });

  run_loop.Run();

  EXPECT_FALSE(snapshot);
  EXPECT_EQ(delegate_.snapshotTakenCount, 0u);
}

// Tests that RetrieveGreySnapshot generates the image if there is no
// image in the cache, and that it is greyscale.
TEST_F(SnapshotTabHelperTest, RetrieveGreySnapshotGenerate) {
  base::RunLoop run_loop;
  base::RunLoop* run_loop_ptr = &run_loop;

  __block UIImage* snapshot = nil;
  SnapshotTabHelper::FromWebState(&web_state_)
      ->RetrieveGreySnapshot(^(UIImage* image) {
        snapshot = image;
        run_loop_ptr->Quit();
      });

  run_loop.Run();

  ASSERT_TRUE(snapshot);
  EXPECT_TRUE(CGSizeEqualToSize(snapshot.size, kWebStateViewSize));
  EXPECT_FALSE(IsDominantColorForImage(snapshot, [UIColor redColor]));
  EXPECT_EQ(delegate_.snapshotTakenCount, 1u);
}

// Tests that UpdateSnapshotWithCallback ignores any cached snapshots, generate
// a new one and updates the cache.
TEST_F(SnapshotTabHelperTest, UpdateSnapshotWithCallback) {
  SetCachedSnapshot(
      UIImageWithSizeAndSolidColor(kDefaultSnapshotSize, [UIColor greenColor]));
  UIImage* original_cached_snapshot = GetCachedSnapshot();

  base::RunLoop run_loop;
  base::RunLoop* run_loop_ptr = &run_loop;

  __block UIImage* snapshot = nil;
  SnapshotTabHelper::FromWebState(&web_state_)
      ->UpdateSnapshotWithCallback(^(UIImage* image) {
        snapshot = image;
        run_loop_ptr->Quit();
      });

  run_loop.Run();

  ASSERT_TRUE(snapshot);
  EXPECT_TRUE(CGSizeEqualToSize(snapshot.size, kWebStateViewSize));
  EXPECT_TRUE(IsDominantColorForImage(snapshot, [UIColor redColor]));

  UIImage* cached_snapshot = GetCachedSnapshot();
  EXPECT_TRUE(UIImagesAreEqual(snapshot, cached_snapshot));
  EXPECT_FALSE(UIImagesAreEqual(snapshot, original_cached_snapshot));
  EXPECT_EQ(delegate_.snapshotTakenCount, 1u);
}

// Tests that UpdateSnapshotStorageWithImage overrides a cached image with
// a new one and updates the cache.
TEST_F(SnapshotTabHelperTest, UpdateSnapshotStorageWithImage) {
  SetCachedSnapshot(
      UIImageWithSizeAndSolidColor(kCachedSnapshotSize, [UIColor greenColor]));
  UIImage* original_cached_snapshot = GetCachedSnapshot();

  // Updates the storage with a new image.
  UIImage* blue_image =
      UIImageWithSizeAndSolidColor(kDefaultSnapshotSize, [UIColor blueColor]);
  SnapshotTabHelper::FromWebState(&web_state_)
      ->UpdateSnapshotStorageWithImage(blue_image);

  base::RunLoop run_loop;
  base::RunLoop* run_loop_ptr = &run_loop;
  __block UIImage* snapshot = nil;
  SnapshotTabHelper::FromWebState(&web_state_)
      ->RetrieveColorSnapshot(^(UIImage* image) {
        snapshot = image;
        run_loop_ptr->Quit();
      });
  run_loop.Run();

  ASSERT_TRUE(snapshot);
  EXPECT_TRUE(CGSizeEqualToSize(snapshot.size, kDefaultSnapshotSize));
  EXPECT_TRUE(IsDominantColorForImage(snapshot, [UIColor blueColor]));

  UIImage* cached_snapshot = GetCachedSnapshot();
  EXPECT_TRUE(UIImagesAreEqual(snapshot, cached_snapshot));
  EXPECT_FALSE(UIImagesAreEqual(snapshot, original_cached_snapshot));
  EXPECT_EQ(delegate_.snapshotTakenCount, 0u);
}

// Tests that GenerateSnapshot ignores any cached snapshots and generate a new
// snapshot without adding it to the cache.
TEST_F(SnapshotTabHelperTest, GenerateSnapshot) {
  SetCachedSnapshot(
      UIImageWithSizeAndSolidColor(kDefaultSnapshotSize, [UIColor greenColor]));

  UIImage* snapshot = SnapshotTabHelper::FromWebState(&web_state_)
                          ->GenerateSnapshotWithoutOverlays();

  ASSERT_TRUE(snapshot);
  EXPECT_TRUE(CGSizeEqualToSize(snapshot.size, kWebStateViewSize));
  EXPECT_TRUE(IsDominantColorForImage(snapshot, [UIColor redColor]));

  UIImage* cached_snapshot = GetCachedSnapshot();
  EXPECT_FALSE(UIImagesAreEqual(snapshot, cached_snapshot));
}

// Tests that UpdateSnapshotStorage doesn't override an old image if taking a
// new snapshot fails.
TEST_F(SnapshotTabHelperTest, FailToUpdateSnapshotStorage) {
  SetCachedSnapshot(
      UIImageWithSizeAndSolidColor(kDefaultSnapshotSize, [UIColor greenColor]));
  UIImage* original_cached_snapshot = GetCachedSnapshot();

  // Forcefully make it fail to take a new snapshot.
  delegate_.canTakeSnapshot = NO;

  base::RunLoop run_loop;
  base::RunLoop* run_loop_ptr = &run_loop;

  SnapshotTabHelper::FromWebState(&web_state_)
      ->UpdateSnapshotWithCallback(^(UIImage* image) {
        ASSERT_FALSE(image);
        run_loop_ptr->Quit();
      });

  run_loop.Run();

  // Make sure that the cached snapshot isn't updated.
  UIImage* cached_snapshot = GetCachedSnapshot();
  EXPECT_TRUE(UIImagesAreEqual(cached_snapshot, original_cached_snapshot));
  EXPECT_EQ(delegate_.snapshotTakenCount, 0u);
}
