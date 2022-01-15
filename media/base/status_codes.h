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
  kWrappedError = 0x0004,

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
  // The remote media component was disconnected unexpectedly, e.g. crash.
  kPipelineErrorDisconnected = 0x0918,

  // Frame operation errors: 0x0A
  kUnsupportedFrameFormatError = 0x0A01,

  // DecoderStream errors: 0x0B
  kDecoderStreamInErrorState = 0x0B00,
  kDecoderStreamReinitFailed = 0x0B01,
  // This is a temporary error for use while the demuxer doesn't return a
  // proper status.
  kDecoderStreamDemuxerError = 0x0B02,

  // Special codes
  kGenericErrorPleaseRemove = 0x7999,
  kCodeOnlyForTesting = std::numeric_limits<StatusCodeType>::max(),
  kMaxValue = kCodeOnlyForTesting,
};

MEDIA_EXPORT std::ostream& operator<<(std::ostream& os, const StatusCode& code);

}  // namespace media

#endif  // MEDIA_BASE_STATUS_CODES_H_
