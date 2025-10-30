// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/proof_source_chromium.h"

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_view_util.h"
#include "crypto/openssl_util.h"
#include "crypto/sign.h"
#include "net/cert/x509_util.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/crypto_protocol.h"

using std::string;

namespace net {

ProofSourceChromium::ProofSourceChromium() = default;

ProofSourceChromium::~ProofSourceChromium() = default;

bool ProofSourceChromium::Initialize(const base::FilePath& cert_path,
                                     const base::FilePath& key_path,
                                     const base::FilePath& sct_path) {
  std::string cert_data;
  if (!base::ReadFileToString(cert_path, &cert_data)) {
    DLOG(FATAL) << "Unable to read certificates.";
    return false;
  }

  certs_in_file_ = X509Certificate::CreateCertificateListFromBytes(
      base::as_byte_span(cert_data), X509Certificate::FORMAT_AUTO);

  if (certs_in_file_.empty()) {
    DLOG(FATAL) << "No certificates.";
    return false;
  }

  std::vector<string> certs;
  for (const scoped_refptr<X509Certificate>& cert : certs_in_file_) {
    certs.emplace_back(
        x509_util::CryptoBufferAsStringPiece(cert->cert_buffer()));
  }
  chain_ = new quic::ProofSource::Chain(certs);

  std::string key_data;
  if (!base::ReadFileToString(key_path, &key_data)) {
    DLOG(FATAL) << "Unable to read key.";
    return false;
  }

  private_key_ = crypto::keypair::PrivateKey::FromPrivateKeyInfo(
      base::as_byte_span(key_data));
  if (!private_key_) {
    DLOG(FATAL) << "Unable to create private key.";
    return false;
  }

  // Loading of the signed certificate timestamp is optional.
  if (sct_path.empty())
    return true;

  if (!base::ReadFileToString(sct_path, &signed_certificate_timestamp_)) {
    DLOG(FATAL) << "Unable to read signed certificate timestamp.";
    return false;
  }

  return true;
}

bool ProofSourceChromium::GetProofInner(
    const quic::QuicSocketAddress& server_addr,
    const string& hostname,
    const string& server_config,
    quic::QuicTransportVersion quic_version,
    std::string_view chlo_hash,
    quiche::QuicheReferenceCountedPointer<quic::ProofSource::Chain>* out_chain,
    quic::QuicCryptoProof* proof) {
  DCHECK(proof != nullptr);
  DCHECK(private_key_) << " this: " << this;

  crypto::sign::Signer signer(crypto::sign::RSA_PSS_SHA256, *private_key_);
  signer.Update(
      base::byte_span_with_nul_from_cstring(quic::kProofSignatureLabel));
  uint32_t chlo_hash_len = chlo_hash.length();
  signer.Update(base::byte_span_from_ref(chlo_hash_len));
  signer.Update(base::as_byte_span(chlo_hash));
  signer.Update(base::as_byte_span(server_config));
  std::vector<uint8_t> signature = signer.Finish();

  proof->signature.assign(base::as_string_view(signature));

  *out_chain = chain_;
  VLOG(1) << "signature: " << base::HexEncode(proof->signature);
  proof->leaf_cert_scts = signed_certificate_timestamp_;
  return true;
}

void ProofSourceChromium::GetProof(const quic::QuicSocketAddress& server_addr,
                                   const quic::QuicSocketAddress& client_addr,
                                   const std::string& hostname,
                                   const std::string& server_config,
                                   quic::QuicTransportVersion quic_version,
                                   std::string_view chlo_hash,
                                   std::unique_ptr<Callback> callback) {
  // As a transitional implementation, just call the synchronous version of
  // GetProof, then invoke the callback with the results and destroy it.
  quiche::QuicheReferenceCountedPointer<quic::ProofSource::Chain> chain;
  string signature;
  string leaf_cert_sct;
  quic::QuicCryptoProof out_proof;

  const bool ok = GetProofInner(server_addr, hostname, server_config,
                                quic_version, chlo_hash, &chain, &out_proof);
  callback->Run(ok, chain, out_proof, nullptr /* details */);
}

quiche::QuicheReferenceCountedPointer<quic::ProofSource::Chain>
ProofSourceChromium::GetCertChain(const quic::QuicSocketAddress& server_address,
                                  const quic::QuicSocketAddress& client_address,
                                  const std::string& hostname,
                                  bool* cert_matched_sni) {
  *cert_matched_sni = false;
  if (!hostname.empty()) {
    for (const scoped_refptr<X509Certificate>& cert : certs_in_file_) {
      if (cert->VerifyNameMatch(hostname)) {
        *cert_matched_sni = true;
        break;
      }
    }
  }
  return chain_;
}

void ProofSourceChromium::ComputeTlsSignature(
    const quic::QuicSocketAddress& server_address,
    const quic::QuicSocketAddress& client_address,
    const std::string& hostname,
    uint16_t signature_algorithm,
    std::string_view in,
    std::unique_ptr<SignatureCallback> callback) {
  DCHECK(private_key_);
  std::vector<uint8_t> sig = crypto::sign::Sign(
      crypto::sign::RSA_PSS_SHA256, *private_key_, base::as_byte_span(in));
  callback->Run(true, std::string(base::as_string_view(sig)), nullptr);
}

absl::InlinedVector<uint16_t, 8>
ProofSourceChromium::SupportedTlsSignatureAlgorithms() const {
  // Allow all signature algorithms that BoringSSL allows.
  return {};
}

quic::ProofSource::TicketCrypter* ProofSourceChromium::GetTicketCrypter() {
  return ticket_crypter_.get();
}

void ProofSourceChromium::SetTicketCrypter(
    std::unique_ptr<quic::ProofSource::TicketCrypter> ticket_crypter) {
  ticket_crypter_ = std::move(ticket_crypter);
}

}  // namespace net
