// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_DEFAULT_AUDIO_DEVICE_CHANGE_DETECTOR_H_
#define REMOTING_HOST_WIN_DEFAULT_AUDIO_DEVICE_CHANGE_DETECTOR_H_

#include <mmdeviceapi.h>
#include <wrl/client.h>

#include "base/synchronization/lock.h"

namespace remoting {

// An IMMNotificationClient implementation to detect the change of the default
// audio output device on the system. It registers itself into the input
// IMMDeviceEnumerator in constructor and unregisters in destructor.
// This class does not use the default ref-counting memory management method
// provided by IUnknown: calling DefaultAudioDeviceChangeDetector::Release()
// won't delete the object.
class DefaultAudioDeviceChangeDetector final : public IMMNotificationClient {
 public:
  explicit DefaultAudioDeviceChangeDetector(
      const Microsoft::WRL::ComPtr<IMMDeviceEnumerator>& enumerator);
  ~DefaultAudioDeviceChangeDetector();

  bool GetAndReset();

 private:
  // IMMNotificationClient implementation.
  HRESULT __stdcall OnDefaultDeviceChanged(EDataFlow flow,
                                           ERole role,
                                           LPCWSTR pwstrDefaultDevice) override;

  HRESULT __stdcall QueryInterface(REFIID iid, void** object) override;

  // No-ops overrides.
  HRESULT __stdcall OnDeviceAdded(LPCWSTR pwstrDeviceId) override;
  HRESULT __stdcall OnDeviceRemoved(LPCWSTR pwstrDeviceId) override;
  HRESULT __stdcall OnDeviceStateChanged(LPCWSTR pwstrDeviceId,
                                         DWORD dwNewState) override;
  HRESULT __stdcall OnPropertyValueChanged(LPCWSTR pwstrDeviceId,
                                           const PROPERTYKEY key) override;
  ULONG __stdcall AddRef() override;
  ULONG __stdcall Release() override;

  const Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator_;
  bool changed_ = false;
  base::Lock lock_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_DEFAULT_AUDIO_DEVICE_CHANGE_DETECTOR_H_
