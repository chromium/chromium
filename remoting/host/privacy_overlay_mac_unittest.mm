// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/host/privacy_overlay_mac.h"

#import <Cocoa/Cocoa.h>
#include <CoreGraphics/CoreGraphics.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/auto_reset.h"
#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/icu_test_util.h"
#include "base/test/mock_chrome_application_mac.h"
#include "base/test/task_environment.h"
#include "remoting/base/string_resources.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/host/input_monitor/local_input_monitor.h"
#include "remoting/host/resources.h"
#include "remoting/protocol/errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "ui/base/resource/resource_bundle.h"

@interface TestPrivacyOverlayBackdropView : PrivacyOverlayBackdropView
@property(nonatomic, readonly) NSUInteger displayInvalidationCount;
@end

@implementation TestPrivacyOverlayBackdropView {
  NSUInteger _displayInvalidationCount;
}

- (NSUInteger)displayInvalidationCount {
  return _displayInvalidationCount;
}

- (void)setNeedsDisplay:(BOOL)needsDisplay {
  if (needsDisplay) {
    ++_displayInvalidationCount;
  }
  [super setNeedsDisplay:needsDisplay];
}

@end

namespace remoting {

namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::ReturnRef;

constexpr CGFloat kGoogleBlueRed = 26.0 / 255.0;
constexpr CGFloat kGoogleBlueGreen = 115.0 / 255.0;
constexpr CGFloat kGoogleBlueBlue = 232.0 / 255.0;
constexpr CGFloat kColorComponentTolerance = 0.04;

NSColor* GoogleBlueColor() {
  return [NSColor colorWithSRGBRed:kGoogleBlueRed
                             green:kGoogleBlueGreen
                              blue:kGoogleBlueBlue
                             alpha:1.0];
}

void ExpectBitmapPixelNearColor(NSBitmapImageRep* rep,
                                NSInteger x,
                                NSInteger y,
                                NSColor* expected_color) {
  ASSERT_NE(rep, nil);
  ASSERT_NE(expected_color, nil);
  // `[NSBitmapImageRep colorAtX:y:]` returns the raw pixel components stored in
  // `rep` tagged with `NSCalibratedRGBColorSpace`. Convert `expected_color`
  // into `rep.colorSpace` (the color space used by
  // `cacheDisplayInRect:toBitmapImageRep:`) and compare raw RGB components
  // directly without re-converting `pixel`.
  NSColorSpace* rep_color_space = rep.colorSpace ?: NSColorSpace.sRGBColorSpace;
  NSColor* expected_in_rep =
      [expected_color colorUsingColorSpace:rep_color_space];
  ASSERT_NE(expected_in_rep, nil);
  NSColor* pixel = [rep colorAtX:x y:y];
  ASSERT_NE(pixel, nil);
  EXPECT_NEAR(pixel.redComponent, expected_in_rep.redComponent,
              kColorComponentTolerance);
  EXPECT_NEAR(pixel.greenComponent, expected_in_rep.greenComponent,
              kColorComponentTolerance);
  EXPECT_NEAR(pixel.blueComponent, expected_in_rep.blueComponent,
              kColorComponentTolerance);
}

int CountNonBackgroundCenterPixels(NSBitmapImageRep* rep, NSSize point_size) {
  NSColor* corner_color = [rep colorAtX:10 y:10];
  if (!corner_color || point_size.width <= 0.0 || point_size.height <= 0.0) {
    return 0;
  }
  CGFloat scale_x = static_cast<CGFloat>(rep.pixelsWide) / point_size.width;
  CGFloat scale_y = static_cast<CGFloat>(rep.pixelsHigh) / point_size.height;
  NSInteger center_x = rep.pixelsWide / 2;
  NSInteger center_y = rep.pixelsHigh / 2;
  NSInteger half_box_x =
      std::min<NSInteger>(std::lround(160.0 * scale_x), center_x - 1);
  NSInteger half_box_y =
      std::min<NSInteger>(std::lround(160.0 * scale_y), center_y - 1);
  NSInteger step_x = std::max<NSInteger>(2, std::lround(2.0 * scale_x));
  NSInteger step_y = std::max<NSInteger>(2, std::lround(2.0 * scale_y));
  int non_background_pixels = 0;
  for (NSInteger y = center_y - half_box_y; y < center_y + half_box_y;
       y += step_y) {
    for (NSInteger x = center_x - half_box_x; x < center_x + half_box_x;
         x += step_x) {
      NSColor* pixel = [rep colorAtX:x y:y];
      if (pixel && ![pixel isEqual:corner_color]) {
        ++non_background_pixels;
      }
    }
  }
  return non_background_pixels;
}

class FakeLocalInputMonitor : public LocalInputMonitor {
 public:
  FakeLocalInputMonitor() = default;
  ~FakeLocalInputMonitor() override = default;

  void StartMonitoringForClientSession(
      base::WeakPtr<ClientSessionControl> client_session_control) override {}

  void StartMonitoring(PointerMoveCallback on_pointer_input,
                       KeyPressedCallback on_keyboard_input,
                       base::RepeatingClosure on_error) override {}
};

class PrivacyOverlayMacTest : public testing::Test {
 protected:
  void SetUp() override {
    if (!NSApp) {
      mock_cr_app::RegisterMockCrApp();
    }
    ASSERT_TRUE(LoadResources("en-US"));
    ON_CALL(client_session_control_, client_jid())
        .WillByDefault(ReturnRef(client_jid_));
  }

  void TearDown() override {
    DestroyOverlay();
    UnloadResources();
  }

  void StartOverlay() {
    auto monitor = std::make_unique<FakeLocalInputMonitor>();
    overlay_ = std::make_unique<PrivacyOverlayMac>(std::move(monitor));
    overlay_->Start(weak_factory_.GetWeakPtr());
  }

