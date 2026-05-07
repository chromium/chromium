// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_CHROMIUM_CLIENT_STREAM_BASE_H_
#define NET_QUIC_QUIC_CHROMIUM_CLIENT_STREAM_BASE_H_

#include "net/base/net_export.h"
#include "net/third_party/quiche/src/quiche/quic/core/http/quic_spdy_client_session_base.h"
#include "net/third_party/quiche/src/quiche/quic/core/http/quic_spdy_stream.h"

namespace net {

// Chromium-owned HTTP/3 stream base that carries client-specific hooks used by
// QuicChromiumClientSession when iterating mixed active stream populations.
class NET_EXPORT_PRIVATE QuicChromiumClientStreamBase
    : public quic::QuicSpdyStream {
 public:
  QuicChromiumClientStreamBase(quic::QuicStreamId id,
                               quic::QuicSpdyClientSessionBase* session,
                               quic::StreamType type);
  QuicChromiumClientStreamBase(quic::PendingStream* pending,
                               quic::QuicSpdyClientSessionBase* session);

  QuicChromiumClientStreamBase(const QuicChromiumClientStreamBase&) = delete;
  QuicChromiumClientStreamBase& operator=(const QuicChromiumClientStreamBase&) =
      delete;
  ~QuicChromiumClientStreamBase() override = default;

  virtual void OnError(int error);

  virtual bool CanMigrateToCellularNetwork() const;

 protected:
  void set_can_migrate_to_cellular_network(bool can_migrate) {
    can_migrate_to_cellular_network_ = can_migrate;
  }

 private:
  bool can_migrate_to_cellular_network_ = true;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_CHROMIUM_CLIENT_STREAM_BASE_H_
