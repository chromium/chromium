// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/quartc/quartc_session.h"

#include "build/build_config.h"
#include "net/third_party/quic/core/crypto/crypto_server_config_protobuf.h"
#include "net/third_party/quic/core/quic_simple_buffer_allocator.h"
#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/core/tls_client_handshaker.h"
#include "net/third_party/quic/core/tls_server_handshaker.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/platform/api/quic_test_mem_slice_vector.h"
#include "net/third_party/quic/quartc/counting_packet_filter.h"
#include "net/third_party/quic/quartc/quartc_factory.h"
#include "net/third_party/quic/quartc/quartc_packet_writer.h"
#include "net/third_party/quic/quartc/simulated_packet_transport.h"
#include "net/third_party/quic/test_tools/mock_clock.h"
#include "net/third_party/quic/test_tools/simulator/packet_filter.h"
#include "net/third_party/quic/test_tools/simulator/simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests flaky on iOS.
// TODO(vasilvv): figure out what's wrong and re-enable if possible.
#if !defined(OS_IOS)
namespace quic {

namespace {

static QuicByteCount kDefaultMaxPacketSize = 1200;

class FakeQuartcSessionDelegate : public QuartcSession::Delegate {
 public:
  explicit FakeQuartcSessionDelegate(QuartcStream::Delegate* stream_delegate)
      : stream_delegate_(stream_delegate) {}
  // Called when peers have established forward-secure encryption
  void OnCryptoHandshakeComplete() override {
    LOG(INFO) << "Crypto handshake complete!";
  }
  // Called when connection closes locally, or remotely by peer.
  void OnConnectionClosed(QuicErrorCode error_code,
                          const QuicString& error_details,
                          ConnectionCloseSource source) override {
    connected_ = false;
  }
  // Called when an incoming QUIC stream is created.
  void OnIncomingStream(QuartcStream* quartc_stream) override {
    last_incoming_stream_ = quartc_stream;
    last_incoming_stream_->SetDelegate(stream_delegate_);
    quartc_stream->set_deliver_on_complete(false);
  }

  void OnCongestionControlChange(QuicBandwidth bandwidth_estimate,
                                 QuicBandwidth pacing_rate,
                                 QuicTime::Delta latest_rtt) override {}

  QuartcStream* incoming_stream() { return last_incoming_stream_; }

  bool connected() { return connected_; }

 private:
  QuartcStream* last_incoming_stream_;
  bool connected_ = true;
  QuartcStream::Delegate* stream_delegate_;
};

class FakeQuartcStreamDelegate : public QuartcStream::Delegate {
 public:
  void OnReceived(QuartcStream* stream,
                  const char* data,
                  size_t size) override {
    received_data_[stream->id()] += QuicString(data, size);
  }

  void OnClose(QuartcStream* stream) override {
    errors_[stream->id()] = stream->stream_error();
  }

  void OnBufferChanged(QuartcStream* stream) override {}

  bool has_data() { return !received_data_.empty(); }
  std::map<QuicStreamId, QuicString> data() { return received_data_; }

  QuicRstStreamErrorCode stream_error(QuicStreamId id) { return errors_[id]; }

 private:
  std::map<QuicStreamId, QuicString> received_data_;
  std::map<QuicStreamId, QuicRstStreamErrorCode> errors_;
};

class QuartcSessionTest : public QuicTest {
 public:
  ~QuartcSessionTest() override {}

  void Init() {
    client_transport_ =
        QuicMakeUnique<simulator::SimulatedQuartcPacketTransport>(
            &simulator_, "client_transport", "server_transport",
            10 * kDefaultMaxPacketSize);
    server_transport_ =
        QuicMakeUnique<simulator::SimulatedQuartcPacketTransport>(
            &simulator_, "server_transport", "client_transport",
            10 * kDefaultMaxPacketSize);

    client_filter_ = QuicMakeUnique<simulator::CountingPacketFilter>(
        &simulator_, "client_filter", client_transport_.get());

    client_server_link_ = QuicMakeUnique<simulator::SymmetricLink>(
        client_filter_.get(), server_transport_.get(),
        QuicBandwidth::FromKBitsPerSecond(10 * 1000),
        QuicTime::Delta::FromMilliseconds(10));

    client_writer_ = QuicMakeUnique<QuartcPacketWriter>(client_transport_.get(),
                                                        kDefaultMaxPacketSize);
    server_writer_ = QuicMakeUnique<QuartcPacketWriter>(server_transport_.get(),
                                                        kDefaultMaxPacketSize);

    client_stream_delegate_ = QuicMakeUnique<FakeQuartcStreamDelegate>();
    client_session_delegate_ = QuicMakeUnique<FakeQuartcSessionDelegate>(
        client_stream_delegate_.get());

    server_stream_delegate_ = QuicMakeUnique<FakeQuartcStreamDelegate>();
    server_session_delegate_ = QuicMakeUnique<FakeQuartcSessionDelegate>(
        server_stream_delegate_.get());
  }

