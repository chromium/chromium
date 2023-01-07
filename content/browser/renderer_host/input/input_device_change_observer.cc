// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/input_device_change_observer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_view_host_impl.h"

#if BUILDFLAG(IS_WIN)
#include "ui/events/devices/input_device_observer_win.h"
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "ui/events/devices/device_data_manager.h"
#elif BUILDFLAG(IS_ANDROID)
#include "ui/events/devices/input_device_observer_android.h"
#endif

namespace content {

InputDeviceChangeObserver::InputDeviceChangeObserver(RenderViewHostImpl* rvhi) {
  render_view_host_impl_ = rvhi;
#if BUILDFLAG(IS_WIN)
  ui::InputDeviceObserverWin::GetInstance()->AddObserver(this);
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  ui::DeviceDataManager::GetInstance()->AddObserver(this);
#elif BUILDFLAG(IS_ANDROID)
  ui::InputDeviceObserverAndroid::GetInstance()->AddObserver(this);
#endif
}

InputDeviceChangeObserver::~InputDeviceChangeObserver() {
#if BUILDFLAG(IS_WIN)
  ui::InputDeviceObserverWin::GetInstance()->RemoveObserver(this);
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  ui::DeviceDataManager::GetInstance()->RemoveObserver(this);
#elif BUILDFLAG(IS_ANDROID)
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
