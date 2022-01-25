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
