// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A toy server specific QuicSession subclass.

#ifndef NET_THIRD_PARTY_QUIC_TOOLS_QUIC_SIMPLE_SERVER_SESSION_H_
#define NET_THIRD_PARTY_QUIC_TOOLS_QUIC_SIMPLE_SERVER_SESSION_H_

#include <stdint.h>

#include <list>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "net/third_party/quic/core/http/quic_server_session_base.h"
#include "net/third_party/quic/core/http/quic_spdy_session.h"
#include "net/third_party/quic/core/quic_crypto_server_stream.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/platform/api/quic_containers.h"
#include "net/third_party/quic/tools/quic_backend_response.h"
#include "net/third_party/quic/tools/quic_simple_server_backend.h"
#include "net/third_party/quic/tools/quic_simple_server_stream.h"

namespace quic {

namespace test {
class QuicSimpleServerSessionPeer;
}  // namespace test

class QuicSimpleServerSession : public QuicServerSessionBase {
 public:
  // A PromisedStreamInfo is an element of the queue to store promised
  // stream which hasn't been created yet. It keeps a mapping between promised
  // stream id with its priority and the headers sent out in PUSH_PROMISE.
  struct PromisedStreamInfo {
   public:
    PromisedStreamInfo(spdy::SpdyHeaderBlock request_headers,
                       QuicStreamId stream_id,
                       spdy::SpdyPriority priority)
        : request_headers(std::move(request_headers)),
          stream_id(stream_id),
          priority(priority),
          is_cancelled(false) {}
    spdy::SpdyHeaderBlock request_headers;
    QuicStreamId stream_id;
    spdy::SpdyPriority priority;
    bool is_cancelled;
  };

  // Takes ownership of |connection|.
  QuicSimpleServerSession(const QuicConfig& config,
                          const ParsedQuicVersionVector& supported_versions,
                          QuicConnection* connection,
                          QuicSession::Visitor* visitor,
                          QuicCryptoServerStream::Helper* helper,
                          const QuicCryptoServerConfig* crypto_config,
                          QuicCompressedCertsCache* compressed_certs_cache,
                          QuicSimpleServerBackend* quic_simple_server_backend);
  QuicSimpleServerSession(const QuicSimpleServerSession&) = delete;
  QuicSimpleServerSession& operator=(const QuicSimpleServerSession&) = delete;

  ~QuicSimpleServerSession() override;

  // When a stream is marked draining, it will decrease the number of open
  // streams. If it is an outgoing stream, try to open a new stream to send
  // remaing push responses.
  void StreamDraining(QuicStreamId id) override;

  // Override base class to detact client sending data on server push stream.
  void OnStreamFrame(const QuicStreamFrame& frame) override;

  // Send out PUSH_PROMISE for all |resources| promised stream id in each frame
  // will increase by 2 for each item in |resources|.
  // And enqueue HEADERS block in those PUSH_PROMISED for sending push response
  // later.
  virtual void PromisePushResources(
      const QuicString& request_url,
      const std::list<QuicBackendResponse::ServerPushInfo>& resources,
      QuicStreamId original_stream_id,
      const spdy::SpdyHeaderBlock& original_request_headers);

 protected:
  // QuicSession methods:
  QuicSpdyStream* CreateIncomingStream(QuicStreamId id) override;
  QuicSimpleServerStream* CreateOutgoingBidirectionalStream() override;
  QuicSimpleServerStream* CreateOutgoingUnidirectionalStream() override;
  // Closing an outgoing stream can reduce open outgoing stream count, try
  // to handle queued promised streams right now.
  void CloseStreamInner(QuicStreamId stream_id, bool locally_reset) override;
  // Override to return true for locally preserved server push stream.
  void HandleFrameOnNonexistentOutgoingStream(QuicStreamId stream_id) override;
  // Override to handle reseting locally preserved streams.
  void HandleRstOnValidNonexistentStream(
      const QuicRstStreamFrame& frame) override;

  // QuicServerSessionBaseMethod:
  QuicCryptoServerStreamBase* CreateQuicCryptoServerStream(
      const QuicCryptoServerConfig* crypto_config,
      QuicCompressedCertsCache* compressed_certs_cache) override;

  QuicSimpleServerBackend* server_backend() {
    return quic_simple_server_backend_;
  }

 private:
  friend class test::QuicSimpleServerSessionPeer;

  // Create a server push headers block by copying request's headers block.
  // But replace or add these pseudo-headers as they are specific to each
  // request:
  // :authority, :path, :method, :scheme, referer.
  // Copying the rest headers ensures they are the same as the original
  // request, especially cookies.
  spdy::SpdyHeaderBlock SynthesizePushRequestHeaders(
      QuicString request_url,
      QuicBackendResponse::ServerPushInfo resource,
      const spdy::SpdyHeaderBlock& original_request_headers);

  // Send PUSH_PROMISE frame on headers stream.
  void SendPushPromise(QuicStreamId original_stream_id,
                       QuicStreamId promised_stream_id,
                       spdy::SpdyHeaderBlock headers);

  // Fetch response from cache for request headers enqueued into
  // promised_headers_and_streams_ and send them on dedicated stream until
  // reaches max_open_stream_ limit.
  // Called when return value of GetNumOpenOutgoingStreams() changes:
  //    CloseStreamInner();
  //    StreamDraining();
  // Note that updateFlowControlOnFinalReceivedByteOffset() won't change the
  // return value becasue all push streams are impossible to become locally
  // closed. Since a locally preserved stream becomes remotely closed after
  // HandlePromisedPushRequests() starts to process it, and if it is reset
  // locally afterwards, it will be immediately become closed and never get into
  // locally_closed_stream_highest_offset_. So all the streams in this map
  // are not outgoing streams.
  void HandlePromisedPushRequests();

  // Keep track of the highest stream id which has been sent in PUSH_PROMISE.
  QuicStreamId highest_promised_stream_id_;

  // Promised streams which hasn't been created yet because of max_open_stream_
  // limit. New element is added to the end of the queue.
  // Since outgoing stream is created in sequence, stream_id of each element in
  // the queue also increases by 2 from previous one's. The front element's
  // stream_id is always next_outgoing_stream_id_, and the last one is always
  // highest_promised_stream_id_.
  QuicDeque<PromisedStreamInfo> promised_streams_;

  QuicSimpleServerBackend* quic_simple_server_backend_;  // Not owned.
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_TOOLS_QUIC_SIMPLE_SERVER_SESSION_H_
