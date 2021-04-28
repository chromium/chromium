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

using StatusCodeType = int32_t;
// TODO(tmathmeyer, liberato, xhwang) These numbers are not yet finalized:
// DO NOT use them for reporting statistics, and DO NOT report them to any
// user-facing feature, including media log.

// Codes are grouped with a bitmask:
// 0xFFFFFFFF
//   └─┬┘├┘└┴ enumeration within the group
//     │ └─ group code
//     └─ reserved for now
// 256 groups is more than anyone will ever need on a computer.
enum class StatusCode : StatusCodeType {
  kOk = 0,

  // General errors: 0x00
  kAborted = 0x00000001,
  kInvalidArgument = 0x00000002,

  // Decoder Errors: 0x01
  kDecoderInitializeNeverCompleted = 0x00000101,
  kDecoderFailedDecode = 0x00000102,
  kDecoderUnsupportedProfile = 0x00000103,
  kDecoderUnsupportedCodec = 0x00000104,
  kDecoderUnsupportedConfig = 0x00000105,
  kEncryptedContentUnsupported = 0x00000106,
  kClearContentUnsupported = 0x00000107,
  kDecoderMissingCdmForEncryptedContent = 0x00000108,
  kDecoderInitializationFailed = 0x00000109,  // Prefer this one.
  kDecoderFailedInitialization = kDecoderInitializationFailed,  // Do not use.
  kDecoderCantChangeCodec = 0x0000010A,
  kDecoderCreationFailed = 0x0000010B,              // Prefer this one.
  kDecoderFailedCreation = kDecoderCreationFailed,  // Do not use.
  kInitializationUnspecifiedFailure = 0x0000010C,
  kDecoderVideoFrameConstructionFailed = 0x0000010D,
  kMakeContextCurrentFailed = 0x0000010E,
  // This is a temporary error for use only by existing code during the
  // DecodeStatus => Status conversion.
  kDecodeErrorDoNotUse = 0x0000010F,

  // Windows Errors: 0x02
  kWindowsWrappedHresult = 0x00000201,
  kWindowsApiNotAvailible = 0x00000202,
  kWindowsD3D11Error = 0x00000203,

  // D3D11VideoDecoder Errors: 0x03
  kPostTextureFailed = 0x00000301,
  kPostAcquireStreamFailed = 0x00000302,
  kCreateEglStreamFailed = 0x00000303,
  kCreateEglStreamConsumerFailed = 0x00000304,
  kCreateEglStreamProducerFailed = 0x00000305,
  kCreateTextureSelectorFailed = 0x00000306,
  kQueryID3D11MultithreadFailed = 0x00000307,
  kGetDecoderConfigCountFailed = 0x00000308,
  kGetDecoderConfigFailed = 0x00000309,
  kProcessTextureFailed = 0x0000030A,
  kUnsupportedTextureFormatForBind = 0x0000030B,
  kCreateDecoderOutputViewFailed = 0x0000030C,
  kAllocateTextureForCopyingWrapperFailed = 0x0000030D,
  kCreateDecoderOutputTextureFailed = 0x0000030E,
  kCreateVideoProcessorInputViewFailed = 0x0000030F,
  kVideoProcessorBltFailed = 0x00000310,
  kCreateVideoProcessorOutputViewFailed = 0x00000311,
  kCreateVideoProcessorEnumeratorFailed = 0x00000312,
  kCreateVideoProcessorFailed = 0x00000313,
  kQueryVideoContextFailed = 0x00000314,
  kAcceleratorFlushFailed = 0x00000315,
  kTryAgainNotSupported = 0x00000316,
  kCryptoConfigFailed = 0x00000317,
  kDecoderBeginFrameFailed = 0x00000318,
  kReleaseDecoderBufferFailed = 0x00000319,
  kGetPicParamBufferFailed = 0x00000320,
  kReleasePicParamBufferFailed = 0x00000321,
  kGetBitstreamBufferFailed = 0x00000322,
  kReleaseBitstreamBufferFailed = 0x00000323,
  kGetSliceControlBufferFailed = 0x00000324,
  kReleaseSliceControlBufferFailed = 0x00000325,
  kDecoderEndFrameFailed = 0x00000326,
  kSubmitDecoderBuffersFailed = 0x00000327,
  kGetQuantBufferFailed = 0x00000328,
  kReleaseQuantBufferFailed = 0x00000329,
  kBitstreamBufferSliceTooBig = 0x00000330,

  // MojoDecoder Errors: 0x04
  kMojoDecoderNoWrappedDecoder = 0x00000401,
  kMojoDecoderStoppedBeforeInitDone = 0x00000402,
  kMojoDecoderUnsupported = 0x00000403,
  kMojoDecoderNoConnection = 0x00000404,
  kMojoDecoderDeletedWithoutInitialization = 0x00000405,

