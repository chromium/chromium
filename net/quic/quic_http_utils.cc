// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_http_utils.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
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

base::Value::Dict QuicRequestNetLogParams(
    quic::QuicStreamId stream_id,
    const quiche::HttpHeaderBlock* headers,
    quic::QuicStreamPriority priority,
    NetLogCaptureMode capture_mode) {
  base::Value::Dict dict = HttpHeaderBlockNetLogParams(headers, capture_mode);
  switch (priority.type()) {
    case quic::QuicPriorityType::kHttp: {
      auto http_priority = priority.http();
      dict.Set("quic_priority_type", "http");
      dict.Set("quic_priority_urgency", http_priority.urgency);
      dict.Set("quic_priority_incremental", http_priority.incremental);
      break;
    }
    case quic::QuicPriorityType::kWebTransport: {
      auto web_transport_priority = priority.web_transport();
      dict.Set("quic_priority_type", "web_transport");
      dict.Set("web_transport_session_id",
               static_cast<int>(web_transport_priority.session_id));

      // `send_group_number` is an uint64_t, `send_order` is an int64_t. But
      // base::Value doesn't support these types.
      // Case to a double instead. As this is just for diagnostics, some loss of
      // precision is acceptable.
      dict.Set("web_transport_send_group_number",
               static_cast<double>(web_transport_priority.send_group_number));
      dict.Set("web_transport_send_order",
               static_cast<double>(web_transport_priority.send_order));
      break;
    }
  }
  dict.Set("quic_stream_id", static_cast<int>(stream_id));
  return dict;
}

base::Value::Dict QuicResponseNetLogParams(
    quic::QuicStreamId stream_id,
    bool fin_received,
    const quiche::HttpHeaderBlock* headers,
    NetLogCaptureMode capture_mode) {
  base::Value::Dict dict = HttpHeaderBlockNetLogParams(headers, capture_mode);
  dict.Set("quic_stream_id", static_cast<int>(stream_id));
  dict.Set("fin", fin_received);
  return dict;
}

}  // namespace net