  void DestroyOverlay() { overlay_.reset(); }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::string client_jid_ = "user@domain/jid123";
  NiceMock<MockClientSessionControl> client_session_control_;
  std::unique_ptr<PrivacyOverlayMac> overlay_;
  base::WeakPtrFactory<ClientSessionControl> weak_factory_{
      &client_session_control_};
};

TEST_F(PrivacyOverlayMacTest,
       CreatesBackdropWindowsWithExpectedStylesAndAttributes) {
  StartOverlay();

  NSArray<NSScreen*>* screens = NSScreen.screens;
  NSArray<PrivacyOverlayBackdropWindow*>* backdrops =
      overlay_->backdrop_windows_for_testing();
  ASSERT_GT(backdrops.count, 0u);
  ASSERT_EQ(backdrops.count, screens.count);

  for (NSUInteger i = 0; i < backdrops.count; ++i) {
    NSScreen* screen = screens[i];
    PrivacyOverlayBackdropWindow* window = backdrops[i];
    EXPECT_TRUE(window.isVisible);
    EXPECT_TRUE(NSEqualRects(window.frame, screen.frame));
    EXPECT_EQ(window.styleMask, NSWindowStyleMaskBorderless);
    EXPECT_FALSE(window.canHide);
    EXPECT_FALSE(window.isMovable);
    EXPECT_TRUE(window.ignoresMouseEvents);
    EXPECT_TRUE(window.isOpaque);
    EXPECT_FALSE(window.hasShadow);
    EXPECT_EQ(window.level, NSScreenSaverWindowLevel);
    EXPECT_EQ(window.sharingType, NSWindowSharingNone);
    EXPECT_FALSE(window.canBecomeKeyWindow);
    EXPECT_FALSE(window.canBecomeMainWindow);
    NSRect unconstrained_rect = NSMakeRect(-50.0, -50.0, 3000.0, 2000.0);
    EXPECT_TRUE(NSEqualRects([window constrainFrameRect:unconstrained_rect
                                               toScreen:screen],
                             unconstrained_rect));
    EXPECT_NE(
        window.collectionBehavior & NSWindowCollectionBehaviorCanJoinAllSpaces,
        0u);
    EXPECT_NE(window.collectionBehavior &
                  NSWindowCollectionBehaviorFullScreenAuxiliary,
              0u);
    EXPECT_NE(window.collectionBehavior & NSWindowCollectionBehaviorStationary,
              0u);
    EXPECT_NE(
        window.collectionBehavior & NSWindowCollectionBehaviorIgnoresCycle, 0u);

    PrivacyOverlayBackdropView* view =
        base::apple::ObjCCastStrict<PrivacyOverlayBackdropView>(
            window.contentView);
    ASSERT_NE(view, nil);
    EXPECT_TRUE(NSEqualSizes(view.frame.size, screen.frame.size));
  }
}

TEST_F(PrivacyOverlayMacTest,
       DestroyingOverlayClosesBackdropWindowsWithoutDisconnectingSession) {
  StartOverlay();
  overlay_->ActivateDisconnectButtonForTesting();

  NSArray<PrivacyOverlayBackdropWindow*>* backdrops =
      [overlay_->backdrop_windows_for_testing() copy];
  PrivacyOverlayButtonWindow* button_window =
      overlay_->button_window_for_testing();
  ASSERT_GT(backdrops.count, 0u);
  ASSERT_NE(button_window, nil);
  EXPECT_TRUE(button_window.isVisible);
  for (PrivacyOverlayBackdropWindow* window in backdrops) {
    EXPECT_TRUE(window.isVisible);
  }

  EXPECT_CALL(client_session_control_, DisconnectSession(_, _, _)).Times(0);
  DestroyOverlay();

  for (PrivacyOverlayBackdropWindow* window in backdrops) {
    EXPECT_FALSE(window.isVisible);
  }
  EXPECT_FALSE(button_window.isVisible);
}

TEST_F(PrivacyOverlayMacTest, PaintsGoogleBlueBackdropBackgroundByDefault) {
  StartOverlay();

  ASSERT_GT(overlay_->backdrop_windows_for_testing().count, 0u);
  PrivacyOverlayBackdropWindow* window =
      overlay_->backdrop_windows_for_testing().firstObject;
  PrivacyOverlayBackdropView* view =
      base::apple::ObjCCastStrict<PrivacyOverlayBackdropView>(
          window.contentView);
  ASSERT_NE(view, nil);
  [view setHighContrastOverrideForTesting:false];

  NSRect bounds = view.bounds;
  ASSERT_GT(NSWidth(bounds), 20.0);
  ASSERT_GT(NSHeight(bounds), 20.0);

  NSBitmapImageRep* rep = [view bitmapImageRepForCachingDisplayInRect:bounds];
  ASSERT_NE(rep, nil);
  [view cacheDisplayInRect:bounds toBitmapImageRep:rep];

  ExpectBitmapPixelNearColor(rep, 10, 10, GoogleBlueColor());
  ExpectBitmapPixelNearColor(rep, rep.pixelsWide - 10, rep.pixelsHigh - 10,
                             GoogleBlueColor());
}

TEST_F(PrivacyOverlayMacTest,
       PaintsHighContrastBackdropWhenIncreaseContrastEnabled) {
  StartOverlay();

  ASSERT_GT(overlay_->backdrop_windows_for_testing().count, 0u);
  PrivacyOverlayBackdropWindow* window =
      overlay_->backdrop_windows_for_testing().firstObject;
  PrivacyOverlayBackdropView* view =
      base::apple::ObjCCastStrict<PrivacyOverlayBackdropView>(
          window.contentView);
  ASSERT_NE(view, nil);
  [view setHighContrastOverrideForTesting:true];

  NSRect bounds = view.bounds;
  NSBitmapImageRep* rep = [view bitmapImageRepForCachingDisplayInRect:bounds];
  ASSERT_NE(rep, nil);
  [view cacheDisplayInRect:bounds toBitmapImageRep:rep];

  ExpectBitmapPixelNearColor(rep, 10, 10, NSColor.windowBackgroundColor);
  ExpectBitmapPixelNearColor(rep, rep.pixelsWide - 10, rep.pixelsHigh - 10,
                             NSColor.windowBackgroundColor);
}

TEST_F(PrivacyOverlayMacTest, DisconnectsSessionWhenNoDisplaysFound) {
  auto reset_screens = PrivacyOverlayMac::SetScreenFramesProviderForTesting(
      base::BindLambdaForTesting([]() { return std::vector<NSRect>{}; }));

  EXPECT_CALL(client_session_control_,
              DisconnectSession(protocol::ErrorCode::OK, _, _))
      .Times(1);
  StartOverlay();

  EXPECT_EQ(overlay_->backdrop_windows_for_testing().count, 0u);
  EXPECT_TRUE(overlay_->is_button_hidden_for_testing());
}

TEST_F(PrivacyOverlayMacTest,
       DisconnectsSessionWhenInitialCaptureExclusionFails) {
  std::vector<NSRect> frames = {NSMakeRect(0.0, 0.0, 1440.0, 900.0),
                                NSMakeRect(1440.0, 0.0, 1920.0, 1080.0)};
  auto reset_screens = PrivacyOverlayMac::SetScreenFramesProviderForTesting(
      base::BindLambdaForTesting([&frames]() { return frames; }));

  int call_count = 0;
  auto reset_applier = PrivacyOverlayMac::SetCaptureExclusionApplierForTesting(
      base::BindLambdaForTesting(
          [&call_count](NSWindow* /*window*/, NSSize /*size*/) {
            ++call_count;
            // Succeed on the first display and fail on the second so
            // `EndOverlay()` also closes the first display's window.
            return call_count == 1;
          }));

  EXPECT_CALL(client_session_control_,
              DisconnectSession(protocol::ErrorCode::OK, _, _))
      .Times(1);
  StartOverlay();

  EXPECT_EQ(call_count, 2);
  EXPECT_EQ(overlay_->backdrop_windows_for_testing().count, 0u);
  EXPECT_TRUE(overlay_->is_button_hidden_for_testing());
}

TEST_F(PrivacyOverlayMacTest,
       UpdateBackdropsReusesResizesAndClosesExcessWindows) {
  struct CaptureExclusionCall {
    NSInteger window_number;
    NSSize size;
  };

  std::vector<NSRect> frames = {NSMakeRect(0.0, 0.0, 1440.0, 900.0)};
  auto reset_screens = PrivacyOverlayMac::SetScreenFramesProviderForTesting(
      base::BindLambdaForTesting([&frames]() { return frames; }));

  std::vector<CaptureExclusionCall> calls;
  auto reset_applier = PrivacyOverlayMac::SetCaptureExclusionApplierForTesting(
      base::BindLambdaForTesting([&calls](NSWindow* window, NSSize size) {
        if ([window isKindOfClass:[PrivacyOverlayBackdropWindow class]]) {
          calls.push_back({window.windowNumber, size});
        }
        return true;
      }));

  StartOverlay();

  NSArray<PrivacyOverlayBackdropWindow*>* backdrops =
      overlay_->backdrop_windows_for_testing();
  ASSERT_EQ(backdrops.count, 1u);
  PrivacyOverlayBackdropWindow* primary_window = backdrops[0];
  EXPECT_TRUE(
      NSEqualRects(primary_window.frame, NSMakeRect(0.0, 0.0, 1440.0, 900.0)));
  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(calls[0].window_number, primary_window.windowNumber);
  EXPECT_TRUE(NSEqualSizes(calls[0].size, NSMakeSize(1440.0, 900.0)));

  PrivacyOverlayBackdropView* initial_view =
      base::apple::ObjCCastStrict<PrivacyOverlayBackdropView>(
          primary_window.contentView);
  TestPrivacyOverlayBackdropView* tracked_view =
      [[TestPrivacyOverlayBackdropView alloc] initWithFrame:initial_view.frame];
  tracked_view.titleText = initial_view.titleText;
  tracked_view.sharingText = initial_view.sharingText;
  tracked_view.iconImage = initial_view.iconImage;
  tracked_view.isRTL = initial_view.isRTL;
  primary_window.contentView = tracked_view;
  const NSUInteger invalidations_before_grow =
      tracked_view.displayInvalidationCount;

  // Transition 1 -> 2 displays while growing display 0 (`1440x900` ->
  // `2560x1440`). `primary_window` must be reused in-place with both a
  // pre-resize `union_size` exclusion update and a post-`setFrame:` exclusion
  // refresh.
  calls.clear();
  frames = {NSMakeRect(0.0, 0.0, 2560.0, 1440.0),
            NSMakeRect(2560.0, 0.0, 1920.0, 1080.0)};
  ASSERT_TRUE(overlay_->UpdateBackdropsForTesting());

  ASSERT_EQ(backdrops.count, 2u);
  EXPECT_EQ(backdrops[0], primary_window);
  EXPECT_TRUE(
      NSEqualRects(primary_window.frame, NSMakeRect(0.0, 0.0, 2560.0, 1440.0)));
  EXPECT_TRUE(NSEqualSizes(primary_window.contentView.frame.size,
                           NSMakeSize(2560.0, 1440.0)));
  const NSUInteger invalidations_after_grow =
      tracked_view.displayInvalidationCount;
  EXPECT_GT(invalidations_after_grow, invalidations_before_grow);

  PrivacyOverlayBackdropWindow* secondary_window = backdrops[1];
  EXPECT_TRUE(secondary_window.isVisible);
  EXPECT_TRUE(NSEqualRects(secondary_window.frame,
                           NSMakeRect(2560.0, 0.0, 1920.0, 1080.0)));

  ASSERT_EQ(calls.size(), 3u);
  EXPECT_EQ(calls[0].window_number, primary_window.windowNumber);
  EXPECT_TRUE(NSEqualSizes(calls[0].size, NSMakeSize(2560.0, 1440.0)));
  EXPECT_EQ(calls[1].window_number, primary_window.windowNumber);
  EXPECT_TRUE(NSEqualSizes(calls[1].size, NSMakeSize(2560.0, 1440.0)));
  EXPECT_EQ(calls[2].window_number, secondary_window.windowNumber);
  EXPECT_TRUE(NSEqualSizes(calls[2].size, NSMakeSize(1920.0, 1080.0)));

  // Transition 2 -> 1 display while shrinking display 0 (`2560x1440` ->
  // `1280x720`). `secondary_window` must be closed and removed, and
  // `primary_window` must refresh capture exclusion once after shrinking.
  calls.clear();
  frames = {NSMakeRect(0.0, 0.0, 1280.0, 720.0)};
  ASSERT_TRUE(overlay_->UpdateBackdropsForTesting());

  ASSERT_EQ(backdrops.count, 1u);
  EXPECT_EQ(backdrops[0], primary_window);
  EXPECT_FALSE(secondary_window.isVisible);
  EXPECT_TRUE(
      NSEqualRects(primary_window.frame, NSMakeRect(0.0, 0.0, 1280.0, 720.0)));
  EXPECT_TRUE(NSEqualSizes(primary_window.contentView.frame.size,
                           NSMakeSize(1280.0, 720.0)));
  const NSUInteger invalidations_after_shrink =
      tracked_view.displayInvalidationCount;
  EXPECT_GT(invalidations_after_shrink, invalidations_after_grow);

  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(calls[0].window_number, primary_window.windowNumber);
  EXPECT_TRUE(NSEqualSizes(calls[0].size, NSMakeSize(1280.0, 720.0)));

  // Re-running `UpdateBackdrops()` with unchanged screen geometry must not
  // trigger redundant display invalidations.
  ASSERT_TRUE(overlay_->UpdateBackdropsForTesting());
  EXPECT_EQ(tracked_view.displayInvalidationCount, invalidations_after_shrink);
}

TEST_F(PrivacyOverlayMacTest,
       UpdateBackdropsFailsClosedWhenResizeCaptureExclusionFails) {
  std::vector<NSRect> frames = {NSMakeRect(0.0, 0.0, 1440.0, 900.0)};
  auto reset_screens = PrivacyOverlayMac::SetScreenFramesProviderForTesting(
      base::BindLambdaForTesting([&frames]() { return frames; }));

  StartOverlay();
  ASSERT_EQ(overlay_->backdrop_windows_for_testing().count, 1u);

  auto reset_applier = PrivacyOverlayMac::SetCaptureExclusionApplierForTesting(
      base::BindLambdaForTesting(
          [](NSWindow* /*window*/, NSSize /*size*/) { return false; }));

  // Growing fails closed on the pre-resize `union_size` exclusion update.
  frames = {NSMakeRect(0.0, 0.0, 2560.0, 1440.0)};
  EXPECT_FALSE(overlay_->UpdateBackdropsForTesting());

  // Shrinking fails closed on the post-`setFrame:` exclusion refresh.
  frames = {NSMakeRect(0.0, 0.0, 1280.0, 720.0)};
  EXPECT_FALSE(overlay_->UpdateBackdropsForTesting());
}

TEST_F(PrivacyOverlayMacTest,
       RendersCenteredContentAndConfiguresAccessibilityOnBackdrop) {
  StartOverlay();

  ASSERT_GT(overlay_->backdrop_windows_for_testing().count, 0u);
  PrivacyOverlayBackdropWindow* window =
      overlay_->backdrop_windows_for_testing().firstObject;
  EXPECT_GT(window.title.length, 0u);

  PrivacyOverlayBackdropView* view =
      base::apple::ObjCCastStrict<PrivacyOverlayBackdropView>(
          window.contentView);
  ASSERT_NE(view, nil);
  EXPECT_NE(view.iconImage, nil);
  EXPECT_TRUE(view.isAccessibilityElement);
  EXPECT_NSEQ(view.accessibilityRole, NSAccessibilityStaticTextRole);
  EXPECT_EQ(view.accessibilityLabel.length, 0u);
  EXPECT_NSEQ(view.accessibilityValue, view.sharingText);
  EXPECT_TRUE([view.accessibilityValue containsString:@"user@domain"]);

  NSRect bounds = view.bounds;
  [view setHighContrastOverrideForTesting:false];
  NSBitmapImageRep* blue_rep =
      [view bitmapImageRepForCachingDisplayInRect:bounds];
  ASSERT_NE(blue_rep, nil);
  [view cacheDisplayInRect:bounds toBitmapImageRep:blue_rep];
  EXPECT_GT(CountNonBackgroundCenterPixels(blue_rep, bounds.size), 0);

  [view setHighContrastOverrideForTesting:true];
  NSBitmapImageRep* hc_rep =
      [view bitmapImageRepForCachingDisplayInRect:bounds];
  ASSERT_NE(hc_rep, nil);
  [view cacheDisplayInRect:bounds toBitmapImageRep:hc_rep];
  EXPECT_GT(CountNonBackgroundCenterPixels(hc_rep, bounds.size), 0);
}

TEST_F(PrivacyOverlayMacTest,
       BackdropViewRendersIconOnlyAndSkipsRedundantInvalidations) {
  NSRect bounds = NSMakeRect(0.0, 0.0, 400.0, 400.0);
  TestPrivacyOverlayBackdropView* view =
      [[TestPrivacyOverlayBackdropView alloc] initWithFrame:bounds];
  [view setHighContrastOverrideForTesting:false];

  NSBitmapImageRep* empty_rep =
      [view bitmapImageRepForCachingDisplayInRect:bounds];
  ASSERT_NE(empty_rep, nil);
  [view cacheDisplayInRect:bounds toBitmapImageRep:empty_rep];
  EXPECT_EQ(CountNonBackgroundCenterPixels(empty_rep, bounds.size), 0);

  NSImage* white_icon = [NSImage imageWithSize:NSMakeSize(64.0, 64.0)
                                       flipped:NO
                                drawingHandler:^BOOL(NSRect dst_rect) {
                                  [NSColor.whiteColor setFill];
                                  NSRectFill(dst_rect);
                                  return YES;
                                }];
  view.iconImage = white_icon;
  NSBitmapImageRep* icon_rep =
      [view bitmapImageRepForCachingDisplayInRect:bounds];
  ASSERT_NE(icon_rep, nil);
  [view cacheDisplayInRect:bounds toBitmapImageRep:icon_rep];
  EXPECT_GT(CountNonBackgroundCenterPixels(icon_rep, bounds.size), 0);

  view.titleText = @"Chrome Remote Desktop";
  view.sharingText = @"Sharing with user@domain.com";
  view.isRTL = YES;
  const NSUInteger baseline_invalidations = view.displayInvalidationCount;

  // Reassigning equal values or identical frame size must not trigger display
  // invalidations.
  view.titleText = [NSMutableString stringWithString:@"Chrome Remote Desktop"];
  view.sharingText =
      [NSMutableString stringWithString:@"Sharing with user@domain.com"];
  view.iconImage = white_icon;
  view.isRTL = YES;
  [view setFrameSize:bounds.size];
  EXPECT_EQ(view.displayInvalidationCount, baseline_invalidations);

  // Mutating each property or resizing must trigger a display invalidation.
  view.titleText = @"Updated Title";
  const NSUInteger after_title = view.displayInvalidationCount;
  EXPECT_GT(after_title, baseline_invalidations);

  view.sharingText = @"Sharing with other@domain.com";
  const NSUInteger after_sharing = view.displayInvalidationCount;
  EXPECT_GT(after_sharing, after_title);

  NSImage* second_icon = [[NSImage alloc] initWithSize:NSMakeSize(32.0, 32.0)];
  view.iconImage = second_icon;
  const NSUInteger after_icon = view.displayInvalidationCount;
  EXPECT_GT(after_icon, after_sharing);

  view.isRTL = NO;
  const NSUInteger after_rtl = view.displayInvalidationCount;
  EXPECT_GT(after_rtl, after_icon);

  [view setFrameSize:NSMakeSize(600.0, 500.0)];
  EXPECT_GT(view.displayInvalidationCount, after_rtl);
}

TEST_F(PrivacyOverlayMacTest,
       ConfiguresLtrReadingOrderWithoutBiDiEmailWrapping) {
  base::test::ScopedRestoreICUDefaultLocale ltr_locale("en-US");
  ASSERT_FALSE(base::i18n::IsRTL());

  client_jid_ = "user@domain.com/resource123";
  StartOverlay();

  EXPECT_FALSE(overlay_->is_rtl_for_testing());
  ASSERT_GT(overlay_->backdrop_windows_for_testing().count, 0u);
  PrivacyOverlayBackdropView* view =
      base::apple::ObjCCastStrict<PrivacyOverlayBackdropView>(
          overlay_->backdrop_windows_for_testing().firstObject.contentView);
  ASSERT_NE(view, nil);
  EXPECT_FALSE(view.isRTL);
  EXPECT_EQ(view.userInterfaceLayoutDirection,
            NSUserInterfaceLayoutDirectionLeftToRight);

  std::u16string sharing_text = base::SysNSStringToUTF16(view.sharingText);
  EXPECT_NE(sharing_text.find(u"user@domain.com"), std::u16string::npos);
  EXPECT_EQ(sharing_text.find(base::i18n::kLeftToRightEmbeddingMark),
            std::u16string::npos);
}

TEST_F(PrivacyOverlayMacTest,
       ConfiguresRtlReadingOrderAndWrapsEmailWithLtrMarks) {
  base::test::ScopedRestoreICUDefaultLocale rtl_locale("ar");
  ASSERT_TRUE(base::i18n::IsRTL());

  client_jid_ = "user@domain.com/resource123";
  StartOverlay();

  EXPECT_TRUE(overlay_->is_rtl_for_testing());
  ASSERT_GT(overlay_->backdrop_windows_for_testing().count, 0u);
  PrivacyOverlayBackdropView* view =
      base::apple::ObjCCastStrict<PrivacyOverlayBackdropView>(
          overlay_->backdrop_windows_for_testing().firstObject.contentView);
  ASSERT_NE(view, nil);
  EXPECT_TRUE(view.isRTL);
  EXPECT_EQ(view.userInterfaceLayoutDirection,
            NSUserInterfaceLayoutDirectionRightToLeft);

  // `FormatEmailForDisplay()` already wraps LTR emails as
  // `RLM + LRE + email + PDF + RLM` on macOS when `base::i18n::IsRTL()` is
  // true; `BeginOverlay()` must preserve those marks without nesting an outer
  // `LRE` around `RLM`.
  std::u16string expected_locale_adjusted_email =
      u"\u200F\u202Auser@domain.com\u202C\u200F";
  std::u16string sharing_text = base::SysNSStringToUTF16(view.sharingText);
  EXPECT_NE(sharing_text.find(expected_locale_adjusted_email),
            std::u16string::npos);
  EXPECT_EQ(sharing_text.find(u"\u202A\u200F"), std::u16string::npos);
}

TEST_F(PrivacyOverlayMacTest,
       WrapsEmailWithLtrMarksWhenMessageContainsRtlInLtrLocale) {
  base::test::ScopedRestoreICUDefaultLocale ltr_locale("en-US");
  ASSERT_FALSE(base::i18n::IsRTL());

  ui::ResourceBundle::GetSharedInstance().OverrideLocaleStringResource(
      IDS_MESSAGE_SHARED,
      u"\u0645\u0634\u0627\u0631\u0643\u0629 \u0645\u0639 $1");

  client_jid_ = "user@domain.com/resource123";
  StartOverlay();

  EXPECT_TRUE(overlay_->is_rtl_for_testing());
  ASSERT_GT(overlay_->backdrop_windows_for_testing().count, 0u);
  PrivacyOverlayBackdropView* view =
      base::apple::ObjCCastStrict<PrivacyOverlayBackdropView>(
          overlay_->backdrop_windows_for_testing().firstObject.contentView);
  ASSERT_NE(view, nil);
  EXPECT_TRUE(view.isRTL);
  EXPECT_EQ(view.userInterfaceLayoutDirection,
            NSUserInterfaceLayoutDirectionRightToLeft);

  std::u16string expected_wrapped_email = u"user@domain.com";
  base::i18n::WrapStringWithLTRFormatting(&expected_wrapped_email);
  std::u16string sharing_text = base::SysNSStringToUTF16(view.sharingText);
  EXPECT_NE(sharing_text.find(expected_wrapped_email), std::u16string::npos);
  EXPECT_EQ(sharing_text.find(u"\u202A\u200F"), std::u16string::npos);
}

TEST_F(PrivacyOverlayMacTest, TruncatesOverlongClientEmailWithEllipsis) {
  std::string long_local_part(180, 'a');
  std::string long_domain_part(180, 'b');
  client_jid_ =
      long_local_part + "@" + long_domain_part + ".example.com/resource123";
  StartOverlay();

  ASSERT_GT(overlay_->backdrop_windows_for_testing().count, 0u);
  PrivacyOverlayBackdropView* view =
      base::apple::ObjCCastStrict<PrivacyOverlayBackdropView>(
          overlay_->backdrop_windows_for_testing().firstObject.contentView);
  ASSERT_NE(view, nil);

  std::u16string sharing_text = base::SysNSStringToUTF16(view.sharingText);
  EXPECT_NE(sharing_text.find(u"\u2026"), std::u16string::npos);

  NSRect sharing_rect = [view sharingTextRectForBounds:view.bounds];
  EXPECT_GT(NSWidth(sharing_rect), 0.0);
  EXPECT_GT(NSHeight(sharing_rect), 0.0);

  // Verify `sharingTextRectForBounds:` remains origin-invariant even on short
  // bounds where the top-overflow clamp engages.
  NSRect zero_origin_short_bounds = NSMakeRect(0.0, 0.0, 300.0, 140.0);
  NSRect offset_short_bounds = NSMakeRect(120.0, 240.0, 300.0, 140.0);
  NSRect rect_at_zero =
      [view sharingTextRectForBounds:zero_origin_short_bounds];
  NSRect rect_at_offset = [view sharingTextRectForBounds:offset_short_bounds];
  EXPECT_TRUE(
      NSEqualRects(rect_at_offset, NSOffsetRect(rect_at_zero, 120.0, 240.0)));
}

TEST_F(PrivacyOverlayMacTest,
       CreatesButtonWindowHiddenAndClickThroughInitially) {
  StartOverlay();

  PrivacyOverlayButtonWindow* button_window =
      overlay_->button_window_for_testing();
  ASSERT_NE(button_window, nil);
  EXPECT_TRUE(overlay_->is_button_hidden_for_testing());
  EXPECT_FALSE(button_window.isVisible);
  EXPECT_TRUE(button_window.ignoresMouseEvents);
  EXPECT_DOUBLE_EQ(button_window.alphaValue, 0.0);
  EXPECT_EQ(button_window.styleMask,
            NSWindowStyleMaskBorderless | NSWindowStyleMaskNonactivatingPanel);
  EXPECT_FALSE(button_window.hidesOnDeactivate);
  EXPECT_FALSE(button_window.canHide);
  EXPECT_FALSE(button_window.isMovable);
  EXPECT_EQ(button_window.level, NSScreenSaverWindowLevel + 1);
  EXPECT_EQ(button_window.sharingType, NSWindowSharingNone);
  EXPECT_FALSE(button_window.canBecomeKeyWindow);
  EXPECT_FALSE(button_window.canBecomeMainWindow);
  NSRect unconstrained_rect = NSMakeRect(-50.0, -50.0, 400.0, 100.0);
  EXPECT_TRUE(NSEqualRects(
      [button_window constrainFrameRect:unconstrained_rect
                               toScreen:NSScreen.screens.firstObject],
      unconstrained_rect));
  ASSERT_NE(button_window.disconnectButton, nil);
  EXPECT_GT(button_window.disconnectButton.title.length, 0u);
  EXPECT_GT(button_window.disconnectButton.accessibilityLabel.length, 0u);
  EXPECT_DOUBLE_EQ(NSWidth(button_window.frame),
                   NSWidth(button_window.disconnectButton.frame) + 24.0);
  EXPECT_DOUBLE_EQ(NSHeight(button_window.frame),
                   NSHeight(button_window.disconnectButton.frame) + 24.0);
}

TEST_F(PrivacyOverlayMacTest,
       PaintsGoogleBlueAndHighContrastButtonWindowBackgrounds) {
  StartOverlay();

  PrivacyOverlayButtonWindow* button_window =
      overlay_->button_window_for_testing();
  ASSERT_NE(button_window, nil);
  PrivacyOverlayButtonView* button_view = button_window.buttonView;
  ASSERT_NE(button_view, nil);

  NSRect bounds = button_view.bounds;
  ASSERT_GT(NSWidth(bounds), 24.0);
  ASSERT_GT(NSHeight(bounds), 24.0);

  [button_view setHighContrastOverrideForTesting:false];
  NSBitmapImageRep* blue_rep =
      [button_view bitmapImageRepForCachingDisplayInRect:bounds];
  ASSERT_NE(blue_rep, nil);
  [button_view cacheDisplayInRect:bounds toBitmapImageRep:blue_rep];
  CGFloat scale_x = static_cast<CGFloat>(blue_rep.pixelsWide) / NSWidth(bounds);
  NSInteger sample_x = std::max<NSInteger>(3, std::lround(6.0 * scale_x));
  NSInteger sample_y = blue_rep.pixelsHigh / 2;

  ExpectBitmapPixelNearColor(blue_rep, sample_x, sample_y, GoogleBlueColor());

  [button_view setHighContrastOverrideForTesting:true];
  NSBitmapImageRep* hc_rep =
      [button_view bitmapImageRepForCachingDisplayInRect:bounds];
  ASSERT_NE(hc_rep, nil);
  [button_view cacheDisplayInRect:bounds toBitmapImageRep:hc_rep];
  ExpectBitmapPixelNearColor(hc_rep, sample_x, sample_y,
                             NSColor.windowBackgroundColor);
}

TEST_F(PrivacyOverlayMacTest,
       LocalClickOnDisconnectButtonDisconnectsSessionAndClosesWindows) {
  StartOverlay();
  overlay_->ActivateDisconnectButtonForTesting();

  PrivacyOverlayButtonWindow* button_window =
      overlay_->button_window_for_testing();
  ASSERT_NE(button_window, nil);

  EXPECT_CALL(client_session_control_,
              DisconnectSession(protocol::ErrorCode::OK, _, _))
      .Times(1);
  [button_window.disconnectButton performClick:nil];
  task_environment_.RunUntilIdle();

  EXPECT_EQ(overlay_->button_window_for_testing(), nil);
  EXPECT_EQ(overlay_->backdrop_windows_for_testing().count, 0u);
  EXPECT_TRUE(overlay_->is_button_hidden_for_testing());
}

TEST_F(PrivacyOverlayMacTest,
       IgnoresRemoteInjectedMouseEventOnDisconnectButton) {
  StartOverlay();
  overlay_->ActivateDisconnectButtonForTesting();

  PrivacyOverlayButtonWindow* button_window =
      overlay_->button_window_for_testing();
  ASSERT_NE(button_window, nil);

  // `CGEventCreateMouseEvent` and `CGEventCreateKeyboardEvent` stamp
  // `kCGEventSourceUnixProcessID` with `getpid()`, matching events synthesized
  // by `InputInjectorMac`.
  CGFloat primary_height = NSHeight(NSScreen.screens.firstObject.frame);
  NSRect button_screen_rect =
      [button_window convertRectToScreen:button_window.disconnectButton.frame];
  CGPoint quartz_click_point = CGPointMake(
      NSMidX(button_screen_rect), primary_height - NSMidY(button_screen_rect));
  base::apple::ScopedCFTypeRef<CGEventRef> cg_down(CGEventCreateMouseEvent(
      nullptr, kCGEventLeftMouseDown, quartz_click_point, kCGMouseButtonLeft));
  ASSERT_TRUE(cg_down);
  ASSERT_EQ(
      CGEventGetIntegerValueField(cg_down.get(), kCGEventSourceUnixProcessID),
      static_cast<int64_t>(getpid()));

  NSEvent* injected_mouse_down = [NSEvent eventWithCGEvent:cg_down.get()];
  ASSERT_NE(injected_mouse_down, nil);
  EXPECT_TRUE(
      PrivacyOverlayMac::IsRemoteInjectedEventForTesting(injected_mouse_down));

  // Negative control: clearing `kCGEventSourceUnixProcessID` to `0` on a
  // separate `CGEventRef` (matching physical hardware events) must not be
  // classified as remote-injected.
  base::apple::ScopedCFTypeRef<CGEventRef> cg_local_down(
      CGEventCreateMouseEvent(nullptr, kCGEventLeftMouseDown,
                              quartz_click_point, kCGMouseButtonLeft));
  ASSERT_TRUE(cg_local_down);
  CGEventSetIntegerValueField(cg_local_down.get(), kCGEventSourceUnixProcessID,
                              0);
  NSEvent* local_mouse_down = [NSEvent eventWithCGEvent:cg_local_down.get()];
  ASSERT_NE(local_mouse_down, nil);
  ASSERT_EQ(CGEventGetIntegerValueField(local_mouse_down.CGEvent,
                                        kCGEventSourceUnixProcessID),
            0);
  EXPECT_FALSE(
      PrivacyOverlayMac::IsRemoteInjectedEventForTesting(local_mouse_down));

  // Keyboard positive (`getpid()`) and negative (`0`) controls.
  base::apple::ScopedCFTypeRef<CGEventRef> cg_key(
      CGEventCreateKeyboardEvent(nullptr, /*virtualKey=*/0, /*keyDown=*/true));
  ASSERT_TRUE(cg_key);
  NSEvent* injected_key_down = [NSEvent eventWithCGEvent:cg_key.get()];
  ASSERT_NE(injected_key_down, nil);
  EXPECT_TRUE(
      PrivacyOverlayMac::IsRemoteInjectedEventForTesting(injected_key_down));
  CGEventSetIntegerValueField(cg_key.get(), kCGEventSourceUnixProcessID, 0);
  NSEvent* local_key_down = [NSEvent eventWithCGEvent:cg_key.get()];
  ASSERT_NE(local_key_down, nil);
  EXPECT_FALSE(
      PrivacyOverlayMac::IsRemoteInjectedEventForTesting(local_key_down));

  // `NSEventTypeFlagsChanged` must not be filtered even when stamped with
  // `getpid()` (crbug.com/578661739).
  base::apple::ScopedCFTypeRef<CGEventRef> cg_flags(
      CGEventCreateKeyboardEvent(nullptr, /*virtualKey=*/56, /*keyDown=*/true));
  ASSERT_TRUE(cg_flags);
  CGEventSetType(cg_flags.get(), kCGEventFlagsChanged);
  CGEventSetFlags(cg_flags.get(), kCGEventFlagMaskShift);
  NSEvent* flags_changed = [NSEvent eventWithCGEvent:cg_flags.get()];
  ASSERT_NE(flags_changed, nil);
  EXPECT_EQ(flags_changed.type, NSEventTypeFlagsChanged);
  EXPECT_FALSE(
      PrivacyOverlayMac::IsRemoteInjectedEventForTesting(flags_changed));

  // Non-input AppKit housekeeping events and `nil` must never be filtered.
  NSEvent* app_defined =
      [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                         location:NSZeroPoint
                    modifierFlags:0
                        timestamp:0
                     windowNumber:button_window.windowNumber
                          context:nil
                          subtype:0
                            data1:0
                            data2:0];
  EXPECT_FALSE(PrivacyOverlayMac::IsRemoteInjectedEventForTesting(app_defined));
  NSEvent* mouse_entered =
      [NSEvent enterExitEventWithType:NSEventTypeMouseEntered
                             location:NSZeroPoint
                        modifierFlags:0
                            timestamp:0
                         windowNumber:button_window.windowNumber
                              context:nil
                          eventNumber:0
                       trackingNumber:0
                             userData:nullptr];
  EXPECT_FALSE(
      PrivacyOverlayMac::IsRemoteInjectedEventForTesting(mouse_entered));
  EXPECT_FALSE(PrivacyOverlayMac::IsRemoteInjectedEventForTesting(nil));

  EXPECT_CALL(client_session_control_, DisconnectSession(_, _, _)).Times(0);
  [button_window sendEvent:injected_mouse_down];
  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&client_session_control_);

