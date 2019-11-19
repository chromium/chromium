// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_PROOF_SOURCE_CHROMIUM_H_
#define NET_QUIC_CRYPTO_PROOF_SOURCE_CHROMIUM_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "crypto/rsa_private_key.h"
#include "net/base/net_export.h"
#include "net/cert/x509_certificate.h"
#include "net/third_party/quiche/src/quic/core/crypto/proof_source.h"

namespace net {

// ProofSourceChromium implements the QUIC quic::ProofSource interface.
// TODO(rtenneti): implement details of this class.
class NET_EXPORT_PRIVATE ProofSourceChromium : public quic::ProofSource {
 public:
  ProofSourceChromium();
  ~ProofSourceChromium() override;

  // Initializes this object based on the certificate chain in |cert_path|,
  // and the PKCS#8 RSA private key in |key_path|. Signed certificate
  // timestamp may be loaded from |sct_path| if it is non-empty.
  bool Initialize(const base::FilePath& cert_path,
                  const base::FilePath& key_path,
                  const base::FilePath& sct_path);

  // quic::ProofSource interface
  void GetProof(const quic::QuicSocketAddress& server_ip,
                const std::string& hostname,
                const std::string& server_config,
                quic::QuicTransportVersion quic_version,
                quic::QuicStringPiece chlo_hash,
                std::unique_ptr<Callback> callback) override;

  quic::QuicReferenceCountedPointer<Chain> GetCertChain(
      const quic::QuicSocketAddress& server_address,
      const std::string& hostname) override;

  void ComputeTlsSignature(
      const quic::QuicSocketAddress& server_address,
      const std::string& hostname,
      uint16_t signature_algorithm,
      quic::QuicStringPiece in,
      std::unique_ptr<SignatureCallback> callback) override;

 private:
  bool GetProofInner(
      const quic::QuicSocketAddress& server_ip,
      const std::string& hostname,
      const std::string& server_config,
      quic::QuicTransportVersion quic_version,
      quic::QuicStringPiece chlo_hash,
      quic::QuicReferenceCountedPointer<quic::ProofSource::Chain>* out_chain,
      quic::QuicCryptoProof* proof);

  std::unique_ptr<crypto::RSAPrivateKey> private_key_;
  quic::QuicReferenceCountedPointer<quic::ProofSource::Chain> chain_;
  std::string signed_certificate_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(ProofSourceChromium);
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_PROOF_SOURCE_CHROMIUM_H_
