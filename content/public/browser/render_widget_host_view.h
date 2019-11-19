// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_VIEW_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_VIEW_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/range/range.h"

namespace gfx {
class Point;
class Rect;
class Size;
}

namespace ui {
enum class DomCode;
class TextInputClient;
}

namespace viz {
class ClientFrameSinkVideoCapturer;
}  // namespace viz

namespace content {

class RenderWidgetHost;
class TouchSelectionControllerClientManager;
struct ScreenInfo;

// RenderWidgetHostView is an interface implemented by an object that acts as
// the "View" portion of a RenderWidgetHost. The RenderWidgetHost and its
// associated RenderProcessHost own the "Model" in this case which is the
// child renderer process. The View is responsible for receiving events from
// the surrounding environment and passing them to the RenderWidgetHost, and
// for actually displaying the content of the RenderWidgetHost when it
// changes.
//
// RenderWidgetHostView Class Hierarchy:
//   RenderWidgetHostView - Public interface.
//   RenderWidgetHostViewBase - Common implementation between platforms.
//   RenderWidgetHostViewAura, ... - Platform specific implementations.
class CONTENT_EXPORT RenderWidgetHostView {
 public:
  virtual ~RenderWidgetHostView() {}

  // Initialize this object for use as a drawing area.  |parent_view| may be
  // left as nullptr on platforms where a parent view is not required to
  // initialize a child window.
  virtual void InitAsChild(gfx::NativeView parent_view) = 0;

  // Returns the associated RenderWidgetHost.
  virtual RenderWidgetHost* GetRenderWidgetHost() = 0;

  // Tells the View to size itself to the specified size.
  virtual void SetSize(const gfx::Size& size) = 0;

  // Instructs the View to automatically resize and send back updates
  // for the new size.
  virtual void EnableAutoResize(const gfx::Size& min_size,
                                const gfx::Size& max_size) = 0;

  // Turns off auto-resize and gives a new size that the view should be.
  virtual void DisableAutoResize(const gfx::Size& new_size) = 0;

  // Tells the View to size and move itself to the specified size and point in
  // screen space.
  virtual void SetBounds(const gfx::Rect& rect) = 0;

  // Indicates whether the scroll offset of the view is at top.
  virtual bool IsScrollOffsetAtTop() = 0;

  // Sets a flag that indicates if it is in virtual reality mode.
  virtual void SetIsInVR(bool is_in_vr) = 0;

  // Coordinate points received from a renderer process need to be transformed
  // to the top-level frame's coordinate space. For coordinates received from
  // the top-level frame's renderer this is a no-op as they are already
  // properly transformed; however, coordinates received from an out-of-process
  // iframe renderer process require transformation.
  virtual gfx::PointF TransformPointToRootCoordSpaceF(
      const gfx::PointF& point) = 0;

  // A int point variant of the above. Use float version to transform,
  // then rounded back to int point.
  gfx::Point TransformPointToRootCoordSpace(const gfx::Point& point) {
    return gfx::ToRoundedPoint(
        TransformPointToRootCoordSpaceF(gfx::PointF(point)));
  }

  // Converts a point in the root view's coordinate space to the coordinate
  // space of whichever view is used to call this method.
  virtual gfx::PointF TransformRootPointToViewCoordSpace(
      const gfx::PointF& point) = 0;

  // Retrieves the native view used to contain plugins and identify the
  // renderer in IPC messages.
  virtual gfx::NativeView GetNativeView() = 0;
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() = 0;

  // Returns a ui::TextInputClient to support text input or nullptr if this RWHV
  // doesn't support text input.
  // Note: Not all the platforms use ui::InputMethod and ui::TextInputClient for
  // text input.  Some platforms (Mac and Android for example) use their own
  // text input system.
  virtual ui::TextInputClient* GetTextInputClient() = 0;

  // Set focus to the associated View component.
  virtual void Focus() = 0;
  // Returns true if the View currently has the focus.
  virtual bool HasFocus() = 0;

  // Shows/hides the view.  These must always be called together in pairs.
  // It is not legal to call Hide() multiple times in a row.
  virtual void Show() = 0;
  virtual void Hide() = 0;

  // Whether the view is showing.
  virtual bool IsShowing() = 0;

  // Indicates if the view is currently occluded (e.g, not visible because it's
  // covered up by other windows), and as a result the view's renderer may be
  // suspended. Calling Show()/Hide() overrides the state set by these methods.
  virtual void WasUnOccluded() = 0;
  virtual void WasOccluded() = 0;

  // Retrieve the bounds of the View, in screen coordinates.
  virtual gfx::Rect GetViewBounds() = 0;

  // Returns the currently selected text in both of editable text fields and
  // non-editable texts.
  virtual base::string16 GetSelectedText() = 0;

  // This only returns non-null on platforms that implement touch
  // selection editing (TSE), currently Aura and Android.
  virtual TouchSelectionControllerClientManager*
  GetTouchSelectionControllerClientManager() = 0;

  // Subclasses should override this method to set the background color. |color|
  // has to be either SK_ColorTRANSPARENT or opaque. If set to
  // SK_ColorTRANSPARENT, the renderer's background color will be overridden to
  // be fully transparent.
  // SetBackgroundColor is called to set the default color of the view,
  // which is shown if the background color of the renderer is not available.
  virtual void SetBackgroundColor(SkColor color) = 0;
  // GetBackgroundColor returns the current background color of the view.
  virtual base::Optional<SkColor> GetBackgroundColor() = 0;

