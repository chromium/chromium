// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/direct_manipulation_helper_win.h"

#include <objbase.h>
#include <cmath>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/win/window_event_target.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

bool LoggingEnabled() {
  static bool logging_enabled =
      base::FeatureList::IsEnabled(features::kPrecisionTouchpadLogging);

  return logging_enabled;
}

// TODO(crbug.com/914914) This is added for help us getting debug log on
// machine with scrolling issue on Windows Precision Touchpad. We will remove it
// after Windows Precision Touchpad scrolling issue fixed.
void DebugLogging(const std::string& s, HRESULT hr) {
  if (!LoggingEnabled())
    return;

  LOG(ERROR) << "Windows PTP: " << s << " " << hr;
}

// static
std::unique_ptr<DirectManipulationHelper>
DirectManipulationHelper::CreateInstance(HWND window,
                                         ui::Compositor* compositor,
                                         ui::WindowEventTarget* event_target) {
  if (!::IsWindow(window) || !compositor || !event_target)
    return nullptr;

  if (!base::FeatureList::IsEnabled(features::kPrecisionTouchpad))
    return nullptr;

  // DM_POINTERHITTEST supported since Win10.
  if (base::win::GetVersion() < base::win::Version::WIN10)
    return nullptr;

  std::unique_ptr<DirectManipulationHelper> instance =
      base::WrapUnique(new DirectManipulationHelper(window, compositor));

  if (instance->Initialize(event_target))
    return instance;

  return nullptr;
}

// static
std::unique_ptr<DirectManipulationHelper>
DirectManipulationHelper::CreateInstanceForTesting(
    ui::WindowEventTarget* event_target,
    Microsoft::WRL::ComPtr<IDirectManipulationViewport> viewport) {
  if (!base::FeatureList::IsEnabled(features::kPrecisionTouchpad))
    return nullptr;

  // DM_POINTERHITTEST supported since Win10.
  if (base::win::GetVersion() < base::win::Version::WIN10)
    return nullptr;

  std::unique_ptr<DirectManipulationHelper> instance =
      base::WrapUnique(new DirectManipulationHelper(0, nullptr));

  instance->event_handler_ =
      Microsoft::WRL::Make<DirectManipulationEventHandler>(event_target);

  instance->event_handler_->SetDirectManipulationHelper(instance.get());

  instance->viewport_ = viewport;

  return instance;
}

DirectManipulationHelper::~DirectManipulationHelper() {
  Destroy();
}

DirectManipulationHelper::DirectManipulationHelper(HWND window,
                                                   ui::Compositor* compositor)
    : window_(window), compositor_(compositor) {}

void DirectManipulationHelper::OnAnimationStep(base::TimeTicks timestamp) {
  // Simulate 1 frame in update_manager_.
  HRESULT hr = update_manager_->Update(nullptr);
  if (!SUCCEEDED(hr))
    DebugLogging("UpdateManager update failed.", hr);
}

void DirectManipulationHelper::OnCompositingShuttingDown(
    ui::Compositor* compositor) {
  DCHECK_EQ(compositor, compositor_);
  Destroy();
}

bool DirectManipulationHelper::Initialize(ui::WindowEventTarget* event_target) {
  // IDirectManipulationUpdateManager is the first COM object created by the
  // application to retrieve other objects in the Direct Manipulation API.
  // It also serves to activate and deactivate Direct Manipulation functionality
  // on a per-HWND basis.
  HRESULT hr =
      ::CoCreateInstance(CLSID_DirectManipulationManager, nullptr,
                         CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&manager_));
  if (!SUCCEEDED(hr)) {
    DebugLogging("DirectManipulationManager create failed.", hr);
    return false;
  }

  // Since we want to use fake viewport, we need UpdateManager to tell a fake
  // fake render frame.
  hr = manager_->GetUpdateManager(IID_PPV_ARGS(&update_manager_));
  if (!SUCCEEDED(hr)) {
    DebugLogging("Get UpdateManager failed.", hr);
    return false;
  }

  hr = manager_->CreateViewport(nullptr, window_, IID_PPV_ARGS(&viewport_));
  if (!SUCCEEDED(hr)) {
    DebugLogging("Viewport create failed.", hr);
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
    DebugLogging("Viewport set ActivateConfiguration failed.", hr);
    return false;
  }

  // Since we are using fake viewport and only want to use Direct Manipulation
  // for touchpad, we need to use MANUALUPDATE option.
  hr = viewport_->SetViewportOptions(
      DIRECTMANIPULATION_VIEWPORT_OPTIONS_MANUALUPDATE);
  if (!SUCCEEDED(hr)) {
    DebugLogging("Viewport set ViewportOptions failed.", hr);
    return false;
  }

  event_handler_ =
      Microsoft::WRL::Make<DirectManipulationEventHandler>(event_target);

  event_handler_->SetDirectManipulationHelper(this);

  // We got Direct Manipulation transform from
  // IDirectManipulationViewportEventHandler.
  hr = viewport_->AddEventHandler(window_, event_handler_.Get(),
                                  &view_port_handler_cookie_);
  if (!SUCCEEDED(hr)) {
    DebugLogging("Viewport add EventHandler failed.", hr);
    return false;
  }

  // Set default rect for viewport before activate.
  gfx::Size viewport_size_in_pixels = {1000, 1000};
  event_handler_->SetViewportSizeInPixels(viewport_size_in_pixels);
  RECT rect = gfx::Rect(viewport_size_in_pixels).ToRECT();
  hr = viewport_->SetViewportRect(&rect);
  if (!SUCCEEDED(hr)) {
    DebugLogging("Viewport set rect failed.", hr);
    return false;
  }

  hr = manager_->Activate(window_);
  if (!SUCCEEDED(hr)) {
    DebugLogging("DirectManipulationManager activate failed.", hr);
    return false;
  }

  hr = viewport_->Enable();
  if (!SUCCEEDED(hr)) {
    DebugLogging("Viewport enable failed.", hr);
    return false;
  }

  hr = update_manager_->Update(nullptr);
  if (!SUCCEEDED(hr)) {
    DebugLogging("UpdateManager update failed.", hr);
    return false;
  }

  DebugLogging("DirectManipulation initialization complete", S_OK);
  return true;
}