  // The parameters are used to control whether the handshake will success or
  // not.
  void CreateClientAndServerSessions() {
    Init();
    client_peer_ =
        CreateSession(Perspective::IS_CLIENT, std::move(client_writer_));
    client_peer_->SetDelegate(client_session_delegate_.get());
    server_peer_ =
        CreateSession(Perspective::IS_SERVER, std::move(server_writer_));
    server_peer_->SetDelegate(server_session_delegate_.get());
  }

  std::unique_ptr<QuartcSession> CreateSession(
      Perspective perspective,
      std::unique_ptr<QuartcPacketWriter> writer) {
    std::unique_ptr<QuicConnection> quic_connection =
        CreateConnection(perspective, writer.get());
    QuicString remote_fingerprint_value = "value";
    QuicConfig config;
    return QuicMakeUnique<QuartcSession>(
        std::move(quic_connection), config, CurrentSupportedVersions(),
        remote_fingerprint_value, perspective, &simulator_,
        simulator_.GetClock(), std::move(writer));
  }

  std::unique_ptr<QuicConnection> CreateConnection(Perspective perspective,
                                                   QuartcPacketWriter* writer) {
    QuicIpAddress ip;
    ip.FromString("0.0.0.0");
    return QuicMakeUnique<QuicConnection>(
        0, QuicSocketAddress(ip, 0), &simulator_, simulator_.GetAlarmFactory(),
        writer, /*owns_writer=*/false, perspective,
        ParsedVersionOfIndex(CurrentSupportedVersions(), 0));
  }

  // Runs all tasks scheduled in the next 200 ms.
  void RunTasks() { simulator_.RunFor(QuicTime::Delta::FromMilliseconds(200)); }

  void StartHandshake() {
    server_peer_->StartCryptoHandshake();
    client_peer_->StartCryptoHandshake();
    RunTasks();
  }

  // Test handshake establishment and sending/receiving of data for two
  // directions.
  void TestStreamConnection() {
    ASSERT_TRUE(server_peer_->IsCryptoHandshakeConfirmed());
    ASSERT_TRUE(client_peer_->IsCryptoHandshakeConfirmed());
    ASSERT_TRUE(server_peer_->IsEncryptionEstablished());
    ASSERT_TRUE(client_peer_->IsEncryptionEstablished());

    // Now we can establish encrypted outgoing stream.
    QuartcStream* outgoing_stream =
        server_peer_->CreateOutgoingBidirectionalStream();
    QuicStreamId stream_id = outgoing_stream->id();
    ASSERT_NE(nullptr, outgoing_stream);
    EXPECT_TRUE(server_peer_->HasOpenDynamicStreams());

    outgoing_stream->SetDelegate(server_stream_delegate_.get());
    outgoing_stream->set_deliver_on_complete(false);

    // Send a test message from peer 1 to peer 2.
    char kTestMessage[] = "Hello";
    test::QuicTestMemSliceVector data(
        {std::make_pair(kTestMessage, strlen(kTestMessage))});
    outgoing_stream->WriteMemSlices(data.span(), /*fin=*/false);
    RunTasks();

    // Wait for peer 2 to receive messages.
    ASSERT_TRUE(client_stream_delegate_->has_data());

    QuartcStream* incoming = client_session_delegate_->incoming_stream();
    ASSERT_TRUE(incoming);
    EXPECT_EQ(incoming->id(), stream_id);
    EXPECT_TRUE(client_peer_->HasOpenDynamicStreams());

    EXPECT_EQ(client_stream_delegate_->data()[stream_id], kTestMessage);
    // Send a test message from peer 2 to peer 1.
    char kTestResponse[] = "Response";
    test::QuicTestMemSliceVector response(
        {std::make_pair(kTestResponse, strlen(kTestResponse))});
    incoming->WriteMemSlices(response.span(), /*fin=*/false);
    RunTasks();
    // Wait for peer 1 to receive messages.
    ASSERT_TRUE(server_stream_delegate_->has_data());

    EXPECT_EQ(server_stream_delegate_->data()[stream_id], kTestResponse);
  }

