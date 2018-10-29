// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/tls_server_handshaker.h"

#include <memory>

#include "net/third_party/quic/core/crypto/quic_crypto_server_config.h"
#include "net/third_party/quic/core/crypto/transport_parameters.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace quic {

TlsServerHandshaker::SignatureCallback::SignatureCallback(
    TlsServerHandshaker* handshaker)
    : handshaker_(handshaker) {}

void TlsServerHandshaker::SignatureCallback::Run(bool ok,
                                                 QuicString signature) {
  if (handshaker_ == nullptr) {
    return;
  }
  if (ok) {
    handshaker_->cert_verify_sig_ = std::move(signature);
  }
  State last_state = handshaker_->state_;
  handshaker_->state_ = STATE_SIGNATURE_COMPLETE;
  handshaker_->signature_callback_ = nullptr;
  if (last_state == STATE_SIGNATURE_PENDING) {
    handshaker_->AdvanceHandshake();
  }
}

void TlsServerHandshaker::SignatureCallback::Cancel() {
  handshaker_ = nullptr;
}

const SSL_PRIVATE_KEY_METHOD TlsServerHandshaker::kPrivateKeyMethod{
    &TlsServerHandshaker::PrivateKeySign,
    nullptr,  // decrypt
    &TlsServerHandshaker::PrivateKeyComplete,
};

// static
bssl::UniquePtr<SSL_CTX> TlsServerHandshaker::CreateSslCtx() {
  bssl::UniquePtr<SSL_CTX> ssl_ctx = TlsHandshaker::CreateSslCtx();
  SSL_CTX_set_tlsext_servername_callback(
      ssl_ctx.get(), TlsServerHandshaker::SelectCertificateCallback);
  return ssl_ctx;
}

TlsServerHandshaker::TlsServerHandshaker(QuicCryptoStream* stream,
                                         QuicSession* session,
                                         SSL_CTX* ssl_ctx,
                                         ProofSource* proof_source)
    : TlsHandshaker(stream, session, ssl_ctx),
      proof_source_(proof_source),
      crypto_negotiated_params_(new QuicCryptoNegotiatedParameters) {
  CrypterPair crypters;
  CryptoUtils::CreateTlsInitialCrypters(Perspective::IS_SERVER,
                                        session->connection_id(), &crypters);
  session->connection()->SetEncrypter(ENCRYPTION_NONE,
                                      std::move(crypters.encrypter));
  session->connection()->SetDecrypter(ENCRYPTION_NONE,
                                      std::move(crypters.decrypter));

  // Configure the SSL to be a server.
  SSL_set_accept_state(ssl());

  if (!SetTransportParameters()) {
    CloseConnection("Failed to set Transport Parameters");
  }
}

TlsServerHandshaker::~TlsServerHandshaker() {
  CancelOutstandingCallbacks();
}

void TlsServerHandshaker::CancelOutstandingCallbacks() {
  if (signature_callback_) {
    signature_callback_->Cancel();
    signature_callback_ = nullptr;
  }
}

bool TlsServerHandshaker::GetBase64SHA256ClientChannelID(
    QuicString* output) const {
  // Channel ID is not supported when TLS is used in QUIC.
  return false;
}

void TlsServerHandshaker::SendServerConfigUpdate(
    const CachedNetworkParameters* cached_network_params) {
  // SCUP messages aren't supported when using the TLS handshake.
}

uint8_t TlsServerHandshaker::NumHandshakeMessages() const {
  // TODO(nharper): Return a sensible value here.
  return 0;
}

uint8_t TlsServerHandshaker::NumHandshakeMessagesWithServerNonces() const {
  // TODO(nharper): Return a sensible value here.
  return 0;
}

int TlsServerHandshaker::NumServerConfigUpdateMessagesSent() const {
  // SCUP messages aren't supported when using the TLS handshake.
  return 0;
}

const CachedNetworkParameters*
TlsServerHandshaker::PreviousCachedNetworkParams() const {
  return nullptr;
}

bool TlsServerHandshaker::ZeroRttAttempted() const {
  // TODO(nharper): Support 0-RTT with TLS 1.3 in QUIC.
  return false;
}

void TlsServerHandshaker::SetPreviousCachedNetworkParams(
    CachedNetworkParameters cached_network_params) {}

bool TlsServerHandshaker::ShouldSendExpectCTHeader() const {
  return false;
}

QuicLongHeaderType TlsServerHandshaker::GetLongHeaderType(
    QuicStreamOffset /*offset*/) const {
  // TODO(fayang): Returns the right value when actually using TLS handshaker.
  return HANDSHAKE;
}

bool TlsServerHandshaker::encryption_established() const {
  return encryption_established_;
}

bool TlsServerHandshaker::handshake_confirmed() const {
  return handshake_confirmed_;
}

