// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/quic_crypto_server_handshaker.h"

#include <memory>

#include "net/third_party/quic/platform/api/quic_arraysize.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_text_utils.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace quic {

class QuicCryptoServerHandshaker::ProcessClientHelloCallback
    : public ProcessClientHelloResultCallback {
 public:
  ProcessClientHelloCallback(
      QuicCryptoServerHandshaker* parent,
      const QuicReferenceCountedPointer<
          ValidateClientHelloResultCallback::Result>& result)
      : parent_(parent), result_(result) {}

  void Run(
      QuicErrorCode error,
      const QuicString& error_details,
      std::unique_ptr<CryptoHandshakeMessage> message,
      std::unique_ptr<DiversificationNonce> diversification_nonce,
      std::unique_ptr<ProofSource::Details> proof_source_details) override {
    if (parent_ == nullptr) {
      return;
    }

    parent_->FinishProcessingHandshakeMessageAfterProcessClientHello(
        *result_, error, error_details, std::move(message),
        std::move(diversification_nonce), std::move(proof_source_details));
  }

  void Cancel() { parent_ = nullptr; }

 private:
  QuicCryptoServerHandshaker* parent_;
  QuicReferenceCountedPointer<ValidateClientHelloResultCallback::Result>
      result_;
};

QuicCryptoServerHandshaker::QuicCryptoServerHandshaker(
    const QuicCryptoServerConfig* crypto_config,
    QuicCryptoServerStream* stream,
    QuicCompressedCertsCache* compressed_certs_cache,
    QuicSession* session,
    QuicCryptoServerStream::Helper* helper)
    : QuicCryptoHandshaker(stream, session),
      stream_(stream),
      session_(session),
      crypto_config_(crypto_config),
      compressed_certs_cache_(compressed_certs_cache),
      signed_config_(new QuicSignedServerConfig),
      helper_(helper),
      num_handshake_messages_(0),
      num_handshake_messages_with_server_nonces_(0),
      send_server_config_update_cb_(nullptr),
      num_server_config_update_messages_sent_(0),
      zero_rtt_attempted_(false),
      chlo_packet_size_(0),
      validate_client_hello_cb_(nullptr),
      process_client_hello_cb_(nullptr),
      encryption_established_(false),
      handshake_confirmed_(false),
      crypto_negotiated_params_(new QuicCryptoNegotiatedParameters) {}

QuicCryptoServerHandshaker::~QuicCryptoServerHandshaker() {
  CancelOutstandingCallbacks();
}

void QuicCryptoServerHandshaker::CancelOutstandingCallbacks() {
  // Detach from the validation callback.  Calling this multiple times is safe.
  if (validate_client_hello_cb_ != nullptr) {
    validate_client_hello_cb_->Cancel();
    validate_client_hello_cb_ = nullptr;
  }
  if (send_server_config_update_cb_ != nullptr) {
    send_server_config_update_cb_->Cancel();
    send_server_config_update_cb_ = nullptr;
  }
  if (process_client_hello_cb_ != nullptr) {
    process_client_hello_cb_->Cancel();
    process_client_hello_cb_ = nullptr;
  }
}

void QuicCryptoServerHandshaker::OnHandshakeMessage(
    const CryptoHandshakeMessage& message) {
  QuicCryptoHandshaker::OnHandshakeMessage(message);
  ++num_handshake_messages_;
  chlo_packet_size_ = session()->connection()->GetCurrentPacket().length();

  // Do not process handshake messages after the handshake is confirmed.
  if (handshake_confirmed_) {
    stream_->CloseConnectionWithDetails(
        QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE,
        "Unexpected handshake message from client");
    return;
  }

  if (message.tag() != kCHLO) {
    stream_->CloseConnectionWithDetails(QUIC_INVALID_CRYPTO_MESSAGE_TYPE,
                                        "Handshake packet not CHLO");
    return;
  }

  if (validate_client_hello_cb_ != nullptr ||
      process_client_hello_cb_ != nullptr) {
    // Already processing some other handshake message.  The protocol
    // does not allow for clients to send multiple handshake messages
    // before the server has a chance to respond.
    stream_->CloseConnectionWithDetails(
        QUIC_CRYPTO_MESSAGE_WHILE_VALIDATING_CLIENT_HELLO,
        "Unexpected handshake message while processing CHLO");
    return;
  }

  CryptoUtils::HashHandshakeMessage(message, &chlo_hash_,
                                    Perspective::IS_SERVER);

  std::unique_ptr<ValidateCallback> cb(new ValidateCallback(this));
  DCHECK(validate_client_hello_cb_ == nullptr);
  DCHECK(process_client_hello_cb_ == nullptr);
  validate_client_hello_cb_ = cb.get();
  crypto_config_->ValidateClientHello(
      message, GetClientAddress().host(),
      session()->connection()->self_address(), transport_version(),
      session()->connection()->clock(), signed_config_, std::move(cb));
}

