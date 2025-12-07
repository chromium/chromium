// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decoder_status.h"

#include <sstream>
#include <string>

#include "base/trace_event/trace_event.h"
#include "media/base/status.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace media {
namespace {

const std::string GetDecodeStatusString(const DecoderStatus& status) {
#define STRINGIFY(V) \
  case V:            \
    return #V
  switch (status.code()) {
    STRINGIFY(DecoderStatus::Codes::kOk);
    STRINGIFY(DecoderStatus::Codes::kFailed);
    STRINGIFY(DecoderStatus::Codes::kAborted);
    STRINGIFY(DecoderStatus::Codes::kInvalidArgument);
    STRINGIFY(DecoderStatus::Codes::kInterrupted);
    STRINGIFY(DecoderStatus::Codes::kDisconnected);
    STRINGIFY(DecoderStatus::Codes::kOutOfMemory);
    STRINGIFY(DecoderStatus::Codes::kNotInitialized);
    STRINGIFY(DecoderStatus::Codes::kMissingCDM);
    STRINGIFY(DecoderStatus::Codes::kFailedToGetVideoFrame);
    STRINGIFY(DecoderStatus::Codes::kPlatformDecodeFailure);
    STRINGIFY(DecoderStatus::Codes::kMalformedBitstream);
    STRINGIFY(DecoderStatus::Codes::kFailedToGetDecoderBuffer);
    STRINGIFY(DecoderStatus::Codes::kDecoderStreamInErrorState);
    STRINGIFY(DecoderStatus::Codes::kDecoderStreamDemuxerError);
    STRINGIFY(DecoderStatus::Codes::kUnsupportedProfile);
    STRINGIFY(DecoderStatus::Codes::kUnsupportedCodec);
    STRINGIFY(DecoderStatus::Codes::kUnsupportedConfig);
    STRINGIFY(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    STRINGIFY(DecoderStatus::Codes::kCantChangeCodec);
    STRINGIFY(DecoderStatus::Codes::kFailedToCreateDecoder);
    STRINGIFY(DecoderStatus::Codes::kKeyFrameRequired);
    STRINGIFY(DecoderStatus::Codes::kMissingTimestamp);
    STRINGIFY(DecoderStatus::Codes::kTooManyDecoders);
    STRINGIFY(DecoderStatus::Codes::kMediaFoundationNotAvailable);
    STRINGIFY(DecoderStatus::Codes::kElidedEndOfStreamForConfigChange);
  }
#undef STRINGIFY
}

}  // namespace

std::ostream& operator<<(std::ostream& os, const DecoderStatus& status) {
  return os << GetDecodeStatusString(status);
}

ScopedDecodeTrace::ScopedDecodeTrace(const char* trace_name,
                                     bool is_key_frame,
                                     base::TimeDelta timestamp)
    : trace_name_(trace_name) {
  DCHECK(trace_name_);
  TRACE_EVENT_BEGIN("media", perfetto::StaticString(trace_name_),
                    perfetto::Track::FromPointer(this), "is_key_frame",
                    is_key_frame, "timestamp_us", timestamp.InMicroseconds());
}

ScopedDecodeTrace::ScopedDecodeTrace(const char* trace_name,
                                     const DecoderBuffer& buffer)
    : trace_name_(trace_name) {
  DCHECK(trace_name_);
  std::string decoder_buffer_str =
      buffer.AsHumanReadableString(/*verbose=*/true);
  TRACE_EVENT_BEGIN("media", perfetto::StaticString(trace_name_),
                    perfetto::Track::FromPointer(this), "decoder_buffer",
                    perfetto::DynamicString(decoder_buffer_str));
}

ScopedDecodeTrace::ScopedDecodeTrace(const char* trace_name)
    : trace_name_(trace_name) {
  DCHECK(trace_name_);
  TRACE_EVENT_BEGIN("media", perfetto::StaticString(trace_name_),
                    perfetto::Track::FromPointer(this), "decoder_buffer",
                    "EOS");
}

ScopedDecodeTrace::~ScopedDecodeTrace() {
  if (!closed_)
    EndTrace(DecoderStatus::Codes::kAborted);
}

void ScopedDecodeTrace::EndTrace(const DecoderStatus& status) {
  DCHECK(!closed_);
  closed_ = true;
  TRACE_EVENT_END("media", perfetto::Track::FromPointer(this), "status",
                  GetDecodeStatusString(status));
}

}  // namespace media
