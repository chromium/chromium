// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/x509_certificate.h"
#include "net/quic/quic_context.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/proof_source.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/proof_verifier.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/crypto_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

using std::string;

namespace net::test {
namespace {

// TestProofVerifierCallback is a simple callback for a quic::ProofVerifier that
// signals a TestCompletionCallback when called and stores the results from the
// quic::ProofVerifier in pointers passed to the constructor.
class TestProofVerifierCallback : public quic::ProofVerifierCallback {
 public:
  TestProofVerifierCallback(TestCompletionCallback* comp_callback,
                            bool* ok,
                            string* error_details)
      : comp_callback_(comp_callback), ok_(ok), error_details_(error_details) {}

  void Run(bool ok,
           const string& error_details,
           std::unique_ptr<quic::ProofVerifyDetails>* details) override {
    *ok_ = ok;
    *error_details_ = error_details;

    comp_callback_->callback().Run(0);
  }

 private:
  const raw_ptr<TestCompletionCallback> comp_callback_;
  const raw_ptr<bool> ok_;
  const raw_ptr<string> error_details_;
};

// RunVerification runs |verifier->VerifyProof| and asserts that the result
// matches |expected_ok|.
void RunVerification(quic::ProofVerifier* verifier,
                     const string& hostname,
                     const uint16_t port,
                     const string& server_config,
                     quic::QuicTransportVersion quic_version,
                     std::string_view chlo_hash,
                     const std::vector<string>& certs,
                     const string& proof,
                     bool expected_ok) {
  std::unique_ptr<quic::ProofVerifyDetails> details;
  TestCompletionCallback comp_callback;
  bool ok;
  string error_details;
  std::unique_ptr<quic::ProofVerifyContext> verify_context(
      quic::test::crypto_test_utils::ProofVerifyContextForTesting());
  auto callback = std::make_unique<TestProofVerifierCallback>(
      &comp_callback, &ok, &error_details);

  quic::QuicAsyncStatus status = verifier->VerifyProof(
      hostname, port, server_config, quic_version, chlo_hash, certs, "", proof,
      verify_context.get(), &error_details, &details, std::move(callback));

  switch (status) {
    case quic::QUIC_FAILURE:
      ASSERT_FALSE(expected_ok);
      ASSERT_NE("", error_details);
      return;
    case quic::QUIC_SUCCESS:
      ASSERT_TRUE(expected_ok);
      ASSERT_EQ("", error_details);
      return;
    case quic::QUIC_PENDING:
      comp_callback.WaitForResult();
      ASSERT_EQ(expected_ok, ok);
      break;
  }
}

class TestCallback : public quic::ProofSource::Callback {
 public:
  explicit TestCallback(
      bool* called,
      bool* ok,
      quiche::QuicheReferenceCountedPointer<quic::ProofSource::Chain>* chain,
      quic::QuicCryptoProof* proof)
      : called_(called), ok_(ok), chain_(chain), proof_(proof) {}

  void Run(
      bool ok,
      const quiche::QuicheReferenceCountedPointer<quic::ProofSource::Chain>&
          chain,
      const quic::QuicCryptoProof& proof,
      std::unique_ptr<quic::ProofSource::Details> /* details */) override {
    *ok_ = ok;
    *chain_ = chain;
    *proof_ = proof;
    *called_ = true;
  }

