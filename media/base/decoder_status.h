// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DECODER_STATUS_H_
#define MEDIA_BASE_DECODER_STATUS_H_

#include <ostream>

#include "base/time/time.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_export.h"
#include "media/base/status.h"

namespace media {

struct DecoderStatusTraits {
  enum class Codes : StatusCodeType {
    // Shared & General errors
    kOk = 0,
    kFailed = 1,
    kAborted = 2,  // TODO(*) document _why_ aborted is a thing
    kInvalidArgument = 3,
    kInterrupted = 4,
    kDisconnected = 5,  // Lost mojo connection, e.g remote crashed or teardown

    // Reasons for failing to decode
    kNotInitialized = 100,
    kMissingCDM = 101,
    kFailedToGetVideoFrame = 102,
    kPlatformDecodeFailure = 103,
    kMalformedBitstream = 104,
    kFailedToGetDecoderBuffer = 107,
    kDecoderStreamInErrorState = 108,
    kDecoderStreamDemuxerError = 110,
    kKeyFrameRequired = 111,
    kMissingTimestamp = 112,

    // Reasons for failing to initialize
    kUnsupportedProfile = 200,
    kUnsupportedCodec = 201,
    kUnsupportedConfig = 202,
    kUnsupportedEncryptionMode = 203,
    kCantChangeCodec = 204,
    kFailedToCreateDecoder = 205,
    kTooManyDecoders = 206,
    kMediaFoundationNotAvailable = 207,
  };
  static constexpr StatusGroupType Group() { return "DecoderStatus"; }
};

using DecoderStatus = TypedStatus<DecoderStatusTraits>;

MEDIA_EXPORT std::ostream& operator<<(std::ostream& os,
                                      const DecoderStatus& status);

// Helper class for ensuring that Decode() traces are properly unique and closed
// if the Decode is aborted via a WeakPtr invalidation. We use the |this|
// pointer of the ScopedDecodeTrace object itself as the id. Since the callback
// owns the class it's guaranteed to be unique.
class MEDIA_EXPORT ScopedDecodeTrace {
 public:
  // Begins an asynchronous trace with the given name and properties. Providing
  // the DecoderBuffer itself yields the most information in the trace.
  ScopedDecodeTrace(const char* trace_name, const DecoderBuffer& buffer);
  ScopedDecodeTrace(const char* trace_name,
                    bool is_key_frame,
                    base::TimeDelta timestamp);

  ScopedDecodeTrace(const ScopedDecodeTrace&) = delete;
  ScopedDecodeTrace& operator=(const ScopedDecodeTrace&) = delete;

  ~ScopedDecodeTrace();

  // Completes the Decode() trace with the given status.
  void EndTrace(const DecoderStatus& status);

 private:
  const char* trace_name_;
  bool closed_ = false;
};

}  // namespace media

#endif  // MEDIA_BASE_DECODER_STATUS_H_
