// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/audio_device_listener_win.h"

#include <Audioclient.h>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/system_monitor.h"
#include "base/time/default_tick_clock.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/windows_version.h"
#include "media/audio/win/core_audio_util_win.h"

using base::win::ScopedCoMem;

namespace media {

static std::string FlowToString(EDataFlow flow) {
  return (flow == eRender) ? "eRender" : "eConsole";
}

static std::string RoleToString(ERole role) {
  switch (role) {
    case eConsole: return "eConsole";
    case eMultimedia: return "eMultimedia";
    case eCommunications: return "eCommunications";
    default: return "undefined";
  }
}

AudioDeviceListenerWin::AudioDeviceListenerWin(const base::Closure& listener_cb)
    : listener_cb_(listener_cb),
      tick_clock_(base::DefaultTickClock::GetInstance()) {
  // CreateDeviceEnumerator can fail on some installations of Windows such
  // as "Windows Server 2008 R2" where the desktop experience isn't available.
  Microsoft::WRL::ComPtr<IMMDeviceEnumerator> device_enumerator(
      CoreAudioUtil::CreateDeviceEnumerator());
  if (!device_enumerator.Get())
    return;

  HRESULT hr = device_enumerator->RegisterEndpointNotificationCallback(this);
  if (FAILED(hr)) {
    LOG(ERROR)  << "RegisterEndpointNotificationCallback failed: "
                << std::hex << hr;
    return;
  }

  device_enumerator_ = device_enumerator;
}

AudioDeviceListenerWin::~AudioDeviceListenerWin() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (device_enumerator_.Get()) {
    HRESULT hr =
        device_enumerator_->UnregisterEndpointNotificationCallback(this);
    LOG_IF(ERROR, FAILED(hr)) << "UnregisterEndpointNotificationCallback() "
                              << "failed: " << std::hex << hr;
  }
}

STDMETHODIMP_(ULONG) AudioDeviceListenerWin::AddRef() {
  return 1;
}

STDMETHODIMP_(ULONG) AudioDeviceListenerWin::Release() {
  return 1;
}

STDMETHODIMP AudioDeviceListenerWin::QueryInterface(REFIID iid, void** object) {
  if (iid == IID_IUnknown || iid == __uuidof(IMMNotificationClient)) {
    *object = static_cast<IMMNotificationClient*>(this);
    return S_OK;
  }

  *object = NULL;
  return E_NOINTERFACE;
}

STDMETHODIMP AudioDeviceListenerWin::OnPropertyValueChanged(
    LPCWSTR device_id, const PROPERTYKEY key) {
  // TODO(dalecurtis): We need to handle changes for the current default device
  // here.  It's tricky because this method may be called many (20+) times for
  // a single change like sample rate.  http://crbug.com/153056
  return S_OK;
}

STDMETHODIMP AudioDeviceListenerWin::OnDeviceAdded(LPCWSTR device_id) {
  // We don't care when devices are added.
  return S_OK;
}

STDMETHODIMP AudioDeviceListenerWin::OnDeviceRemoved(LPCWSTR device_id) {
  // We don't care when devices are removed.
  return S_OK;
}

STDMETHODIMP AudioDeviceListenerWin::OnDeviceStateChanged(LPCWSTR device_id,
                                                          DWORD new_state) {
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  if (monitor)
    monitor->ProcessDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO);

  return S_OK;
}

STDMETHODIMP AudioDeviceListenerWin::OnDefaultDeviceChanged(
    EDataFlow flow, ERole role, LPCWSTR new_default_device_id) {
  // Only listen for console and communication device changes.
  if ((role != eConsole && role != eCommunications) ||
      (flow != eRender && flow != eCapture)) {
    return S_OK;
  }

  // If no device is now available, |new_default_device_id| will be NULL.
  std::string new_device_id;
  if (new_default_device_id)
    new_device_id = base::WideToUTF8(new_default_device_id);

  // Only output device changes should be forwarded.  Do not attempt to filter
  // changes based on device id since some devices may not change their device
  // id and instead trigger some internal flow change: http://crbug.com/506712
  //
  // We rate limit device changes to avoid a single device change causing back
  // to back changes for eCommunications and eConsole; this is worth doing as
  // it provides a substantially faster resumption of playback.
  bool did_run_listener_cb = false;
  const base::TimeTicks now = tick_clock_->NowTicks();
  if (flow == eRender &&
      now - last_device_change_time_ >
          base::TimeDelta::FromMilliseconds(kDeviceChangeLimitMs)) {
    last_device_change_time_ = now;
    listener_cb_.Run();
    did_run_listener_cb = true;
  }

  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  if (monitor)
    monitor->ProcessDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO);

  DVLOG(1) << "OnDefaultDeviceChanged() "
           << "new_default_device: "
           << (new_default_device_id
                   ? CoreAudioUtil::GetFriendlyName(new_device_id, flow, role)
                   : "no device")
           << ", flow: " << FlowToString(flow)
           << ", role: " << RoleToString(role)
           << ", notified manager: " << (did_run_listener_cb ? "Yes" : "No");

  return S_OK;
}

}  // namespace media
