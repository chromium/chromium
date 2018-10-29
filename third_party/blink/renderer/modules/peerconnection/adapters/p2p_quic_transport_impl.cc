// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport_impl.h"

#include "net/quic/quic_chromium_connection_helper.h"
#include "net/third_party/quic/core/crypto/proof_source.h"
#include "net/third_party/quic/core/crypto/quic_random.h"
#include "net/third_party/quic/core/quic_config.h"
#include "net/third_party/quic/core/tls_client_handshaker.h"
#include "net/third_party/quic/core/tls_server_handshaker.h"
#include "net/third_party/quic/tools/quic_simple_crypto_server_stream_helper.h"

namespace blink {

namespace {

static const char kClosingDetails[] = "Application closed connection.";
static const size_t kHostnameLength = 32;

// TODO(https://crbug.com/874300): Create a secure connection by implementing a
// P2PProofSource and P2PProofVerifier and remove these once the TLS 1.3
// handshake is implemented for QUIC. This will allow us to verify for both the
// server and client:
// - The self signed certificate fingerprint matches the remote
//   fingerprint that was signaled.
// - The peer owns the certificate, by verifying the signature of the hash of
//   the handshake context.
//
// Used by QuicCryptoServerConfig to provide dummy proof credentials
// (taken from quic/quartc).
class DummyProofSource : public quic::ProofSource {
 public:
  DummyProofSource() {}
  ~DummyProofSource() override {}

  // ProofSource override.
  void GetProof(const quic::QuicSocketAddress& server_addr,
                const quic::QuicString& hostname,
                const quic::QuicString& server_config,
                quic::QuicTransportVersion transport_version,
                quic::QuicStringPiece chlo_hash,
                std::unique_ptr<Callback> callback) override {
    quic::QuicReferenceCountedPointer<ProofSource::Chain> chain;
    quic::QuicCryptoProof proof;
    std::vector<quic::QuicString> certs;
    certs.push_back("Dummy cert");
    chain = new ProofSource::Chain(certs);
    proof.signature = "Dummy signature";
    proof.leaf_cert_scts = "Dummy timestamp";
    callback->Run(true, chain, proof, nullptr /* details */);
  }

  quic::QuicReferenceCountedPointer<Chain> GetCertChain(
      const quic::QuicSocketAddress& server_address,
      const quic::QuicString& hostname) override {
    return quic::QuicReferenceCountedPointer<Chain>();
  }

  void ComputeTlsSignature(
      const quic::QuicSocketAddress& server_address,
      const quic::QuicString& hostname,
      uint16_t signature_algorithm,
      quic::QuicStringPiece in,
      std::unique_ptr<SignatureCallback> callback) override {
    callback->Run(true, "Dummy signature");
  }
};

// Used by QuicCryptoClientConfig to ignore the peer's credentials
// and establish an insecure QUIC connection (taken from quic/quartc).
class InsecureProofVerifier : public quic::ProofVerifier {
 public:
  InsecureProofVerifier() {}
  ~InsecureProofVerifier() override {}

  // ProofVerifier override.
  quic::QuicAsyncStatus VerifyProof(
      const quic::QuicString& hostname,
      const uint16_t port,
      const quic::QuicString& server_config,
      quic::QuicTransportVersion transport_version,
      quic::QuicStringPiece chlo_hash,
      const std::vector<quic::QuicString>& certs,
      const quic::QuicString& cert_sct,
      const quic::QuicString& signature,
      const quic::ProofVerifyContext* context,
      quic::QuicString* error_details,
      std::unique_ptr<quic::ProofVerifyDetails>* verify_details,
      std::unique_ptr<quic::ProofVerifierCallback> callback) override {
    return quic::QUIC_SUCCESS;
  }

  quic::QuicAsyncStatus VerifyCertChain(
      const quic::QuicString& hostname,
      const std::vector<quic::QuicString>& certs,
      const quic::ProofVerifyContext* context,
      quic::QuicString* error_details,
      std::unique_ptr<quic::ProofVerifyDetails>* details,
      std::unique_ptr<quic::ProofVerifierCallback> callback) override {
    return quic::QUIC_SUCCESS;
  }

