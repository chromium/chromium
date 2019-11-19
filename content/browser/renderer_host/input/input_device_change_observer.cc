// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/input_device_change_observer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/common/web_preferences.h"

#if defined(OS_WIN)
#include "ui/events/devices/input_device_observer_win.h"
#elif defined(OS_LINUX)
#include "ui/events/devices/device_data_manager.h"
#elif defined(OS_ANDROID)
#include "ui/events/devices/input_device_observer_android.h"
#endif

namespace content {

InputDeviceChangeObserver::InputDeviceChangeObserver(RenderViewHostImpl* rvhi) {
  render_view_host_impl_ = rvhi;
#if defined(OS_WIN)
  ui::InputDeviceObserverWin::GetInstance()->AddObserver(this);
#elif defined(OS_LINUX)
  ui::DeviceDataManager::GetInstance()->AddObserver(this);
#elif defined(OS_ANDROID)
  ui::InputDeviceObserverAndroid::GetInstance()->AddObserver(this);
#endif
}

InputDeviceChangeObserver::~InputDeviceChangeObserver() {
#if defined(OS_WIN)
  ui::InputDeviceObserverWin::GetInstance()->RemoveObserver(this);
#elif defined(OS_LINUX)
  ui::DeviceDataManager::GetInstance()->RemoveObserver(this);
#elif defined(OS_ANDROID)
  ui::InputDeviceObserverAndroid::GetInstance()->RemoveObserver(this);
#endif
  render_view_host_impl_ = nullptr;
}

void InputDeviceChangeObserver::OnInputDeviceConfigurationChanged(uint8_t) {
  TRACE_EVENT0("input",
               "InputDeviceChangeObserver::OnInputDeviceConfigurationChanged");
  render_view_host_impl_->OnHardwareConfigurationChanged();
}

}  // namespace content
