// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/direct_manipulation_helper_win.h"

#include <objbase.h>

#include <cmath>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/win/win_util.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/win/window_event_target.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

namespace {

constexpr gfx::Size kDefaultSize{1000, 1000};

}  // namespace

// static
std::unique_ptr<DirectManipulationHelper>
DirectManipulationHelper::CreateInstance(HWND window) {
  if (!::IsWindow(window)) {
    return nullptr;
  }

  // IDirectManipulationUpdateManager is the first COM object created by the
  // application to retrieve other objects in the Direct Manipulation API.
  // It also serves to activate and deactivate Direct Manipulation functionality
  // on a per-HWND basis.
  ComPtr<IDirectManipulationManager> manager;
  HRESULT hr = ::CoCreateInstance(CLSID_DirectManipulationManager, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&manager));
  if (!SUCCEEDED(hr)) {
    return nullptr;
  }

  return CreateInstanceImpl(std::move(manager), window);
}

// static
std::unique_ptr<DirectManipulationHelper>
DirectManipulationHelper::CreateInstanceForTesting(
    ComPtr<IDirectManipulationManager> manager) {
  return CreateInstanceImpl(std::move(manager), /*window=*/nullptr);
}

// static
std::unique_ptr<DirectManipulationHelper>
DirectManipulationHelper::CreateInstanceImpl(
    ComPtr<IDirectManipulationManager> manager,
    HWND window) {
  // Since we want to use fake viewport, we need UpdateManager to tell a fake
  // fake render frame.
  ComPtr<IDirectManipulationUpdateManager> update_manager;
  HRESULT hr = manager->GetUpdateManager(IID_PPV_ARGS(&update_manager));
  if (!SUCCEEDED(hr)) {
    return nullptr;
  }

  ComPtr<IDirectManipulationViewport> viewport;
  hr = manager->CreateViewport(nullptr, window, IID_PPV_ARGS(&viewport));
  if (!SUCCEEDED(hr)) {
    return nullptr;
  }

  absl::Cleanup clean_up_viewport_on_error = [&viewport] {
    viewport->Abandon();
  };

  DIRECTMANIPULATION_CONFIGURATION configuration =
      DIRECTMANIPULATION_CONFIGURATION_INTERACTION |
      DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_X |
      DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_Y |
      DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_INERTIA |
      DIRECTMANIPULATION_CONFIGURATION_RAILS_X |
      DIRECTMANIPULATION_CONFIGURATION_RAILS_Y |
      DIRECTMANIPULATION_CONFIGURATION_SCALING;

  hr = viewport->ActivateConfiguration(configuration);
  if (!SUCCEEDED(hr)) {
    return nullptr;
  }

  // Since we are using fake viewport and only want to use Direct Manipulation
  // for touchpad, we need to use MANUALUPDATE option.
  hr = viewport->SetViewportOptions(
      DIRECTMANIPULATION_VIEWPORT_OPTIONS_MANUALUPDATE);
  if (!SUCCEEDED(hr)) {
    return nullptr;
  }

  // Set default rect for viewport before activate.
  RECT rect = gfx::Rect(kDefaultSize).ToRECT();
  hr = viewport->SetViewportRect(&rect);
  if (!SUCCEEDED(hr)) {
    return nullptr;
  }

  hr = manager->Activate(window);
  if (!SUCCEEDED(hr)) {
    return nullptr;
  }

  absl::Cleanup deactivate_manager_on_error = [&manager, &window] {
    manager->Deactivate(window);
  };

  hr = viewport->Enable();
  if (!SUCCEEDED(hr)) {
    return nullptr;
  }

  std::move(deactivate_manager_on_error).Cancel();
  std::move(clean_up_viewport_on_error).Cancel();

  return base::WrapUnique(new DirectManipulationHelper(
      std::move(manager), std::move(update_manager), std::move(viewport),
      window));
}

DirectManipulationHelper::DirectManipulationHelper(
    ComPtr<IDirectManipulationManager> manager,
    ComPtr<IDirectManipulationUpdateManager> update_manager,
    ComPtr<IDirectManipulationViewport> viewport,
    HWND window)
    : manager_(std::move(manager)),
      update_manager_(std::move(update_manager)),
      viewport_(std::move(viewport)),
      window_(window),
      size_in_pixels_(kDefaultSize) {}

