// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/frame_window_tree_host.h"

#include "base/fuchsia/fuchsia_logging.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/base/ime/input_method.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace {

fuchsia::ui::views::ViewRef DupViewRef(
    const fuchsia::ui::views::ViewRef& view_ref) {
  fuchsia::ui::views::ViewRef dup;
  zx_status_t status =
      view_ref.reference.duplicate(ZX_RIGHT_SAME_RIGHTS, &dup.reference);
  ZX_CHECK(status == ZX_OK, status) << "zx_object_duplicate";
  return dup;
}

}  // namespace

class FrameWindowTreeHost::WindowParentingClientImpl
    : public aura::client::WindowParentingClient {
 public:
  explicit WindowParentingClientImpl(aura::Window* root_window)
      : root_window_(root_window) {
    aura::client::SetWindowParentingClient(root_window_, this);
  }
  ~WindowParentingClientImpl() override {
    aura::client::SetWindowParentingClient(root_window_, nullptr);
  }

  WindowParentingClientImpl(const WindowParentingClientImpl&) = delete;
  WindowParentingClientImpl& operator=(const WindowParentingClientImpl&) =
      delete;

  // WindowParentingClient implementation.
  aura::Window* GetDefaultParent(aura::Window* window,
                                 const gfx::Rect& bounds) override {
    return root_window_;
  }

 private:
  aura::Window* root_window_;
};

FrameWindowTreeHost::FrameWindowTreeHost(
    fuchsia::ui::views::ViewToken view_token,
    scenic::ViewRefPair view_ref_pair,
    content::WebContents* web_contents)
    : view_ref_(DupViewRef(view_ref_pair.view_ref)),
      web_contents_(web_contents) {
  CreateCompositor();

  ui::PlatformWindowInitProperties properties;
  properties.view_token = std::move(view_token);
  properties.view_ref_pair = std::move(view_ref_pair);
  CreateAndSetPlatformWindow(std::move(properties));

  window_parenting_client_ =
      std::make_unique<WindowParentingClientImpl>(window());
}

FrameWindowTreeHost::~FrameWindowTreeHost() = default;

fuchsia::ui::views::ViewRef FrameWindowTreeHost::CreateViewRef() {
  return DupViewRef(view_ref_);
}

void FrameWindowTreeHost::OnActivationChanged(bool active) {
  // Route focus & blur events to the window's focus observer and its
  // InputMethod.
  if (active) {
    aura::client::GetFocusClient(window())->FocusWindow(window());
    GetInputMethod()->OnFocus();
  } else {
    aura::client::GetFocusClient(window())->FocusWindow(nullptr);
    GetInputMethod()->OnBlur();
  }
}

void FrameWindowTreeHost::OnWindowStateChanged(
    ui::PlatformWindowState new_state) {
  // Tell the root aura::Window whether it is shown or hidden.
  if (new_state == ui::PlatformWindowState::kMinimized) {
    Hide();
    web_contents_->WasOccluded();
  } else {
    Show();
    web_contents_->WasShown();
  }
}

void FrameWindowTreeHost::OnWindowBoundsChanged(const BoundsChange& bounds) {
  aura::WindowTreeHostPlatform::OnBoundsChanged(bounds);

  if (web_contents_->GetMainFrame()->IsRenderFrameLive()) {
    web_contents_->GetMainFrame()->GetView()->SetInsets(
        bounds.system_ui_overlap);
  }
}
