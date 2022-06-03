// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_http_utils.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "net/spdy/spdy_log_util.h"

namespace net {

namespace {

enum AltSvcFormat { GOOGLE_FORMAT = 0, IETF_FORMAT = 1, ALTSVC_FORMAT_MAX };

void RecordAltSvcFormat(AltSvcFormat format) {
  UMA_HISTOGRAM_ENUMERATION("Net.QuicAltSvcFormat", format, ALTSVC_FORMAT_MAX);
}

}  // namespace

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
  dict.SetIntKey("quic_priority", static_cast<int>(priority));
  dict.SetIntKey("quic_stream_id", static_cast<int>(stream_id));
  return dict;
}

base::Value QuicResponseNetLogParams(quic::QuicStreamId stream_id,
                                     bool fin_received,
                                     const spdy::Http2HeaderBlock* headers,
                                     NetLogCaptureMode capture_mode) {
  base::Value dict = Http2HeaderBlockNetLogParams(headers, capture_mode);
  dict.SetIntKey("quic_stream_id", static_cast<int>(stream_id));
  dict.SetBoolKey("fin", fin_received);
  return dict;
}

quic::ParsedQuicVersionVector FilterSupportedAltSvcVersions(
    const spdy::SpdyAltSvcWireFormat::AlternativeService& quic_alt_svc,
    const quic::ParsedQuicVersionVector& supported_versions) {
  quic::ParsedQuicVersionVector supported_alt_svc_versions;
  DCHECK("quic" == quic_alt_svc.protocol_id || "hq" == quic_alt_svc.protocol_id)
      << quic_alt_svc.protocol_id;

  for (uint32_t quic_version : quic_alt_svc.version) {
    for (const quic::ParsedQuicVersion& supported : supported_versions) {
      if (supported.UsesQuicCrypto() &&
          supported.SupportsGoogleAltSvcFormat() &&
          static_cast<uint32_t>(supported.transport_version) == quic_version) {
        supported_alt_svc_versions.push_back(supported);
        RecordAltSvcFormat(GOOGLE_FORMAT);
      }
    }
  }
  return supported_alt_svc_versions;
}

}  // namespace net
