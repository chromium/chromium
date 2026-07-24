// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_DEFAULT_AUDIO_DEVICE_CHANGE_DETECTOR_H_
#define REMOTING_HOST_WIN_DEFAULT_AUDIO_DEVICE_CHANGE_DETECTOR_H_

#include <mmdeviceapi.h>

#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/synchronization/lock.h"

namespace remoting {

// An IMMNotificationClient implementation to detect the change of the default
// audio output device on the system. It registers itself with the input
// IMMDeviceEnumerator in the constructor. The owner must call Unregister()
// before releasing its reference so that the enumerator releases its own
// reference to this object. Instances must be created with
// Microsoft::WRL::Make and held in a Microsoft::WRL::ComPtr so that the
// instance is kept alive while the system holds outstanding references.
class DefaultAudioDeviceChangeDetector final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IMMNotificationClient> {
 public:
  explicit DefaultAudioDeviceChangeDetector(
      const Microsoft::WRL::ComPtr<IMMDeviceEnumerator>& enumerator);

  DefaultAudioDeviceChangeDetector(const DefaultAudioDeviceChangeDetector&) =
      delete;
  DefaultAudioDeviceChangeDetector& operator=(
      const DefaultAudioDeviceChangeDetector&) = delete;

  ~DefaultAudioDeviceChangeDetector() override;

  // Unregisters this object from the IMMDeviceEnumerator. Must be called by
  // the owner before releasing its reference.
  void Unregister();

  bool GetAndReset();

 private:
  // IMMNotificationClient implementation.
  HRESULT __stdcall OnDefaultDeviceChanged(EDataFlow flow,
                                           ERole role,
                                           LPCWSTR pwstrDefaultDevice) override;

  // No-ops overrides.
  HRESULT __stdcall OnDeviceAdded(LPCWSTR pwstrDeviceId) override;
  HRESULT __stdcall OnDeviceRemoved(LPCWSTR pwstrDeviceId) override;
  HRESULT __stdcall OnDeviceStateChanged(LPCWSTR pwstrDeviceId,
                                         DWORD dwNewState) override;
  HRESULT __stdcall OnPropertyValueChanged(LPCWSTR pwstrDeviceId,
                                           const PROPERTYKEY key) override;

  const Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator_;
  bool registered_ = false;
  bool changed_ = false;
  base::Lock lock_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_DEFAULT_AUDIO_DEVICE_CHANGE_DETECTOR_H_
