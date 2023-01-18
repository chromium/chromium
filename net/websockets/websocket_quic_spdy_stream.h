// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_QUIC_SPDY_STREAM_H_
#define NET_WEBSOCKETS_WEBSOCKET_QUIC_SPDY_STREAM_H_

#include "net/quic/quic_chromium_client_stream.h"
#include "net/third_party/quiche/src/quiche/quic/core/http/quic_spdy_client_session_base.h"

namespace net {
class NET_EXPORT_PRIVATE WebSocketQuicSpdyStream : public quic::QuicSpdyStream {
 public:
  class NET_EXPORT_PRIVATE Delegate {
   public:
    Delegate() = default;
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    virtual void OnInitialHeadersComplete(
        bool fin,
        size_t frame_len,
        const quic::QuicHeaderList& header_list) = 0;
    virtual void OnBodyAvailable() = 0;
    virtual void ClearStream() = 0;

   protected:
    virtual ~Delegate() = default;
  };
  WebSocketQuicSpdyStream(quic::QuicStreamId id,
                          quic::QuicSpdyClientSessionBase* session,
                          quic::StreamType type);

  WebSocketQuicSpdyStream(const WebSocketQuicSpdyStream&) = delete;
  WebSocketQuicSpdyStream& operator=(const WebSocketQuicSpdyStream&) = delete;
  ~WebSocketQuicSpdyStream() override;

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  void OnInitialHeadersComplete(
      bool fin,
      size_t frame_len,
      const quic::QuicHeaderList& header_list) override;
  void OnBodyAvailable() override;

 private:
  // The transaction should own the delegate. `delegate_` notifies this object
  // of its destruction, because they may be destroyed in any order.
  raw_ptr<WebSocketQuicSpdyStream::Delegate> delegate_ = nullptr;
};

}  // namespace net
#endif  // NET_WEBSOCKETS_WEBSOCKET_QUIC_SPDY_STREAM_H_
