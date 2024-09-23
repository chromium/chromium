// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_STATUS_H_
#define MEDIA_GPU_WINDOWS_D3D11_STATUS_H_

#include <wrl/client.h>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "media/base/status.h"

namespace media {

enum class D3D11StatusCode : StatusCodeType {
  kOk = 0,

  kFailedToGetAngleDevice = 1,
  kUnsupportedFeatureLevel = 2,
  kFailedToGetVideoDevice = 3,
  kFailedToGetDeviceContext = 4,
  kFailedToInitializeGPUProcess = 5,
  kDecoderFailedDecode = 6,
  kDecoderUnsupportedProfile = 7,
  kDecoderUnsupportedCodec = 8,
  kDecoderUnsupportedConfig = 9,
  kDecoderCreationFailed = 10,
  kMakeContextCurrentFailed = 11,
  kCreateTextureSelectorFailed = 12,
  kQueryID3D11MultithreadFailed = 13,
  kGetDecoderConfigCountFailed = 14,
  kGetDecoderConfigFailed = 15,
  kProcessTextureFailed = 16,
  kUnsupportedTextureFormatForBind = 17,
  kCreateDecoderOutputViewFailed = 18,
  kAllocateTextureForCopyingWrapperFailed = 19,
  kCreateDecoderOutputTextureFailed = 20,
  kCreateVideoProcessorInputViewFailed = 21,
  kVideoProcessorBltFailed = 22,
  kCreateVideoProcessorOutputViewFailed = 23,
  kCreateVideoProcessorFailed = 24,
  kQueryVideoContextFailed = 25,
  kAcceleratorFlushFailed = 26,
  kTryAgainNotSupported = 27,
  kCryptoConfigFailed = 28,
  kDecoderBeginFrameFailed = 29,
  kGetPicParamBufferFailed = 30,
  kReleasePicParamBufferFailed = 31,
  kGetBitstreamBufferFailed = 32,
  kReleaseBitstreamBufferFailed = 33,
  kGetSliceControlBufferFailed = 34,
  kReleaseSliceControlBufferFailed = 35,
  kDecoderEndFrameFailed = 36,
  kSubmitDecoderBuffersFailed = 37,
  kGetQuantBufferFailed = 38,
  kReleaseQuantBufferFailed = 39,
  kBitstreamBufferSliceTooBig = 40,
  kCreateSharedImageFailed = 41,
  kGetKeyedMutexFailed = 42,
  kAcquireKeyedMutexFailed = 43,
  kReleaseKeyedMutexFailed = 44,
  kCreateSharedHandleFailed = 45,
  kProduceVideoDecodeImageRepresentationFailed = 46,
  kVideoDecodeImageRepresentationBeginScopedWriteAccessFailed = 47,
  kGetCommandBufferHelperFailed = 48,
  kDecoderGetCreationParametersFailed = 49,
  kGetDeviceFailed = 50,
  kCreateFenceFailed = 51,
  kFenceSignalFailed = 52,
  kWaitForFenceFailed = 53,
};

struct D3D11StatusTraits {
  using Codes = D3D11StatusCode;
  static constexpr StatusGroupType Group() { return "D3D11Status"; }

  static void OnCreateFrom(TypedStatus<D3D11StatusTraits>* s, HRESULT hresult) {
    // Store it as a string for easy human consumption.
    std::stringstream hresult_str_repr;
    hresult_str_repr << std::hex << hresult;
    s->WithData("hresult", hresult_str_repr.str());

    // Store it as an integer for easy machine consumption.
    s->WithData("hresult_raw", static_cast<int32_t>(hresult));

    // Store the system error that might have been generated, if it's an
    // allowable string.
    std::string sys_err = logging::SystemErrorCodeToString(hresult);
    if (base::IsStringUTF8AllowingNoncharacters(sys_err))
      s->WithData("hresult_msg", sys_err);
  }
};

using D3D11Status = TypedStatus<D3D11StatusTraits>;

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_STATUS_H_
