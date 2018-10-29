// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/http/quic_spdy_client_session_base.h"

#include "net/third_party/quic/core/http/quic_client_promised_info.h"
#include "net/third_party/quic/core/http/spdy_utils.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_string.h"

using spdy::SpdyHeaderBlock;

namespace quic {

QuicSpdyClientSessionBase::QuicSpdyClientSessionBase(
    QuicConnection* connection,
    QuicClientPushPromiseIndex* push_promise_index,
    const QuicConfig& config)
    : QuicSpdySession(connection,
                      nullptr,
                      config,
                      connection->supported_versions()),
      push_promise_index_(push_promise_index),
      largest_promised_stream_id_(
          QuicUtils::GetInvalidStreamId(connection->transport_version())) {}

QuicSpdyClientSessionBase::~QuicSpdyClientSessionBase() {
  //  all promised streams for this session
  for (auto& it : promised_by_id_) {
    QUIC_DVLOG(1) << "erase stream " << it.first << " url " << it.second->url();
    push_promise_index_->promised_by_url()->erase(it.second->url());
  }
  delete connection();
}

void QuicSpdyClientSessionBase::OnConfigNegotiated() {
  QuicSpdySession::OnConfigNegotiated();
}

void QuicSpdyClientSessionBase::OnCryptoHandshakeEvent(
    CryptoHandshakeEvent event) {
  QuicSpdySession::OnCryptoHandshakeEvent(event);
}

void QuicSpdyClientSessionBase::OnInitialHeadersComplete(
    QuicStreamId stream_id,
    const SpdyHeaderBlock& response_headers) {
  // Note that the strong ordering of the headers stream means that
  // QuicSpdyClientStream::OnPromiseHeadersComplete must have already
  // been called (on the associated stream) if this is a promised
  // stream. However, this stream may not have existed at this time,
  // hence the need to query the session.
  QuicClientPromisedInfo* promised = GetPromisedById(stream_id);
  if (!promised)
    return;

  promised->OnResponseHeaders(response_headers);
}

void QuicSpdyClientSessionBase::OnPromiseHeaderList(
    QuicStreamId stream_id,
    QuicStreamId promised_stream_id,
    size_t frame_len,
    const QuicHeaderList& header_list) {
  if (QuicContainsKey(static_streams(), stream_id)) {
    connection()->CloseConnection(
        QUIC_INVALID_HEADERS_STREAM_DATA, "stream is static",
        ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    return;
  }
  if (promised_stream_id !=
          QuicUtils::GetInvalidStreamId(connection()->transport_version()) &&
      largest_promised_stream_id_ !=
          QuicUtils::GetInvalidStreamId(connection()->transport_version()) &&
      promised_stream_id <= largest_promised_stream_id_) {
    connection()->CloseConnection(
        QUIC_INVALID_STREAM_ID,
        "Received push stream id lesser or equal to the"
        " last accepted before",
        ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    return;
  }
  if (!IsIncomingStream(promised_stream_id)) {
    connection()->CloseConnection(
        QUIC_INVALID_STREAM_ID, "Received push stream id for outgoing stream.",
        ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    return;
  }
  largest_promised_stream_id_ = promised_stream_id;

  QuicSpdyStream* stream = GetSpdyDataStream(stream_id);
  if (!stream) {
    // It's quite possible to receive headers after a stream has been reset.
    return;
  }
  stream->OnPromiseHeaderList(promised_stream_id, frame_len, header_list);
}

bool QuicSpdyClientSessionBase::HandlePromised(QuicStreamId /* associated_id */,
                                               QuicStreamId promised_id,
                                               const SpdyHeaderBlock& headers) {
  // Due to pathalogical packet re-ordering, it is possible that
  // frames for the promised stream have already arrived, and the
  // promised stream could be active or closed.
  if (IsClosedStream(promised_id)) {
    // There was a RST on the data stream already, perhaps
    // QUIC_REFUSED_STREAM?
    QUIC_DVLOG(1) << "Promise ignored for stream " << promised_id
                  << " that is already closed";
    return false;
  }

  if (push_promise_index_->promised_by_url()->size() >= get_max_promises()) {
    QUIC_DVLOG(1) << "Too many promises, rejecting promise for stream "
                  << promised_id;
    ResetPromised(promised_id, QUIC_REFUSED_STREAM);
    return false;
  }

  const QuicString url = SpdyUtils::GetPromisedUrlFromHeaders(headers);
  QuicClientPromisedInfo* old_promised = GetPromisedByUrl(url);
  if (old_promised) {
    QUIC_DVLOG(1) << "Promise for stream " << promised_id
                  << " is duplicate URL " << url
                  << " of previous promise for stream " << old_promised->id();
    ResetPromised(promised_id, QUIC_DUPLICATE_PROMISE_URL);
    return false;
  }

  if (GetPromisedById(promised_id)) {
    // OnPromiseHeadersComplete() would have closed the connection if
    // promised id is a duplicate.
    QUIC_BUG << "Duplicate promise for id " << promised_id;
    return false;
  }

  QuicClientPromisedInfo* promised =
      new QuicClientPromisedInfo(this, promised_id, url);
  std::unique_ptr<QuicClientPromisedInfo> promised_owner(promised);
  promised->Init();
  QUIC_DVLOG(1) << "stream " << promised_id << " emplace url " << url;
  (*push_promise_index_->promised_by_url())[url] = promised;
  promised_by_id_[promised_id] = std::move(promised_owner);
  bool result = promised->OnPromiseHeaders(headers);
  if (result) {
    DCHECK(promised_by_id_.find(promised_id) != promised_by_id_.end());
  }
  return result;
}

QuicClientPromisedInfo* QuicSpdyClientSessionBase::GetPromisedByUrl(
    const QuicString& url) {
  auto it = push_promise_index_->promised_by_url()->find(url);
  if (it != push_promise_index_->promised_by_url()->end()) {
    return it->second;
  }
  return nullptr;
}

QuicClientPromisedInfo* QuicSpdyClientSessionBase::GetPromisedById(
    const QuicStreamId id) {
  auto it = promised_by_id_.find(id);
  if (it != promised_by_id_.end()) {
    return it->second.get();
  }
  return nullptr;
}

QuicSpdyStream* QuicSpdyClientSessionBase::GetPromisedStream(
    const QuicStreamId id) {
  DynamicStreamMap::iterator it = dynamic_streams().find(id);
  if (it != dynamic_streams().end()) {
    return static_cast<QuicSpdyStream*>(it->second.get());
  }
  return nullptr;
}

void QuicSpdyClientSessionBase::DeletePromised(
    QuicClientPromisedInfo* promised) {
  push_promise_index_->promised_by_url()->erase(promised->url());
  // Since promised_by_id_ contains the unique_ptr, this will destroy
  // promised.
  promised_by_id_.erase(promised->id());
  headers_stream()->MaybeReleaseSequencerBuffer();
}

void QuicSpdyClientSessionBase::OnPushStreamTimedOut(QuicStreamId stream_id) {}

void QuicSpdyClientSessionBase::ResetPromised(
    QuicStreamId id,
    QuicRstStreamErrorCode error_code) {
  SendRstStream(id, error_code, 0);
  if (!IsOpenStream(id)) {
    MaybeIncreaseLargestPeerStreamId(id);
  }
}

void QuicSpdyClientSessionBase::CloseStreamInner(QuicStreamId stream_id,
                                                 bool locally_reset) {
  QuicSpdySession::CloseStreamInner(stream_id, locally_reset);
  headers_stream()->MaybeReleaseSequencerBuffer();
}

bool QuicSpdyClientSessionBase::ShouldReleaseHeadersStreamSequencerBuffer() {
  return num_active_requests() == 0 && promised_by_id_.empty();
}

}  // namespace quic
