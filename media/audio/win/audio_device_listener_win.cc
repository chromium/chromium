// Copyright 2012 The Chromium Authors
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
  return flow == eRender ? "eRender" : "eCapture";
}

static std::string RoleToString(ERole role) {
  switch (role) {
    case eConsole:
      return "eConsole";
    case eMultimedia:
      return "eMultimedia";
    case eCommunications:
      return "eCommunications";
    default:
      return "undefined";
  }
}

AudioDeviceListenerWin::AudioDeviceListenerWin(
    base::RepeatingClosure listener_cb)
    : listener_cb_(std::move(listener_cb)),
      tick_clock_(base::DefaultTickClock::GetInstance()) {
  // CreateDeviceEnumerator can fail on some installations of Windows such
  // as "Windows Server 2008 R2" where the desktop experience isn't available.
  auto device_enumerator = CoreAudioUtil::CreateDeviceEnumerator();
  if (!device_enumerator) {
    DLOG(ERROR) << "Failed to create device enumeration.";
    return;
  }

  HRESULT hr = device_enumerator->RegisterEndpointNotificationCallback(this);
  if (FAILED(hr)) {
    DLOG(ERROR) << "RegisterEndpointNotificationCallback failed: " << std::hex
                << hr;
    return;
  }

  device_enumerator_ = device_enumerator;
}

AudioDeviceListenerWin::~AudioDeviceListenerWin() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!device_enumerator_)
    return;

  HRESULT hr = device_enumerator_->UnregisterEndpointNotificationCallback(this);
  DLOG_IF(ERROR, FAILED(hr)) << "UnregisterEndpointNotificationCallback() "
                             << "failed: " << std::hex << hr;
}

ULONG AudioDeviceListenerWin::AddRef() {
  return 1;
}

ULONG AudioDeviceListenerWin::Release() {
  return 1;
}

HRESULT AudioDeviceListenerWin::QueryInterface(REFIID iid, void** object) {
  if (iid == IID_IUnknown || iid == __uuidof(IMMNotificationClient)) {
    *object = static_cast<IMMNotificationClient*>(this);
    return S_OK;
  }

  *object = nullptr;
  return E_NOINTERFACE;
}

HRESULT AudioDeviceListenerWin::OnPropertyValueChanged(LPCWSTR device_id,
                                                       const PROPERTYKEY key) {
  // Property changes are handled by IAudioSessionControl listeners hung off of
  // each WASAPIAudioOutputStream() since not all property changes make it to
  // this method and those that do are spammed 10s of times.
  return S_OK;
}

HRESULT AudioDeviceListenerWin::OnDeviceAdded(LPCWSTR device_id) {
  // We don't care when devices are added.
  return S_OK;
}

HRESULT AudioDeviceListenerWin::OnDeviceRemoved(LPCWSTR device_id) {
  // We don't care when devices are removed.
  return S_OK;
}

HRESULT AudioDeviceListenerWin::OnDeviceStateChanged(LPCWSTR device_id,
                                                     DWORD new_state) {
  if (auto* monitor = base::SystemMonitor::Get())
    monitor->ProcessDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO);
  return S_OK;
}

HRESULT AudioDeviceListenerWin::OnDefaultDeviceChanged(
    EDataFlow flow,
    ERole role,
    LPCWSTR new_default_device_id) {
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
  if (flow == eRender && (now - last_device_change_time_ > kDeviceChangeLimit ||
                          new_device_id.compare(last_device_id_) != 0)) {
    last_device_change_time_ = now;
    last_device_id_ = new_device_id;
    listener_cb_.Run();
    did_run_listener_cb = true;
  }

  if (auto* monitor = base::SystemMonitor::Get())
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