  // Return value indicates whether the mouse is locked successfully or not.
  virtual bool LockMouse(bool request_unadjusted_movement) = 0;
  virtual void UnlockMouse() = 0;
  // Returns true if the mouse pointer is currently locked.
  virtual bool IsMouseLocked() = 0;
  // Get the pointer lock unadjusted movement setting for testing.
  // Returns true if mouse is locked and is in unadjusted movement mode.
  virtual bool GetIsMouseLockedUnadjustedMovementForTesting() = 0;

  // Start/Stop intercepting future system keyboard events.
  virtual bool LockKeyboard(
      base::Optional<base::flat_set<ui::DomCode>> dom_codes) = 0;
  virtual void UnlockKeyboard() = 0;
  // Returns true if keyboard lock is active.
  virtual bool IsKeyboardLocked() = 0;

  // Return a mapping dictionary from keyboard code to key values for the
  // highest-priority ASCII-capable layout in the list of currently installed
  // keyboard layouts.
  virtual base::flat_map<std::string, std::string> GetKeyboardLayoutMap() = 0;

  // Retrives the size of the viewport for the visible region. May be smaller
  // than the view size if a portion of the view is obstructed (e.g. by a
  // virtual keyboard).
  virtual gfx::Size GetVisibleViewportSize() = 0;

  // Set insets for the visible region of the root window. Used to compute the
  // visible viewport.
  virtual void SetInsets(const gfx::Insets& insets) = 0;

  // Returns true if the current display surface is available.
  virtual bool IsSurfaceAvailableForCopy() = 0;

  // Copies the given subset of the view's surface, optionally scales it, and
  // returns the result as a bitmap via the provided callback. This is meant for
  // one-off snapshots. For continuous video capture of the surface, please use
  // CreateVideoCapturer() instead.
  //
  // |src_rect| is either the subset of the view's surface, in view coordinates,
  // or empty to indicate that all of it should be copied. This is NOT the same
  // coordinate system as that used GetViewBounds() (https://crbug.com/73362).
  //
  // |output_size| is the size of the resulting bitmap, or empty to indicate no
  // scaling is desired. If an empty size is provided, note that the resulting
  // bitmap's size may not be the same as |src_rect.size()| due to the pixel
  // scale used by the underlying device.
  //
  // |callback| is guaranteed to be run, either synchronously or at some point
  // in the future (depending on the platform implementation and the current
  // state of the Surface). If the copy failed, the bitmap's drawsNothing()
  // method will return true.
  //
  // If the view's renderer is suspended (see WasOccluded()), this may result in
  // copying old data or failing.
  virtual void CopyFromSurface(
      const gfx::Rect& src_rect,
      const gfx::Size& output_size,
      base::OnceCallback<void(const SkBitmap&)> callback) = 0;

  // Ensures that all surfaces are synchronized for the next call to
  // CopyFromSurface. This is used by web tests.
  virtual void EnsureSurfaceSynchronizedForWebTest() = 0;

  // Creates a video capturer, which will allow the caller to receive a stream
  // of media::VideoFrames captured from this view. The capturer is configured
  // to target this view, so there is no need to call ChangeTarget() before
  // Start(). See viz.mojom.FrameSinkVideoCapturer for documentation.
  virtual std::unique_ptr<viz::ClientFrameSinkVideoCapturer>
  CreateVideoCapturer() = 0;

  // Informs the view that its associated render widget has frames to draw and
  // wants to have BeginFrame messages sent to it.  This should only be called
  // when the value has changed.  Views must initially default to false.
  virtual void SetNeedsBeginFrames(bool needs_begin_frames) = 0;

  // Informs the view that its associated render widget also wants to receive
  // animate_only BeginFrames.
  virtual void SetWantsAnimateOnlyBeginFrames() = 0;

  // This method returns the ScreenInfo used by the view to render. If the
  // information is not knowable (e.g, because the view is not attached to a
  // screen yet), then a default best-guess will be used.
  virtual void GetScreenInfo(ScreenInfo* screen_info) = 0;

  // This must always return the same device scale factor as GetScreenInfo.
  virtual float GetDeviceScaleFactor() = 0;

#if defined(OS_MACOSX)
  // Set the view's active state (i.e., tint state of controls).
  virtual void SetActive(bool active) = 0;

  // Brings up the dictionary showing a definition for the selected text.
  virtual void ShowDefinitionForSelection() = 0;

  // Tells the view to speak the currently selected text.
  virtual void SpeakSelection() = 0;
#endif  // defined(OS_MACOSX)

  // Indicates that this view should show the contents of |view| if it doesn't
  // have anything to show.
  virtual void TakeFallbackContentFrom(RenderWidgetHostView* view) = 0;

  // Set the last time a tab change starts to be processed for this
  // RenderWidgetHostView. Will overwrite any previously stored value.
  virtual void SetRecordTabSwitchTimeRequest(base::TimeTicks start_time,
                                             bool destination_is_loaded,
                                             bool destination_is_frozen) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_VIEW_H_
