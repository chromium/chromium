// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/src/quic/core/crypto/proof_source.h"
#include "net/third_party/quiche/src/quic/core/crypto/proof_verifier.h"

#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_crypto_config_factory_impl.h"

namespace blink {

namespace {

// Length of HKDF input keying material, equal to its number of bytes.
// https://tools.ietf.org/html/rfc5869#section-2.2.
const size_t kInputKeyingMaterialLength = 32;

// TODO(https://crbug.com/874300): Implement a secure QUIC handshake, meaning
// that both side's certificates are verified. This can be done by creating a
// P2PProofSource and P2PProofVerifier, and removing these objects once the
// TLS 1.3 handshake is implemented for QUIC.
// - The self signed certificate fingerprint matches the remote
//   fingerprint that was signaled.
// - The peer owns the certificate, by verifying the signature of the hash of
//   the handshake context.
//
// Ignores the peer's credentials (taken from quic/quartc).
class InsecureProofVerifier : public quic::ProofVerifier {
 public:
  InsecureProofVerifier() {}
  ~InsecureProofVerifier() override {}

  // ProofVerifier override.
  quic::QuicAsyncStatus VerifyProof(
      const std::string& hostname,
      const uint16_t port,
      const std::string& server_config,
      quic::QuicTransportVersion transport_version,
      quic::QuicStringPiece chlo_hash,
      const std::vector<std::string>& certs,
      const std::string& cert_sct,
      const std::string& signature,
      const quic::ProofVerifyContext* context,
      std::string* error_details,
      std::unique_ptr<quic::ProofVerifyDetails>* verify_details,
      std::unique_ptr<quic::ProofVerifierCallback> callback) override {
    return quic::QUIC_SUCCESS;
  }

  quic::QuicAsyncStatus VerifyCertChain(
      const std::string& hostname,
      const std::vector<std::string>& certs,
      const std::string& ocsp_response,
      const std::string& cert_sct,
      const quic::ProofVerifyContext* context,
      std::string* error_details,
      std::unique_ptr<quic::ProofVerifyDetails>* details,
      std::unique_ptr<quic::ProofVerifierCallback> callback) override {
    return quic::QUIC_SUCCESS;
  }

  std::unique_ptr<quic::ProofVerifyContext> CreateDefaultContext() override {
    return nullptr;
  }
};

}  // namespace

// Used by QuicCryptoServerConfig to provide dummy proof credentials
// (taken from quic/quartc).
class DummyProofSource : public quic::ProofSource {
 public:
  DummyProofSource() {}
  ~DummyProofSource() override {}

  // ProofSource override.
  void GetProof(const quic::QuicSocketAddress& server_addr,
                const std::string& hostname,
                const std::string& server_config,
                quic::QuicTransportVersion transport_version,
                quic::QuicStringPiece chlo_hash,
                std::unique_ptr<Callback> callback) override {
    quic::QuicCryptoProof proof;
    proof.signature = "Dummy signature";
    proof.leaf_cert_scts = "Dummy timestamp";
    callback->Run(true, GetCertChain(server_addr, hostname), proof,
                  nullptr /* details */);
  }

  quic::QuicReferenceCountedPointer<Chain> GetCertChain(
      const quic::QuicSocketAddress& server_address,
      const std::string& hostname) override {
    std::vector<std::string> certs;
    certs.push_back("Dummy cert");
    return quic::QuicReferenceCountedPointer<Chain>(
        new quic::ProofSource::Chain(certs));
  }
  void ComputeTlsSignature(
      const quic::QuicSocketAddress& server_address,
      const std::string& hostname,
      uint16_t signature_algorithm,
      quic::QuicStringPiece in,
      std::unique_ptr<SignatureCallback> callback) override {
    callback->Run(true, "Dummy signature");
  }
};

P2PQuicCryptoConfigFactoryImpl::P2PQuicCryptoConfigFactoryImpl(
    quic::QuicRandom* const random_generator)
    : random_generator_(random_generator) {}

std::unique_ptr<quic::QuicCryptoClientConfig>
P2PQuicCryptoConfigFactoryImpl::CreateClientCryptoConfig() {
  std::unique_ptr<quic::ProofVerifier> proof_verifier(
      new InsecureProofVerifier);
  return std::make_unique<quic::QuicCryptoClientConfig>(
      std::move(proof_verifier));
}

std::unique_ptr<quic::QuicCryptoServerConfig>
P2PQuicCryptoConfigFactoryImpl::CreateServerCryptoConfig() {
  // Generate a random source address token secret every time since this is
  // a transient client.
  char source_address_token_secret[kInputKeyingMaterialLength];
  random_generator_->RandBytes(source_address_token_secret,
                               kInputKeyingMaterialLength);
  std::unique_ptr<quic::ProofSource> proof_source(new DummyProofSource);
  return std::make_unique<quic::QuicCryptoServerConfig>(
      std::string(source_address_token_secret, kInputKeyingMaterialLength),
      random_generator_, std::move(proof_source),
      quic::KeyExchangeSource::Default());
}

}  // namespace blink
