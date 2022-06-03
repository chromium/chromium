// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/scrollbar_theme_mac.h"

#import <objc/runtime.h>

#include "base/mac/scoped_objc_class_swizzler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/scrollbar_theme_settings.h"

namespace {

const float kScaleFromDip = 2.0;
const int kRegularTrackBoxWidth = 100;
const int kSmallTrackBoxWidth = 50;
const int kLegacyScrollbarExtraWidth = 10;

NSScrollerStyle scroller_style;
NSControlSize control_size;

}  // namespace

@class NSScrollerImp;

// A class that provides alternate implementations of select NSScrollerImp
// methods. Once these methods are swizzled into place we can catch parameters
// and return the values we want for the tests.
@interface FakeNSScrollerImp : NSObject

+ (NSScrollerImp*)scrollerImpWithStyle:(NSScrollerStyle)newScrollerStyle
                           controlSize:(NSControlSize)newControlSize
                            horizontal:(BOOL)horizontal
                  replacingScrollerImp:(id)previous;
- (CGFloat)trackBoxWidth;

@end

@implementation FakeNSScrollerImp

+ (std::unique_ptr<base::mac::ScopedObjCClassSwizzler>&)factorySwizzler {
  static std::unique_ptr<base::mac::ScopedObjCClassSwizzler> factorySwizzler;

  return factorySwizzler;
}

+ (std::unique_ptr<base::mac::ScopedObjCClassSwizzler>&)trackBoxWidthSwizzler {
  static std::unique_ptr<base::mac::ScopedObjCClassSwizzler>
      trackBoxWidthSwizzler;

  return trackBoxWidthSwizzler;
}

+ (void)setUpSwizzles {
  [self factorySwizzler].reset(new base::mac::ScopedObjCClassSwizzler(
      NSClassFromString(@"NSScrollerImp"), [FakeNSScrollerImp class],
      @selector(scrollerImpWithStyle:
                         controlSize:horizontal:replacingScrollerImp:)));

  [self trackBoxWidthSwizzler].reset(new base::mac::ScopedObjCClassSwizzler(
      NSClassFromString(@"NSScrollerImp"), [FakeNSScrollerImp class],
      @selector(trackBoxWidth)));
}

+ (void)tearDownSwizzles {
  [self factorySwizzler].reset();
  [self trackBoxWidthSwizzler].reset();
}

+ (NSScrollerImp*)scrollerImpWithStyle:(NSScrollerStyle)newScrollerStyle
                           controlSize:(NSControlSize)newControlSize
                            horizontal:(BOOL)horizontal
                  replacingScrollerImp:(id)previous {
  // Capture the incoming scroller style and control size (these parameters
  // can't be retrieved from the NSScrollerImp).
  scroller_style = newScrollerStyle;
  control_size = newControlSize;

  // With the style and size stored call the NSScrollerImp factory
  // method as usual.
  return [FakeNSScrollerImp factorySwizzler]
      ->InvokeOriginal<NSScrollerImp*, NSScrollerStyle, NSControlSize, BOOL,
                       id>(
          NSClassFromString(@"NSScrollerImp"),
          @selector(scrollerImpWithStyle:
                             controlSize:horizontal:replacingScrollerImp:),
          newScrollerStyle, newControlSize, horizontal, previous);
}

- (CGFloat)trackBoxWidth {
  // Compute a track box width. |control_size| is the size we were "created"
  // with (in the factory above).
  int trackBoxWidth = control_size == NSControlSizeSmall
                          ? kSmallTrackBoxWidth
                          : kRegularTrackBoxWidth;

  // If the NSScrollerImp was created for legacy scrollbars, make it a little
  // wider.
  int extraWidth =
      scroller_style == NSScrollerStyleLegacy ? kLegacyScrollbarExtraWidth : 0;

  return trackBoxWidth + extraWidth;
}

@end

namespace blink {

class ScrollbarThemeMacTest : public testing::Test {
 public:
  void SetUp() override {
    // Swap out the NSScrollerImp factory and -trackBoxWidth implementations
    // for our own.
    [FakeNSScrollerImp setUpSwizzles];
  }

  void SetOverlayScrollbarsEnabled(bool flag) {
    ScrollbarThemeSettings::SetOverlayScrollbarsEnabled(flag);
  }

  void TearDown() override { [FakeNSScrollerImp tearDownSwizzles]; }

  ScrollbarThemeMac scrollbar_theme_mac;
};

TEST_F(ScrollbarThemeMacTest, SmallControlSizeLegacy) {
  SetOverlayScrollbarsEnabled(false);

  EXPECT_EQ((kSmallTrackBoxWidth + kLegacyScrollbarExtraWidth) * kScaleFromDip,
            scrollbar_theme_mac.ScrollbarThickness(kScaleFromDip,
                                                   EScrollbarWidth::kThin));
}

TEST_F(ScrollbarThemeMacTest, RegularControlSizeLegacy) {
  SetOverlayScrollbarsEnabled(false);

  EXPECT_EQ(
      (kRegularTrackBoxWidth + kLegacyScrollbarExtraWidth) * kScaleFromDip,
      scrollbar_theme_mac.ScrollbarThickness(kScaleFromDip,
                                             EScrollbarWidth::kAuto));
}

TEST_F(ScrollbarThemeMacTest, SmallControlSizeOverlay) {
  SetOverlayScrollbarsEnabled(true);

  EXPECT_EQ(kSmallTrackBoxWidth * kScaleFromDip,
            scrollbar_theme_mac.ScrollbarThickness(kScaleFromDip,
                                                   EScrollbarWidth::kThin));
}

TEST_F(ScrollbarThemeMacTest, RegularControlSizeOverlay) {
  SetOverlayScrollbarsEnabled(true);

  EXPECT_EQ(kRegularTrackBoxWidth * kScaleFromDip,
            scrollbar_theme_mac.ScrollbarThickness(kScaleFromDip,
                                                   EScrollbarWidth::kAuto));
}

}  // namespace blink