void QuicCryptoServerHandshaker::FinishProcessingHandshakeMessage(
    QuicReferenceCountedPointer<ValidateClientHelloResultCallback::Result>
        result,
    std::unique_ptr<ProofSource::Details> details) {
  const CryptoHandshakeMessage& message = result->client_hello;

  // Clear the callback that got us here.
  DCHECK(validate_client_hello_cb_ != nullptr);
  DCHECK(process_client_hello_cb_ == nullptr);
  validate_client_hello_cb_ = nullptr;

  if (stream_->UseStatelessRejectsIfPeerSupported()) {
    stream_->SetPeerSupportsStatelessRejects(
        QuicCryptoServerStreamBase::DoesPeerSupportStatelessRejects(message));
  }

  std::unique_ptr<ProcessClientHelloCallback> cb(
      new ProcessClientHelloCallback(this, result));
  process_client_hello_cb_ = cb.get();
  ProcessClientHello(result, std::move(details), std::move(cb));
}

void QuicCryptoServerHandshaker::
    FinishProcessingHandshakeMessageAfterProcessClientHello(
        const ValidateClientHelloResultCallback::Result& result,
        QuicErrorCode error,
        const QuicString& error_details,
        std::unique_ptr<CryptoHandshakeMessage> reply,
        std::unique_ptr<DiversificationNonce> diversification_nonce,
        std::unique_ptr<ProofSource::Details> proof_source_details) {
  // Clear the callback that got us here.
  DCHECK(process_client_hello_cb_ != nullptr);
  DCHECK(validate_client_hello_cb_ == nullptr);
  process_client_hello_cb_ = nullptr;

  const CryptoHandshakeMessage& message = result.client_hello;
  if (error != QUIC_NO_ERROR) {
    stream_->CloseConnectionWithDetails(error, error_details);
    return;
  }

  if (reply->tag() != kSHLO) {
    if (reply->tag() == kSREJ) {
      DCHECK(stream_->UseStatelessRejectsIfPeerSupported());
      DCHECK(stream_->PeerSupportsStatelessRejects());
      // Before sending the SREJ, cause the connection to save crypto packets
      // so that they can be added to the time wait list manager and
      // retransmitted.
      session()->connection()->EnableSavingCryptoPackets();
    }
    SendHandshakeMessage(*reply);

    if (reply->tag() == kSREJ) {
      DCHECK(stream_->UseStatelessRejectsIfPeerSupported());
      DCHECK(stream_->PeerSupportsStatelessRejects());
      DCHECK(!handshake_confirmed());
      QUIC_DLOG(INFO) << "Closing connection "
                      << session()->connection()->connection_id()
                      << " because of a stateless reject.";
      session()->connection()->CloseConnection(
          QUIC_CRYPTO_HANDSHAKE_STATELESS_REJECT, "stateless reject",
          ConnectionCloseBehavior::SILENT_CLOSE);
    }
    return;
  }

  // If we are returning a SHLO then we accepted the handshake.  Now
  // process the negotiated configuration options as part of the
  // session config.
  QuicConfig* config = session()->config();
  OverrideQuicConfigDefaults(config);
  QuicString process_error_details;
  const QuicErrorCode process_error =
      config->ProcessPeerHello(message, CLIENT, &process_error_details);
  if (process_error != QUIC_NO_ERROR) {
    stream_->CloseConnectionWithDetails(process_error, process_error_details);
    return;
  }

  session()->OnConfigNegotiated();

  config->ToHandshakeMessage(reply.get());

  // Receiving a full CHLO implies the client is prepared to decrypt with
  // the new server write key.  We can start to encrypt with the new server
  // write key.
  //
  // NOTE: the SHLO will be encrypted with the new server write key.
  session()->connection()->SetEncrypter(
      ENCRYPTION_INITIAL,
      std::move(crypto_negotiated_params_->initial_crypters.encrypter));
  session()->connection()->SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  // Set the decrypter immediately so that we no longer accept unencrypted
  // packets.
  session()->connection()->SetDecrypter(
      ENCRYPTION_INITIAL,
      std::move(crypto_negotiated_params_->initial_crypters.decrypter));
  session()->connection()->SetDiversificationNonce(*diversification_nonce);

  SendHandshakeMessage(*reply);

  session()->connection()->SetEncrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::move(crypto_negotiated_params_->forward_secure_crypters.encrypter));
  session()->connection()->SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);

  session()->connection()->SetAlternativeDecrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::move(crypto_negotiated_params_->forward_secure_crypters.decrypter),
      false /* don't latch */);

  encryption_established_ = true;
  handshake_confirmed_ = true;
  session()->OnCryptoHandshakeEvent(QuicSession::HANDSHAKE_CONFIRMED);
}