void DirectManipulationHelper::SetSizeInPixels(
    const gfx::Size& size_in_pixels) {
  if (!event_handler_->SetViewportSizeInPixels(size_in_pixels))
    return;

  HRESULT hr = viewport_->Stop();
  if (!SUCCEEDED(hr)) {
    DebugLogging("Viewport stop failed.", hr);
    return;
  }

  RECT rect = gfx::Rect(size_in_pixels).ToRECT();
  hr = viewport_->SetViewportRect(&rect);
  if (!SUCCEEDED(hr))
    DebugLogging("Viewport set rect failed.", hr);
}

void DirectManipulationHelper::OnPointerHitTest(WPARAM w_param) {
  // Update the device scale factor.
  event_handler_->SetDeviceScaleFactor(
      display::win::ScreenWin::GetScaleFactorForHWND(window_));

  // Only DM_POINTERHITTEST can be the first message of input sequence of
  // touchpad input.
  // TODO(chaopeng) Check if Windows API changes:
  // For WM_POINTER, the pointer type will show the event from mouse.
  // For WM_POINTERACTIVATE, the pointer id will be different with the following
  // message.
  using GetPointerTypeFn = BOOL(WINAPI*)(UINT32, POINTER_INPUT_TYPE*);
  UINT32 pointer_id = GET_POINTERID_WPARAM(w_param);
  POINTER_INPUT_TYPE pointer_type;
  static const auto get_pointer_type = reinterpret_cast<GetPointerTypeFn>(
      base::win::GetUser32FunctionPointer("GetPointerType"));
  if (get_pointer_type && get_pointer_type(pointer_id, &pointer_type) &&
      pointer_type == PT_TOUCHPAD) {
    HRESULT hr = viewport_->SetContact(pointer_id);
    if (!SUCCEEDED(hr))
      DebugLogging("Viewport set contact failed.", hr);
  }
}

void DirectManipulationHelper::AddAnimationObserver() {
  DCHECK(compositor_);
  compositor_->AddAnimationObserver(this);
  has_animation_observer_ = true;
}

void DirectManipulationHelper::RemoveAnimationObserver() {
  DCHECK(compositor_);
  compositor_->RemoveAnimationObserver(this);
  has_animation_observer_ = false;
}

void DirectManipulationHelper::SetDeviceScaleFactorForTesting(float factor) {
  event_handler_->SetDeviceScaleFactor(factor);
}

void DirectManipulationHelper::Destroy() {
  if (!compositor_)
    return;
  if (has_animation_observer_)
    RemoveAnimationObserver();
  compositor_ = nullptr;

  if (event_handler_)
    event_handler_->SetDirectManipulationHelper(nullptr);

  HRESULT hr;
  if (viewport_) {
    hr = viewport_->Stop();
    if (!SUCCEEDED(hr))
      DebugLogging("Viewport stop failed.", hr);

    hr = viewport_->RemoveEventHandler(view_port_handler_cookie_);
    if (!SUCCEEDED(hr))
      DebugLogging("Viewport remove event handler failed.", hr);

    hr = viewport_->Abandon();
    if (!SUCCEEDED(hr))
      DebugLogging("Viewport abandon failed.", hr);
  }

  if (manager_) {
    hr = manager_->Deactivate(window_);
    if (!SUCCEEDED(hr))
      DebugLogging("DirectManipulationManager deactivate failed.", hr);
  }
}

}  // namespace content
