// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_HTTP_UTILS_H_
#define NET_QUIC_QUIC_HTTP_UTILS_H_

#include "base/values.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/log/net_log_capture_mode.h"
#include "net/third_party/quiche/src/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/spdy/core/spdy_header_block.h"
#include "net/third_party/quiche/src/spdy/core/spdy_protocol.h"

namespace net {

// TODO(crbug/988608): Convert to SpdyStreamPrecedence directly instead of to
// SpdyPriority which will go away eventually.
NET_EXPORT_PRIVATE spdy::SpdyPriority ConvertRequestPriorityToQuicPriority(
    RequestPriority priority);

NET_EXPORT_PRIVATE RequestPriority
ConvertQuicPriorityToRequestPriority(spdy::SpdyPriority priority);

// Converts a spdy::SpdyHeaderBlock, stream_id and priority into NetLog event
// parameters.
NET_EXPORT base::Value QuicRequestNetLogParams(
    quic::QuicStreamId stream_id,
    const spdy::SpdyHeaderBlock* headers,
    spdy::SpdyPriority priority,
    NetLogCaptureMode capture_mode);

// Converts a spdy::SpdyHeaderBlock and stream into NetLog event parameters.
NET_EXPORT base::Value QuicResponseNetLogParams(
    quic::QuicStreamId stream_id,
    bool fin_received,
    const spdy::SpdyHeaderBlock* headers,
    NetLogCaptureMode capture_mode);

// Parses |alt_svc_versions| into a quic::ParsedQuicVersionVector and removes
// all entries that aren't found in |supported_versions|.
NET_EXPORT quic::ParsedQuicVersionVector FilterSupportedAltSvcVersions(
    const spdy::SpdyAltSvcWireFormat::AlternativeService& quic_alt_svc,
    const quic::ParsedQuicVersionVector& supported_versions);

}  // namespace net

#endif  // NET_QUIC_QUIC_HTTP_UTILS_H_