void QuicCryptoServerHandshaker::SendServerConfigUpdate(
    const CachedNetworkParameters* cached_network_params) {
  if (!handshake_confirmed_) {
    return;
  }

  if (send_server_config_update_cb_ != nullptr) {
    QUIC_DVLOG(1)
        << "Skipped server config update since one is already in progress";
    return;
  }

  std::unique_ptr<SendServerConfigUpdateCallback> cb(
      new SendServerConfigUpdateCallback(this));
  send_server_config_update_cb_ = cb.get();

  crypto_config_->BuildServerConfigUpdateMessage(
      session()->connection()->transport_version(), chlo_hash_,
      previous_source_address_tokens_, session()->connection()->self_address(),
      GetClientAddress().host(), session()->connection()->clock(),
      session()->connection()->random_generator(), compressed_certs_cache_,
      *crypto_negotiated_params_, cached_network_params, std::move(cb));
}

QuicCryptoServerHandshaker::SendServerConfigUpdateCallback::
    SendServerConfigUpdateCallback(QuicCryptoServerHandshaker* parent)
    : parent_(parent) {}

void QuicCryptoServerHandshaker::SendServerConfigUpdateCallback::Cancel() {
  parent_ = nullptr;
}

// From BuildServerConfigUpdateMessageResultCallback
void QuicCryptoServerHandshaker::SendServerConfigUpdateCallback::Run(
    bool ok,
    const CryptoHandshakeMessage& message) {
  if (parent_ == nullptr) {
    return;
  }
  parent_->FinishSendServerConfigUpdate(ok, message);
}

void QuicCryptoServerHandshaker::FinishSendServerConfigUpdate(
    bool ok,
    const CryptoHandshakeMessage& message) {
  // Clear the callback that got us here.
  DCHECK(send_server_config_update_cb_ != nullptr);
  send_server_config_update_cb_ = nullptr;

  if (!ok) {
    QUIC_DVLOG(1) << "Server: Failed to build server config update (SCUP)!";
    return;
  }

  QUIC_DVLOG(1) << "Server: Sending server config update: "
                << message.DebugString();
  const QuicData& data = message.GetSerialized();
  stream_->WriteOrBufferData(QuicStringPiece(data.data(), data.length()), false,
                             nullptr);

  ++num_server_config_update_messages_sent_;
}

uint8_t QuicCryptoServerHandshaker::NumHandshakeMessages() const {
  return num_handshake_messages_;
}

uint8_t QuicCryptoServerHandshaker::NumHandshakeMessagesWithServerNonces()
    const {
  return num_handshake_messages_with_server_nonces_;
}

int QuicCryptoServerHandshaker::NumServerConfigUpdateMessagesSent() const {
  return num_server_config_update_messages_sent_;
}

const CachedNetworkParameters*
QuicCryptoServerHandshaker::PreviousCachedNetworkParams() const {
  return previous_cached_network_params_.get();
}

bool QuicCryptoServerHandshaker::ZeroRttAttempted() const {
  return zero_rtt_attempted_;
}

void QuicCryptoServerHandshaker::SetPreviousCachedNetworkParams(
    CachedNetworkParameters cached_network_params) {
  previous_cached_network_params_.reset(
      new CachedNetworkParameters(cached_network_params));
}

bool QuicCryptoServerHandshaker::ShouldSendExpectCTHeader() const {
  return signed_config_->proof.send_expect_ct_header;
}

QuicLongHeaderType QuicCryptoServerHandshaker::GetLongHeaderType(
    QuicStreamOffset /*offset*/) const {
  if (last_sent_handshake_message_tag() == kSREJ) {
    return RETRY;
  }
  if (last_sent_handshake_message_tag() == kSHLO) {
    return ZERO_RTT_PROTECTED;
  }
  return HANDSHAKE;
}

