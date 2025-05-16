// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_WIN_TEST_SUPPORT_FAKE_WIN_WASAPI_ENVIRONMENT_H_
#define MEDIA_AUDIO_WIN_TEST_SUPPORT_FAKE_WIN_WASAPI_ENVIRONMENT_H_

#include <mmdeviceapi.h>
#include <windows.h>

#include "base/win/windows_types.h"
#include "media/audio/win/test_support/wasapi_test_error_code.h"

namespace media {

// This class is meant to mock the behavior of the WASAPI environment for
// testing. It does that by simulating platform errors and providing fake
// implementations of Windows APIs that return fake Windows interfaces.
class FakeWinWASAPIEnvironment {
 public:
  FakeWinWASAPIEnvironment();
  FakeWinWASAPIEnvironment(const FakeWinWASAPIEnvironment&) = delete;
  FakeWinWASAPIEnvironment& operator=(const FakeWinWASAPIEnvironment&) = delete;
  ~FakeWinWASAPIEnvironment();

  static HRESULT ActivateAudioInterfaceAsync(
      LPCWSTR deviceInterfacePath,
      REFIID riid,
      PROPVARIANT* activationParams,
      IActivateAudioInterfaceCompletionHandler* completionHandler,
      IActivateAudioInterfaceAsyncOperation** activationOperation);

  void SimulateError(WASAPITestErrorCode error) { error_ = error; }

  static WASAPITestErrorCode GetError() { return error_; }

 private:
  static WASAPITestErrorCode error_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_WIN_TEST_SUPPORT_FAKE_WIN_WASAPI_ENVIRONMENT_H_
