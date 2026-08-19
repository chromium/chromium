// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_WIN_TEST_SUPPORT_WASAPI_TEST_ERROR_CODE_H_
#define MEDIA_AUDIO_WIN_TEST_SUPPORT_WASAPI_TEST_ERROR_CODE_H_

namespace media {

enum class WASAPITestErrorCode {
  kOk,
  kActivateAudioInterfaceAsyncFailed,
  kAudioClientActivationTimeout,
  kAudioClientActivationAsyncOperationFailed,
  kAudioClientActivationFailed,
  kAudioClientGetBufferSizeFailed,
};

}  // namespace media

#endif  // MEDIA_AUDIO_WIN_TEST_SUPPORT_WASAPI_TEST_ERROR_CODE_H_
