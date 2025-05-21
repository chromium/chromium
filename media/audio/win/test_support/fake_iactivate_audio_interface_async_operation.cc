// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/test_support/fake_iactivate_audio_interface_async_operation.h"

#include <mmdeviceapi.h>
#include <windows.h>

#include <audioclient.h>
#include <wrl.h>

#include "media/audio/win/test_support/fake_iaudio_client.h"
#include "media/audio/win/test_support/fake_win_wasapi_environment.h"

namespace media {

IFACEMETHODIMP FakeIActivateAudioInterfaceAsyncOperation::GetActivateResult(
    HRESULT* activateResult,
    IUnknown** activatedInterface) {
  if (FakeWinWASAPIEnvironment::GetError() ==
      WASAPITestErrorCode::kAudioClientActivationAsyncOperationFailed) {
    return E_ILLEGAL_METHOD_CALL;
  }

  if (FakeWinWASAPIEnvironment::GetError() ==
      WASAPITestErrorCode::kAudioClientActivationFailed) {
    *activateResult = E_OUTOFMEMORY;
    return S_OK;
  }

  Microsoft::WRL::ComPtr<IAudioClient> audio_client =
      Microsoft::WRL::Make<FakeIAudioClient>(
          FakeIAudioClient::ClientType::kProcessLoopbackDevice);
  *activateResult = S_OK;
  *activatedInterface = audio_client.Detach();
  return S_OK;
}

}  // namespace media
