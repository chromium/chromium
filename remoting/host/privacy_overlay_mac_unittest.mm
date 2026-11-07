// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/host/privacy_overlay_mac.h"

#import <Cocoa/Cocoa.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/apple/foundation_util.h"
#include "base/auto_reset.h"
#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/mock_chrome_application_mac.h"
#include "base/test/task_environment.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/host/input_monitor/local_input_monitor.h"
#include "remoting/protocol/errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
    ON_CALL(client_session_control_, client_jid())
        .WillByDefault(ReturnRef(client_jid_));
  }

  void TearDown() override { DestroyOverlay(); }

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

  NSArray<PrivacyOverlayBackdropWindow*>* backdrops =
      [overlay_->backdrop_windows_for_testing() copy];
  ASSERT_GT(backdrops.count, 0u);
  for (PrivacyOverlayBackdropWindow* window in backdrops) {
    EXPECT_TRUE(window.isVisible);
  }

  EXPECT_CALL(client_session_control_, DisconnectSession(_, _, _)).Times(0);
  DestroyOverlay();

  for (PrivacyOverlayBackdropWindow* window in backdrops) {
    EXPECT_FALSE(window.isVisible);
  }
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

  ASSERT_EQ(calls.size(), 1u);
  EXPECT_EQ(calls[0].window_number, primary_window.windowNumber);
  EXPECT_TRUE(NSEqualSizes(calls[0].size, NSMakeSize(1280.0, 720.0)));
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

}  // namespace
}  // namespace remoting