  std::unique_ptr<quic::ProofVerifyContext> CreateDefaultContext() override {
    return nullptr;
  }
};

// A dummy helper for a server crypto stream that accepts all client hellos
// and generates a random connection ID.
class DummyCryptoServerStreamHelper
    : public quic::QuicCryptoServerStream::Helper {
 public:
  explicit DummyCryptoServerStreamHelper(quic::QuicRandom* random)
      : random_(random) {}
  ~DummyCryptoServerStreamHelper() override {}

  quic::QuicConnectionId GenerateConnectionIdForReject(
      quic::QuicConnectionId connection_id) const override {
    return random_->RandUint64();
  }

  bool CanAcceptClientHello(const quic::CryptoHandshakeMessage& message,
                            const quic::QuicSocketAddress& client_address,
                            const quic::QuicSocketAddress& peer_address,
                            const quic::QuicSocketAddress& self_address,
                            quic::QuicString* error_details) const override {
    return true;
  }

 private:
  // Used to generate random connection IDs. Needs to outlive this.
  quic::QuicRandom* random_;
};
}  // namespace

P2PQuicTransportImpl::P2PQuicTransportImpl(
    P2PQuicTransportConfig p2p_transport_config,
    std::unique_ptr<net::QuicChromiumConnectionHelper> helper,
    std::unique_ptr<quic::QuicConnection> connection,
    const quic::QuicConfig& quic_config,
    quic::QuicClock* clock)
    : quic::QuicSession(connection.get(),
                        nullptr /* visitor */,
                        quic_config,
                        quic::CurrentSupportedVersions()),
      helper_(std::move(helper)),
      connection_(std::move(connection)),
      perspective_(p2p_transport_config.is_server
                       ? quic::Perspective::IS_SERVER
                       : quic::Perspective::IS_CLIENT),
      packet_transport_(p2p_transport_config.packet_transport),
      delegate_(p2p_transport_config.delegate),
      clock_(clock) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(delegate_);
  DCHECK(clock_);
  DCHECK(packet_transport_);
  DCHECK_GT(p2p_transport_config.certificates.size(), 0u);
  if (p2p_transport_config.can_respond_to_crypto_handshake) {
    InitializeCryptoStream();
  }
  // TODO(https://crbug.com/874296): The web API accepts multiple certificates,
  // and we might want to pass these down to let QUIC decide on what to use.
  certificate_ = p2p_transport_config.certificates[0];
  packet_transport_->SetReceiveDelegate(this);
}

P2PQuicTransportImpl::~P2PQuicTransportImpl() {
  packet_transport_->SetReceiveDelegate(nullptr);
}

void P2PQuicTransportImpl::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (IsClosed()) {
    return;
  }
  // The error code used for the connection closing is
  // quic::QUIC_CONNECTION_CANCELLED. This allows us to distinguish that the
  // application closed the connection, as opposed to it closing from a
  // failure/error.
  connection_->CloseConnection(
      quic::QuicErrorCode::QUIC_CONNECTION_CANCELLED, kClosingDetails,
      quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
}

void P2PQuicTransportImpl::Start(
    std::vector<std::unique_ptr<rtc::SSLFingerprint>> remote_fingerprints) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(remote_fingerprints_.size(), 0u);
  DCHECK_GT(remote_fingerprints.size(), 0u);
  if (IsClosed()) {
    // We could have received a close from the remote side before calling this.
    return;
  }
  // These will be used to verify the remote certificate during the handshake.
  remote_fingerprints_ = std::move(remote_fingerprints);

  if (perspective_ == quic::Perspective::IS_CLIENT) {
    quic::QuicCryptoClientStream* client_crypto_stream =
        static_cast<quic::QuicCryptoClientStream*>(crypto_stream_.get());
    client_crypto_stream->CryptoConnect();
  }
}

void P2PQuicTransportImpl::OnPacketDataReceived(const char* data,
                                                size_t data_len) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Received data from the |packet_transport_|. Create a QUIC packet and send
  // it to be processed by the QuicSession/Connection.
  quic::QuicReceivedPacket packet(data, data_len, clock_->Now());
  ProcessUdpPacket(connection()->self_address(), connection()->peer_address(),
                   packet);
}

quic::QuicCryptoStream* P2PQuicTransportImpl::GetMutableCryptoStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return crypto_stream_.get();
}

const quic::QuicCryptoStream* P2PQuicTransportImpl::GetCryptoStream() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return crypto_stream_.get();
}

P2PQuicStreamImpl* P2PQuicTransportImpl::CreateStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return CreateOutgoingBidirectionalStream();
}

P2PQuicStreamImpl* P2PQuicTransportImpl::CreateOutgoingBidirectionalStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  P2PQuicStreamImpl* stream = CreateStreamInternal(GetNextOutgoingStreamId());
  ActivateStream(std::unique_ptr<P2PQuicStreamImpl>(stream));
  return stream;
}

P2PQuicStreamImpl* P2PQuicTransportImpl::CreateIncomingStream(
    quic::QuicStreamId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  P2PQuicStreamImpl* stream = CreateStreamInternal(id);
  ActivateStream(std::unique_ptr<P2PQuicStreamImpl>(stream));
  delegate_->OnStream(stream);
  return stream;
}

