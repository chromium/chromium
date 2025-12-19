// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_VIEW_MAC_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_VIEW_MAC_DELEGATE_H_

#import <Cocoa/Cocoa.h>

namespace blink {
class WebGestureEvent;
class WebMouseWheelEvent;
}

namespace ui {
struct DidOverscrollParams;
}

// The options that define the context under which mouse events are accepted.
// Acceptance under a lower option implies acceptance under any higher option,
// but not vice versa.
enum class AcceptMouseEvents {
  // Accepts mouse events only when the window is active.
  kWhenInActiveWindow = 0,
  // Accepts mouse events when any window of the application is active.
  kWhenInActiveApp = 1,
  // Accepts mouse events regardless of window or application activation.
  kAlways = 2,
};

// The options that define the context under which tooltips are shown.
enum class AcceptTooltipEvents {
  // Default behavior. Shows tooltips only when the window is key.
  kWhenInKeyWindow = 0,
  // Shows tooltips when any window of the application is active.
  kWhenInActiveApp = 1,
};

// This protocol is used as a delegate for the NSView class used in the
// hierarchy. There are two ways to extend the view:
// - Implement the methods listed in the protocol below.
// - Implement any method, and if the view is requested to perform that method
//   and cannot, the delegate's implementation will be used.
//
// Like any Objective-C delegate, it is not retained by the delegator object.
// The delegator object will call the -viewGone: method when it is going away.
@protocol RenderWidgetHostViewMacDelegate
// The standard set of touch callbacks found on NSResponder. These messages are
// forwarded by the RenderWidgetHostViewMac/Cocoa to its delegate.
- (void)touchesMovedWithEvent:(NSEvent*)event;
- (void)touchesBeganWithEvent:(NSEvent*)event;
- (void)touchesCancelledWithEvent:(NSEvent*)event;
- (void)touchesEndedWithEvent:(NSEvent*)event;

// Callbacks to the delegate to indicate that the renderer has handled either
// a gesture scroll event or an overscroll event.
- (void)rendererHandledGestureScrollEvent:(const blink::WebGestureEvent&)event
                                 consumed:(BOOL)consumed;
- (void)rendererHandledOverscrollEvent:(const ui::DidOverscrollParams&)params;

@optional
// Notification that the view is gone.
- (void)viewGone:(NSView*)view;

// Handle an event. All incoming key and mouse events flow through this delegate
// method if implemented. Return YES if the event is fully handled, or NO if
// normal processing should take place.
- (BOOL)handleEvent:(NSEvent*)event;

// Provides validation of user interface items. If the return value is NO, then
// the delegate is unaware of that item and |valid| is undefined.  Otherwise,
// |valid| contains the validity of the specified item.
- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item
                      isValidItem:(BOOL*)valid;

// The standard set of responder found on NSResponder. These messages are
// forwarded by the RenderWidgetHostViewMac/Cocoa to its delegate.
- (void)becomeFirstResponder;
- (void)resignFirstResponder;

- (void)windowDidBecomeKey;

// By default, only active window accepts mouse events. The content embedder may
// override this method to override the default behavior.
- (AcceptMouseEvents)acceptsMouseEventsOption;

// By default, only the key window accepts tooltips events. The content embedder
// may override this method to override the default behavior.
- (AcceptTooltipEvents)acceptsTooltipEvents;
@end

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_VIEW_MAC_DELEGATE_H_
