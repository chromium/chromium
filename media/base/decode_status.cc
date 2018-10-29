// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decode_status.h"

#include <ostream>

#include "base/trace_event/trace_event.h"

namespace media {

const char* GetDecodeStatusString(DecodeStatus status) {
  switch (status) {
    case DecodeStatus::OK:
      return "DecodeStatus::OK";
    case DecodeStatus::ABORTED:
      return "DecodeStatus::ABORTED";
    case DecodeStatus::DECODE_ERROR:
      return "DecodeStatus::DECODE_ERROR";
  }
}

std::ostream& operator<<(std::ostream& os, const DecodeStatus& status) {
  os << GetDecodeStatusString(status);
  return os;
}

// static
bool ScopedDecodeTrace::IsEnabled() {
  bool enable_decode_traces = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("media", &enable_decode_traces);
  return enable_decode_traces;
}

ScopedDecodeTrace::ScopedDecodeTrace(const char* trace_name,
                                     bool is_key_frame,
                                     base::TimeDelta timestamp)
    : trace_name_(trace_name) {
  DCHECK(trace_name_);
  TRACE_EVENT_ASYNC_BEGIN2("media", trace_name_, this, "is_key_frame",
                           is_key_frame, "timestamp_us",
                           timestamp.InMicroseconds());
}

ScopedDecodeTrace::ScopedDecodeTrace(const char* trace_name,
                                     const DecoderBuffer& buffer)
    : trace_name_(trace_name) {
  DCHECK(trace_name_);
  TRACE_EVENT_ASYNC_BEGIN1("media", trace_name_, this, "decoder_buffer",
                           buffer.AsHumanReadableString());
}

ScopedDecodeTrace::~ScopedDecodeTrace() {
  if (!closed_)
    EndTrace(DecodeStatus::ABORTED);
}

void ScopedDecodeTrace::EndTrace(DecodeStatus status) {
  DCHECK(!closed_);
  closed_ = true;
  TRACE_EVENT_ASYNC_END1("media", trace_name_, this, "status",
                         GetDecodeStatusString(status));
}

}  // namespace media
