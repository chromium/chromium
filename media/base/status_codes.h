// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_STATUS_CODES_H_
#define MEDIA_BASE_STATUS_CODES_H_

#include <cstdint>
#include <limits>
#include <ostream>

#include "media/base/media_export.h"

namespace media {

using StatusCodeType = uint16_t;
// TODO(tmathmeyer, liberato, xhwang) These numbers are not yet finalized:
// DO NOT use them for reporting statistics, and DO NOT report them to any
// user-facing feature, including media log.

// Codes are grouped with a bitmask:
// 0xFFFF
//   ├┘└┴ enumeration within the group
//   └─ group code
// 256 groups is more than anyone will ever need on a computer.
enum class StatusCode : StatusCodeType {
  kOk = 0,

  // General errors: 0x00
  kAborted = 0x0001,
  kInvalidArgument = 0x0002,
  kKeyFrameRequired = 0x0003,

  // Decoder Errors: 0x01
  kDecoderInitializeNeverCompleted = 0x0101,
  kDecoderFailedDecode = 0x0102,
  kDecoderUnsupportedProfile = 0x0103,
  kDecoderUnsupportedCodec = 0x0104,
  kDecoderUnsupportedConfig = 0x0105,
  kEncryptedContentUnsupported = 0x0106,
  kClearContentUnsupported = 0x0107,
  kDecoderMissingCdmForEncryptedContent = 0x0108,
  kDecoderInitializationFailed = 0x0109,  // Prefer this one.
  kDecoderFailedInitialization = kDecoderInitializationFailed,  // Do not use.
  kDecoderCantChangeCodec = 0x010A,
  kDecoderCreationFailed = 0x010B,                  // Prefer this one.
  kDecoderFailedCreation = kDecoderCreationFailed,  // Do not use.
  kInitializationUnspecifiedFailure = 0x010C,
  kDecoderVideoFrameConstructionFailed = 0x010D,
  kMakeContextCurrentFailed = 0x010E,
  // This is a temporary error for use only by existing code during the
  // DecodeStatus => Status conversion.
  kDecodeErrorDoNotUse = 0x010F,

  // Windows Errors: 0x02
  kWindowsWrappedHresult = 0x0201,
  kWindowsApiNotAvailible = 0x0202,
  kWindowsD3D11Error = 0x0203,

  // D3D11VideoDecoder Errors: 0x03
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

  // MojoDecoder Errors: 0x04
  kMojoDecoderNoWrappedDecoder = 0x0401,
  kMojoDecoderStoppedBeforeInitDone = 0x0402,
  kMojoDecoderUnsupported = 0x0403,
  kMojoDecoderNoConnection = 0x0404,
  kMojoDecoderDeletedWithoutInitialization = 0x0405,

  // Chromeos Errors: 0x05
  kChromeOSVideoDecoderNoDecoders = 0x0501,
  kV4l2NoDevice = 0x0502,
  kV4l2FailedToStopStreamQueue = 0x0503,
  kV4l2NoDecoder = 0x0504,
  kV4l2FailedFileCapabilitiesCheck = 0x0505,
  kV4l2FailedResourceAllocation = 0x0506,
  kV4l2BadFormat = 0x0507,
  kV4L2FailedToStartStreamQueue = 0x0508,
  kVaapiReinitializedDuringDecode = 0x0509,
  kVaapiFailedAcceleratorCreation = 0x0510,

  // Encoder Error: 0x06
  kEncoderInitializeNeverCompleted = 0x0601,
  kEncoderInitializeTwice = 0x0602,
  kEncoderFailedEncode = 0x0603,
  kEncoderUnsupportedProfile = 0x0604,
  kEncoderUnsupportedCodec = 0x0605,
  kEncoderUnsupportedConfig = 0x0606,
  kEncoderInitializationError = 0x0607,
  kEncoderFailedFlush = 0x0608,