  EXPECT_NE(overlay_->button_window_for_testing(), nil);
  EXPECT_GT(overlay_->backdrop_windows_for_testing().count, 0u);

  // Local assistive technology (`AXPress` -> `accessibilityPerformPress`) must
  // still disconnect cleanly after a remote-injected event was dropped.
  EXPECT_CALL(client_session_control_,
              DisconnectSession(protocol::ErrorCode::OK, _, _))
      .Times(1);
  [button_window.disconnectButton accessibilityPerformPress];
  task_environment_.RunUntilIdle();

  EXPECT_EQ(overlay_->button_window_for_testing(), nil);
  EXPECT_EQ(overlay_->backdrop_windows_for_testing().count, 0u);
}

TEST_F(PrivacyOverlayMacTest,
       DisconnectsSessionWhenButtonWindowInitialCaptureExclusionFails) {
  NSMutableArray<PrivacyOverlayBackdropWindow*>* created_backdrops =
      [[NSMutableArray alloc] init];
  int button_applier_calls = 0;
  auto reset_applier = PrivacyOverlayMac::SetCaptureExclusionApplierForTesting(
      base::BindLambdaForTesting([created_backdrops, &button_applier_calls](
                                     NSWindow* window, NSSize /*size*/) {
        if ([window isKindOfClass:[PrivacyOverlayBackdropWindow class]]) {
          [created_backdrops
              addObject:base::apple::ObjCCastStrict<
                            PrivacyOverlayBackdropWindow>(window)];
          return true;
        }
        if ([window isKindOfClass:[PrivacyOverlayButtonWindow class]]) {
          ++button_applier_calls;
          return false;
        }
        return true;
      }));

  EXPECT_CALL(client_session_control_,
              DisconnectSession(protocol::ErrorCode::OK, _, _))
      .Times(1);
  StartOverlay();

  EXPECT_GT(created_backdrops.count, 0u);
  for (PrivacyOverlayBackdropWindow* backdrop in created_backdrops) {
    EXPECT_FALSE(backdrop.isVisible);
  }
  EXPECT_EQ(button_applier_calls, 1);
  EXPECT_EQ(overlay_->backdrop_windows_for_testing().count, 0u);
  EXPECT_EQ(overlay_->button_window_for_testing(), nil);
  EXPECT_TRUE(overlay_->is_button_hidden_for_testing());
}

