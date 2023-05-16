// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/mouse_cursor_overlay_controller.h"

#include <Cocoa/Cocoa.h>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/raw_ptr.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/cocoa/tracking_area.h"

namespace {
using LocationUpdateCallback = base::RepeatingCallback<void(const NSPoint&)>;
}  // namespace;

// Uses a CrTrackingArea to monitor for mouse events and forwards them to the
// MouseCursorOverlayController::Observer.
@interface MouseCursorOverlayTracker : NSObject {
 @private
  LocationUpdateCallback _callback;
  ui::ScopedCrTrackingArea _trackingArea;
}
- (instancetype)initWithCallback:(LocationUpdateCallback)callback
                         andView:(NSView*)nsView;
- (void)stopTracking:(NSView*)nsView;
@end

@implementation MouseCursorOverlayTracker

- (instancetype)initWithCallback:(LocationUpdateCallback)callback
                         andView:(NSView*)nsView {
  if ((self = [super init])) {
    _callback = std::move(callback);
    constexpr NSTrackingAreaOptions kTrackingOptions =
        NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited |
        NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect |
        NSTrackingEnabledDuringMouseDrag;
    base::scoped_nsobject<CrTrackingArea> trackingArea([[CrTrackingArea alloc]
        initWithRect:NSZeroRect
             options:kTrackingOptions
               owner:self
            userInfo:nil]);
    _trackingArea.reset(trackingArea.get());
    [nsView addTrackingArea:_trackingArea.get()];
  }
  return self;
}

- (void)stopTracking:(NSView*)nsView {
  [nsView removeTrackingArea:_trackingArea.get()];
  _trackingArea.reset();
  _callback.Reset();
}

- (void)mouseMoved:(NSEvent*)theEvent {
  _callback.Run([theEvent locationInWindow]);
}

- (void)mouseEntered:(NSEvent*)theEvent {
  _callback.Run([theEvent locationInWindow]);
}

- (void)mouseExited:(NSEvent*)theEvent {
  _callback.Run([theEvent locationInWindow]);
}

@end

namespace content {

class MouseCursorOverlayController::Observer {
 public:
  explicit Observer(MouseCursorOverlayController* controller, NSView* view)
      : controller_(controller), view_([view retain]) {
    DCHECK(controller_);
    DCHECK(view_);
    controller_->OnMouseHasGoneIdle();
    mouse_tracker_.reset([[MouseCursorOverlayTracker alloc]
        initWithCallback:base::BindRepeating(&Observer::OnMouseMoved,
                                             base::Unretained(this))
                 andView:view_.get()]);
  }

  Observer(const Observer&) = delete;
  Observer& operator=(const Observer&) = delete;

  ~Observer() { StopTracking(); }

  void StopTracking() {
    if (mouse_tracker_) {
      [mouse_tracker_ stopTracking:view_.get()];
      mouse_tracker_.reset();
      controller_->OnMouseHasGoneIdle();
    }
  }

  static NSView* GetTargetView(const std::unique_ptr<Observer>& observer) {
    if (observer) {
      return observer->view_.get();
    }
    return nil;
  }

 private:
  void OnMouseMoved(const NSPoint& location_in_window) {
    // Compute the location within the view using Aura conventions: (0,0) is the
    // upper-left corner. So, if the NSView is flipped in Cocoa, it's not
    // flipped in Aura.
    NSPoint location_aura =
        [view_ convertPoint:location_in_window fromView:nil];
    if (![view_ isFlipped]) {
      location_aura.y = NSHeight([view_ bounds]) - location_aura.y;
    }
    controller_->OnMouseMoved(gfx::PointF(location_aura.x, location_aura.y));
  }

  const raw_ptr<MouseCursorOverlayController> controller_;
  base::scoped_nsobject<NSView> view_;
  base::scoped_nsobject<MouseCursorOverlayTracker> mouse_tracker_;
};

MouseCursorOverlayController::MouseCursorOverlayController()
    : mouse_activity_ended_timer_(
          FROM_HERE,
          kIdleTimeout,
          base::BindRepeating(&MouseCursorOverlayController::OnMouseHasGoneIdle,
                              base::Unretained(this))),
      mouse_move_behavior_atomic_(kNotMoving) {
  // MouseCursorOverlayController can be constructed on any thread, but
  // thereafter must be used according to class-level comments.
  DETACH_FROM_SEQUENCE(ui_sequence_checker_);
}

MouseCursorOverlayController::~MouseCursorOverlayController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  observer_.reset();
  Stop();
}

void MouseCursorOverlayController::SetTargetView(gfx::NativeView view) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  observer_.reset();
  if (view) {
    observer_ = std::make_unique<Observer>(this, view.GetNativeNSView());
  }
}

gfx::NativeCursor MouseCursorOverlayController::GetCurrentCursorOrDefault()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  NSCursor* cursor = [NSCursor currentCursor];
  if (!cursor) {
    cursor = [NSCursor arrowCursor];
  }
  return cursor;
}

gfx::RectF MouseCursorOverlayController::ComputeRelativeBoundsForOverlay(
    const gfx::NativeCursor& cursor,
    const gfx::PointF& location_aura) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  gfx::Size target_size;
  if (NSView* view = Observer::GetTargetView(observer_)) {
    const NSRect view_bounds = [view bounds];
    target_size = gfx::Size(NSWidth(view_bounds), NSHeight(view_bounds));
  } else {
    // The target for capture can be a views::Widget, which is an NSWindow,
    // not a NSView. This path is used in that case.
    target_size = target_size_;
  }

  if (target_size.GetArea()) {
    // The documentation on NSCursor reference states that the hot spot is in
    // flipped coordinates which, from the perspective of the Aura coordinate
    // system, means it's not flipped.
    const NSPoint hotspot = [cursor hotSpot];
    const NSSize size = [[cursor image] size];
    return gfx::ScaleRect(
        gfx::RectF(location_aura.x() - hotspot.x, location_aura.y() - hotspot.y,
                   size.width, size.height),
        1.0 / target_size.width(), 1.0 / target_size.height());
  }

  return gfx::RectF();
}

void MouseCursorOverlayController::DisconnectFromToolkitForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  observer_->StopTracking();

  // Note: Not overriding the mouse cursor since the default is already
  // [NSCursor arrowCursor], which provides the tests a bitmap they can work
  // with.
}

// static
SkBitmap MouseCursorOverlayController::GetCursorImage(
    const gfx::NativeCursor& cursor) {
  return skia::NSImageToSkBitmapWithColorSpace(
      [cursor image], /*is_opaque=*/false, base::mac::GetSystemColorSpace());
}

}  // namespace content
