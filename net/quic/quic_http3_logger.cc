// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "net/quic/quic_http3_logger.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_values.h"

namespace net {

namespace {

base::Value NetLogSettingsParams(const quic::SettingsFrame& frame) {
  base::Value dict(base::Value::Type::DICTIONARY);
  // TODO(renjietang): Use string literal for setting identifiers.
  for (auto setting : frame.values) {
    dict.SetIntKey(
        quic::SpdyUtils::H3SettingsToString(
            static_cast<quic::Http3AndQpackSettingsIdentifiers>(setting.first)),
        setting.second);
  }
  return dict;
}

}  // namespace

QuicHttp3Logger::QuicHttp3Logger(const NetLogWithSource& net_log)
    : net_log_(net_log) {}

QuicHttp3Logger::~QuicHttp3Logger() {}

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
  for (const auto& value : frame.values) {
    if (value.first == quic::SETTINGS_QPACK_MAX_TABLE_CAPACITY) {
      UMA_HISTOGRAM_COUNTS_10000(
          "Net.QuicSession.ReceivedSettings.MaxTableCapacity", value.second);
    } else if (value.first == quic::SETTINGS_MAX_HEADER_LIST_SIZE) {
      UMA_HISTOGRAM_COUNTS_10000(
          "Net.QuicSession.ReceivedSettings.MaxHeaderListSize", value.second);
    } else if (value.first == quic::SETTINGS_QPACK_BLOCKED_STREAMS) {
      UMA_HISTOGRAM_COUNTS_1000(
          "Net.QuicSession.ReceivedSettings.BlockedStreams", value.second);
    }
  }

  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(NetLogEventType::HTTP3_SETTINGS_RECEIVED,
                    [&] { return NetLogSettingsParams(frame); });
}

void QuicHttp3Logger::OnSettingsFrameSent(const quic::SettingsFrame& frame) {
  if (!net_log_.IsCapturing())
    return;
  net_log_.AddEvent(NetLogEventType::HTTP3_SETTINGS_SENT,
                    [&] { return NetLogSettingsParams(frame); });
}

}  // namespace net
