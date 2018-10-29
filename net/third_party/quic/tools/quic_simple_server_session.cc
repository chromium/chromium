// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/tools/quic_simple_server_session.h"

#include <utility>

#include "net/third_party/quic/core/quic_connection.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/tools/quic_simple_server_stream.h"

namespace quic {

QuicSimpleServerSession::QuicSimpleServerSession(
    const QuicConfig& config,
    const ParsedQuicVersionVector& supported_versions,
    QuicConnection* connection,
    QuicSession::Visitor* visitor,
    QuicCryptoServerStream::Helper* helper,
    const QuicCryptoServerConfig* crypto_config,
    QuicCompressedCertsCache* compressed_certs_cache,
    QuicSimpleServerBackend* quic_simple_server_backend)
    : QuicServerSessionBase(config,
                            supported_versions,
                            connection,
                            visitor,
                            helper,
                            crypto_config,
                            compressed_certs_cache),
      highest_promised_stream_id_(
          QuicUtils::GetInvalidStreamId(connection->transport_version())),
      quic_simple_server_backend_(quic_simple_server_backend) {}

QuicSimpleServerSession::~QuicSimpleServerSession() {
  delete connection();
}

QuicCryptoServerStreamBase*
QuicSimpleServerSession::CreateQuicCryptoServerStream(
    const QuicCryptoServerConfig* crypto_config,
    QuicCompressedCertsCache* compressed_certs_cache) {
  return new QuicCryptoServerStream(
      crypto_config, compressed_certs_cache,
      GetQuicReloadableFlag(enable_quic_stateless_reject_support), this,
      stream_helper());
}

void QuicSimpleServerSession::StreamDraining(QuicStreamId id) {
  QuicSpdySession::StreamDraining(id);
  if (!IsIncomingStream(id)) {
    HandlePromisedPushRequests();
  }
}

void QuicSimpleServerSession::OnStreamFrame(const QuicStreamFrame& frame) {
  if (!IsIncomingStream(frame.stream_id)) {
    QUIC_LOG(WARNING) << "Client shouldn't send data on server push stream";
    connection()->CloseConnection(
        QUIC_INVALID_STREAM_ID, "Client sent data on server push stream",
        ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    return;
  }
  QuicSpdySession::OnStreamFrame(frame);
}

void QuicSimpleServerSession::PromisePushResources(
    const QuicString& request_url,
    const std::list<QuicBackendResponse::ServerPushInfo>& resources,
    QuicStreamId original_stream_id,
    const spdy::SpdyHeaderBlock& original_request_headers) {
  if (!server_push_enabled()) {
    return;
  }

  for (QuicBackendResponse::ServerPushInfo resource : resources) {
    spdy::SpdyHeaderBlock headers = SynthesizePushRequestHeaders(
        request_url, resource, original_request_headers);
    highest_promised_stream_id_ += 2;
    SendPushPromise(original_stream_id, highest_promised_stream_id_,
                    headers.Clone());
    promised_streams_.push_back(PromisedStreamInfo(
        std::move(headers), highest_promised_stream_id_, resource.priority));
  }

  // Procese promised push request as many as possible.
  HandlePromisedPushRequests();
}

QuicSpdyStream* QuicSimpleServerSession::CreateIncomingStream(QuicStreamId id) {
  if (!ShouldCreateIncomingStream(id)) {
    return nullptr;
  }

  QuicSpdyStream* stream = new QuicSimpleServerStream(
      id, this, BIDIRECTIONAL, quic_simple_server_backend_);
  ActivateStream(QuicWrapUnique(stream));
  return stream;
}

QuicSimpleServerStream*
QuicSimpleServerSession::CreateOutgoingBidirectionalStream() {
  DCHECK(false);
  return nullptr;
}

QuicSimpleServerStream*
QuicSimpleServerSession::CreateOutgoingUnidirectionalStream() {
  if (!ShouldCreateOutgoingStream()) {
    return nullptr;
  }

  QuicSimpleServerStream* stream = new QuicSimpleServerStream(
      GetNextOutgoingStreamId(), this, WRITE_UNIDIRECTIONAL,
      quic_simple_server_backend_);
  ActivateStream(QuicWrapUnique(stream));
  return stream;
}

void QuicSimpleServerSession::CloseStreamInner(QuicStreamId stream_id,
                                               bool locally_reset) {
  QuicSpdySession::CloseStreamInner(stream_id, locally_reset);
  HandlePromisedPushRequests();
}

void QuicSimpleServerSession::HandleFrameOnNonexistentOutgoingStream(
    QuicStreamId stream_id) {
  // If this stream is a promised but not created stream (stream_id within the
  // range of next_outgoing_stream_id_ and highes_promised_stream_id_),
  // connection shouldn't be closed.
  // Otherwise behave in the same way as base class.
  if (highest_promised_stream_id_ ==
          QuicUtils::GetInvalidStreamId(connection()->transport_version()) ||
      stream_id > highest_promised_stream_id_) {
    QuicSession::HandleFrameOnNonexistentOutgoingStream(stream_id);
  }
}

void QuicSimpleServerSession::HandleRstOnValidNonexistentStream(
    const QuicRstStreamFrame& frame) {
  QuicSession::HandleRstOnValidNonexistentStream(frame);
  if (!IsClosedStream(frame.stream_id)) {
    // If a nonexistent stream is not a closed stream and still valid, it must
    // be a locally preserved stream. Resetting this kind of stream means
    // cancelling the promised server push.
    // Since PromisedStreamInfo are queued in sequence, the corresponding
    // index for it in promised_streams_ can be calculated.
    DCHECK(frame.stream_id >= next_outgoing_stream_id());
    size_t index = (frame.stream_id - next_outgoing_stream_id()) / 2;
    DCHECK(index <= promised_streams_.size());
    promised_streams_[index].is_cancelled = true;
    control_frame_manager().WriteOrBufferRstStream(frame.stream_id,
                                                   QUIC_RST_ACKNOWLEDGEMENT, 0);
    connection()->OnStreamReset(frame.stream_id, QUIC_RST_ACKNOWLEDGEMENT);
  }
}

spdy::SpdyHeaderBlock QuicSimpleServerSession::SynthesizePushRequestHeaders(
    QuicString request_url,
    QuicBackendResponse::ServerPushInfo resource,
    const spdy::SpdyHeaderBlock& original_request_headers) {
  QuicUrl push_request_url = resource.request_url;

  spdy::SpdyHeaderBlock spdy_headers = original_request_headers.Clone();
  // :authority could be different from original request.
  spdy_headers[":authority"] = push_request_url.host();
  spdy_headers[":path"] = push_request_url.path();
  ;
  // Push request always use GET.
  spdy_headers[":method"] = "GET";
  spdy_headers["referer"] = request_url;
  spdy_headers[":scheme"] = push_request_url.scheme();
  // It is not possible to push a response to a request that includes a request
  // body.
  spdy_headers["content-length"] = "0";
  // Remove "host" field as push request is a directly generated HTTP2 request
  // which should use ":authority" instead of "host".
  spdy_headers.erase("host");
  return spdy_headers;
}

void QuicSimpleServerSession::SendPushPromise(QuicStreamId original_stream_id,
                                              QuicStreamId promised_stream_id,
                                              spdy::SpdyHeaderBlock headers) {
  QUIC_DLOG(INFO) << "stream " << original_stream_id
                  << " send PUSH_PROMISE for promised stream "
                  << promised_stream_id;
  WritePushPromise(original_stream_id, promised_stream_id, std::move(headers));
}

void QuicSimpleServerSession::HandlePromisedPushRequests() {
  while (!promised_streams_.empty() && ShouldCreateOutgoingStream()) {
    PromisedStreamInfo& promised_info = promised_streams_.front();
    DCHECK_EQ(next_outgoing_stream_id(), promised_info.stream_id);

    if (promised_info.is_cancelled) {
      // This stream has been reset by client. Skip this stream id.
      promised_streams_.pop_front();
      GetNextOutgoingStreamId();
      return;
    }

    QuicSimpleServerStream* promised_stream =
        static_cast<QuicSimpleServerStream*>(
            CreateOutgoingUnidirectionalStream());
    DCHECK_NE(promised_stream, nullptr);
    DCHECK_EQ(promised_info.stream_id, promised_stream->id());
    QUIC_DLOG(INFO) << "created server push stream " << promised_stream->id();

    promised_stream->SetPriority(promised_info.priority);

    spdy::SpdyHeaderBlock request_headers(
        std::move(promised_info.request_headers));

    promised_streams_.pop_front();
    promised_stream->PushResponse(std::move(request_headers));
  }
}

}  // namespace quic
