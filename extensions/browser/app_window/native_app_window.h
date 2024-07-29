// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_APP_WINDOW_NATIVE_APP_WINDOW_H_
#define EXTENSIONS_BROWSER_APP_WINDOW_NATIVE_APP_WINDOW_H_

#include <memory>
#include <vector>

#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "extensions/common/mojom/frame.mojom-forward.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom-forward.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/base_window.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

class SkRegion;

namespace input {
struct NativeWebKeyboardEvent;
}

namespace extensions {

// This is an interface to a native implementation of a app window, used for
// new-style packaged apps. App windows contain a web contents, but no tabs
// or URL bar.
class NativeAppWindow : public ui::BaseWindow,
                        public web_modal::WebContentsModalDialogHost {
 public:
  using ShapeRects = std::vector<gfx::Rect>;

  // Sets whether the window is fullscreen and the type of fullscreen.
  // |fullscreen_types| is a bit field of AppWindow::FullscreenType.
  virtual void SetFullscreen(int fullscreen_types) = 0;

  // Returns whether the window is fullscreen or about to enter fullscreen.
  virtual bool IsFullscreenOrPending() const = 0;

  // Called when the icon of the window changes.
  virtual void UpdateWindowIcon() = 0;

  // Called when the title of the window changes.
  virtual void UpdateWindowTitle() = 0;

  // Called when the draggable regions are changed.
  virtual void DraggableRegionsChanged(
      const std::vector<blink::mojom::DraggableRegionPtr>& regions) = 0;

  // Returns the region used by frameless windows for dragging. May return
  // nullptr.
  virtual SkRegion* GetDraggableRegion() = 0;

  // Called when the window shape is changed. If |region| is nullptr then the
  // window is restored to the default shape.
  virtual void UpdateShape(std::unique_ptr<ShapeRects> rects) = 0;

  // Allows the window to handle unhandled keyboard messages coming back from
  // the renderer.
  virtual bool HandleKeyboardEvent(
      const input::NativeWebKeyboardEvent& event) = 0;

  // Returns true if the window has no frame, as for a window opened by
  // chrome.app.window.create with the option 'frame' set to 'none'.
  virtual bool IsFrameless() const = 0;

  // Returns information about the window's frame.
  virtual bool HasFrameColor() const = 0;
  virtual SkColor ActiveFrameColor() const = 0;
  virtual SkColor InactiveFrameColor() const = 0;

  // Returns the difference between the window bounds (including titlebar and
  // borders) and the content bounds, if any.
  virtual gfx::Insets GetFrameInsets() const = 0;

  // Returns the radii of the window's corners.
  virtual gfx::RoundedCornersF GetWindowRadii() const = 0;

  // Returns the minimum size constraints of the content.
  virtual gfx::Size GetContentMinimumSize() const = 0;

  // Returns the maximum size constraints of the content.
  virtual gfx::Size GetContentMaximumSize() const = 0;

  // Updates the minimum and maximum size constraints of the content.
  virtual void SetContentSizeConstraints(const gfx::Size& min_size,
                                         const gfx::Size& max_size) = 0;

  // Sets whether the window should be visible on all workspaces.
  virtual void SetVisibleOnAllWorkspaces(bool always_visible) = 0;

  // Returns false if the underlying native window ignores alpha transparency
  // when compositing.
  virtual bool CanHaveAlphaEnabled() const = 0;

  // Sets whether the window should be activated on pointer event.
  virtual void SetActivateOnPointer(bool activate_on_pointer) = 0;

  ~NativeAppWindow() override {}
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_APP_WINDOW_NATIVE_APP_WINDOW_H_
