// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/core/tls_client_handshaker.h"
#include "net/third_party/quic/core/tls_server_handshaker.h"
#include "net/third_party/quic/platform/api/quic_arraysize.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quic/test_tools/fake_proof_source.h"
#include "net/third_party/quic/test_tools/mock_quic_session_visitor.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"

namespace quic {
namespace test {
namespace {

using ::testing::_;

class FakeProofVerifier : public ProofVerifier {
 public:
  FakeProofVerifier()
      : verifier_(crypto_test_utils::ProofVerifierForTesting()) {}

  QuicAsyncStatus VerifyProof(
      const QuicString& hostname,
      const uint16_t port,
      const QuicString& server_config,
      QuicTransportVersion quic_version,
      QuicStringPiece chlo_hash,
      const std::vector<QuicString>& certs,
      const QuicString& cert_sct,
      const QuicString& signature,
      const ProofVerifyContext* context,
      QuicString* error_details,
      std::unique_ptr<ProofVerifyDetails>* details,
      std::unique_ptr<ProofVerifierCallback> callback) override {
    return verifier_->VerifyProof(
        hostname, port, server_config, quic_version, chlo_hash, certs, cert_sct,
        signature, context, error_details, details, std::move(callback));
  }

  QuicAsyncStatus VerifyCertChain(
      const QuicString& hostname,
      const std::vector<QuicString>& certs,
      const ProofVerifyContext* context,
      QuicString* error_details,
      std::unique_ptr<ProofVerifyDetails>* details,
      std::unique_ptr<ProofVerifierCallback> callback) override {
    if (!active_) {
      return verifier_->VerifyCertChain(hostname, certs, context, error_details,
                                        details, std::move(callback));
    }
    pending_ops_.push_back(QuicMakeUnique<VerifyChainPendingOp>(
        hostname, certs, context, error_details, details, std::move(callback),
        verifier_.get()));
    return QUIC_PENDING;
  }

  std::unique_ptr<ProofVerifyContext> CreateDefaultContext() override {
    return nullptr;
  }

  void Activate() { active_ = true; }

  size_t NumPendingCallbacks() const { return pending_ops_.size(); }

  void InvokePendingCallback(size_t n) {
    CHECK(NumPendingCallbacks() > n);
    pending_ops_[n]->Run();
    auto it = pending_ops_.begin() + n;
    pending_ops_.erase(it);
  }

 private:
  // Implementation of ProofVerifierCallback that fails if the callback is ever
  // run.
  class FailingProofVerifierCallback : public ProofVerifierCallback {
   public:
    void Run(bool ok,
             const QuicString& error_details,
             std::unique_ptr<ProofVerifyDetails>* details) override {
      FAIL();
    }
  };

  class VerifyChainPendingOp {
   public:
    VerifyChainPendingOp(const QuicString& hostname,
                         const std::vector<QuicString>& certs,
                         const ProofVerifyContext* context,
                         QuicString* error_details,
                         std::unique_ptr<ProofVerifyDetails>* details,
                         std::unique_ptr<ProofVerifierCallback> callback,
                         ProofVerifier* delegate)
        : hostname_(hostname),
          certs_(certs),
          context_(context),
          error_details_(error_details),
          details_(details),
          callback_(std::move(callback)),
          delegate_(delegate) {}

    void Run() {
      // FakeProofVerifier depends on crypto_test_utils::ProofVerifierForTesting
      // running synchronously. It passes a FailingProofVerifierCallback and
      // runs the original callback after asserting that the verification ran
      // synchronously.
      QuicAsyncStatus status = delegate_->VerifyCertChain(
          hostname_, certs_, context_, error_details_, details_,
          QuicMakeUnique<FailingProofVerifierCallback>());
      ASSERT_NE(status, QUIC_PENDING);
      callback_->Run(status == QUIC_SUCCESS, *error_details_, details_);
    }

