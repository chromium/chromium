// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_screen_mac.h"

#import <Cocoa/Cocoa.h>

#include <optional>

#import "base/apple/scoped_objc_class_swizzler.h"
#include "base/check_deref.h"
#include "components/headless/display_util/headless_display_util.h"
#include "ui/display/screen.h"
#import "ui/gfx/mac/coordinate_conversion.h"

// Class to donate the headless screen configuration aware implementation of
// -[NSScreen frame].
@interface HeadlessScreenNSScreenDonor : NSObject
- (NSRect)frame;
@end

@implementation HeadlessScreenNSScreenDonor
- (NSRect)frame {
  display::Screen& screen = CHECK_DEREF(display::Screen::Get());
  display::Display primary_display = screen.GetPrimaryDisplay();
  const gfx::Rect bounds = primary_display.bounds();
  CHECK_EQ(bounds.x(), 0);
  CHECK_EQ(bounds.y(), 0);
  return NSMakeRect(0, 0, bounds.width(), bounds.height());
}
@end

namespace headless {

// Holds Apple Class Swizzler instance.
class HeadlessScreenMac::ClassSwizzler {
 public:
  ClassSwizzler() {
    swizzler_ = std::make_unique<base::apple::ScopedObjCClassSwizzler>(
        [NSScreen class], [HeadlessScreenNSScreenDonor class],
        @selector(frame));
  }

 private:
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler> swizzler_;
};

// static
HeadlessScreenMac* HeadlessScreenMac::Create(
    const gfx::Size& window_size,
    std::string_view screen_info_spec) {
  return new HeadlessScreenMac(window_size, screen_info_spec);
}

HeadlessScreenMac::HeadlessScreenMac(const gfx::Size& window_size,
                                     std::string_view screen_info_spec)
    : HeadlessScreen(window_size, screen_info_spec) {
  // Override [NSScreen frame] with the headless screen aware implementation.
  class_swizzler_ = std::make_unique<ClassSwizzler>();
}

HeadlessScreenMac::~HeadlessScreenMac() = default;

display::Display HeadlessScreenMac::GetDisplayNearestWindow(
    gfx::NativeWindow window) const {
  // There are no NSWindows in headless, so this method should not be called at
  // all, however content::RenderWidgetHostViewMac ctor calls it with nil
  // keyWindow, so return our best guess.
  return GetPrimaryDisplay();
}

display::Display HeadlessScreenMac::GetDisplayNearestView(
    gfx::NativeView view) const {
  // On Mac native window (i.e. NSWindow) does not exist in headless, so we have
  // to rely on native view (i.e. NSView) for nearest display determination.
  if (view && view.GetNativeNSView()) {
    NSView* ns_view = view.GetNativeNSView();
    while (NSView* parent = [ns_view superview]) {
      ns_view = parent;
    }

    const gfx::Rect bounds = gfx::ScreenRectFromNSRect([ns_view frame]);
    if (std::optional<display::Display> display =
            GetDisplayFromScreenRect(display_list().displays(), bounds)) {
      return display.value();
    }
  }
  return GetPrimaryDisplay();
}

}  // namespace headless