  // VaapiVideoDecoder: 0x07
  kVaapiBadContext = 0x0701,
  kVaapiNoBuffer = 0x0702,
  kVaapiNoBufferHandle = 0x0703,
  kVaapiNoPixmap = 0x0704,
  kVaapiNoImage = 0x0705,
  kVaapiNoSurface = 0x0706,
  kVaapiFailedToInitializeImage = 0x0707,
  kVaapiFailedToBindTexture = 0x0708,
  kVaapiFailedToBindImage = 0x0709,
  kVaapiUnsupportedFormat = 0x070A,
  kVaapiFailedToExportImage = 0x070B,
  kVaapiBadImageSize = 0x070C,
  kVaapiNoTexture = 0x070D,

  // Format Errors: 0x08
  kH264ParsingError = 0x0801,
  kH264BufferTooSmall = 0x0802,

  // Pipeline Errors: 0x09
  // Deprecated: kPipelineErrorUrlNotFound = 0x0901,
  kPipelineErrorNetwork = 0x0902,
  kPipelineErrorDecode = 0x0903,
  // Deprecated: kPipelineErrorDecrypt = 0x0904,
  kPipelineErrorAbort = 0x0905,
  kPipelineErrorInitializationFailed = 0x0906,
  // Unused: 0x0907
  kPipelineErrorCouldNotRender = 0x0908,
  kPipelineErrorRead = 0x0909,
  // Deprecated: kPipelineErrorOperationPending = 0x090a,
  kPipelineErrorInvalidState = 0x090b,
  // Demuxer related errors.
  kPipelineErrorDemuxerErrorCouldNotOpen = 0x090c,
  kPipelineErrorDemuxerErrorCouldNotParse = 0x090d,
  kPipelineErrorDemuxerErrorNoSupportedStreams = 0x090e,
  // Decoder related errors.
  kPipelineErrorDecoderErrorNotSupported = 0x090f,
  // ChunkDemuxer related errors.
  kPipelineErrorChuckDemuxerErrorAppendFailed = 0x0910,
  kPipelineErrorChunkDemuxerErrorEosStatusDecodeError = 0x0911,
  kPipelineErrorChunkDemuxerErrorEosStatusNetworkError = 0x0912,
  // Audio rendering errors.
  kPipelineErrorAudioRendererError = 0x0913,
  // Deprecated: kPipelineErrorAudioRendererErrorSpliceFailed = 0x0914,
  kPipelineErrorExternalRendererFailed = 0x0915,
  // Android only. Used as a signal to fallback MediaPlayerRenderer, and thus
  // not exactly an 'error' per say.
  kPipelineErrorDemuxerErrorDetectedHLS = 0x0916,
  // Used when hardware context is reset (e.g. OS sleep/resume), where we should
  // recreate the Renderer instead of fail the playback. See
  // https://crbug.com/1208618
  kPipelineErrorHardwareContextReset = 0x0917,

  // Frame operation errors: 0x0A
  kUnsupportedFrameFormatError = 0x0A01,

  // DecoderStream errors: 0x0B
  kDecoderStreamInErrorState = 0x0B00,
  kDecoderStreamReinitFailed = 0x0B01,
  // This is a temporary error for use while the demuxer doesn't return a
  // proper status.
  kDecoderStreamDemuxerError = 0x0B02,

  // DecodeStatus temporary codes.  These names were chosen to match the
  // DecodeStatus enum, so that un-converted code can DecodeStatus::OK/etc.
  // Note that OK must result in Status::is_ok(), since converted code will
  // check for it.  These will be removed when the conversion is complete.
  //
  // DO NOT ADD NEW USES OF OK/ABORTED/DECODE_ERROR.
  OK = kOk,  // Everything went as planned.
  // Read aborted due to Reset() during pending read.
  ABORTED = kAborted,  // Read aborted due to Reset() during pending read.
  // Decoder returned decode error. Note: Prefixed by DECODE_
  // since ERROR is a reserved name (special macro) on Windows.
  DECODE_ERROR = kDecodeErrorDoNotUse,

  // Special codes
  kGenericErrorPleaseRemove = 0x7999,
  kCodeOnlyForTesting = std::numeric_limits<StatusCodeType>::max(),
  kMaxValue = kCodeOnlyForTesting,
};

MEDIA_EXPORT std::ostream& operator<<(std::ostream& os, const StatusCode& code);

}  // namespace media

#endif  // MEDIA_BASE_STATUS_CODES_H_
