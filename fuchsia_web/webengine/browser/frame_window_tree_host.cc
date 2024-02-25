// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/frame_window_tree_host.h"

#include "base/fuchsia/fuchsia_logging.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "fuchsia_web/webengine/features.h"
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

ui::PlatformWindowInitProperties CreatePlatformWindowInitProperties(
    ui::ViewRefPair view_ref_pair,
    ui::ScenicWindowDelegate* scenic_window_delegate) {
  ui::PlatformWindowInitProperties properties;
  properties.view_ref_pair = std::move(view_ref_pair);
  properties.enable_keyboard =
      base::FeatureList::IsEnabled(features::kKeyboardInput);
  properties.enable_virtual_keyboard =
      base::FeatureList::IsEnabled(features::kVirtualKeyboard);
  properties.scenic_window_delegate = scenic_window_delegate;
  return properties;
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
                                 const gfx::Rect& bounds,
                                 const int64_t display_id) override {
    return root_window_;
  }

 private:
  aura::Window* root_window_;
};

FrameWindowTreeHost::FrameWindowTreeHost(
    fuchsia::ui::views::ViewToken view_token,
    ui::ViewRefPair view_ref_pair,
    content::WebContents* web_contents,
    OnPixelScaleUpdateCallback on_pixel_scale_update)
    : view_ref_(DupViewRef(view_ref_pair.view_ref)),
      web_contents_(web_contents),
      on_pixel_scale_update_(std::move(on_pixel_scale_update)) {
  CreateCompositor();

  ui::PlatformWindowInitProperties properties =
      CreatePlatformWindowInitProperties(std::move(view_ref_pair), this);
  properties.view_token = std::move(view_token);
  CreateAndSetPlatformWindow(std::move(properties));

  window_parenting_client_ =
      std::make_unique<WindowParentingClientImpl>(window());
}

FrameWindowTreeHost::FrameWindowTreeHost(
    fuchsia::ui::views::ViewCreationToken view_creation_token,
    ui::ViewRefPair view_ref_pair,
    content::WebContents* web_contents,
    OnPixelScaleUpdateCallback on_pixel_scale_update)
    : view_ref_(DupViewRef(view_ref_pair.view_ref)),
      web_contents_(web_contents),
      on_pixel_scale_update_(std::move(on_pixel_scale_update)) {
  CreateCompositor();

  ui::PlatformWindowInitProperties properties =
      CreatePlatformWindowInitProperties(std::move(view_ref_pair), this);
  properties.view_creation_token = std::move(view_creation_token);
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
    ui::PlatformWindowState old_state,
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

  if (web_contents_->GetPrimaryMainFrame()->IsRenderFrameLive()) {
    web_contents_->GetPrimaryMainFrame()->GetView()->SetInsets(
        bounds.system_ui_overlap);
  }
}

void FrameWindowTreeHost::OnScenicPixelScale(ui::PlatformWindow* window,
                                             float scale) {
  scenic_pixel_scale_ = scale;
  if (on_pixel_scale_update_)
    on_pixel_scale_update_.Run(scenic_pixel_scale_);
}
