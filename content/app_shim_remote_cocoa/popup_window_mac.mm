// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/app_shim_remote_cocoa/popup_window_mac.h"

#import "content/app_shim_remote_cocoa/render_widget_host_view_cocoa.h"
#include "ui/gfx/mac/coordinate_conversion.h"

@interface RenderWidgetPopupWindow : NSWindow
@end

@implementation RenderWidgetPopupWindow {
  // The event tap that allows monitoring of all events, to properly close with
  // a click outside the bounds of the window.
  id __strong _clickEventTap;
}

- (instancetype)initWithContentRect:(NSRect)contentRect
                          styleMask:(NSUInteger)windowStyle
                            backing:(NSBackingStoreType)bufferingType
                              defer:(BOOL)deferCreation {
  if (self = [super initWithContentRect:contentRect
                              styleMask:windowStyle
                                backing:bufferingType
                                  defer:deferCreation]) {
    self.backgroundColor = NSColor.clearColor;
    [self startObservingClicks];
  }
  return self;
}

- (void)dealloc {
  [self stopObservingClicks];
}

- (void)close {
  [self stopObservingClicks];
  [super close];
}

// Gets called when the menubar is clicked.
// Needed because the local event monitor doesn't see the click on the menubar.
- (void)beganTracking:(NSNotification*)notification {
  [self close];
}

// Install the callback.
- (void)startObservingClicks {
  RenderWidgetPopupWindow* __weak weakSelf = self;
  _clickEventTap = [NSEvent
      addLocalMonitorForEventsMatchingMask:NSEventMaskAny
                                   handler:^NSEvent*(NSEvent* event) {
                                     RenderWidgetPopupWindow* strongSelf =
                                         weakSelf;

                                     if (event.window == strongSelf) {
                                       return event;
                                     }
                                     NSEventType eventType = event.type;
                                     if (eventType ==
                                             NSEventTypeLeftMouseDown ||
                                         eventType ==
                                             NSEventTypeRightMouseDown) {
                                       [strongSelf close];
                                     }
                                     return event;
                                   }];

  [NSNotificationCenter.defaultCenter
      addObserver:self
         selector:@selector(beganTracking:)
             name:NSMenuDidBeginTrackingNotification
           object:NSApp.mainMenu];
}

// Remove the callback.
- (void)stopObservingClicks {
  if (!_clickEventTap)
    return;

  [NSEvent removeMonitor:_clickEventTap];
  _clickEventTap = nil;

  [NSNotificationCenter.defaultCenter
      removeObserver:self
                name:NSMenuDidBeginTrackingNotification
              object:[NSApp mainMenu]];
}

@end

namespace remote_cocoa {

PopupWindowMac::PopupWindowMac(const gfx::Rect& content_rect,
                               RenderWidgetHostViewCocoa* cocoa_view)
    : cocoa_view_(cocoa_view) {
  cocoa_view_.closeOnDeactivate = YES;
  cocoa_view_.canBeKeyView = NO;

  popup_window_ = [[RenderWidgetPopupWindow alloc]
      initWithContentRect:gfx::ScreenRectToNSRect(content_rect)
                styleMask:NSWindowStyleMaskBorderless
                  backing:NSBackingStoreBuffered
                    defer:NO];
  popup_window_.hasShadow = YES;
  popup_window_.level = NSPopUpMenuWindowLevel;
  popup_window_.releasedWhenClosed = NO;
  [popup_window_ makeKeyAndOrderFront:nil];
  [popup_window_.contentView addSubview:cocoa_view_];
  cocoa_view_.frame = popup_window_.contentView.bounds;
  cocoa_view_.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  [NSNotificationCenter.defaultCenter
      addObserver:cocoa_view_
         selector:@selector(popupWindowWillClose:)
             name:NSWindowWillCloseNotification
           object:popup_window_];
}

PopupWindowMac::~PopupWindowMac() {
  [NSNotificationCenter.defaultCenter
      removeObserver:cocoa_view_
                name:NSWindowWillCloseNotification
              object:popup_window_];
}

}  // namespace remote_cocoa
