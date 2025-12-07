// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/test_support/fake_win_wasapi_environment.h"

#include <mmdeviceapi.h>
#include <windows.h>

#include <wrl.h>

#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "base/win/windows_types.h"
#include "media/audio/win/audio_low_latency_input_win.h"
#include "media/audio/win/test_support/fake_iactivate_audio_interface_async_operation.h"
#include "media/audio/win/test_support/fake_iaudio_client.h"

namespace media {

namespace {

void ActivateAudioInterface(
    IActivateAudioInterfaceCompletionHandler* completionHandler,
    IActivateAudioInterfaceAsyncOperation* activationOperation) {
  activationOperation =
      Microsoft::WRL::Make<FakeIActivateAudioInterfaceAsyncOperation>()
          .Detach();
  completionHandler->ActivateCompleted(activationOperation);
}

}  // namespace

// static
WASAPITestErrorCode FakeWinWASAPIEnvironment::error_ = WASAPITestErrorCode::kOk;

FakeWinWASAPIEnvironment::FakeWinWASAPIEnvironment() {
  // Set the error code to the default success value.
  error_ = WASAPITestErrorCode::kOk;

  // Override the callback for ActivateAudioInterfaceAsync to use our fake
  // implementation.
  WASAPIAudioInputStream::OverrideActivateAudioInterfaceAsyncCallbackForTesting(
      base::BindRepeating(&ActivateAudioInterfaceAsync));
}

FakeWinWASAPIEnvironment::~FakeWinWASAPIEnvironment() {
  // Reset the error code to the default success value.
  error_ = WASAPITestErrorCode::kOk;

  // Restore the original callback for ActivateAudioInterfaceAsync.
  WASAPIAudioInputStream::OverrideActivateAudioInterfaceAsyncCallbackForTesting(
      base::BindRepeating(&ActivateAudioInterfaceAsync));
}

// static
HRESULT FakeWinWASAPIEnvironment::ActivateAudioInterfaceAsync(
    LPCWSTR deviceInterfacePath,
    REFIID riid,
    PROPVARIANT* activationParams,
    IActivateAudioInterfaceCompletionHandler* completionHandler,
    IActivateAudioInterfaceAsyncOperation** activationOperation) {
  if (error_ == WASAPITestErrorCode::kActivateAudioInterfaceAsyncFailed) {
    return E_ILLEGAL_METHOD_CALL;
  }

  // COM interfaces are refcounted, so we need to wrap the pointers in ComPtr to
  // ensure that they are properly managed by the callback.
  Microsoft::WRL::ComPtr<IActivateAudioInterfaceCompletionHandler>
      completionHandlerPtr(completionHandler);
  Microsoft::WRL::ComPtr<IActivateAudioInterfaceAsyncOperation>
      activationOperationPtr(*activationOperation);
  auto activate_audio_interface_callback = base::BindOnce(
      &ActivateAudioInterface, completionHandlerPtr, activationOperationPtr);

  if (FakeWinWASAPIEnvironment::GetError() ==
      WASAPITestErrorCode::kAudioClientActivationTimeout) {
    base::ThreadPool::PostDelayedTask(
        FROM_HERE, std::move(activate_audio_interface_callback),
        base::Milliseconds(15000));
    return S_OK;
  }

  base::ThreadPool::PostTask(FROM_HERE,
                             std::move(activate_audio_interface_callback));
  return S_OK;
}

}  // namespace media