   private:
    QuicString hostname_;
    std::vector<QuicString> certs_;
    const ProofVerifyContext* context_;
    QuicString* error_details_;
    std::unique_ptr<ProofVerifyDetails>* details_;
    std::unique_ptr<ProofVerifierCallback> callback_;
    ProofVerifier* delegate_;
  };

  std::unique_ptr<ProofVerifier> verifier_;
  bool active_ = false;
  std::vector<std::unique_ptr<VerifyChainPendingOp>> pending_ops_;
};

class TestQuicCryptoStream : public QuicCryptoStream {
 public:
  explicit TestQuicCryptoStream(QuicSession* session)
      : QuicCryptoStream(session) {}

  ~TestQuicCryptoStream() override = default;

  virtual TlsHandshaker* handshaker() const = 0;

  QuicLongHeaderType GetLongHeaderType(QuicStreamOffset offset) const override {
    return handshaker()->GetLongHeaderType(offset);
  }

  bool encryption_established() const override {
    return handshaker()->encryption_established();
  }

  bool handshake_confirmed() const override {
    return handshaker()->handshake_confirmed();
  }

  const QuicCryptoNegotiatedParameters& crypto_negotiated_params()
      const override {
    return handshaker()->crypto_negotiated_params();
  }

  CryptoMessageParser* crypto_message_parser() override {
    return handshaker()->crypto_message_parser();
  }

  void WriteCryptoData(const QuicStringPiece& data) override {
    pending_writes_.push_back(QuicString(data));
  }

  const std::vector<QuicString>& pending_writes() { return pending_writes_; }

  // Sends the pending frames to |stream| and clears the array of pending
  // writes.
  void SendFramesToStream(QuicCryptoStream* stream) {
    QUIC_LOG(INFO) << "Sending " << pending_writes_.size() << " frames";
    for (size_t i = 0; i < pending_writes_.size(); ++i) {
      QuicStreamFrame frame(QuicUtils::GetCryptoStreamId(
                                session()->connection()->transport_version()),
                            false, stream->stream_bytes_read(),
                            pending_writes_[i]);
      stream->OnStreamFrame(frame);
    }
    pending_writes_.clear();
  }

 private:
  std::vector<QuicString> pending_writes_;
};

class TestQuicCryptoClientStream : public TestQuicCryptoStream {
 public:
  explicit TestQuicCryptoClientStream(QuicSession* session)
      : TestQuicCryptoStream(session),
        proof_verifier_(new FakeProofVerifier),
        ssl_ctx_(TlsClientHandshaker::CreateSslCtx()),
        handshaker_(new TlsClientHandshaker(
            this,
            session,
            QuicServerId("test.example.com", 443, false),
            proof_verifier_.get(),
            ssl_ctx_.get(),
            crypto_test_utils::ProofVerifyContextForTesting(),
            "quic-tester")) {}

  ~TestQuicCryptoClientStream() override = default;

  TlsHandshaker* handshaker() const override { return handshaker_.get(); }

  bool CryptoConnect() { return handshaker_->CryptoConnect(); }

  FakeProofVerifier* GetFakeProofVerifier() const {
    return proof_verifier_.get();
  }

 private:
  std::unique_ptr<FakeProofVerifier> proof_verifier_;
  bssl::UniquePtr<SSL_CTX> ssl_ctx_;
  std::unique_ptr<TlsClientHandshaker> handshaker_;
};

class TestQuicCryptoServerStream : public TestQuicCryptoStream {
 public:
  TestQuicCryptoServerStream(QuicSession* session,
                             FakeProofSource* proof_source)
      : TestQuicCryptoStream(session),
        proof_source_(proof_source),
        ssl_ctx_(TlsServerHandshaker::CreateSslCtx()),
        handshaker_(new TlsServerHandshaker(this,
                                            session,
                                            ssl_ctx_.get(),
                                            proof_source_)) {}

  ~TestQuicCryptoServerStream() override = default;

  void CancelOutstandingCallbacks() {
    handshaker_->CancelOutstandingCallbacks();
  }