const QuicCryptoNegotiatedParameters&
TlsServerHandshaker::crypto_negotiated_params() const {
  return *crypto_negotiated_params_;
}

CryptoMessageParser* TlsServerHandshaker::crypto_message_parser() {
  return TlsHandshaker::crypto_message_parser();
}

void TlsServerHandshaker::AdvanceHandshake() {
  if (state_ == STATE_CONNECTION_CLOSED) {
    QUIC_LOG(INFO) << "TlsServerHandshaker received handshake message after "
                      "connection was closed";
    return;
  }
  if (state_ == STATE_HANDSHAKE_COMPLETE) {
    // TODO(nharper): Handle post-handshake messages.
    return;
  }

  int rv = SSL_do_handshake(ssl());
  if (rv == 1) {
    FinishHandshake();
    return;
  }

  int ssl_error = SSL_get_error(ssl(), rv);
  bool should_close = true;
  switch (state_) {
    case STATE_LISTENING:
    case STATE_SIGNATURE_COMPLETE:
      should_close = ssl_error != SSL_ERROR_WANT_READ;
      break;
    case STATE_SIGNATURE_PENDING:
      should_close = ssl_error != SSL_ERROR_WANT_PRIVATE_KEY_OPERATION;
      break;
    default:
      should_close = true;
  }
  if (should_close) {
    QUIC_LOG(WARNING) << "SSL_do_handshake failed; SSL_get_error returns "
                      << ssl_error << ", state_ = " << state_;
    ERR_print_errors_fp(stderr);
    CloseConnection("TLS Handshake failed");
  }
}

void TlsServerHandshaker::CloseConnection(const QuicString& reason_phrase) {
  // TODO(nharper): Instead of QUIC_HANDSHAKE_FAILED, this should be
  // TLS_HANDSHAKE_FAILED (0x0201), but according to quic_error_codes.h,
  // we only send 1-byte error codes right now.
  state_ = STATE_CONNECTION_CLOSED;
  stream()->CloseConnectionWithDetails(QUIC_HANDSHAKE_FAILED, reason_phrase);
}

bool TlsServerHandshaker::ProcessTransportParameters(
    QuicString* error_details) {
  TransportParameters client_params;
  const uint8_t* client_params_bytes;
  size_t params_bytes_len;
  SSL_get_peer_quic_transport_params(ssl(), &client_params_bytes,
                                     &params_bytes_len);
  if (params_bytes_len == 0 ||
      !ParseTransportParameters(client_params_bytes, params_bytes_len,
                                Perspective::IS_CLIENT, &client_params)) {
    *error_details = "Unable to parse Transport Parameters";
    return false;
  }
  if (CryptoUtils::ValidateClientHelloVersion(
          client_params.version, session()->connection()->version(),
          session()->supported_versions(), error_details) != QUIC_NO_ERROR ||
      session()->config()->ProcessTransportParameters(
          client_params, CLIENT, error_details) != QUIC_NO_ERROR) {
    return false;
  }

  session()->OnConfigNegotiated();
  return true;
}

bool TlsServerHandshaker::SetTransportParameters() {
  TransportParameters server_params;
  server_params.perspective = Perspective::IS_SERVER;
  server_params.supported_versions =
      CreateQuicVersionLabelVector(session()->supported_versions());
  server_params.version =
      CreateQuicVersionLabel(session()->connection()->version());

  if (!session()->config()->FillTransportParameters(&server_params)) {
    return false;
  }

  // TODO(nharper): Provide an actual value for the stateless reset token.
  server_params.stateless_reset_token.resize(16);
  std::vector<uint8_t> server_params_bytes;
  if (!SerializeTransportParameters(server_params, &server_params_bytes) ||
      SSL_set_quic_transport_params(ssl(), server_params_bytes.data(),
                                    server_params_bytes.size()) != 1) {
    return false;
  }
  return true;
}

void TlsServerHandshaker::FinishHandshake() {
  QUIC_LOG(INFO) << "Server: handshake finished";
  state_ = STATE_HANDSHAKE_COMPLETE;
  std::vector<uint8_t> client_secret, server_secret;
  if (!DeriveSecrets(&client_secret, &server_secret)) {
    CloseConnection("Failed to derive shared secrets");
    return;
  }

  QUIC_LOG(INFO) << "Server: setting crypters";
  std::unique_ptr<QuicEncrypter> initial_encrypter =
      CreateEncrypter(server_secret);
  session()->connection()->SetEncrypter(ENCRYPTION_INITIAL,
                                        std::move(initial_encrypter));
  std::unique_ptr<QuicEncrypter> encrypter = CreateEncrypter(server_secret);
  session()->connection()->SetEncrypter(ENCRYPTION_FORWARD_SECURE,
                                        std::move(encrypter));

  std::unique_ptr<QuicDecrypter> initial_decrypter =
      CreateDecrypter(client_secret);
  session()->connection()->SetDecrypter(ENCRYPTION_INITIAL,
                                        std::move(initial_decrypter));
  std::unique_ptr<QuicDecrypter> decrypter = CreateDecrypter(client_secret);
  session()->connection()->SetAlternativeDecrypter(ENCRYPTION_FORWARD_SECURE,
                                                   std::move(decrypter), true);

  session()->connection()->SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);

  session()->NeuterUnencryptedData();
  encryption_established_ = true;
  handshake_confirmed_ = true;
}