TEST_F(PrivacyOverlayMacTest,
       DisconnectsSessionWhenActivateButtonCaptureExclusionFails) {
  StartOverlay();

  NSArray<PrivacyOverlayBackdropWindow*>* backdrops =
      [overlay_->backdrop_windows_for_testing() copy];
  PrivacyOverlayButtonWindow* button_window =
      overlay_->button_window_for_testing();
  ASSERT_GT(backdrops.count, 0u);
  ASSERT_NE(button_window, nil);
  ASSERT_FALSE(button_window.isVisible);

  auto reset_applier = PrivacyOverlayMac::SetCaptureExclusionApplierForTesting(
      base::BindLambdaForTesting(
          [](NSWindow* /*window*/, NSSize /*size*/) { return false; }));

  EXPECT_CALL(client_session_control_,
              DisconnectSession(protocol::ErrorCode::OK, _, _))
      .Times(1);
  overlay_->ActivateDisconnectButtonForTesting();

  EXPECT_FALSE(button_window.isVisible);
  for (PrivacyOverlayBackdropWindow* backdrop in backdrops) {
    EXPECT_FALSE(backdrop.isVisible);
  }
  EXPECT_EQ(overlay_->backdrop_windows_for_testing().count, 0u);
  EXPECT_EQ(overlay_->button_window_for_testing(), nil);
  EXPECT_TRUE(overlay_->is_button_hidden_for_testing());
}

