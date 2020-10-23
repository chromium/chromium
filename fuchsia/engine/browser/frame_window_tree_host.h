// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_FRAME_WINDOW_TREE_HOST_H_
#define FUCHSIA_ENGINE_BROWSER_FRAME_WINDOW_TREE_HOST_H_

#include <lib/ui/scenic/cpp/view_ref_pair.h>

#include "ui/aura/window_tree_host_platform.h"

namespace content {
class WebContents;
}  // namespace content

// aura::WindowTreeHost implementation used to present web content inside
// web.Frame.
class FrameWindowTreeHost : public aura::WindowTreeHostPlatform {
 public:
  FrameWindowTreeHost(fuchsia::ui::views::ViewToken view_token,
                      scenic::ViewRefPair view_ref_pair,
                      content::WebContents* web_contents);
  ~FrameWindowTreeHost() final;

  FrameWindowTreeHost(const FrameWindowTreeHost&) = delete;
  FrameWindowTreeHost& operator=(const FrameWindowTreeHost&) = delete;

  // Creates and returns a ViewRef for the window.
  fuchsia::ui::views::ViewRef CreateViewRef();

 private:
  class WindowParentingClientImpl;

  // aura::WindowTreeHostPlatform overrides.
  void OnActivationChanged(bool active) final;
  void OnWindowStateChanged(ui::PlatformWindowState new_state) final;

  const fuchsia::ui::views::ViewRef view_ref_;
  std::unique_ptr<WindowParentingClientImpl> window_parenting_client_;
  content::WebContents* const web_contents_;
};

#endif  // FUCHSIA_ENGINE_BROWSER_FRAME_WINDOW_TREE_HOST_H_
