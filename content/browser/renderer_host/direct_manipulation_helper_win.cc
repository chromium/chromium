// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/direct_manipulation_helper_win.h"

#include <objbase.h>

#include <cmath>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/win/win_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/win/window_event_target.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

// static
std::unique_ptr<DirectManipulationHelper>
DirectManipulationHelper::CreateInstance(
    HWND window,
    base::WeakPtr<aura::WindowTreeHost> window_tree_host,
    ui::WindowEventTarget* event_target) {
  if (!::IsWindow(window) || !window_tree_host ||
      !window_tree_host->compositor() || !event_target) {
    return nullptr;
  }

  std::unique_ptr<DirectManipulationHelper> instance = base::WrapUnique(
      new DirectManipulationHelper(window, window_tree_host, event_target));

  if (instance->Initialize()) {
    return instance;
  }

  return nullptr;
}

// static
std::unique_ptr<DirectManipulationHelper>
DirectManipulationHelper::CreateInstanceForTesting(
    ui::WindowEventTarget* event_target,
    Microsoft::WRL::ComPtr<IDirectManipulationViewport> viewport) {
  std::unique_ptr<DirectManipulationHelper> instance =
      base::WrapUnique(new DirectManipulationHelper(
          /*window=*/nullptr, /*window_tree_host=*/nullptr, event_target));
  instance->viewport_ = viewport;
  return instance;
}

DirectManipulationHelper::~DirectManipulationHelper() {
  Destroy();
}

DirectManipulationHelper::DirectManipulationHelper(
    HWND window,
    base::WeakPtr<aura::WindowTreeHost> window_tree_host,
    ui::WindowEventTarget* event_target)
    : window_(window),
      window_tree_host_(window_tree_host),
      event_target_(event_target) {
  event_handler_ = Microsoft::WRL::Make<DirectManipulationEventHandler>(
      weak_factory_.GetWeakPtr());
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

bool DirectManipulationHelper::Initialize() {
  // Shouldn't be called again after Destroy().
  DCHECK(event_handler_);

  // IDirectManipulationUpdateManager is the first COM object created by the
  // application to retrieve other objects in the Direct Manipulation API.
  // It also serves to activate and deactivate Direct Manipulation functionality
  // on a per-HWND basis.
  HRESULT hr =
      ::CoCreateInstance(CLSID_DirectManipulationManager, nullptr,
                         CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&manager_));
  if (!SUCCEEDED(hr)) {
    return false;
  }

  // Since we want to use fake viewport, we need UpdateManager to tell a fake
  // fake render frame.
  hr = manager_->GetUpdateManager(IID_PPV_ARGS(&update_manager_));
  if (!SUCCEEDED(hr)) {
    return false;
  }

  hr = manager_->CreateViewport(nullptr, window_, IID_PPV_ARGS(&viewport_));
  if (!SUCCEEDED(hr)) {
    return false;
  }

  DIRECTMANIPULATION_CONFIGURATION configuration =
      DIRECTMANIPULATION_CONFIGURATION_INTERACTION |
      DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_X |
      DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_Y |
      DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_INERTIA |
      DIRECTMANIPULATION_CONFIGURATION_RAILS_X |
      DIRECTMANIPULATION_CONFIGURATION_RAILS_Y |
      DIRECTMANIPULATION_CONFIGURATION_SCALING;

  hr = viewport_->ActivateConfiguration(configuration);
  if (!SUCCEEDED(hr)) {
    return false;
  }

  // Since we are using fake viewport and only want to use Direct Manipulation
  // for touchpad, we need to use MANUALUPDATE option.
  hr = viewport_->SetViewportOptions(
      DIRECTMANIPULATION_VIEWPORT_OPTIONS_MANUALUPDATE);
  if (!SUCCEEDED(hr)) {
    return false;
  }

  // We got Direct Manipulation transform from
  // IDirectManipulationViewportEventHandler.
  hr = viewport_->AddEventHandler(window_, event_handler_.Get(),
                                  &view_port_handler_cookie_);
  if (!SUCCEEDED(hr)) {
    return false;
  }

  // Set default rect for viewport before activate.
  gfx::Size viewport_size_in_pixels = {1000, 1000};
  event_handler_->SetViewportSizeInPixels(viewport_size_in_pixels);
  RECT rect = gfx::Rect(viewport_size_in_pixels).ToRECT();
  hr = viewport_->SetViewportRect(&rect);
  if (!SUCCEEDED(hr)) {
    return false;
  }

  hr = manager_->Activate(window_);
  if (!SUCCEEDED(hr)) {
    return false;
  }

  hr = viewport_->Enable();
  if (!SUCCEEDED(hr)) {
    return false;
  }

  hr = update_manager_->Update(nullptr);
  if (!SUCCEEDED(hr)) {
    return false;
  }

  return true;
}

void DirectManipulationHelper::SetSizeInPixels(
    const gfx::Size& size_in_pixels) {
  DCHECK(event_handler_);
  if (!event_handler_->SetViewportSizeInPixels(size_in_pixels))
    return;

  HRESULT hr = viewport_->Stop();
  if (!SUCCEEDED(hr))
    return;

  RECT rect = gfx::Rect(size_in_pixels).ToRECT();
  hr = viewport_->SetViewportRect(&rect);
}

void DirectManipulationHelper::OnPointerHitTest(WPARAM w_param) {
  // Update the device scale factor.
  DCHECK(event_handler_);
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
  if (has_animation_observer_) {
    RemoveAnimationObserver();
  }
  window_tree_host_.reset();
  event_target_ = nullptr;
  event_handler_.Reset();

  if (viewport_) {
    viewport_->Stop();
    viewport_->RemoveEventHandler(view_port_handler_cookie_);
    viewport_->Abandon();
  }

  if (manager_) {
    manager_->Deactivate(window_);
  }
}

}  // namespace content
