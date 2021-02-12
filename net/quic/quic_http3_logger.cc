// Copyright (c) 2019 The Chromium Authors. All rights reserved.
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

base::Value NetLogSettingsParams(const quic::SettingsFrame& frame) {
  base::Value dict(base::Value::Type::DICTIONARY);
  for (auto setting : frame.values) {
    dict.SetIntKey(
        quic::H3SettingsToString(
            static_cast<quic::Http3AndQpackSettingsIdentifiers>(setting.first)),
        setting.second);
  }
  return dict;
}

base::Value NetLogPriorityUpdateParams(const quic::PriorityUpdateFrame& frame) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("type", frame.prioritized_element_type ==
                                    quic::PrioritizedElementType::REQUEST_STREAM
                                ? "request_stream"
                                : "push_stream");
  dict.SetKey("prioritized_element_id",
              NetLogNumberValue(frame.prioritized_element_id));
  dict.SetStringKey("priority_field_value", frame.priority_field_value);
  return dict;
}

base::Value NetLogTwoIntParams(base::StringPiece name1,
                               uint64_t value1,
                               base::StringPiece name2,
                               uint64_t value2) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey(name1, NetLogNumberValue(value1));
  dict.SetKey(name2, NetLogNumberValue(value2));
  return dict;
}

base::Value NetLogThreeIntParams(base::StringPiece name1,
                                 uint64_t value1,
                                 base::StringPiece name2,
                                 uint64_t value2,
                                 base::StringPiece name3,
                                 uint64_t value3) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey(name1, NetLogNumberValue(value1));
  dict.SetKey(name2, NetLogNumberValue(value2));
  dict.SetKey(name3, NetLogNumberValue(value3));
  return dict;
}

base::ListValue ElideQuicHeaderListForNetLog(
    const quic::QuicHeaderList& headers,
    NetLogCaptureMode capture_mode) {
  base::ListValue headers_list;
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

QuicHttp3Logger::~QuicHttp3Logger() {}

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

void QuicHttp3Logger::OnCancelPushFrameReceived(
    const quic::CancelPushFrame& frame) {
  if (!net_log_.IsCapturing()) {
    return;
  }
  net_log_.AddEventWithIntParams(NetLogEventType::HTTP3_CANCEL_PUSH_RECEIVED,
                                 "push_id", frame.push_id);
}

void QuicHttp3Logger::OnSettingsFrameReceived(
    const quic::SettingsFrame& frame) {
  // Increment value by one because empty SETTINGS frames are allowed,
  // but histograms do not support the value zero.
  UMA_HISTOGRAM_CUSTOM_COUNTS("Net.QuicSession.ReceivedSettings.CountPlusOne",
                              frame.values.size() + 1, /* min = */ 1,
                              /* max = */ 10, /* buckets = */ 10);
  int reserved_identifier_count = 0;
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

void QuicHttp3Logger::OnMaxPushIdFrameReceived(
    const quic::MaxPushIdFrame& frame) {
  if (!net_log_.IsCapturing()) {
    return;
  }
  net_log_.AddEventWithIntParams(NetLogEventType::HTTP3_MAX_PUSH_ID_RECEIVED,
                                 "push_id", frame.push_id);
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
        base::Value dict(base::Value::Type::DICTIONARY);
        dict.SetKey("stream_id",
                    NetLogNumberValue(static_cast<uint64_t>(stream_id)));
        dict.SetKey("headers",
                    ElideQuicHeaderListForNetLog(headers, capture_mode));
        return dict;
      });
}

void QuicHttp3Logger::OnPushPromiseFrameReceived(
    quic::QuicStreamId stream_id,
    quic::QuicStreamId push_id,
    quic::QuicByteCount compressed_headers_length) {
  if (!net_log_.IsCapturing()) {
    return;
  }
  net_log_.AddEvent(NetLogEventType::HTTP3_PUSH_PROMISE_RECEIVED,
                    [stream_id, push_id, compressed_headers_length] {
                      return NetLogThreeIntParams("stream_id", stream_id,
                                                  "push_id", push_id,
                                                  "compressed_headers_length",
                                                  compressed_headers_length);
                    });
}

void QuicHttp3Logger::OnPushPromiseDecoded(quic::QuicStreamId stream_id,
                                           quic::QuicStreamId push_id,
                                           quic::QuicHeaderList headers) {
  if (!net_log_.IsCapturing()) {
    return;
  }
  net_log_.AddEvent(
      NetLogEventType::HTTP3_PUSH_PROMISE_DECODED,
      [stream_id, push_id, &headers](NetLogCaptureMode capture_mode) {
        base::Value dict(base::Value::Type::DICTIONARY);
        dict.SetKey("stream_id",
                    NetLogNumberValue(static_cast<uint64_t>(stream_id)));
        dict.SetKey("push_id",
                    NetLogNumberValue(static_cast<uint64_t>(push_id)));
        dict.SetKey("headers",
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

void QuicHttp3Logger::OnMaxPushIdFrameSent(const quic::MaxPushIdFrame& frame) {
  if (!net_log_.IsCapturing()) {
    return;
  }
  net_log_.AddEventWithIntParams(NetLogEventType::HTTP3_MAX_PUSH_ID_SENT,
                                 "push_id", frame.push_id);
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
        base::Value dict(base::Value::Type::DICTIONARY);
        dict.SetKey("stream_id",
                    NetLogNumberValue(static_cast<uint64_t>(stream_id)));
        dict.SetKey("headers",
                    ElideHttp2HeaderBlockForNetLog(header_block, capture_mode));
        return dict;
      });
}

void QuicHttp3Logger::OnPushPromiseFrameSent(
    quic::QuicStreamId stream_id,
    quic::QuicStreamId push_id,
    const spdy::Http2HeaderBlock& header_block) {
  if (!net_log_.IsCapturing()) {
    return;
  }
  net_log_.AddEvent(
      NetLogEventType::HTTP3_PUSH_PROMISE_SENT,
      [stream_id, push_id, &header_block](NetLogCaptureMode capture_mode) {
        base::Value dict(base::Value::Type::DICTIONARY);
        dict.SetKey("stream_id",
                    NetLogNumberValue(static_cast<uint64_t>(stream_id)));
        dict.SetKey("push_id",
                    NetLogNumberValue(static_cast<uint64_t>(push_id)));
        dict.SetKey("headers",
                    ElideHttp2HeaderBlockForNetLog(header_block, capture_mode));
        return dict;
      });
}

}  // namespace net
