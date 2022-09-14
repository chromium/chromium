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

base::Value QuicRequestNetLogParams(quic::QuicStreamId stream_id,
                                    const spdy::Http2HeaderBlock* headers,
                                    spdy::SpdyPriority priority,
                                    NetLogCaptureMode capture_mode) {
  base::Value dict = Http2HeaderBlockNetLogParams(headers, capture_mode);
  DCHECK(dict.is_dict());
  dict.GetDict().Set("quic_priority", static_cast<int>(priority));
  dict.GetDict().Set("quic_stream_id", static_cast<int>(stream_id));
  return dict;
}

base::Value QuicResponseNetLogParams(quic::QuicStreamId stream_id,
                                     bool fin_received,
                                     const spdy::Http2HeaderBlock* headers,
                                     NetLogCaptureMode capture_mode) {
  base::Value dict = Http2HeaderBlockNetLogParams(headers, capture_mode);
  dict.GetDict().Set("quic_stream_id", static_cast<int>(stream_id));
  dict.GetDict().Set("fin", fin_received);
  return dict;
}

}  // namespace net
