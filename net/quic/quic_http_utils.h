// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_HTTP_UTILS_H_
#define NET_QUIC_QUIC_HTTP_UTILS_H_

#include "base/values.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/log/net_log_capture_mode.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quiche/spdy/core/http2_header_block.h"
#include "net/third_party/quiche/src/quiche/spdy/core/spdy_protocol.h"

namespace net {

// TODO(crbug/988608): Convert to SpdyStreamPrecedence directly instead of to
// SpdyPriority which will go away eventually.
NET_EXPORT_PRIVATE spdy::SpdyPriority ConvertRequestPriorityToQuicPriority(
    RequestPriority priority);

NET_EXPORT_PRIVATE RequestPriority
ConvertQuicPriorityToRequestPriority(spdy::SpdyPriority priority);

// Converts a spdy::Http2HeaderBlock, stream_id and priority into NetLog event
// parameters.
NET_EXPORT base::Value QuicRequestNetLogParams(
    quic::QuicStreamId stream_id,
    const spdy::Http2HeaderBlock* headers,
    spdy::SpdyPriority priority,
    NetLogCaptureMode capture_mode);

// Converts a spdy::Http2HeaderBlock and stream into NetLog event parameters.
NET_EXPORT base::Value QuicResponseNetLogParams(
    quic::QuicStreamId stream_id,
    bool fin_received,
    const spdy::Http2HeaderBlock* headers,
    NetLogCaptureMode capture_mode);

}  // namespace net

#endif  // NET_QUIC_QUIC_HTTP_UTILS_H_
