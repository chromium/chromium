// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_FRAME_WINDOW_TREE_HOST_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_FRAME_WINDOW_TREE_HOST_H_

#include <fuchsia/ui/views/cpp/fidl.h>

#include "fuchsia_web/webengine/web_engine_export.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/platform_window/fuchsia/scenic_window_delegate.h"
#include "ui/platform_window/fuchsia/view_ref_pair.h"

namespace content {
class WebContents;
}  // namespace content

// aura::WindowTreeHost implementation used to present web content inside
// web.Frame.
class WEB_ENGINE_EXPORT FrameWindowTreeHost final
    : public aura::WindowTreeHostPlatform,
      public ui::ScenicWindowDelegate {
 public:
  using OnPixelScaleUpdateCallback = base::RepeatingCallback<void(float)>;

  FrameWindowTreeHost(fuchsia::ui::views::ViewToken view_token,
                      ui::ViewRefPair view_ref_pair,
                      content::WebContents* web_contents,
                      OnPixelScaleUpdateCallback on_pixel_scale_update);
  FrameWindowTreeHost(fuchsia::ui::views::ViewCreationToken view_creation_token,
                      ui::ViewRefPair view_ref_pair,
                      content::WebContents* web_contents,
                      OnPixelScaleUpdateCallback on_pixel_scale_update);
  ~FrameWindowTreeHost() override;

  FrameWindowTreeHost(const FrameWindowTreeHost&) = delete;
  FrameWindowTreeHost& operator=(const FrameWindowTreeHost&) = delete;

  // Creates and returns a ViewRef for the window.
  fuchsia::ui::views::ViewRef CreateViewRef();

  float scenic_scale_factor() { return scenic_pixel_scale_; }

 private:
  class WindowParentingClientImpl;

  // aura::WindowTreeHostPlatform overrides.
  void OnActivationChanged(bool active) override;
  void OnWindowStateChanged(ui::PlatformWindowState old_state,
                            ui::PlatformWindowState new_state) override;
  void OnWindowBoundsChanged(const BoundsChange& bounds);

  // ScenicWindowDelegate implementation.
  void OnScenicPixelScale(ui::PlatformWindow* window, float scale) final;

  const fuchsia::ui::views::ViewRef view_ref_;
  std::unique_ptr<WindowParentingClientImpl> window_parenting_client_;
  content::WebContents* const web_contents_;
  float scenic_pixel_scale_ = 1.0;
  OnPixelScaleUpdateCallback on_pixel_scale_update_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_FRAME_WINDOW_TREE_HOST_H_