  // Test that client and server are not connected after handshake failure.
  void TestDisconnectAfterFailedHandshake() {
    EXPECT_TRUE(!client_session_delegate_->connected());
    EXPECT_TRUE(!server_session_delegate_->connected());

    EXPECT_FALSE(client_peer_->IsEncryptionEstablished());
    EXPECT_FALSE(client_peer_->IsCryptoHandshakeConfirmed());

    EXPECT_FALSE(server_peer_->IsEncryptionEstablished());
    EXPECT_FALSE(server_peer_->IsCryptoHandshakeConfirmed());
  }

 protected:
  simulator::Simulator simulator_;

  std::unique_ptr<simulator::SimulatedQuartcPacketTransport> client_transport_;
  std::unique_ptr<simulator::SimulatedQuartcPacketTransport> server_transport_;
  std::unique_ptr<simulator::CountingPacketFilter> client_filter_;
  std::unique_ptr<simulator::SymmetricLink> client_server_link_;

  std::unique_ptr<QuartcPacketWriter> client_writer_;
  std::unique_ptr<QuartcPacketWriter> server_writer_;
  std::unique_ptr<QuartcSession> client_peer_;
  std::unique_ptr<QuartcSession> server_peer_;

  std::unique_ptr<FakeQuartcStreamDelegate> client_stream_delegate_;
  std::unique_ptr<FakeQuartcSessionDelegate> client_session_delegate_;
  std::unique_ptr<FakeQuartcStreamDelegate> server_stream_delegate_;
  std::unique_ptr<FakeQuartcSessionDelegate> server_session_delegate_;
};

TEST_F(QuartcSessionTest, StreamConnection) {
  CreateClientAndServerSessions();
  StartHandshake();
  TestStreamConnection();
}

TEST_F(QuartcSessionTest, PreSharedKeyHandshake) {
  CreateClientAndServerSessions();
  client_peer_->SetPreSharedKey("foo");
  server_peer_->SetPreSharedKey("foo");
  StartHandshake();
  TestStreamConnection();
}

// Test that data streams are not created before handshake.
TEST_F(QuartcSessionTest, CannotCreateDataStreamBeforeHandshake) {
  CreateClientAndServerSessions();
  EXPECT_EQ(nullptr, server_peer_->CreateOutgoingBidirectionalStream());
  EXPECT_EQ(nullptr, client_peer_->CreateOutgoingBidirectionalStream());
}

TEST_F(QuartcSessionTest, CancelQuartcStream) {
  CreateClientAndServerSessions();
  StartHandshake();
  ASSERT_TRUE(client_peer_->IsCryptoHandshakeConfirmed());
  ASSERT_TRUE(server_peer_->IsCryptoHandshakeConfirmed());

  QuartcStream* stream = client_peer_->CreateOutgoingBidirectionalStream();
  ASSERT_NE(nullptr, stream);

  uint32_t id = stream->id();
  EXPECT_FALSE(client_peer_->IsClosedStream(id));
  stream->SetDelegate(client_stream_delegate_.get());
  client_peer_->CancelStream(id);
  EXPECT_EQ(stream->stream_error(),
            QuicRstStreamErrorCode::QUIC_STREAM_CANCELLED);
  EXPECT_TRUE(client_peer_->IsClosedStream(id));
}

// TODO(b/112561077):  This is the wrong layer for this test.  We should write a
// test specifically for QuartcPacketWriter with a stubbed-out
// QuartcPacketTransport and remove
// SimulatedQuartcPacketTransport::last_packet_number().
TEST_F(QuartcSessionTest, WriterGivesPacketNumberToTransport) {
  CreateClientAndServerSessions();
  StartHandshake();
  ASSERT_TRUE(client_peer_->IsCryptoHandshakeConfirmed());
  ASSERT_TRUE(server_peer_->IsCryptoHandshakeConfirmed());

  QuartcStream* stream = client_peer_->CreateOutgoingBidirectionalStream();
  stream->SetDelegate(client_stream_delegate_.get());

  char kClientMessage[] = "Hello";
  test::QuicTestMemSliceVector stream_data(
      {std::make_pair(kClientMessage, strlen(kClientMessage))});
  stream->WriteMemSlices(stream_data.span(), /*fin=*/false);
  RunTasks();

  // The transport should see the latest packet number sent by QUIC.
  EXPECT_EQ(
      client_transport_->last_packet_number(),
      client_peer_->connection()->sent_packet_manager().GetLargestSentPacket());
}

TEST_F(QuartcSessionTest, CloseConnection) {
  CreateClientAndServerSessions();
  StartHandshake();
  ASSERT_TRUE(client_peer_->IsCryptoHandshakeConfirmed());
  ASSERT_TRUE(server_peer_->IsCryptoHandshakeConfirmed());

  client_peer_->CloseConnection("Connection closed by client");
  EXPECT_FALSE(client_session_delegate_->connected());
  RunTasks();
  EXPECT_FALSE(server_session_delegate_->connected());
}

TEST_F(QuartcSessionTest, StreamRetransmissionEnabled) {
  CreateClientAndServerSessions();
  StartHandshake();
  ASSERT_TRUE(client_peer_->IsCryptoHandshakeConfirmed());
  ASSERT_TRUE(server_peer_->IsCryptoHandshakeConfirmed());

  QuartcStream* stream = client_peer_->CreateOutgoingBidirectionalStream();
  QuicStreamId stream_id = stream->id();
  stream->SetDelegate(client_stream_delegate_.get());
  stream->set_cancel_on_loss(false);

  client_filter_->set_packets_to_drop(1);

  char kClientMessage[] = "Hello";
  test::QuicTestMemSliceVector stream_data(
      {std::make_pair(kClientMessage, strlen(kClientMessage))});
  stream->WriteMemSlices(stream_data.span(), /*fin=*/false);
  RunTasks();

  // Stream data should make it despite packet loss.
  ASSERT_TRUE(server_stream_delegate_->has_data());
  EXPECT_EQ(server_stream_delegate_->data()[stream_id], kClientMessage);
}

TEST_F(QuartcSessionTest, StreamRetransmissionDisabled) {
  CreateClientAndServerSessions();
  StartHandshake();
  ASSERT_TRUE(client_peer_->IsCryptoHandshakeConfirmed());
  ASSERT_TRUE(server_peer_->IsCryptoHandshakeConfirmed());

  QuartcStream* stream = client_peer_->CreateOutgoingBidirectionalStream();
  QuicStreamId stream_id = stream->id();
  stream->SetDelegate(client_stream_delegate_.get());
  stream->set_cancel_on_loss(true);

  client_filter_->set_packets_to_drop(1);

  char kMessage[] = "Hello";
  test::QuicTestMemSliceVector stream_data(
      {std::make_pair(kMessage, strlen(kMessage))});
  stream->WriteMemSlices(stream_data.span(), /*fin=*/false);
  simulator_.RunFor(QuicTime::Delta::FromMilliseconds(1));

  // Send another packet to trigger loss detection.
  QuartcStream* stream_1 = client_peer_->CreateOutgoingBidirectionalStream();
  stream_1->SetDelegate(client_stream_delegate_.get());

  char kMessage1[] = "Second message";
  test::QuicTestMemSliceVector stream_data_1(
      {std::make_pair(kMessage1, strlen(kMessage1))});
  stream_1->WriteMemSlices(stream_data_1.span(), /*fin=*/false);
  RunTasks();

  // QUIC should try to retransmit the first stream by loss detection.  Instead,
  // it will cancel itself.
  EXPECT_THAT(server_stream_delegate_->data()[stream_id], testing::IsEmpty());

  EXPECT_TRUE(client_peer_->IsClosedStream(stream_id));
  EXPECT_TRUE(server_peer_->IsClosedStream(stream_id));
  EXPECT_EQ(client_stream_delegate_->stream_error(stream_id),
            QUIC_STREAM_CANCELLED);
  EXPECT_EQ(server_stream_delegate_->stream_error(stream_id),
            QUIC_STREAM_CANCELLED);
}

}  // namespace

}  // namespace quic
#endif  // !defined(OS_IOS)