P2PQuicStreamImpl* P2PQuicTransportImpl::CreateStreamInternal(
    quic::QuicStreamId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(crypto_stream_);
  DCHECK(IsEncryptionEstablished());
  DCHECK(!IsClosed());
  return new P2PQuicStreamImpl(id, this);
}

void P2PQuicTransportImpl::InitializeCryptoStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!crypto_stream_);
  switch (perspective_) {
    case quic::Perspective::IS_CLIENT: {
      if (!crypto_client_config_) {
        // The |crypto_client_config_| has not already been set (by the test).
        std::unique_ptr<quic::ProofVerifier> proof_verifier(
            new InsecureProofVerifier);
        crypto_client_config_ = std::make_unique<quic::QuicCryptoClientConfig>(
            std::move(proof_verifier),
            quic::TlsClientHandshaker::CreateSslCtx());
      }
      // The host must be unique for every endpoint the client communicates
      // with.
      char random_hostname[kHostnameLength];
      helper_->GetRandomGenerator()->RandBytes(random_hostname,
                                               kHostnameLength);
      quic::QuicServerId server_id(
          /*host=*/quic::QuicString(random_hostname, kHostnameLength),
          /*port=*/0,
          /*privacy_mode_enabled=*/false);
      crypto_stream_ = std::make_unique<quic::QuicCryptoClientStream>(
          server_id, /*QuicSession=*/this,
          crypto_client_config_->proof_verifier()->CreateDefaultContext(),
          crypto_client_config_.get(), /*ProofHandler=*/this);
      QuicSession::Initialize();
      break;
    }
    case quic::Perspective::IS_SERVER: {
      std::unique_ptr<quic::ProofSource> proof_source(new DummyProofSource);
      crypto_server_config_ = std::make_unique<quic::QuicCryptoServerConfig>(
          quic::QuicCryptoServerConfig::TESTING, helper_->GetRandomGenerator(),
          std::move(proof_source), quic::KeyExchangeSource::Default(),
          quic::TlsServerHandshaker::CreateSslCtx());
      // Provide server with serialized config string to prove ownership.
      quic::QuicCryptoServerConfig::ConfigOptions options;
      // The |message| is used to handle the return value of AddDefaultConfig
      // which is raw pointer of the CryptoHandshakeMessage.
      std::unique_ptr<quic::CryptoHandshakeMessage> message(
          crypto_server_config_->AddDefaultConfig(
              helper_->GetRandomGenerator(), helper_->GetClock(), options));
      compressed_certs_cache_.reset(new quic::QuicCompressedCertsCache(
          quic::QuicCompressedCertsCache::kQuicCompressedCertsCacheSize));
      bool use_stateless_rejects_if_peer_supported = false;
      server_stream_helper_ = std::make_unique<DummyCryptoServerStreamHelper>(
          helper_->GetRandomGenerator());

      crypto_stream_ = std::make_unique<quic::QuicCryptoServerStream>(
          crypto_server_config_.get(), compressed_certs_cache_.get(),
          use_stateless_rejects_if_peer_supported, this,
          server_stream_helper_.get());
      QuicSession::Initialize();
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void P2PQuicTransportImpl::OnCryptoHandshakeEvent(CryptoHandshakeEvent event) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  QuicSession::OnCryptoHandshakeEvent(event);
  if (event == HANDSHAKE_CONFIRMED) {
    DCHECK(IsEncryptionEstablished());
    DCHECK(IsCryptoHandshakeConfirmed());
    delegate_->OnConnected();
  }
}

void P2PQuicTransportImpl::OnConnectionClosed(
    quic::QuicErrorCode error,
    const std::string& error_details,
    quic::ConnectionCloseSource source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  quic::QuicSession::OnConnectionClosed(error, error_details, source);
  if (error != quic::QuicErrorCode::QUIC_CONNECTION_CANCELLED) {
    delegate_->OnConnectionFailed(
        error_details, source == quic::ConnectionCloseSource::FROM_PEER);
  } else if (source == quic::ConnectionCloseSource::FROM_PEER) {
    // This connection was closed by the application of the remote side.
    delegate_->OnRemoteStopped();
  }
}

bool P2PQuicTransportImpl::IsClosed() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return !connection_->connected();
}

void P2PQuicTransportImpl::set_crypto_client_config(
    std::unique_ptr<quic::QuicCryptoClientConfig> crypto_client_config) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  crypto_client_config_ = std::move(crypto_client_config);
}

}  // namespace blink
