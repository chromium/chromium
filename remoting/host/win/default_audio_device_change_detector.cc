// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/default_audio_device_change_detector.h"

#include <unknwn.h>

#include "base/logging.h"

namespace remoting {

DefaultAudioDeviceChangeDetector::DefaultAudioDeviceChangeDetector(
    const Microsoft::WRL::ComPtr<IMMDeviceEnumerator>& enumerator)
    : enumerator_(enumerator) {
  DCHECK(enumerator_);
  HRESULT hr = enumerator_->RegisterEndpointNotificationCallback(this);
  if (FAILED(hr)) {
    // We cannot predict which kind of error the API may return, but this is not
    // a fatal error.
    LOG(WARNING) << "Failed to register IMMNotificationClient, we may not be "
                    "able to detect the new default audio device. Error "
                 << hr;
  }
}

DefaultAudioDeviceChangeDetector::~DefaultAudioDeviceChangeDetector() {
  enumerator_->UnregisterEndpointNotificationCallback(this);
}

bool DefaultAudioDeviceChangeDetector::GetAndReset() {
  bool result = false;
  {
    base::AutoLock lock(lock_);
    result = changed_;
    changed_ = false;
  }
  return result;
}

HRESULT DefaultAudioDeviceChangeDetector::OnDefaultDeviceChanged(
    EDataFlow flow,
    ERole role,
    LPCWSTR pwstrDefaultDevice) {
  {
    base::AutoLock lock(lock_);
    changed_ = true;
  }
  return S_OK;
}

HRESULT DefaultAudioDeviceChangeDetector::QueryInterface(REFIID iid,
                                                         void** object) {
  if (iid == IID_IUnknown || iid == __uuidof(IMMNotificationClient)) {
    *object = static_cast<IMMNotificationClient*>(this);
    return S_OK;
  }
  *object = nullptr;
  return E_NOINTERFACE;
}

HRESULT DefaultAudioDeviceChangeDetector::OnDeviceAdded(LPCWSTR pwstrDeviceId) {
  return S_OK;
}

HRESULT DefaultAudioDeviceChangeDetector::OnDeviceRemoved(
    LPCWSTR pwstrDeviceId) {
  return S_OK;
}

HRESULT DefaultAudioDeviceChangeDetector::OnDeviceStateChanged(
    LPCWSTR pwstrDeviceId,
    DWORD dwNewState) {
  return S_OK;
}

HRESULT DefaultAudioDeviceChangeDetector::OnPropertyValueChanged(
    LPCWSTR pwstrDeviceId,
    const PROPERTYKEY key) {
  return S_OK;
}

ULONG DefaultAudioDeviceChangeDetector::AddRef() {
  return 1;
}

ULONG DefaultAudioDeviceChangeDetector::Release() {
  return 1;
}

}  // namespace remoting
