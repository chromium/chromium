// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "net/quic/quic_http3_logger.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "net/http/http_log_util.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_values.h"
#include "net/spdy/spdy_log_util.h"

namespace net {

namespace {

base::Value::Dict NetLogSettingsParams(const quic::SettingsFrame& frame) {
  base::Value::Dict dict;
  for (auto setting : frame.values) {
    dict.Set(
        quic::H3SettingsToString(
            static_cast<quic::Http3AndQpackSettingsIdentifiers>(setting.first)),
        static_cast<int>(setting.second));
  }
  return dict;
}

base::Value::Dict NetLogPriorityUpdateParams(
    const quic::PriorityUpdateFrame& frame) {
  base::Value::Dict dict;
  dict.Set("prioritized_element_id",
           NetLogNumberValue(frame.prioritized_element_id));
  dict.Set("priority_field_value", frame.priority_field_value);
  return dict;
}

base::Value::Dict NetLogTwoIntParams(base::StringPiece name1,
                                     uint64_t value1,
                                     base::StringPiece name2,
                                     uint64_t value2) {
  base::Value::Dict dict;
  dict.Set(name1, NetLogNumberValue(value1));
  dict.Set(name2, NetLogNumberValue(value2));
  return dict;
}

base::Value::Dict NetLogThreeIntParams(base::StringPiece name1,
                                       uint64_t value1,
                                       base::StringPiece name2,
                                       uint64_t value2,
                                       base::StringPiece name3,
                                       uint64_t value3) {
  base::Value::Dict dict;
  dict.Set(name1, NetLogNumberValue(value1));
  dict.Set(name2, NetLogNumberValue(value2));
  dict.Set(name3, NetLogNumberValue(value3));
  return dict;
}

base::Value::List ElideQuicHeaderListForNetLog(
    const quic::QuicHeaderList& headers,
    NetLogCaptureMode capture_mode) {
  base::Value::List headers_list;
  for (const auto& header : headers) {
    base::StringPiece key = header.first;
    base::StringPiece value = header.second;
    headers_list.Append(NetLogStringValue(
        base::StrCat({key, ": ",
                      ElideHeaderValueForNetLog(capture_mode, std::string(key),
                                                std::string(value))})));
  }
  return headers_list;
}

}  // namespace

QuicHttp3Logger::QuicHttp3Logger(const NetLogWithSource& net_log)
    : net_log_(net_log) {}

QuicHttp3Logger::~QuicHttp3Logger() = default;

void QuicHttp3Logger::OnControlStreamCreated(quic::QuicStreamId stream_id) {
  if (!net_log_.IsCapturing()) {
    return;
  }
  net_log_.AddEventWithIntParams(
      NetLogEventType::HTTP3_LOCAL_CONTROL_STREAM_CREATED, "stream_id",
      stream_id);
}

void QuicHttp3Logger::OnQpackEncoderStreamCreated(
    quic::QuicStreamId stream_id) {
  if (!net_log_.IsCapturing()) {
    return;
  }
  net_log_.AddEventWithIntParams(
      NetLogEventType::HTTP3_LOCAL_QPACK_ENCODER_STREAM_CREATED, "stream_id",
      stream_id);
}

void QuicHttp3Logger::OnQpackDecoderStreamCreated(
    quic::QuicStreamId stream_id) {
  if (!net_log_.IsCapturing()) {
    return;
  }
  net_log_.AddEventWithIntParams(
      NetLogEventType::HTTP3_LOCAL_QPACK_DECODER_STREAM_CREATED, "stream_id",
      stream_id);
}

void QuicHttp3Logger::OnPeerControlStreamCreated(quic::QuicStreamId stream_id) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEventWithIntParams(
      NetLogEventType::HTTP3_PEER_CONTROL_STREAM_CREATED, "stream_id",
      stream_id);
}

void QuicHttp3Logger::OnPeerQpackEncoderStreamCreated(
    quic::QuicStreamId stream_id) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEventWithIntParams(
      NetLogEventType::HTTP3_PEER_QPACK_ENCODER_STREAM_CREATED, "stream_id",
      stream_id);
}

