// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_http_utils.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "net/spdy/spdy_log_util.h"

namespace net {

spdy::SpdyPriority ConvertRequestPriorityToQuicPriority(
    const RequestPriority priority) {
  DCHECK_GE(priority, MINIMUM_PRIORITY);
  DCHECK_LE(priority, MAXIMUM_PRIORITY);
  return static_cast<spdy::SpdyPriority>(HIGHEST - priority);
}

RequestPriority ConvertQuicPriorityToRequestPriority(
    spdy::SpdyPriority priority) {
  // Handle invalid values gracefully.
  return (priority >= 5) ? IDLE
                         : static_cast<RequestPriority>(HIGHEST - priority);
}

base::Value::Dict QuicRequestNetLogParams(quic::QuicStreamId stream_id,
                                          const spdy::Http2HeaderBlock* headers,
                                          quic::QuicStreamPriority priority,
                                          NetLogCaptureMode capture_mode) {
  base::Value::Dict dict = Http2HeaderBlockNetLogParams(headers, capture_mode);
  dict.Set("quic_priority_urgency", priority.urgency);
  dict.Set("quic_priority_incremental", priority.incremental);
  dict.Set("quic_stream_id", static_cast<int>(stream_id));
  return dict;
}

base::Value::Dict QuicResponseNetLogParams(
    quic::QuicStreamId stream_id,
    bool fin_received,
    const spdy::Http2HeaderBlock* headers,
    NetLogCaptureMode capture_mode) {
  base::Value::Dict dict = Http2HeaderBlockNetLogParams(headers, capture_mode);
  dict.Set("quic_stream_id", static_cast<int>(stream_id));
  dict.Set("fin", fin_received);
  return dict;
}

}  // namespace net
