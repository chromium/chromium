// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/mouse_cursor_overlay_controller.h"

#include <Cocoa/Cocoa.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/cocoa/tracking_area.h"

namespace {
using LocationUpdateCallback = base::RepeatingCallback<void(const NSPoint&)>;
}  // namespace;

// Uses a CrTrackingArea to monitor for mouse events and forwards them to the
// MouseCursorOverlayController::Observer.
@interface MouseCursorOverlayTracker : NSObject {
 @private
  LocationUpdateCallback callback_;
  ui::ScopedCrTrackingArea trackingArea_;
}
- (instancetype)initWithCallback:(LocationUpdateCallback)callback
                         andView:(NSView*)nsView;
- (void)stopTracking:(NSView*)nsView;
@end

@implementation MouseCursorOverlayTracker

- (instancetype)initWithCallback:(LocationUpdateCallback)callback
                         andView:(NSView*)nsView {
  if ((self = [super init])) {
    callback_ = std::move(callback);
    constexpr NSTrackingAreaOptions kTrackingOptions =
        NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited |
        NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect |
        NSTrackingEnabledDuringMouseDrag;
    trackingArea_.reset([[CrTrackingArea alloc] initWithRect:NSZeroRect
                                                     options:kTrackingOptions
                                                       owner:self
                                                    userInfo:nil]);
    [nsView addTrackingArea:trackingArea_.get()];
  }
  return self;
}

- (void)stopTracking:(NSView*)nsView {
  [nsView removeTrackingArea:trackingArea_.get()];
  trackingArea_.reset();
  callback_.Reset();
}

- (void)mouseMoved:(NSEvent*)theEvent {
  callback_.Run([theEvent locationInWindow]);
}

- (void)mouseEntered:(NSEvent*)theEvent {
  callback_.Run([theEvent locationInWindow]);
}

- (void)mouseExited:(NSEvent*)theEvent {
  callback_.Run([theEvent locationInWindow]);
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

  MouseCursorOverlayController* const controller_;
  base::scoped_nsobject<NSView> view_;
  base::scoped_nsobject<MouseCursorOverlayTracker> mouse_tracker_;

  DISALLOW_COPY_AND_ASSIGN(Observer);
};

MouseCursorOverlayController::MouseCursorOverlayController()
    : mouse_move_behavior_atomic_(kNotMoving), weak_factory_(this) {
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

  if (NSView* view = Observer::GetTargetView(observer_)) {
    const NSRect view_bounds = [view bounds];
    if (!NSIsEmptyRect(view_bounds)) {
      // The documentation on NSCursor reference states that the hot spot is in
      // flipped coordinates which, from the perspective of the Aura coordinate
      // system, means it's not flipped.
      const NSPoint hotspot = [cursor hotSpot];
      const NSSize size = [[cursor image] size];
      return gfx::ScaleRect(
          gfx::RectF(location_aura.x() - hotspot.x,
                     location_aura.y() - hotspot.y, size.width, size.height),
          1.0 / NSWidth(view_bounds), 1.0 / NSHeight(view_bounds));
    }
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