TEST_F(PrivacyOverlayMacTest,
       ActivateDisconnectButtonSkipsRedundantWorkWhenAlreadyVisible) {
  std::vector<NSRect> frames = {NSMakeRect(0.0, 0.0, 1440.0, 900.0)};
  auto reset_screens = PrivacyOverlayMac::SetScreenFramesProviderForTesting(
      base::BindLambdaForTesting([&frames]() { return frames; }));

  int button_exclusion_calls = 0;
  auto reset_applier = PrivacyOverlayMac::SetCaptureExclusionApplierForTesting(
      base::BindLambdaForTesting(
          [&button_exclusion_calls](NSWindow* window, NSSize /*size*/) {
            if ([window isKindOfClass:[PrivacyOverlayButtonWindow class]]) {
              ++button_exclusion_calls;
            }
            return true;
          }));

  StartOverlay();
  ASSERT_EQ(button_exclusion_calls, 1);

  // First activation transitions `is_button_hidden_` from true to false and
  // refreshes capture exclusion.
  overlay_->ActivateDisconnectButtonForTesting();
  EXPECT_FALSE(overlay_->is_button_hidden_for_testing());
  EXPECT_EQ(button_exclusion_calls, 2);

  // Repeated activation while already visible at the same origin is a no-op.
  overlay_->ActivateDisconnectButtonForTesting();
  EXPECT_EQ(button_exclusion_calls, 2);

  // Changing the target origin refreshes capture exclusion even while visible.
  frames = {NSMakeRect(200.0, 100.0, 1440.0, 900.0)};
  overlay_->ActivateDisconnectButtonForTesting();
  EXPECT_EQ(button_exclusion_calls, 3);
}

}  // namespace
}  // namespace remoting