  TlsHandshaker* handshaker() const override { return handshaker_.get(); }

  FakeProofSource* GetFakeProofSource() const { return proof_source_; }

 private:
  FakeProofSource* proof_source_;
  bssl::UniquePtr<SSL_CTX> ssl_ctx_;
  std::unique_ptr<TlsServerHandshaker> handshaker_;
};

void MoveStreamFrames(TestQuicCryptoStream* client,
                      TestQuicCryptoStream* server) {
  while (!client->pending_writes().empty() ||
         !server->pending_writes().empty()) {
    client->SendFramesToStream(server);
    server->SendFramesToStream(client);
  }
}

class TlsHandshakerTest : public QuicTest {
 public:
  TlsHandshakerTest()
      : client_conn_(new MockQuicConnection(&conn_helper_,
                                            &alarm_factory_,
                                            Perspective::IS_CLIENT)),
        server_conn_(new MockQuicConnection(&conn_helper_,
                                            &alarm_factory_,
                                            Perspective::IS_SERVER)),
        client_session_(client_conn_, /*create_mock_crypto_stream=*/false),
        server_session_(server_conn_, /*create_mock_crypto_stream=*/false) {
    client_stream_ = new TestQuicCryptoClientStream(&client_session_);
    client_session_.SetCryptoStream(client_stream_);
    server_stream_ =
        new TestQuicCryptoServerStream(&server_session_, &proof_source_);
    server_session_.SetCryptoStream(server_stream_);
    client_session_.Initialize();
    server_session_.Initialize();
    EXPECT_FALSE(client_stream_->encryption_established());
    EXPECT_FALSE(client_stream_->handshake_confirmed());
    EXPECT_FALSE(server_stream_->encryption_established());
    EXPECT_FALSE(server_stream_->handshake_confirmed());
  }

  MockQuicConnectionHelper conn_helper_;
  MockAlarmFactory alarm_factory_;
  MockQuicConnection* client_conn_;
  MockQuicConnection* server_conn_;
  MockQuicSession client_session_;
  MockQuicSession server_session_;

