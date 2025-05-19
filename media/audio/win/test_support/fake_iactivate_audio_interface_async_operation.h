// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_WIN_TEST_SUPPORT_FAKE_IACTIVATE_AUDIO_INTERFACE_ASYNC_OPERATION_H_
#define MEDIA_AUDIO_WIN_TEST_SUPPORT_FAKE_IACTIVATE_AUDIO_INTERFACE_ASYNC_OPERATION_H_

#include <mmdeviceapi.h>
#include <windows.h>

#include <wrl.h>

namespace media {

// FakeIActivateAudioInterfaceAsyncOperation is a mock implementation of the
// IActivateAudioInterfaceAsyncOperation interface. It is used for testing
// purposes and simulates the behavior of an audio interface activation
// operation without requiring actual audio hardware or a real WASAPI
// environment.
class FakeIActivateAudioInterfaceAsyncOperation
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          Microsoft::WRL::FtmBase,
          IActivateAudioInterfaceAsyncOperation> {
 public:
  FakeIActivateAudioInterfaceAsyncOperation() = default;
  ~FakeIActivateAudioInterfaceAsyncOperation() override = default;

  // IActivateAudioInterfaceAsyncOperation methods
  IFACEMETHODIMP(GetActivateResult)(HRESULT* activateResult,
                                    IUnknown** activatedInterface) override;
};

}  // namespace media

#endif  // MEDIA_AUDIO_WIN_TEST_SUPPORT_FAKE_IACTIVATE_AUDIO_INTERFACE_ASYNC_OPERATION_H_
