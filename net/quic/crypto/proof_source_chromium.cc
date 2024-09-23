// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/quic/crypto/proof_source_chromium.h"

#include "base/strings/string_number_conversions.h"
#include "crypto/openssl_util.h"
#include "net/cert/x509_util.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/crypto_protocol.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

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

  const uint8_t* p = reinterpret_cast<const uint8_t*>(key_data.data());
  std::vector<uint8_t> input(p, p + key_data.size());
  private_key_ = crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(input);
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
  DCHECK(private_key_.get()) << " this: " << this;

  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  bssl::ScopedEVP_MD_CTX sign_context;
  EVP_PKEY_CTX* pkey_ctx;

  uint32_t len_tmp = chlo_hash.length();
  if (!EVP_DigestSignInit(sign_context.get(), &pkey_ctx, EVP_sha256(), nullptr,
                          private_key_->key()) ||
      !EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PSS_PADDING) ||
      !EVP_PKEY_CTX_set_rsa_pss_saltlen(pkey_ctx, -1) ||
      !EVP_DigestSignUpdate(
          sign_context.get(),
          reinterpret_cast<const uint8_t*>(quic::kProofSignatureLabel),
          sizeof(quic::kProofSignatureLabel)) ||
      !EVP_DigestSignUpdate(sign_context.get(),
                            reinterpret_cast<const uint8_t*>(&len_tmp),
                            sizeof(len_tmp)) ||
      !EVP_DigestSignUpdate(sign_context.get(),
                            reinterpret_cast<const uint8_t*>(chlo_hash.data()),
                            len_tmp) ||
      !EVP_DigestSignUpdate(
          sign_context.get(),
          reinterpret_cast<const uint8_t*>(server_config.data()),
          server_config.size())) {
    return false;
  }
  // Determine the maximum length of the signature.
  size_t len = 0;
  if (!EVP_DigestSignFinal(sign_context.get(), nullptr, &len)) {
    return false;
  }
  std::vector<uint8_t> signature(len);
  // Sign it.
  if (!EVP_DigestSignFinal(sign_context.get(), signature.data(), &len)) {
    return false;
  }
  signature.resize(len);
  proof->signature.assign(reinterpret_cast<const char*>(signature.data()),
                          signature.size());
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
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  bssl::ScopedEVP_MD_CTX sign_context;
  EVP_PKEY_CTX* pkey_ctx;

  size_t siglen;
  string sig;
  if (!EVP_DigestSignInit(sign_context.get(), &pkey_ctx, EVP_sha256(), nullptr,
                          private_key_->key()) ||
      !EVP_PKEY_CTX_set_rsa_padding(pkey_ctx, RSA_PKCS1_PSS_PADDING) ||
      !EVP_PKEY_CTX_set_rsa_pss_saltlen(pkey_ctx, -1) ||
      !EVP_DigestSignUpdate(sign_context.get(),
                            reinterpret_cast<const uint8_t*>(in.data()),
                            in.size()) ||
      !EVP_DigestSignFinal(sign_context.get(), nullptr, &siglen)) {
    callback->Run(false, sig, nullptr);
    return;
  }
  sig.resize(siglen);
  if (!EVP_DigestSignFinal(
          sign_context.get(),
          reinterpret_cast<uint8_t*>(const_cast<char*>(sig.data())), &siglen)) {
    callback->Run(false, sig, nullptr);
    return;
  }
  sig.resize(siglen);

  callback->Run(true, sig, nullptr);
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
