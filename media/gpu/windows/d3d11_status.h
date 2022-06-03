// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_STATUS_H_
#define MEDIA_GPU_WINDOWS_D3D11_STATUS_H_

#include "media/base/status.h"

namespace media {

enum class D3D11StatusCode : StatusCodeType {  // D3D11VideoDecoder Errors: 0x03
  kOk = 0x0,

  // TODO: these are returned by D3D11VideoDecoder::InitializeAcceleratedDecoder
  // but are also shared by non-d3d11 decoders.  The same fn returns specific
  // d3d11 errors too.  We could split them into a common error type and add a
  // d3d11-specific caused-by, i suppose.
  kDecoderInitializeNeverCompleted = 0x0101,
  kDecoderFailedDecode = 0x0102,
  kDecoderUnsupportedProfile = 0x0103,
  kDecoderUnsupportedCodec = 0x0104,
  kDecoderUnsupportedConfig = 0x0105,
  kDecoderCreationFailed = 0x010B,
  kDecoderVideoFrameConstructionFailed = 0x010D,
  kMakeContextCurrentFailed = 0x010E,

  kPostTextureFailed = 0x0301,
  kPostAcquireStreamFailed = 0x0302,
  kCreateEglStreamFailed = 0x0303,
  kCreateEglStreamConsumerFailed = 0x0304,
  kCreateEglStreamProducerFailed = 0x0305,
  kCreateTextureSelectorFailed = 0x0306,
  kQueryID3D11MultithreadFailed = 0x0307,
  kGetDecoderConfigCountFailed = 0x0308,
  kGetDecoderConfigFailed = 0x0309,
  kProcessTextureFailed = 0x030A,
  kUnsupportedTextureFormatForBind = 0x030B,
  kCreateDecoderOutputViewFailed = 0x030C,
  kAllocateTextureForCopyingWrapperFailed = 0x030D,
  kCreateDecoderOutputTextureFailed = 0x030E,
  kCreateVideoProcessorInputViewFailed = 0x030F,
  kVideoProcessorBltFailed = 0x0310,
  kCreateVideoProcessorOutputViewFailed = 0x0311,
  kCreateVideoProcessorEnumeratorFailed = 0x0312,
  kCreateVideoProcessorFailed = 0x0313,
  kQueryVideoContextFailed = 0x0314,
  kAcceleratorFlushFailed = 0x0315,
  kTryAgainNotSupported = 0x0316,
  kCryptoConfigFailed = 0x0317,
  kDecoderBeginFrameFailed = 0x0318,
  kReleaseDecoderBufferFailed = 0x0319,
  kGetPicParamBufferFailed = 0x0320,
  kReleasePicParamBufferFailed = 0x0321,
  kGetBitstreamBufferFailed = 0x0322,
  kReleaseBitstreamBufferFailed = 0x0323,
  kGetSliceControlBufferFailed = 0x0324,
  kReleaseSliceControlBufferFailed = 0x0325,
  kDecoderEndFrameFailed = 0x0326,
  kSubmitDecoderBuffersFailed = 0x0327,
  kGetQuantBufferFailed = 0x0328,
  kReleaseQuantBufferFailed = 0x0329,
  kBitstreamBufferSliceTooBig = 0x0330,
  kCreateSharedImageFailed = 0x0331,
  kGetKeyedMutexFailed = 0x0332,
  kAcquireKeyedMutexFailed = 0x0333,
  kReleaseKeyedMutexFailed = 0x0334,
  kCreateSharedHandleFailed = 0x0335,

  kCodeOnlyForTesting = std::numeric_limits<StatusCodeType>::max(),
};

struct D3D11StatusTraits {
  using Codes = D3D11StatusCode;
  static constexpr StatusGroupType Group() { return "D3D11StatusCode"; }
  static constexpr D3D11StatusCode DefaultEnumValue() {
    return D3D11StatusCode::kOk;
  }
};

using D3D11Status = TypedStatus<D3D11StatusTraits>;

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_STATUS_H_
