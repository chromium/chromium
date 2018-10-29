// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QUIC_CRYPTO_CLIENT_HANDSHAKER_H_
#define NET_THIRD_PARTY_QUIC_CORE_QUIC_CRYPTO_CLIENT_HANDSHAKER_H_

#include "net/third_party/quic/core/crypto/channel_id.h"
#include "net/third_party/quic/core/crypto/proof_verifier.h"
#include "net/third_party/quic/core/crypto/quic_crypto_client_config.h"
#include "net/third_party/quic/core/quic_crypto_client_stream.h"
#include "net/third_party/quic/core/quic_server_id.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_string.h"

namespace quic {

// An implementation of QuicCryptoClientStream::HandshakerDelegate which uses
// QUIC crypto as the crypto handshake protocol.
class QUIC_EXPORT_PRIVATE QuicCryptoClientHandshaker
    : public QuicCryptoClientStream::HandshakerDelegate,
      public QuicCryptoHandshaker {
 public:
  QuicCryptoClientHandshaker(
      const QuicServerId& server_id,
      QuicCryptoClientStream* stream,
      QuicSession* session,
      std::unique_ptr<ProofVerifyContext> verify_context,
      QuicCryptoClientConfig* crypto_config,
      QuicCryptoClientStream::ProofHandler* proof_handler);
  QuicCryptoClientHandshaker(const QuicCryptoClientHandshaker&) = delete;
  QuicCryptoClientHandshaker& operator=(const QuicCryptoClientHandshaker&) =
      delete;

  ~QuicCryptoClientHandshaker() override;

  // From QuicCryptoClientStream::HandshakerDelegate
  bool CryptoConnect() override;
  int num_sent_client_hellos() const override;
  int num_scup_messages_received() const override;
  bool WasChannelIDSent() const override;
  bool WasChannelIDSourceCallbackRun() const override;
  QuicLongHeaderType GetLongHeaderType(QuicStreamOffset offset) const override;
  QuicString chlo_hash() const override;
  bool encryption_established() const override;
  bool handshake_confirmed() const override;
  const QuicCryptoNegotiatedParameters& crypto_negotiated_params()
      const override;
  CryptoMessageParser* crypto_message_parser() override;

  // From QuicCryptoHandshaker
  void OnHandshakeMessage(const CryptoHandshakeMessage& message) override;

 private:
  // ChannelIDSourceCallbackImpl is passed as the callback method to
  // GetChannelIDKey. The ChannelIDSource calls this class with the result of
  // channel ID lookup when lookup is performed asynchronously.
  class ChannelIDSourceCallbackImpl : public ChannelIDSourceCallback {
   public:
    explicit ChannelIDSourceCallbackImpl(QuicCryptoClientHandshaker* parent);
    ~ChannelIDSourceCallbackImpl() override;

    // ChannelIDSourceCallback interface.
    void Run(std::unique_ptr<ChannelIDKey>* channel_id_key) override;

    // Cancel causes any future callbacks to be ignored. It must be called on
    // the same thread as the callback will be made on.
    void Cancel();

   private:
    QuicCryptoClientHandshaker* parent_;
  };

  // ProofVerifierCallbackImpl is passed as the callback method to VerifyProof.
  // The ProofVerifier calls this class with the result of proof verification
  // when verification is performed asynchronously.
  class ProofVerifierCallbackImpl : public ProofVerifierCallback {
   public:
    explicit ProofVerifierCallbackImpl(QuicCryptoClientHandshaker* parent);
    ~ProofVerifierCallbackImpl() override;

    // ProofVerifierCallback interface.
    void Run(bool ok,
             const QuicString& error_details,
             std::unique_ptr<ProofVerifyDetails>* details) override;

    // Cancel causes any future callbacks to be ignored. It must be called on
    // the same thread as the callback will be made on.
    void Cancel();

   private:
    QuicCryptoClientHandshaker* parent_;
  };

  enum State {
    STATE_IDLE,
    STATE_INITIALIZE,
    STATE_SEND_CHLO,
    STATE_RECV_REJ,
    STATE_VERIFY_PROOF,
    STATE_VERIFY_PROOF_COMPLETE,
    STATE_GET_CHANNEL_ID,
    STATE_GET_CHANNEL_ID_COMPLETE,
    STATE_RECV_SHLO,
    STATE_INITIALIZE_SCUP,
    STATE_NONE,
  };

  // Handles new server config and optional source-address token provided by the
  // server during a connection.
  void HandleServerConfigUpdateMessage(
      const CryptoHandshakeMessage& server_config_update);

  // DoHandshakeLoop performs a step of the handshake state machine. Note that
  // |in| may be nullptr if the call did not result from a received message.
  void DoHandshakeLoop(const CryptoHandshakeMessage* in);

  // Start the handshake process.
  void DoInitialize(QuicCryptoClientConfig::CachedState* cached);

  // Send either InchoateClientHello or ClientHello message to the server.
  void DoSendCHLO(QuicCryptoClientConfig::CachedState* cached);

  // Process REJ message from the server.
  void DoReceiveREJ(const CryptoHandshakeMessage* in,
                    QuicCryptoClientConfig::CachedState* cached);

  // Start the proof verification process. Returns the QuicAsyncStatus returned
  // by the ProofVerifier's VerifyProof.
  QuicAsyncStatus DoVerifyProof(QuicCryptoClientConfig::CachedState* cached);

  // If proof is valid then it sets the proof as valid (which persists the
  // server config). If not, it closes the connection.
  void DoVerifyProofComplete(QuicCryptoClientConfig::CachedState* cached);

  // Start the look up of Channel ID process. Returns either QUIC_SUCCESS if
  // RequiresChannelID returns false or QuicAsyncStatus returned by
  // GetChannelIDKey.
  QuicAsyncStatus DoGetChannelID(QuicCryptoClientConfig::CachedState* cached);

  // If there is no channel ID, then close the connection otherwise transtion to
  // STATE_SEND_CHLO state.
  void DoGetChannelIDComplete();

  // Process SHLO message from the server.
  void DoReceiveSHLO(const CryptoHandshakeMessage* in,
                     QuicCryptoClientConfig::CachedState* cached);

  // Start the proof verification if |server_id_| is https and |cached| has
  // signature.
  void DoInitializeServerConfigUpdate(
      QuicCryptoClientConfig::CachedState* cached);

  // Called to set the proof of |cached| valid.  Also invokes the session's
  // OnProofValid() method.
  void SetCachedProofValid(QuicCryptoClientConfig::CachedState* cached);

  // Returns true if the server crypto config in |cached| requires a ChannelID
  // and the client config settings also allow sending a ChannelID.
  bool RequiresChannelID(QuicCryptoClientConfig::CachedState* cached);

  // Returns the QuicSession that this stream belongs to.
  QuicSession* session() const { return session_; }

  QuicCryptoClientStream* stream_;

  QuicSession* session_;

  State next_state_;
  // num_client_hellos_ contains the number of client hello messages that this
  // connection has sent.
  int num_client_hellos_;

  QuicCryptoClientConfig* const crypto_config_;

  // SHA-256 hash of the most recently sent CHLO.
  QuicString chlo_hash_;

  // Server's (hostname, port, is_https, privacy_mode) tuple.
  const QuicServerId server_id_;

  // Generation counter from QuicCryptoClientConfig's CachedState.
  uint64_t generation_counter_;

  // True if a channel ID was sent.
  bool channel_id_sent_;

  // True if channel_id_source_callback_ was run.
  bool channel_id_source_callback_run_;

  // channel_id_source_callback_ contains the callback object that we passed
  // to an asynchronous channel ID lookup. The ChannelIDSource owns this
  // object.
  ChannelIDSourceCallbackImpl* channel_id_source_callback_;

  // These members are used to store the result of an asynchronous channel ID
  // lookup. These members must not be used after
  // STATE_GET_CHANNEL_ID_COMPLETE.
  std::unique_ptr<ChannelIDKey> channel_id_key_;

  // verify_context_ contains the context object that we pass to asynchronous
  // proof verifications.
  std::unique_ptr<ProofVerifyContext> verify_context_;

  // proof_verify_callback_ contains the callback object that we passed to an
  // asynchronous proof verification. The ProofVerifier owns this object.
  ProofVerifierCallbackImpl* proof_verify_callback_;
  // proof_handler_ contains the callback object used by a quic client
  // for proof verification. It is not owned by this class.
  QuicCryptoClientStream::ProofHandler* proof_handler_;

  // These members are used to store the result of an asynchronous proof
  // verification. These members must not be used after
  // STATE_VERIFY_PROOF_COMPLETE.
  bool verify_ok_;
  QuicString verify_error_details_;
  std::unique_ptr<ProofVerifyDetails> verify_details_;

  // True if the server responded to a previous CHLO with a stateless
  // reject.  Used for book-keeping between the STATE_RECV_REJ,
  // STATE_VERIFY_PROOF*, and subsequent STATE_SEND_CHLO state.
  bool stateless_reject_received_;

  // Only used in chromium, not internally.
  base::TimeTicks proof_verify_start_time_;

  int num_scup_messages_received_;

  bool encryption_established_;
  bool handshake_confirmed_;
  QuicReferenceCountedPointer<QuicCryptoNegotiatedParameters>
      crypto_negotiated_params_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QUIC_CRYPTO_CLIENT_HANDSHAKER_H_