bool QuicCryptoServerHandshaker::GetBase64SHA256ClientChannelID(
    QuicString* output) const {
  if (!encryption_established() ||
      crypto_negotiated_params_->channel_id.empty()) {
    return false;
  }

  const QuicString& channel_id(crypto_negotiated_params_->channel_id);
  uint8_t digest[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const uint8_t*>(channel_id.data()), channel_id.size(),
         digest);

  QuicTextUtils::Base64Encode(digest, QUIC_ARRAYSIZE(digest), output);
  return true;
}

bool QuicCryptoServerHandshaker::encryption_established() const {
  return encryption_established_;
}

bool QuicCryptoServerHandshaker::handshake_confirmed() const {
  return handshake_confirmed_;
}

const QuicCryptoNegotiatedParameters&
QuicCryptoServerHandshaker::crypto_negotiated_params() const {
  return *crypto_negotiated_params_;
}

CryptoMessageParser* QuicCryptoServerHandshaker::crypto_message_parser() {
  return QuicCryptoHandshaker::crypto_message_parser();
}

void QuicCryptoServerHandshaker::ProcessClientHello(
    QuicReferenceCountedPointer<ValidateClientHelloResultCallback::Result>
        result,
    std::unique_ptr<ProofSource::Details> proof_source_details,
    std::unique_ptr<ProcessClientHelloResultCallback> done_cb) {
  const CryptoHandshakeMessage& message = result->client_hello;
  QuicString error_details;
  if (!helper_->CanAcceptClientHello(
          message, GetClientAddress(), session()->connection()->peer_address(),
          session()->connection()->self_address(), &error_details)) {
    done_cb->Run(QUIC_HANDSHAKE_FAILED, error_details, nullptr, nullptr,
                 nullptr);
    return;
  }
  if (!result->info.server_nonce.empty()) {
    ++num_handshake_messages_with_server_nonces_;
  }

  if (num_handshake_messages_ == 1) {
    // Client attempts zero RTT handshake by sending a non-inchoate CHLO.
    QuicStringPiece public_value;
    zero_rtt_attempted_ = message.GetStringPiece(kPUBS, &public_value);
  }

  // Store the bandwidth estimate from the client.
  if (result->cached_network_params.bandwidth_estimate_bytes_per_second() > 0) {
    previous_cached_network_params_.reset(
        new CachedNetworkParameters(result->cached_network_params));
  }
  previous_source_address_tokens_ = result->info.source_address_tokens;

  const bool use_stateless_rejects_in_crypto_config =
      stream_->UseStatelessRejectsIfPeerSupported() &&
      stream_->PeerSupportsStatelessRejects();
  QuicConnection* connection = session()->connection();
  const QuicConnectionId server_designated_connection_id =
      GenerateConnectionIdForReject(use_stateless_rejects_in_crypto_config);
  crypto_config_->ProcessClientHello(
      result, /*reject_only=*/false, connection->connection_id(),
      connection->self_address(), GetClientAddress(), connection->version(),
      session()->supported_versions(), use_stateless_rejects_in_crypto_config,
      server_designated_connection_id, connection->clock(),
      connection->random_generator(), compressed_certs_cache_,
      crypto_negotiated_params_, signed_config_,
      QuicCryptoStream::CryptoMessageFramingOverhead(transport_version()),
      chlo_packet_size_, std::move(done_cb));
}

void QuicCryptoServerHandshaker::OverrideQuicConfigDefaults(
    QuicConfig* config) {}

QuicCryptoServerHandshaker::ValidateCallback::ValidateCallback(
    QuicCryptoServerHandshaker* parent)
    : parent_(parent) {}

void QuicCryptoServerHandshaker::ValidateCallback::Cancel() {
  parent_ = nullptr;
}

void QuicCryptoServerHandshaker::ValidateCallback::Run(
    QuicReferenceCountedPointer<Result> result,
    std::unique_ptr<ProofSource::Details> details) {
  if (parent_ != nullptr) {
    parent_->FinishProcessingHandshakeMessage(std::move(result),
                                              std::move(details));
  }
}

QuicConnectionId QuicCryptoServerHandshaker::GenerateConnectionIdForReject(
    bool use_stateless_rejects) {
  if (!use_stateless_rejects) {
    return 0;
  }
  return helper_->GenerateConnectionIdForReject(
      session()->connection()->connection_id());
}

const QuicSocketAddress QuicCryptoServerHandshaker::GetClientAddress() {
  return session()->connection()->peer_address();
}

}  // namespace quic