  // Chromeos Errors: 0x05
  kChromeOSVideoDecoderNoDecoders = 0x00000501,
  kV4l2NoDevice = 0x00000502,
  kV4l2FailedToStopStreamQueue = 0x00000503,
  kV4l2NoDecoder = 0x00000504,
  kV4l2FailedFileCapabilitiesCheck = 0x00000505,
  kV4l2FailedResourceAllocation = 0x00000506,
  kV4l2BadFormat = 0x00000507,
  kV4L2FailedToStartStreamQueue = 0x00000508,
  kVaapiReinitializedDuringDecode = 0x00000509,
  kVaapiFailedAcceleratorCreation = 0x00000510,

  // Encoder Error: 0x06
  kEncoderInitializeNeverCompleted = 0x00000601,
  kEncoderInitializeTwice = 0x00000602,
  kEncoderFailedEncode = 0x00000603,
  kEncoderUnsupportedProfile = 0x00000604,
  kEncoderUnsupportedCodec = 0x00000605,
  kEncoderUnsupportedConfig = 0x00000606,
  kEncoderInitializationError = 0x00000607,
  kEncoderFailedFlush = 0x00000608,

  // VaapiVideoDecoder: 0x07
  kVaapiBadContext = 0x00000701,
  kVaapiNoBuffer = 0x00000702,
  kVaapiNoBufferHandle = 0x00000703,
  kVaapiNoPixmap = 0x00000704,
  kVaapiNoImage = 0x00000705,
  kVaapiNoSurface = 0x00000706,
  kVaapiFailedToInitializeImage = 0x00000707,
  kVaapiFailedToBindTexture = 0x00000708,
  kVaapiFailedToBindImage = 0x00000709,
  kVaapiUnsupportedFormat = 0x0000070A,
  kVaapiFailedToExportImage = 0x0000070B,
  kVaapiBadImageSize = 0x0000070C,
  kVaapiNoTexture = 0x0000070D,

  // Format Errors: 0x08
  kH264ParsingError = 0x00000801,
  kH264BufferTooSmall = 0x00000802,

  // Pipeline Errors: 0x09
  // Deprecated: kPipelineErrorUrlNotFound = 0x00000901,
  kPipelineErrorNetwork = 0x00000902,
  kPipelineErrorDecode = 0x00000903,
  // Deprecated: kPipelineErrorDecrypt = 0x00000904,
  kPipelineErrorAbort = 0x00000905,
  kPipelineErrorInitializationFailed = 0x00000906,
  // Unused: 0x00000907
  kPipelineErrorCouldNotRender = 0x00000908,
  kPipelineErrorRead = 0x00000909,
  // Deprecated: kPipelineErrorOperationPending = 0x0000090a,
  kPipelineErrorInvalidState = 0x0000090b,
  // Demuxer related errors.
  kPipelineErrorDemuxerErrorCouldNotOpen = 0x0000090c,
  kPipelineErrorDemuxerErrorCouldNotParse = 0x0000090d,
  kPipelineErrorDemuxerErrorNoSupportedStreams = 0x0000090e,
  // Decoder related errors.
  kPipelineErrorDecoderErrorNotSupported = 0x0000090f,
  // ChunkDemuxer related errors.
  kPipelineErrorChuckDemuxerErrorAppendFailed = 0x00000910,
  kPipelineErrorChunkDemuxerErrorEosStatusDecodeError = 0x00000911,
  kPipelineErrorChunkDemuxerErrorEosStatusNetworkError = 0x00000912,
  // Audio rendering errors.
  kPipelineErrorAudioRendererError = 0x00000913,
  // Deprecated: kPipelineErrorAudioRendererErrorSpliceFailed = 0x00000914,
  kPipelineErrorExternalRendererFailed = 0x00000915,
  // Android only. Used as a signal to fallback MediaPlayerRenderer, and thus
  // not exactly an 'error' per say.
  kPipelineErrorDemuxerErrorDetectedHLS = 0x00000916,

  // Frame operation errors: 0x0A
  kUnsupportedFrameFormatError = 0x00000A01,

  // DecoderStream errors: 0x0B
  kDecoderStreamInErrorState = 0x00000B00,
  kDecoderStreamReinitFailed = 0x00000B01,
  // This is a temporary error for use while the demuxer doesn't return a
  // proper status.
  kDecoderStreamDemuxerError = 0x00000B02,

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
  kGenericErrorPleaseRemove = 0x79999999,
  kCodeOnlyForTesting = std::numeric_limits<StatusCodeType>::max(),
  kMaxValue = kCodeOnlyForTesting,
};

MEDIA_EXPORT std::ostream& operator<<(std::ostream& os, const StatusCode& code);

}  // namespace media

#endif  // MEDIA_BASE_STATUS_CODES_H_
