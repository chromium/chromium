// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A client specific QuicSession subclass.

#ifndef NET_THIRD_PARTY_QUIC_CORE_HTTP_QUIC_SPDY_CLIENT_SESSION_H_
#define NET_THIRD_PARTY_QUIC_CORE_HTTP_QUIC_SPDY_CLIENT_SESSION_H_

#include <memory>

#include "base/macros.h"
#include "net/third_party/quic/core/http/quic_spdy_client_session_base.h"
#include "net/third_party/quic/core/http/quic_spdy_client_stream.h"
#include "net/third_party/quic/core/quic_crypto_client_stream.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/platform/api/quic_string.h"

namespace quic {

class QuicConnection;
class QuicServerId;

class QuicSpdyClientSession : public QuicSpdyClientSessionBase {
 public:
  // Takes ownership of |connection|. Caller retains ownership of
  // |promised_by_url|.
  QuicSpdyClientSession(const QuicConfig& config,
                        QuicConnection* connection,
                        const QuicServerId& server_id,
                        QuicCryptoClientConfig* crypto_config,
                        QuicClientPushPromiseIndex* push_promise_index);
  QuicSpdyClientSession(const QuicSpdyClientSession&) = delete;
  QuicSpdyClientSession& operator=(const QuicSpdyClientSession&) = delete;
  ~QuicSpdyClientSession() override;
  // Set up the QuicSpdyClientSession. Must be called prior to use.
  void Initialize() override;

  // QuicSession methods:
  QuicSpdyClientStream* CreateOutgoingBidirectionalStream() override;
  QuicSpdyClientStream* CreateOutgoingUnidirectionalStream() override;
  QuicCryptoClientStreamBase* GetMutableCryptoStream() override;
  const QuicCryptoClientStreamBase* GetCryptoStream() const override;

  bool IsAuthorized(const QuicString& authority) override;

  // QuicSpdyClientSessionBase methods:
  void OnProofValid(const QuicCryptoClientConfig::CachedState& cached) override;
  void OnProofVerifyDetailsAvailable(
      const ProofVerifyDetails& verify_details) override;

  // Performs a crypto handshake with the server.
  virtual void CryptoConnect();

  // Returns the number of client hello messages that have been sent on the
  // crypto stream. If the handshake has completed then this is one greater
  // than the number of round-trips needed for the handshake.
  int GetNumSentClientHellos() const;

  int GetNumReceivedServerConfigUpdates() const;

  void set_respect_goaway(bool respect_goaway) {
    respect_goaway_ = respect_goaway;
  }

 protected:
  // QuicSession methods:
  QuicSpdyStream* CreateIncomingStream(QuicStreamId id) override;
  // If an outgoing stream can be created, return true.
  bool ShouldCreateOutgoingStream() override;

  // If an incoming stream can be created, return true.
  bool ShouldCreateIncomingStream(QuicStreamId id) override;

  // Create the crypto stream. Called by Initialize().
  virtual std::unique_ptr<QuicCryptoClientStreamBase> CreateQuicCryptoStream();

  // Unlike CreateOutgoingBidirectionalStream, which applies a bunch of
  // sanity checks, this simply returns a new QuicSpdyClientStream. This may be
  // used by subclasses which want to use a subclass of QuicSpdyClientStream for
  // streams but wish to use the sanity checks in
  // CreateOutgoingBidirectionalStream.
  virtual std::unique_ptr<QuicSpdyClientStream> CreateClientStream();

  const QuicServerId& server_id() { return server_id_; }
  QuicCryptoClientConfig* crypto_config() { return crypto_config_; }

 private:
  std::unique_ptr<QuicCryptoClientStreamBase> crypto_stream_;
  QuicServerId server_id_;
  QuicCryptoClientConfig* crypto_config_;

  // If this is set to false, the client will ignore server GOAWAYs and allow
  // the creation of streams regardless of the high chance they will fail.
  bool respect_goaway_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_HTTP_QUIC_SPDY_CLIENT_SESSION_H_