void QuicHttp3Logger::OnPeerQpackDecoderStreamCreated(
    quic::QuicStreamId stream_id) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEventWithIntParams(
      NetLogEventType::HTTP3_PEER_QPACK_DECODER_STREAM_CREATED, "stream_id",
      stream_id);
}

void QuicHttp3Logger::OnSettingsFrameReceived(
    const quic::SettingsFrame& frame) {
  // Increment value by one because empty SETTINGS frames are allowed,
  // but histograms do not support the value zero.
  UMA_HISTOGRAM_CUSTOM_COUNTS("Net.QuicSession.ReceivedSettings.CountPlusOne",
                              frame.values.size() + 1, /* min = */ 1,
                              /* max = */ 10, /* buckets = */ 10);
  int reserved_identifier_count = 0;
  bool settings_extended_connect_enabled = false;
  for (const auto& value : frame.values) {
    if (value.first == quic::SETTINGS_QPACK_MAX_TABLE_CAPACITY) {
      UMA_HISTOGRAM_COUNTS_1M(
          "Net.QuicSession.ReceivedSettings.MaxTableCapacity2", value.second);
    } else if (value.first == quic::SETTINGS_MAX_FIELD_SECTION_SIZE) {
      UMA_HISTOGRAM_COUNTS_1M(
          "Net.QuicSession.ReceivedSettings.MaxHeaderListSize2", value.second);
    } else if (value.first == quic::SETTINGS_QPACK_BLOCKED_STREAMS) {
      UMA_HISTOGRAM_COUNTS_1000(
          "Net.QuicSession.ReceivedSettings.BlockedStreams", value.second);
    } else if (value.first == quic::SETTINGS_ENABLE_CONNECT_PROTOCOL) {
      settings_extended_connect_enabled = value.second == 1;
    } else if (value.first >= 0x21 && value.first % 0x1f == 2) {
      // Reserved setting identifiers are defined at
      // https://quicwg.org/base-drafts/draft-ietf-quic-http.html#name-defined-settings-parameters.
      // These should not be treated specially on the receive side, because they
      // are sent to exercise the requirement that unknown identifiers are
      // ignored.  Here an exception is made for logging only, to understand
      // what kind of identifiers are received.
      reserved_identifier_count++;
    }
  }
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Net.QuicSession.ReceivedSettings.ReservedCountPlusOne",
      reserved_identifier_count + 1, /* min = */ 1,
      /* max = */ 5, /* buckets = */ 5);
  UMA_HISTOGRAM_BOOLEAN(
      "Net.QuicSession.ReceivedSettings.EnableExtendedConnect",
      settings_extended_connect_enabled);

  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(NetLogEventType::HTTP3_SETTINGS_RECEIVED,
                    [&frame] { return NetLogSettingsParams(frame); });
}

void QuicHttp3Logger::OnGoAwayFrameReceived(const quic::GoAwayFrame& frame) {
  if (!net_log_.IsCapturing()) {
    return;
  }
  net_log_.AddEventWithIntParams(NetLogEventType::HTTP3_GOAWAY_RECEIVED,
                                 "stream_id", frame.id);
}

void QuicHttp3Logger::OnPriorityUpdateFrameReceived(
    const quic::PriorityUpdateFrame& frame) {
  if (!net_log_.IsCapturing()) {
    return;
  }
  net_log_.AddEvent(NetLogEventType::HTTP3_PRIORITY_UPDATE_RECEIVED,
                    [&frame] { return NetLogPriorityUpdateParams(frame); });
}

void QuicHttp3Logger::OnDataFrameReceived(quic::QuicStreamId stream_id,
                                          quic::QuicByteCount payload_length) {
  if (!net_log_.IsCapturing()) {
    return;
  }
  net_log_.AddEvent(
      NetLogEventType::HTTP3_DATA_FRAME_RECEIVED, [stream_id, payload_length] {
        return NetLogTwoIntParams("stream_id", stream_id, "payload_length",
                                  payload_length);
      });
}