  FakeProofSource proof_source_;
  TestQuicCryptoClientStream* client_stream_;
  TestQuicCryptoServerStream* server_stream_;
};

TEST_F(TlsHandshakerTest, CryptoHandshake) {
  EXPECT_CALL(*client_conn_, CloseConnection(_, _, _)).Times(0);
  EXPECT_CALL(*server_conn_, CloseConnection(_, _, _)).Times(0);
  client_stream_->CryptoConnect();
  MoveStreamFrames(client_stream_, server_stream_);

  EXPECT_TRUE(client_stream_->handshake_confirmed());
  EXPECT_TRUE(client_stream_->encryption_established());
  EXPECT_TRUE(server_stream_->handshake_confirmed());
  EXPECT_TRUE(server_stream_->encryption_established());
}

TEST_F(TlsHandshakerTest, HandshakeWithAsyncProofSource) {
  EXPECT_CALL(*client_conn_, CloseConnection(_, _, _)).Times(0);
  EXPECT_CALL(*server_conn_, CloseConnection(_, _, _)).Times(0);
  // Enable FakeProofSource to capture call to ComputeTlsSignature and run it
  // asynchronously.
  FakeProofSource* proof_source = server_stream_->GetFakeProofSource();
  proof_source->Activate();

  // Start handshake.
  client_stream_->CryptoConnect();
  MoveStreamFrames(client_stream_, server_stream_);

  ASSERT_EQ(proof_source->NumPendingCallbacks(), 1);
  proof_source->InvokePendingCallback(0);

  MoveStreamFrames(client_stream_, server_stream_);

  EXPECT_TRUE(client_stream_->handshake_confirmed());
  EXPECT_TRUE(client_stream_->encryption_established());
  EXPECT_TRUE(server_stream_->handshake_confirmed());
  EXPECT_TRUE(server_stream_->encryption_established());
}

TEST_F(TlsHandshakerTest, CancelPendingProofSource) {
  EXPECT_CALL(*client_conn_, CloseConnection(_, _, _)).Times(0);
  EXPECT_CALL(*server_conn_, CloseConnection(_, _, _)).Times(0);
  // Enable FakeProofSource to capture call to ComputeTlsSignature and run it
  // asynchronously.
  FakeProofSource* proof_source = server_stream_->GetFakeProofSource();
  proof_source->Activate();

  // Start handshake.
  client_stream_->CryptoConnect();
  MoveStreamFrames(client_stream_, server_stream_);

  ASSERT_EQ(proof_source->NumPendingCallbacks(), 1);
  server_stream_ = nullptr;

  proof_source->InvokePendingCallback(0);
}

TEST_F(TlsHandshakerTest, HandshakeWithAsyncProofVerifier) {
  EXPECT_CALL(*client_conn_, CloseConnection(_, _, _)).Times(0);
  EXPECT_CALL(*server_conn_, CloseConnection(_, _, _)).Times(0);
  // Enable FakeProofVerifier to capture call to VerifyCertChain and run it
  // asynchronously.
  FakeProofVerifier* proof_verifier = client_stream_->GetFakeProofVerifier();
  proof_verifier->Activate();

  // Start handshake.
  client_stream_->CryptoConnect();
  MoveStreamFrames(client_stream_, server_stream_);

  ASSERT_EQ(proof_verifier->NumPendingCallbacks(), 1u);
  proof_verifier->InvokePendingCallback(0);

  MoveStreamFrames(client_stream_, server_stream_);

  EXPECT_TRUE(client_stream_->handshake_confirmed());
  EXPECT_TRUE(client_stream_->encryption_established());
  EXPECT_TRUE(server_stream_->handshake_confirmed());
  EXPECT_TRUE(server_stream_->encryption_established());
}

TEST_F(TlsHandshakerTest, ClientConnectionClosedOnTlsAlert) {
  // Have client send ClientHello.
  client_stream_->CryptoConnect();
  EXPECT_CALL(*client_conn_, CloseConnection(QUIC_HANDSHAKE_FAILED, _, _));

  // Send fake "internal_error" fatal TLS alert from server to client.
  char alert_msg[] = {
      // TLSPlaintext struct:
      21,          // ContentType alert
      0x03, 0x01,  // ProcotolVersion legacy_record_version
      0, 2,        // uint16 length
      // Alert struct (TLSPlaintext fragment):
      2,   // AlertLevel fatal
      80,  // AlertDescription internal_error
  };
  QuicStreamFrame alert(
      QuicUtils::GetCryptoStreamId(client_conn_->transport_version()), false,
      client_stream_->stream_bytes_read(),
      QuicStringPiece(alert_msg, QUIC_ARRAYSIZE(alert_msg)));
  client_stream_->OnStreamFrame(alert);

  EXPECT_FALSE(client_stream_->handshake_confirmed());
}

TEST_F(TlsHandshakerTest, ServerConnectionClosedOnTlsAlert) {
  EXPECT_CALL(*server_conn_, CloseConnection(QUIC_HANDSHAKE_FAILED, _, _));

  // Send fake "internal_error" fatal TLS alert from client to server.
  char alert_msg[] = {
      // TLSPlaintext struct:
      21,          // ContentType alert
      0x03, 0x01,  // ProcotolVersion legacy_record_version
      0, 2,        // uint16 length
      // Alert struct (TLSPlaintext fragment):
      2,   // AlertLevel fatal
      80,  // AlertDescription internal_error
  };
  QuicStreamFrame alert(
      QuicUtils::GetCryptoStreamId(server_conn_->transport_version()), false,
      server_stream_->stream_bytes_read(),
      QuicStringPiece(alert_msg, QUIC_ARRAYSIZE(alert_msg)));
  server_stream_->OnStreamFrame(alert);

  EXPECT_FALSE(server_stream_->handshake_confirmed());
}

}  // namespace
}  // namespace test
}  // namespace quic
