// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/app_shim_remote_cocoa/popup_window_mac.h"

#import "content/app_shim_remote_cocoa/render_widget_host_view_cocoa.h"
#include "ui/gfx/mac/coordinate_conversion.h"

@interface RenderWidgetPopupWindow : NSWindow {
  // The event tap that allows monitoring of all events, to properly close with
  // a click outside the bounds of the window.
  id _clickEventTap;
}
@end

@implementation RenderWidgetPopupWindow

- (instancetype)initWithContentRect:(NSRect)contentRect
                          styleMask:(NSUInteger)windowStyle
                            backing:(NSBackingStoreType)bufferingType
                              defer:(BOOL)deferCreation {
  if (self = [super initWithContentRect:contentRect
                              styleMask:windowStyle
                                backing:bufferingType
                                  defer:deferCreation]) {
    [self setBackgroundColor:[NSColor clearColor]];
    [self startObservingClicks];
  }
  return self;
}

- (void)dealloc {
  [self stopObservingClicks];
  [super dealloc];
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
  _clickEventTap = [NSEvent
      addLocalMonitorForEventsMatchingMask:NSEventMaskAny
                                   handler:^NSEvent*(NSEvent* event) {
                                     if ([event window] == self)
                                       return event;
                                     NSEventType eventType = [event type];
                                     if (eventType ==
                                             NSEventTypeLeftMouseDown ||
                                         eventType == NSEventTypeRightMouseDown)
                                       [self close];
                                     return event;
                                   }];

  NSNotificationCenter* notificationCenter =
      [NSNotificationCenter defaultCenter];
  [notificationCenter addObserver:self
                         selector:@selector(beganTracking:)
                             name:NSMenuDidBeginTrackingNotification
                           object:[NSApp mainMenu]];
}

// Remove the callback.
- (void)stopObservingClicks {
  if (!_clickEventTap)
    return;

  [NSEvent removeMonitor:_clickEventTap];
  _clickEventTap = nil;

  NSNotificationCenter* notificationCenter =
      [NSNotificationCenter defaultCenter];
  [notificationCenter removeObserver:self
                                name:NSMenuDidBeginTrackingNotification
                              object:[NSApp mainMenu]];
}

@end

namespace remote_cocoa {

PopupWindowMac::PopupWindowMac(const gfx::Rect& content_rect,
                               RenderWidgetHostViewCocoa* cocoa_view)
    : cocoa_view_(cocoa_view) {
  [cocoa_view_ setCloseOnDeactivate:YES];
  [cocoa_view_ setCanBeKeyView:NO];

  popup_window_.reset([[RenderWidgetPopupWindow alloc]
      initWithContentRect:gfx::ScreenRectToNSRect(content_rect)
                styleMask:NSWindowStyleMaskBorderless
                  backing:NSBackingStoreBuffered
                    defer:NO]);
  [popup_window_ setHasShadow:YES];
  [popup_window_ setLevel:NSPopUpMenuWindowLevel];
  [popup_window_ setReleasedWhenClosed:NO];
  [popup_window_ makeKeyAndOrderFront:nil];
  [[popup_window_ contentView] addSubview:cocoa_view_];
  [cocoa_view_ setFrame:[[popup_window_ contentView] bounds]];
  [cocoa_view_ setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  [[NSNotificationCenter defaultCenter]
      addObserver:cocoa_view_
         selector:@selector(popupWindowWillClose:)
             name:NSWindowWillCloseNotification
           object:popup_window_];
}

PopupWindowMac::~PopupWindowMac() {
  [[NSNotificationCenter defaultCenter]
      removeObserver:cocoa_view_
                name:NSWindowWillCloseNotification
              object:popup_window_];
  [popup_window_ close];
  popup_window_.autorelease();
}

}  // namespace remote_cocoa