// static
TlsServerHandshaker* TlsServerHandshaker::HandshakerFromSsl(SSL* ssl) {
  return static_cast<TlsServerHandshaker*>(
      TlsHandshaker::HandshakerFromSsl(ssl));
}

// static
ssl_private_key_result_t TlsServerHandshaker::PrivateKeySign(SSL* ssl,
                                                             uint8_t* out,
                                                             size_t* out_len,
                                                             size_t max_out,
                                                             uint16_t sig_alg,
                                                             const uint8_t* in,
                                                             size_t in_len) {
  return HandshakerFromSsl(ssl)->PrivateKeySign(
      out, out_len, max_out, sig_alg,
      QuicStringPiece(reinterpret_cast<const char*>(in), in_len));
}

ssl_private_key_result_t TlsServerHandshaker::PrivateKeySign(
    uint8_t* out,
    size_t* out_len,
    size_t max_out,
    uint16_t sig_alg,
    QuicStringPiece in) {
  signature_callback_ = new SignatureCallback(this);
  proof_source_->ComputeTlsSignature(
      session()->connection()->self_address(), hostname_, sig_alg, in,
      std::unique_ptr<SignatureCallback>(signature_callback_));
  if (state_ == STATE_SIGNATURE_COMPLETE) {
    return PrivateKeyComplete(out, out_len, max_out);
  }
  state_ = STATE_SIGNATURE_PENDING;
  return ssl_private_key_retry;
}

// static
ssl_private_key_result_t TlsServerHandshaker::PrivateKeyComplete(
    SSL* ssl,
    uint8_t* out,
    size_t* out_len,
    size_t max_out) {
  return HandshakerFromSsl(ssl)->PrivateKeyComplete(out, out_len, max_out);
}

ssl_private_key_result_t TlsServerHandshaker::PrivateKeyComplete(
    uint8_t* out,
    size_t* out_len,
    size_t max_out) {
  if (state_ == STATE_SIGNATURE_PENDING) {
    return ssl_private_key_retry;
  }
  if (cert_verify_sig_.size() > max_out || cert_verify_sig_.empty()) {
    return ssl_private_key_failure;
  }
  *out_len = cert_verify_sig_.size();
  memcpy(out, cert_verify_sig_.data(), *out_len);
  cert_verify_sig_.clear();
  cert_verify_sig_.shrink_to_fit();
  return ssl_private_key_success;
}

// static
int TlsServerHandshaker::SelectCertificateCallback(SSL* ssl,
                                                   int* out_alert,
                                                   void* arg) {
  return HandshakerFromSsl(ssl)->SelectCertificate(out_alert);
}

int TlsServerHandshaker::SelectCertificate(int* out_alert) {
  const char* hostname = SSL_get_servername(ssl(), TLSEXT_NAMETYPE_host_name);
  if (hostname) {
    hostname_ = hostname;
  } else {
    QUIC_LOG(INFO) << "No hostname indicated in SNI";
  }

  QuicReferenceCountedPointer<ProofSource::Chain> chain =
      proof_source_->GetCertChain(session()->connection()->self_address(),
                                  hostname_);
  if (chain->certs.empty()) {
    QUIC_LOG(ERROR) << "No certs provided for host '" << hostname_ << "'";
    return SSL_TLSEXT_ERR_ALERT_FATAL;
  }

  std::vector<CRYPTO_BUFFER*> certs;
  certs.resize(chain->certs.size());
  for (size_t i = 0; i < certs.size(); i++) {
    certs[i] = CRYPTO_BUFFER_new(
        reinterpret_cast<const uint8_t*>(chain->certs[i].data()),
        chain->certs[i].length(), nullptr);
  }

  SSL_set_chain_and_key(ssl(), certs.data(), certs.size(), nullptr,
                        &kPrivateKeyMethod);

  for (size_t i = 0; i < certs.size(); i++) {
    CRYPTO_BUFFER_free(certs[i]);
  }

  QuicString error_details;
  if (!ProcessTransportParameters(&error_details)) {
    CloseConnection(error_details);
    *out_alert = SSL_AD_INTERNAL_ERROR;
    return SSL_TLSEXT_ERR_ALERT_FATAL;
  }

  QUIC_LOG(INFO) << "Set " << chain->certs.size() << " certs for server";
  return SSL_TLSEXT_ERR_OK;
}

}  // namespace quic