void QuicHttp3Logger::OnHeadersFrameReceived(
    quic::QuicStreamId stream_id,
    quic::QuicByteCount compressed_headers_length) {
  if (!net_log_.IsCapturing()) {
    return;
  }
  net_log_.AddEvent(NetLogEventType::HTTP3_HEADERS_RECEIVED,
                    [stream_id, compressed_headers_length] {
                      return NetLogTwoIntParams("stream_id", stream_id,
                                                "compressed_headers_length",
                                                compressed_headers_length);
                    });
}

void QuicHttp3Logger::OnHeadersDecoded(quic::QuicStreamId stream_id,
                                       quic::QuicHeaderList headers) {
  if (!net_log_.IsCapturing()) {
    return;
  }
  net_log_.AddEvent(
      NetLogEventType::HTTP3_HEADERS_DECODED,
      [stream_id, &headers](NetLogCaptureMode capture_mode) {
        base::Value::Dict dict;
        dict.Set("stream_id",
                 NetLogNumberValue(static_cast<uint64_t>(stream_id)));
        dict.Set("headers",
                 ElideQuicHeaderListForNetLog(headers, capture_mode));
        return dict;
      });
}

void QuicHttp3Logger::OnUnknownFrameReceived(
    quic::QuicStreamId stream_id,
    uint64_t frame_type,
    quic::QuicByteCount payload_length) {
  if (!net_log_.IsCapturing()) {
    return;
  }
  net_log_.AddEvent(NetLogEventType::HTTP3_UNKNOWN_FRAME_RECEIVED,
                    [stream_id, frame_type, payload_length] {
                      return NetLogThreeIntParams(
                          "stream_id", stream_id, "frame_type", frame_type,
                          "payload_length", payload_length);
                    });
}

void QuicHttp3Logger::OnSettingsFrameSent(const quic::SettingsFrame& frame) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(NetLogEventType::HTTP3_SETTINGS_SENT,
                    [&frame] { return NetLogSettingsParams(frame); });
}

void QuicHttp3Logger::OnSettingsFrameResumed(const quic::SettingsFrame& frame) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(NetLogEventType::HTTP3_SETTINGS_RESUMED,
                    [&frame] { return NetLogSettingsParams(frame); });
}

void QuicHttp3Logger::OnGoAwayFrameSent(quic::QuicStreamId stream_id) {
  if (!net_log_.IsCapturing()) {
    return;
  }
  net_log_.AddEventWithIntParams(NetLogEventType::HTTP3_GOAWAY_SENT,
                                 "stream_id", stream_id);
}

void QuicHttp3Logger::OnPriorityUpdateFrameSent(
    const quic::PriorityUpdateFrame& frame) {
  if (!net_log_.IsCapturing()) {
    return;
  }
  net_log_.AddEvent(NetLogEventType::HTTP3_PRIORITY_UPDATE_SENT,
                    [&frame] { return NetLogPriorityUpdateParams(frame); });
}

void QuicHttp3Logger::OnDataFrameSent(quic::QuicStreamId stream_id,
                                      quic::QuicByteCount payload_length) {
  if (!net_log_.IsCapturing()) {
    return;
  }
  net_log_.AddEvent(
      NetLogEventType::HTTP3_DATA_SENT, [stream_id, payload_length] {
        return NetLogTwoIntParams("stream_id", stream_id, "payload_length",
                                  payload_length);
      });
}

void QuicHttp3Logger::OnHeadersFrameSent(
    quic::QuicStreamId stream_id,
    const spdy::Http2HeaderBlock& header_block) {
  if (!net_log_.IsCapturing()) {
    return;
  }
  net_log_.AddEvent(
      NetLogEventType::HTTP3_HEADERS_SENT,
      [stream_id, &header_block](NetLogCaptureMode capture_mode) {
        base::Value::Dict dict;
        dict.Set("stream_id",
                 NetLogNumberValue(static_cast<uint64_t>(stream_id)));
        dict.Set("headers",
                 ElideHttp2HeaderBlockForNetLog(header_block, capture_mode));
        return dict;
      });
}

}  // namespace net
