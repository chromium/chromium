// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_impl.h"

#import "base/mac/scoped_objc_class_swizzler.h"
#include "base/no_destructor.h"
#include "content/public/browser/web_contents.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#import "ui/base/cocoa/base_view.h"
#import "ui/gfx/mac/coordinate_conversion.h"

// Overrides events and actions for NSPopUpButtonCell.
@interface FakeNSPopUpButtonCell : NSObject
@end

@implementation FakeNSPopUpButtonCell

- (void)performClickWithFrame:(NSRect)frame inView:(NSView*)view {
}

- (void)attachPopUpWithFrame:(NSRect)frame inView:(NSView*)view {
}

@end

namespace headless {

namespace {

// Swizzles all event and acctions for NSPopUpButtonCell to avoid showing in
// headless mode.
class HeadlessPopUpMethods {
 public:
  static void Init() {
    static base::NoDestructor<HeadlessPopUpMethods> swizzler;
    ALLOW_UNUSED_LOCAL(swizzler);
  }

 private:
  friend class base::NoDestructor<HeadlessPopUpMethods>;
  HeadlessPopUpMethods()
      : popup_perform_click_swizzler_([NSPopUpButtonCell class],
                                      [FakeNSPopUpButtonCell class],
                                      @selector(performClickWithFrame:inView:)),
        popup_attach_swizzler_([NSPopUpButtonCell class],
                               [FakeNSPopUpButtonCell class],
                               @selector(attachPopUpWithFrame:inView:)) {}

  base::mac::ScopedObjCClassSwizzler popup_perform_click_swizzler_;
  base::mac::ScopedObjCClassSwizzler popup_attach_swizzler_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessPopUpMethods);
};

NSString* const kActivityReason = @"Batch headless process";
const NSActivityOptions kActivityOptions =
    (NSActivityUserInitiatedAllowingIdleSystemSleep |
     NSActivityLatencyCritical) &
    ~(NSActivitySuddenTerminationDisabled |
      NSActivityAutomaticTerminationDisabled);

}  // namespace

void HeadlessBrowserImpl::PlatformInitialize() {
  HeadlessPopUpMethods::Init();
}

void HeadlessBrowserImpl::PlatformStart() {
  // Disallow headless to be throttled as a background process.
  [[NSProcessInfo processInfo] beginActivityWithOptions:kActivityOptions
                                                 reason:kActivityReason];
}

void HeadlessBrowserImpl::PlatformInitializeWebContents(
    HeadlessWebContentsImpl* web_contents) {
  NSView* web_view =
      web_contents->web_contents()->GetNativeView().GetNativeNSView();
  [web_view setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
  // TODO(eseckler): Support enabling BeginFrameControl on Mac. This is tricky
  // because it's a ui::Compositor startup setting and ui::Compositors are
  // recycled on Mac, see browser_compositor_view_mac.mm.
}

void HeadlessBrowserImpl::PlatformSetWebContentsBounds(
    HeadlessWebContentsImpl* web_contents,
    const gfx::Rect& bounds) {
  NSView* web_view =
      web_contents->web_contents()->GetNativeView().GetNativeNSView();
  NSRect frame = gfx::ScreenRectToNSRect(bounds);
  [web_view setFrame:frame];
}

ui::Compositor* HeadlessBrowserImpl::PlatformGetCompositor(
    HeadlessWebContentsImpl* web_contents) {
  // TODO(eseckler): Support BeginFrameControl on Mac.
  return nullptr;
}

}  // namespace headless
