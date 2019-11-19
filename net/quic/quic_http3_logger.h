// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_HTTP3_LOGGER_H_
#define NET_QUIC_QUIC_HTTP3_LOGGER_H_

#include <stddef.h>

#include <bitset>
#include <string>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "net/log/net_log_with_source.h"
#include "net/third_party/quiche/src/quic/core/http/quic_spdy_session.h"

namespace net {

// This class is a debug visitor of a quic::QuicSpdySession which logs events
// to |net_log| and records histograms.
class NET_EXPORT_PRIVATE QuicHttp3Logger : public quic::Http3DebugVisitor {
 public:
  explicit QuicHttp3Logger(const NetLogWithSource& net_log);

  ~QuicHttp3Logger() override;

  // Implementation of Http3DebugVisitor.
  void OnPeerControlStreamCreated(quic::QuicStreamId stream_id) override;
  void OnPeerQpackEncoderStreamCreated(quic::QuicStreamId stream_id) override;
  void OnPeerQpackDecoderStreamCreated(quic::QuicStreamId stream_id) override;
  void OnSettingsFrameReceived(const quic::SettingsFrame& frame) override;
  void OnSettingsFrameSent(const quic::SettingsFrame& frame) override;

 private:
  NetLogWithSource net_log_;

  DISALLOW_COPY_AND_ASSIGN(QuicHttp3Logger);
};
}  // namespace net

#endif  // NET_QUIC_QUIC_HTTP3_LOGGER_H_