DirectManipulationHelper::~DirectManipulationHelper() {
  Destroy();
}

void DirectManipulationHelper::OnAnimationStep(base::TimeTicks timestamp) {
  // Simulate 1 frame in update_manager_.
  update_manager_->Update(nullptr);
}

void DirectManipulationHelper::OnCompositingShuttingDown(
    ui::Compositor* notifying_compositor) {
  DCHECK_EQ(notifying_compositor, compositor());
  Destroy();
}

void DirectManipulationHelper::UpdateEventHandler(
    base::WeakPtr<aura::WindowTreeHost> window_tree_host,
    ui::WindowEventTarget* event_target) {
  if (window_tree_host.get() != window_tree_host_.get() &&
      has_animation_observer_) {
    RemoveAnimationObserver();
  }

  if (event_handler_) {
    event_handler_.Reset();
    viewport_->Stop();
    viewport_->RemoveEventHandler(view_port_handler_cookie_);
  }

  window_tree_host_ = window_tree_host;
  event_target_ = event_target;

  if (!event_target) {
    // No need for an event handler without a target.
    return;
  }

  event_handler_ = Microsoft::WRL::Make<DirectManipulationEventHandler>(
      weak_factory_.GetWeakPtr());
  event_handler_->SetViewportSizeInPixels(size_in_pixels_);

  // We got Direct Manipulation transform from
  // IDirectManipulationViewportEventHandler.
  HRESULT hr = viewport_->AddEventHandler(window_, event_handler_.Get(),
                                          &view_port_handler_cookie_);
  if (!SUCCEEDED(hr)) {
    event_handler_.Reset();
    return;
  }

  update_manager_->Update(nullptr);
}

void DirectManipulationHelper::SetSizeInPixels(
    const gfx::Size& size_in_pixels) {
  if (size_in_pixels == size_in_pixels_) {
    return;
  }

  size_in_pixels_ = size_in_pixels;

  if (event_handler_) {
    event_handler_->SetViewportSizeInPixels(size_in_pixels);
  }

  HRESULT hr = viewport_->Stop();
  if (!SUCCEEDED(hr))
    return;

  RECT rect = gfx::Rect(size_in_pixels).ToRECT();
  hr = viewport_->SetViewportRect(&rect);
}

void DirectManipulationHelper::OnPointerHitTest(WPARAM w_param) {
  if (!event_handler_) {
    return;
  }

  // Update the device scale factor.
  event_handler_->SetDeviceScaleFactor(
      display::win::GetScreenWin()->GetScaleFactorForHWND(window_));

  // Only DM_POINTERHITTEST can be the first message of input sequence of
  // touchpad input.
  // TODO(chaopeng) Check if Windows API changes:
  // For WM_POINTER, the pointer type will show the event from mouse.
  // For WM_POINTERACTIVATE, the pointer id will be different with the following
  // message.
  UINT32 pointer_id = GET_POINTERID_WPARAM(w_param);
  POINTER_INPUT_TYPE pointer_type;
  if (::GetPointerType(pointer_id, &pointer_type) &&
      pointer_type == PT_TOUCHPAD) {
    viewport_->SetContact(pointer_id);
  }
}

void DirectManipulationHelper::AddAnimationObserver() {
  if (compositor()) {
    compositor()->AddAnimationObserver(this);
  }
  has_animation_observer_ = true;
}

void DirectManipulationHelper::RemoveAnimationObserver() {
  if (compositor()) {
    compositor()->RemoveAnimationObserver(this);
  }
  has_animation_observer_ = false;
}

void DirectManipulationHelper::SetDeviceScaleFactorForTesting(float factor) {
  DCHECK(event_handler_);
  event_handler_->SetDeviceScaleFactor(factor);
}

void DirectManipulationHelper::Destroy() {
  UpdateEventHandler(nullptr, nullptr);
  viewport_->Abandon();
  manager_->Deactivate(window_);
}

}  // namespace content
