// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_FRAME_LAYOUT_MANAGER_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_FRAME_LAYOUT_MANAGER_H_

#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"

// Layout manager used for the root window that hosts the WebContents window.
// The main WebContents window is stretched to occupy the whole parent. Note
// that the root window may host other windows (particularly menus for drop-down
// boxes). These windows get the location and size they request. The main
// window for the web content is identified by window.type() ==
// WINDOW_TYPE_CONTROL (set in WebContentsViewAura).
class FrameLayoutManager : public aura::LayoutManager {
 public:
  FrameLayoutManager();
  ~FrameLayoutManager() override;

  FrameLayoutManager(const FrameLayoutManager&) = delete;
  FrameLayoutManager& operator=(const FrameLayoutManager&) = delete;

  // Renders web content within a virtual window of a given |size|, which is
  // proportionately scaled to fit within the View.
  void ForceContentDimensions(gfx::Size size);

 private:
  // Sizes the main window to fit inside its container.
  // Sets the window's internal resolution to |render_size_override_|, if set,
  // and adjusts its scaling factor so that it fits inside the container without
  // clipping.
  void UpdateContentBounds();

  // aura::LayoutManager implementation.
  void OnWindowResized() override;
  void OnWindowAddedToLayout(aura::Window* child) override;
  void OnWillRemoveWindowFromLayout(aura::Window* child) override;
  void OnWindowRemovedFromLayout(aura::Window* child) override;
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override;
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override;

  // The main window used for the WebContents.
  aura::Window* main_child_ = nullptr;

  gfx::Size render_size_override_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_FRAME_LAYOUT_MANAGER_H_