 private:
  raw_ptr<bool> called_;
  raw_ptr<bool> ok_;
  raw_ptr<quiche::QuicheReferenceCountedPointer<quic::ProofSource::Chain>>
      chain_;
  raw_ptr<quic::QuicCryptoProof> proof_;
};

class ProofTest : public ::testing::TestWithParam<quic::ParsedQuicVersion> {};

}  // namespace

INSTANTIATE_TEST_SUITE_P(QuicTransportVersion,
                         ProofTest,
                         ::testing::ValuesIn(AllSupportedQuicVersions()),
                         ::testing::PrintToStringParamName());

TEST_P(ProofTest, Verify) {
  std::unique_ptr<quic::ProofSource> source(
      quic::test::crypto_test_utils::ProofSourceForTesting());
  std::unique_ptr<quic::ProofVerifier> verifier(
      quic::test::crypto_test_utils::ProofVerifierForTesting());

  const string server_config = "server config bytes";
  const string hostname = "test.example.com";
  const uint16_t port = 8443;
  const string first_chlo_hash = "first chlo hash bytes";
  const string second_chlo_hash = "first chlo hash bytes";
  const quic::QuicTransportVersion quic_version = GetParam().transport_version;

  bool called = false;
  bool first_called = false;
  bool ok, first_ok;
  quiche::QuicheReferenceCountedPointer<quic::ProofSource::Chain> chain;
  quiche::QuicheReferenceCountedPointer<quic::ProofSource::Chain> first_chain;
  string error_details;
  quic::QuicCryptoProof proof, first_proof;
  quic::QuicSocketAddress server_addr;
  quic::QuicSocketAddress client_addr;

  auto cb = std::make_unique<TestCallback>(&called, &ok, &chain, &proof);
  auto first_cb = std::make_unique<TestCallback>(&first_called, &first_ok,
                                                 &first_chain, &first_proof);

  // GetProof here expects the async method to invoke the callback
  // synchronously.
  source->GetProof(server_addr, client_addr, hostname, server_config,
                   quic_version, first_chlo_hash, std::move(first_cb));
  source->GetProof(server_addr, client_addr, hostname, server_config,
                   quic_version, second_chlo_hash, std::move(cb));
  ASSERT_TRUE(called);
  ASSERT_TRUE(first_called);
  ASSERT_TRUE(ok);
  ASSERT_TRUE(first_ok);

  // Check that the proof source is caching correctly:
  ASSERT_EQ(first_chain->certs, chain->certs);
  ASSERT_NE(proof.signature, first_proof.signature);
  ASSERT_EQ(first_proof.leaf_cert_scts, proof.leaf_cert_scts);

  RunVerification(verifier.get(), hostname, port, server_config, quic_version,
                  first_chlo_hash, chain->certs, proof.signature, true);

  RunVerification(verifier.get(), "foo.com", port, server_config, quic_version,
                  first_chlo_hash, chain->certs, proof.signature, false);

  RunVerification(verifier.get(), server_config.substr(1, string::npos), port,
                  server_config, quic_version, first_chlo_hash, chain->certs,
                  proof.signature, false);

  const string corrupt_signature = "1" + proof.signature;
  RunVerification(verifier.get(), hostname, port, server_config, quic_version,
                  first_chlo_hash, chain->certs, corrupt_signature, false);

  std::vector<string> wrong_certs;
  for (size_t i = 1; i < chain->certs.size(); i++) {
    wrong_certs.push_back(chain->certs[i]);
  }

  RunVerification(verifier.get(), "foo.com", port, server_config, quic_version,
                  first_chlo_hash, wrong_certs, corrupt_signature, false);
}

namespace {

class TestingSignatureCallback : public quic::ProofSource::SignatureCallback {
 public:
  TestingSignatureCallback(bool* ok_out, std::string* signature_out)
      : ok_out_(ok_out), signature_out_(signature_out) {}

  void Run(bool ok,
           std::string signature,
           std::unique_ptr<quic::ProofSource::Details> /*details*/) override {
    *ok_out_ = ok;
    *signature_out_ = std::move(signature);
  }

