// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_QUIC_SPDY_STREAM_H_
#define NET_WEBSOCKETS_WEBSOCKET_QUIC_SPDY_STREAM_H_

#include <stddef.h>

#include "base/memory/raw_ptr.h"
#include "net/base/net_export.h"
#include "net/quic/quic_chromium_client_stream_base.h"

namespace quic {
class QuicHeaderList;
}  // namespace quic

namespace net {

class IOBuffer;

class NET_EXPORT_PRIVATE WebSocketQuicSpdyStream
    : public QuicChromiumClientStreamBase {
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
    virtual void OnCanWriteNewData() = 0;
    virtual void OnClose(int status) = 0;

   protected:
    virtual ~Delegate() = default;
  };
  WebSocketQuicSpdyStream(quic::QuicStreamId id,
                          quic::QuicSpdyClientSessionBase* session,
                          quic::StreamType type);

  WebSocketQuicSpdyStream(const WebSocketQuicSpdyStream&) = delete;
  WebSocketQuicSpdyStream& operator=(const WebSocketQuicSpdyStream&) = delete;
  ~WebSocketQuicSpdyStream() override;

  // Sets the delegate to receive stream events.
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  void OnInitialHeadersComplete(
      bool fin,
      size_t frame_len,
      const quic::QuicHeaderList& header_list) override;
  void OnBodyAvailable() override;
  void OnClose() override;
  int Read(IOBuffer* buf, int buf_len);

  void OnCanWriteNewData() override;

  // Decouples the delegate from this stream and closes the stream, if it is
  // not closed already. Once the peer's FIN has been consumed the closing
  // handshake is complete, so the stream is closed with a FIN of our own: the
  // orderly closure of RFC 9220 section 3. A stream abandoned before that is
  // reset with QUIC_STREAM_CANCELLED, the RST exception of that same section,
  // to signal intentional closure to the peer.
  void DetachDelegate();

 private:
  // Maps QUIC connection and stream errors to net error codes.
  // Returns OK if there are no errors, otherwise returns the appropriate
  // net error code based on the QUIC error type.
  int MapQuicErrorToNetError();

  // The transaction should own the delegate. `delegate_` notifies this object
  // of its destruction, because they may be destroyed in any order.
  raw_ptr<WebSocketQuicSpdyStream::Delegate> delegate_ = nullptr;
};

}  // namespace net
#endif  // NET_WEBSOCKETS_WEBSOCKET_QUIC_SPDY_STREAM_H_