 private:
  raw_ptr<bool> ok_out_;
  raw_ptr<std::string> signature_out_;
};

}  // namespace

TEST_P(ProofTest, TlsSignature) {
  std::unique_ptr<quic::ProofSource> source(
      quic::test::crypto_test_utils::ProofSourceForTesting());

  quic::QuicSocketAddress server_address;
  const string hostname = "test.example.com";

  quic::QuicSocketAddress client_address;

  bool cert_matched_sni;
  quiche::QuicheReferenceCountedPointer<quic::ProofSource::Chain> chain =
      source->GetCertChain(server_address, client_address, hostname,
                           &cert_matched_sni);
  ASSERT_GT(chain->certs.size(), 0ul);

  // Generate a value to be signed similar to the example in TLS 1.3 section
  // 4.4.3. The value to be signed starts with octed 0x20 repeated 64 times,
  // followed by the context string, followed by a single 0 byte, followed by
  // the transcript hash. Since there's no TLS stack here, we're using 32 bytes
  // of 01 as the transcript hash.
  string to_be_signed(64, ' ');
  to_be_signed.append("TLS 1.3, server CertificateVerify");
  to_be_signed.append(1, '\0');
  to_be_signed.append(32, 1);

  string sig;
  bool success;
  std::unique_ptr<TestingSignatureCallback> callback =
      std::make_unique<TestingSignatureCallback>(&success, &sig);
  source->ComputeTlsSignature(server_address, client_address, hostname,
                              SSL_SIGN_RSA_PSS_SHA256, to_be_signed,
                              std::move(callback));
  EXPECT_TRUE(success);

  // Verify that the signature from ComputeTlsSignature can be verified with the
  // leaf cert from GetCertChain.
  const uint8_t* data;
  const uint8_t* orig_data;
  orig_data = data = reinterpret_cast<const uint8_t*>(chain->certs[0].data());
  bssl::UniquePtr<X509> leaf(d2i_X509(nullptr, &data, chain->certs[0].size()));
  ASSERT_NE(leaf.get(), nullptr);
  EXPECT_EQ(data - orig_data, static_cast<ptrdiff_t>(chain->certs[0].size()));
  bssl::UniquePtr<EVP_PKEY> pkey(X509_get_pubkey(leaf.get()));
  bssl::ScopedEVP_MD_CTX md_ctx;
  EVP_PKEY_CTX* ctx;
  ASSERT_EQ(EVP_DigestVerifyInit(md_ctx.get(), &ctx, EVP_sha256(), nullptr,
                                 pkey.get()),
            1);
  ASSERT_EQ(EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PSS_PADDING), 1);
  ASSERT_EQ(EVP_PKEY_CTX_set_rsa_pss_saltlen(ctx, -1), 1);
  ASSERT_EQ(EVP_DigestVerifyUpdate(md_ctx.get(), to_be_signed.data(),
                                   to_be_signed.size()),
            1);
  EXPECT_EQ(EVP_DigestVerifyFinal(md_ctx.get(),
                                  reinterpret_cast<const uint8_t*>(sig.data()),
                                  sig.size()),
            1);
}

TEST_P(ProofTest, UseAfterFree) {
  std::unique_ptr<quic::ProofSource> source(
      quic::test::crypto_test_utils::ProofSourceForTesting());

  const string server_config = "server config bytes";
  const string hostname = "test.example.com";
  const string chlo_hash = "proof nonce bytes";
  bool called = false;
  bool ok;
  quiche::QuicheReferenceCountedPointer<quic::ProofSource::Chain> chain;
  string error_details;
  quic::QuicCryptoProof proof;
  quic::QuicSocketAddress server_addr;
  quic::QuicSocketAddress client_addr;
  auto cb = std::make_unique<TestCallback>(&called, &ok, &chain, &proof);

  // GetProof here expects the async method to invoke the callback
  // synchronously.
  source->GetProof(server_addr, client_addr, hostname, server_config,
                   GetParam().transport_version, chlo_hash, std::move(cb));
  ASSERT_TRUE(called);
  ASSERT_TRUE(ok);

  // Make sure we can safely access results after deleting where they came from.
  EXPECT_FALSE(chain->HasOneRef());
  source = nullptr;
  EXPECT_TRUE(chain->HasOneRef());

  EXPECT_FALSE(chain->certs.empty());
  for (const string& cert : chain->certs) {
    EXPECT_FALSE(cert.empty());
  }
}

}  // namespace net::test
