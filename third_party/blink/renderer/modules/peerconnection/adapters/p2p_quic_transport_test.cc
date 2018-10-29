// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_chromium_alarm_factory.h"
#include "net/quic/test_task_runner.h"
#include "net/test/gtest_util.h"
#include "net/third_party/quic/core/tls_client_handshaker.h"
#include "net/third_party/quic/test_tools/mock_clock.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_packet_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport_factory_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport_impl.h"
#include "third_party/webrtc/rtc_base/rtccertificate.h"
#include "third_party/webrtc/rtc_base/sslfingerprint.h"
#include "third_party/webrtc/rtc_base/sslidentity.h"

namespace blink {

namespace {

// The types of callbacks that can be fired on a P2PQuicTransport::Delegate.
enum class TransportCallbackType {
  kNone,
  kOnRemoteStopped,
  kOnConnectionFailed,
  kOnConnected,
  kOnStream
};

// The types of callbacks that can be fired on a P2PQuicStream::Delegate.
enum class StreamCallbackType { kNone, kOnRemoteReset, kOnRemoteFinish };

// The QuicStreamDelegate implements counters for callbacks. It can also set
// expectations for a specific callback. When an expectation is set the
// quic::TestTaskRunner drives the test until the callbacks have been fired, and
// we are no longer expecting the callback.
class QuicStreamDelegateForTesting final : public P2PQuicStream::Delegate {
 public:
  ~QuicStreamDelegateForTesting() override {}

  void OnRemoteReset() override {
    if (callback_type_expected_ == StreamCallbackType::kOnRemoteReset) {
      callback_type_expected_ = StreamCallbackType::kNone;
    }
    remote_reset_count_++;
  };

  void OnRemoteFinish() override {
    if (callback_type_expected_ == StreamCallbackType::kOnRemoteFinish) {
      callback_type_expected_ = StreamCallbackType::kNone;
    }
    remote_finish_count_++;
  };

  int remote_reset_count() { return remote_reset_count_; }

  int remote_finish_count() { return remote_finish_count_; }

  // Sets the type of callback expected to be called.
  void ExpectCallback(StreamCallbackType callback_type) {
    callback_type_expected_ = callback_type;
  }

  // Returns if we are expecting a callback that hasn't been fired yet.
  bool IsExpectingCallback() const {
    return callback_type_expected_ != StreamCallbackType::kNone;
  }

 private:
  int remote_reset_count_ = 0;
  int remote_finish_count_ = 0;
  StreamCallbackType callback_type_expected_ = StreamCallbackType::kNone;
};

// Implements counters for callbacks. It can also set expectations for a
// specific callback. When an expectation is set the quic::TestTaskRunner
// drives the test until the callbacks have been fired, and we are no longer
// expecting the callback.
//
// TODO(https://crbug.com/874296): If these files get moved to the platform
// directory we will run the tests in a different test environment. In that case
// it will make more sense to use the TestCompletionCallback and the RunLoop for
// driving the test.
class QuicTransportDelegateForTest final : public P2PQuicTransport::Delegate {
 public:
  ~QuicTransportDelegateForTest() override {}
  void OnRemoteStopped() override {
    if (callback_type_expected_ == TransportCallbackType::kOnRemoteStopped) {
      callback_type_expected_ = TransportCallbackType::kNone;
    }
    stopped_count_++;
  }

  void OnConnectionFailed(const std::string& error_details,
                          bool from_remote) override {
    if (callback_type_expected_ == TransportCallbackType::kOnConnectionFailed) {
      callback_type_expected_ = TransportCallbackType::kNone;
    }
    connection_failed_count_++;
  }

  void OnConnected() override {
    if (callback_type_expected_ == TransportCallbackType::kOnConnected) {
      callback_type_expected_ = TransportCallbackType::kNone;
    }
    connected_count_++;
  }

  // We store the remotely created stream.
  void OnStream(P2PQuicStream* stream) override {
    if (callback_type_expected_ == TransportCallbackType::kOnStream) {
      callback_type_expected_ = TransportCallbackType::kNone;
    }
    streams_.push_back(static_cast<P2PQuicStreamImpl*>(stream));
    on_stream_count_++;
  }

  int stopped_count() const { return stopped_count_; }

  int connection_failed_count() const { return connection_failed_count_; }

  int connected_count() const { return connected_count_; }

  int on_stream_count() const { return on_stream_count_; }

  // Sets the type of callback expected to be called.
  void ExpectCallback(TransportCallbackType callback_type) {
    callback_type_expected_ = callback_type;
  }

  // Returns if we are expecting a callback that hasn't been fired yet.
  bool IsExpectingCallback() const {
    return callback_type_expected_ != TransportCallbackType::kNone;
  }

  std::vector<P2PQuicStreamImpl*> streams() const { return streams_; }

 private:
  TransportCallbackType callback_type_expected_ = TransportCallbackType::kNone;
  int stopped_count_ = 0;
  int connection_failed_count_ = 0;
  int connected_count_ = 0;
  int on_stream_count_ = 0;
  // The delegates created for each stream as a result of the remote side
  // creating streams and sending data (triggering OnStream). P2PQuicStreamsImpl
  // are owned by the P2PQuicTransport.
  std::vector<P2PQuicStreamImpl*> streams_;
};

// This is a fake packet transport to be used by the P2PQuicTransportImpl. It
// allows to easily connect two packet transports together. We send packets
// asynchronously, by using the same alarm factory that is being used for the
// underlying QUIC library.
class FakePacketTransport : public P2PQuicPacketTransport,
                            public quic::QuicAlarm::Delegate {
 public:
  FakePacketTransport(quic::QuicAlarmFactory* alarm_factory,
                      quic::MockClock* clock)
      : alarm_(alarm_factory->CreateAlarm(new AlarmDelegate(this))),
        clock_(clock) {}
  ~FakePacketTransport() override {
    // The write observer should be unset when it is destroyed.
    DCHECK(!write_observer_);
  };

  // Called by QUIC for writing data to the other side. The flow for writing a
  // packet is P2PQuicTransportImpl --> quic::QuicConnection -->
  // quic::QuicPacketWriter --> FakePacketTransport. In this case the
  // FakePacketTransport just writes directly to the FakePacketTransport on the
  // other side.
  int WritePacket(const QuicPacket& packet) override {
    // For the test there should always be a peer_packet_transport_ connected at
    // this point.
    if (!peer_packet_transport_) {
      return 0;
    }
    last_packet_num_ = packet.packet_number;
    packet_queue_.emplace_back(packet.buffer, packet.buf_len);
    alarm_->Cancel();
    // We don't want get 0 RTT.
    alarm_->Set(clock_->Now() + quic::QuicTime::Delta::FromMicroseconds(10));

    return packet.buf_len;
  }

  // Sets the P2PQuicTransportImpl as the delegate.
  void SetReceiveDelegate(
      P2PQuicPacketTransport::ReceiveDelegate* delegate) override {
    // We can't set two ReceiveDelegates for one packet transport.
    DCHECK(!delegate_ || !delegate);
    delegate_ = delegate;
  }

  void SetWriteObserver(
      P2PQuicPacketTransport::WriteObserver* write_observer) override {
    // We can't set two WriteObservers for one packet transport.
    DCHECK(!write_observer_ || !write_observer);
    write_observer_ = write_observer;
  }

  bool Writable() override { return true; }

  // Connects the other FakePacketTransport, so we can write to the peer.
  void ConnectPeerTransport(FakePacketTransport* peer_packet_transport) {
    DCHECK(!peer_packet_transport_);
    peer_packet_transport_ = peer_packet_transport;
  }

  // Disconnects the delegate, so we no longer write to it. The test must call
  // this before destructing either of the packet transports!
  void DisconnectPeerTransport(FakePacketTransport* peer_packet_transport) {
    DCHECK(peer_packet_transport_ == peer_packet_transport);
    peer_packet_transport_ = nullptr;
  }

  // The callback used in order for us to communicate between
  // FakePacketTransports.
  void OnDataReceivedFromPeer(const char* data, size_t data_len) {
    DCHECK(delegate_);
    delegate_->OnPacketDataReceived(data, data_len);
  }

  int last_packet_num() { return last_packet_num_; }

 private:
  // Wraps the FakePacketTransport so that we can pass in a raw pointer that can
  // be reference counted when calling CreateAlarm().
  class AlarmDelegate : public quic::QuicAlarm::Delegate {
   public:
    explicit AlarmDelegate(FakePacketTransport* packet_transport)
        : packet_transport_(packet_transport) {}

    void OnAlarm() override { packet_transport_->OnAlarm(); }

   private:
    FakePacketTransport* packet_transport_;
  };

  // Called when we should write any buffered data.
  void OnAlarm() override {
    // Send the data to the peer at this point.
    peer_packet_transport_->OnDataReceivedFromPeer(
        packet_queue_.front().c_str(), packet_queue_.front().length());
    packet_queue_.pop_front();

    // If there's more packets to be sent out, reset the alarm to send it as the
    // next task.
    if (!packet_queue_.empty()) {
      alarm_->Cancel();
      alarm_->Set(clock_->Now());
    }
  }
  // If async, packets are queued here to send.
  quic::QuicDeque<quic::QuicString> packet_queue_;
  // Alarm used to send data asynchronously.
  quic::QuicArenaScopedPtr<quic::QuicAlarm> alarm_;
  // The P2PQuicTransportImpl, which sets itself as the delegate in its
  // constructor. After receiving data it forwards it along to QUIC.
  P2PQuicPacketTransport::ReceiveDelegate* delegate_ = nullptr;

  // The P2PQuicPacketWriter, which sets itself as a write observer
  // during the P2PQuicTransportFactoryImpl::CreateQuicTransport. It is
  // owned by the QuicConnection and will
  P2PQuicPacketTransport::WriteObserver* write_observer_ = nullptr;

  // The other FakePacketTransport that we are writing to. It's the
  // responsibility of the test to disconnect this delegate
  // (set_delegate(nullptr);) before it is destructed.
  FakePacketTransport* peer_packet_transport_ = nullptr;
  quic::QuicPacketNumber last_packet_num_;
  quic::MockClock* clock_;
};

// A helper class to bundle test objects together.
class QuicPeerForTest {
 public:
  QuicPeerForTest(
      std::unique_ptr<FakePacketTransport> packet_transport,
      std::unique_ptr<QuicTransportDelegateForTest> quic_transport_delegate,
      std::unique_ptr<P2PQuicTransportImpl> quic_transport,
      rtc::scoped_refptr<rtc::RTCCertificate> certificate)
      : packet_transport_(std::move(packet_transport)),
        quic_transport_delegate_(std::move(quic_transport_delegate)),
        quic_transport_(std::move(quic_transport)),
        certificate_(certificate) {}

  FakePacketTransport* packet_transport() { return packet_transport_.get(); }

  QuicTransportDelegateForTest* quic_transport_delegate() {
    return quic_transport_delegate_.get();
  }

  P2PQuicTransportImpl* quic_transport() { return quic_transport_.get(); }

  rtc::scoped_refptr<rtc::RTCCertificate> certificate() { return certificate_; }

 private:
  std::unique_ptr<FakePacketTransport> packet_transport_;
  std::unique_ptr<QuicTransportDelegateForTest> quic_transport_delegate_;
  std::unique_ptr<P2PQuicTransportImpl> quic_transport_;
  rtc::scoped_refptr<rtc::RTCCertificate> certificate_;
};

rtc::scoped_refptr<rtc::RTCCertificate> CreateTestCertificate() {
  rtc::KeyParams params;
  rtc::SSLIdentity* ssl_identity =
      rtc::SSLIdentity::Generate("dummy_certificate", params);
  return rtc::RTCCertificate::Create(
      std::unique_ptr<rtc::SSLIdentity>(ssl_identity));
}

// Allows faking a failing handshake.
class FailingProofVerifier : public quic::ProofVerifier {
 public:
  FailingProofVerifier() {}
  ~FailingProofVerifier() override {}

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
    return quic::QUIC_FAILURE;
  }

  quic::QuicAsyncStatus VerifyCertChain(
      const quic::QuicString& hostname,
      const std::vector<quic::QuicString>& certs,
      const quic::ProofVerifyContext* context,
      quic::QuicString* error_details,
      std::unique_ptr<quic::ProofVerifyDetails>* details,
      std::unique_ptr<quic::ProofVerifierCallback> callback) override {
    return quic::QUIC_FAILURE;
  }

  std::unique_ptr<quic::ProofVerifyContext> CreateDefaultContext() override {
    return nullptr;
  }
};
}  // namespace

// Unit tests for the P2PQuicTransport, using an underlying fake packet
// transport that sends packets directly between endpoints. This also tests
// P2PQuicStreams for test cases that involve two streams connected between
// separate endpoints. This is because the P2PQuicStream is highly coupled to
// the P2PQuicSession for communicating between endpoints, so we would like to
// test it with the real session object.
//
// The test is driven using the quic::TestTaskRunner to run posted tasks until
// callbacks have been fired.
class P2PQuicTransportTest : public testing::Test {
 public:
  P2PQuicTransportTest() {}

  ~P2PQuicTransportTest() override {
    // This must be done before desctructing the transports so that we don't
    // have any dangling pointers.
    client_peer_->packet_transport()->DisconnectPeerTransport(
        server_peer_->packet_transport());
    server_peer_->packet_transport()->DisconnectPeerTransport(
        client_peer_->packet_transport());
  }

  // Connects both peer's underlying transports and creates both
  // P2PQuicTransportImpls.
  void Initialize(bool can_respond_to_crypto_handshake = true) {
    // Quic crashes if packets are sent at time 0, and the clock defaults to 0.
    clock_.AdvanceTime(quic::QuicTime::Delta::FromMilliseconds(1000));
    runner_ = new net::test::TestTaskRunner(&clock_);
    net::QuicChromiumAlarmFactory* alarm_factory =
        new net::QuicChromiumAlarmFactory(runner_.get(), &clock_);
    quic_transport_factory_ = std::make_unique<P2PQuicTransportFactoryImpl>(
        &clock_, std::unique_ptr<net::QuicChromiumAlarmFactory>(alarm_factory));

    std::unique_ptr<FakePacketTransport> client_packet_transport =
        std::make_unique<FakePacketTransport>(alarm_factory, &clock_);
    std::unique_ptr<FakePacketTransport> server_packet_transport =
        std::make_unique<FakePacketTransport>(alarm_factory, &clock_);
    // Connect the transports so that they can speak to each other.
    client_packet_transport->ConnectPeerTransport(
        server_packet_transport.get());
    server_packet_transport->ConnectPeerTransport(
        client_packet_transport.get());
    rtc::scoped_refptr<rtc::RTCCertificate> client_cert =
        CreateTestCertificate();

    std::unique_ptr<QuicTransportDelegateForTest>
        client_quic_transport_delegate =
            std::make_unique<QuicTransportDelegateForTest>();
    std::vector<rtc::scoped_refptr<rtc::RTCCertificate>> client_certificates;
    client_certificates.push_back(client_cert);
    P2PQuicTransportConfig client_config(client_quic_transport_delegate.get(),
                                         client_packet_transport.get(),
                                         client_certificates);
    client_config.is_server = false;
    client_config.can_respond_to_crypto_handshake =
        can_respond_to_crypto_handshake;
    // We can't downcast a unique_ptr to an object, so we have to release, cast
    // it, then create a unique_ptr of the downcasted pointer.
    P2PQuicTransportImpl* client_quic_transport_ptr =
        static_cast<P2PQuicTransportImpl*>(
            quic_transport_factory_
                ->CreateQuicTransport(std::move(client_config))
                .release());
    std::unique_ptr<P2PQuicTransportImpl> client_quic_transport =
        std::unique_ptr<P2PQuicTransportImpl>(client_quic_transport_ptr);
    client_peer_ = std::make_unique<QuicPeerForTest>(
        std::move(client_packet_transport),
        std::move(client_quic_transport_delegate),
        std::move(client_quic_transport), client_cert);

    std::unique_ptr<QuicTransportDelegateForTest>
        server_quic_transport_delegate =
            std::make_unique<QuicTransportDelegateForTest>();

    rtc::scoped_refptr<rtc::RTCCertificate> server_cert =
        CreateTestCertificate();
    std::vector<rtc::scoped_refptr<rtc::RTCCertificate>> server_certificates;
    server_certificates.push_back(server_cert);
    P2PQuicTransportConfig server_config(server_quic_transport_delegate.get(),
                                         server_packet_transport.get(),
                                         server_certificates);
    server_config.is_server = true;
    server_config.can_respond_to_crypto_handshake =
        can_respond_to_crypto_handshake;
    P2PQuicTransportImpl* server_quic_transport_ptr =
        static_cast<P2PQuicTransportImpl*>(
            quic_transport_factory_
                ->CreateQuicTransport(std::move(server_config))
                .release());
    std::unique_ptr<P2PQuicTransportImpl> server_quic_transport =
        std::unique_ptr<P2PQuicTransportImpl>(server_quic_transport_ptr);
    server_peer_ = std::make_unique<QuicPeerForTest>(
        std::move(server_packet_transport),
        std::move(server_quic_transport_delegate),
        std::move(server_quic_transport), server_cert);
  }

  // Sets a FailingProofVerifier to the client transport before initializing
  // the its crypto stream. This allows the client to fail the proof
  // verification step during the crypto handshake.
  void InitializeWithFailingProofVerification() {
    // Allows us to initialize the crypto streams after constructing the
    // objects.
    Initialize(false);
    // Create the client crypto config and insert it into the client transport.
    std::unique_ptr<quic::ProofVerifier> proof_verifier(
        new FailingProofVerifier);
    std::unique_ptr<quic::QuicCryptoClientConfig> crypto_client_config =
        std::make_unique<quic::QuicCryptoClientConfig>(
            std::move(proof_verifier),
            quic::TlsClientHandshaker::CreateSslCtx());
    client_peer_->quic_transport()->set_crypto_client_config(
        std::move(crypto_client_config));
    // Now initialize the crypto streams.
    client_peer_->quic_transport()->InitializeCryptoStream();
    server_peer_->quic_transport()->InitializeCryptoStream();
  }

  // Drives the test until we are't expecting any more callbacks to be fired.
  // This is done using the net::test::TestTaskRunner, which runs the tasks
  // in the correct order and then advances the quic::MockClock to the time the
  // task is run.
  void RunUntilCallbacksFired() {
    while (server_peer_->quic_transport_delegate()->IsExpectingCallback() ||
           client_peer_->quic_transport_delegate()->IsExpectingCallback() ||
           ExpectingStreamCallback()) {
      // We shouldn't enter a case where we are expecting a callback
      // and we're out of tasks to run.
      ASSERT_GT(runner_->GetPostedTasks().size(), 0u);
      runner_->RunNextTask();
    }
  }

  bool ExpectingStreamCallback() {
    return streams_setup_ && (client_stream_delegate_->IsExpectingCallback() ||
                              server_stream_delegate_->IsExpectingCallback());
  }

  // Drives the test by running the current tasks that are posted.
  void RunCurrentTasks() {
    size_t posted_tasks_size = runner_->GetPostedTasks().size();
    for (size_t i = 0; i < posted_tasks_size; ++i) {
      runner_->RunNextTask();
    }
  }

  // Starts the handshake, by setting the remote fingerprints and kicking off
  // the handshake from the client.
  void StartHandshake() {
    std::vector<std::unique_ptr<rtc::SSLFingerprint>> server_fingerprints;
    server_fingerprints.emplace_back(rtc::SSLFingerprint::Create(
        "sha-256", server_peer_->certificate()->identity()));
    // The server side doesn't currently need call this to set the remote
    // fingerprints, but once P2P certificate verification is supported in the
    // TLS 1.3 handshake this will ben necessary.
    server_peer_->quic_transport()->Start(std::move(server_fingerprints));

    std::vector<std::unique_ptr<rtc::SSLFingerprint>> client_fingerprints;
    client_fingerprints.emplace_back(rtc::SSLFingerprint::Create(
        "sha-256", client_peer_->certificate()->identity()));
    client_peer_->quic_transport()->Start(std::move(client_fingerprints));
  }

  // Sets up an initial handshake and connection between peers.
  void Connect() {
    client_peer_->quic_transport_delegate()->ExpectCallback(
        TransportCallbackType::kOnConnected);
    server_peer_->quic_transport_delegate()->ExpectCallback(
        TransportCallbackType::kOnConnected);
    StartHandshake();
    RunUntilCallbacksFired();
    ExpectSecureConnection();
  }

  // Creates a P2PQuicStreamImpl on both the client and server side that are
  // connected to each other.
  void SetupConnectedStreams() {
    // We must already have a secure connection before streams are created.
    ASSERT_TRUE(client_peer_->quic_transport()->IsEncryptionEstablished());
    ASSERT_TRUE(server_peer_->quic_transport()->IsEncryptionEstablished());

    client_stream_ = client_peer_->quic_transport()->CreateStream();
    ASSERT_TRUE(client_stream_);
    client_stream_id_ = client_stream_->id();
    client_stream_delegate_ = std::make_unique<QuicStreamDelegateForTesting>();
    client_stream_->SetDelegate(client_stream_delegate_.get());

    // Send some data to trigger the remote side (server side) to get an
    // incoming stream.
    server_peer_->quic_transport_delegate()->ExpectCallback(
        TransportCallbackType::kOnStream);
    client_stream_->WriteOrBufferData("hello", false, nullptr);
    RunUntilCallbacksFired();

    ASSERT_EQ(1u, server_peer_->quic_transport()->GetNumActiveStreams());
    ASSERT_EQ(1u, client_peer_->quic_transport()->GetNumActiveStreams());
    ASSERT_EQ(1u, server_peer_->quic_transport_delegate()->streams().size());
    server_stream_ = server_peer_->quic_transport_delegate()->streams()[0];
    ASSERT_TRUE(server_stream_);
    server_stream_id_ = server_stream_->id();
    server_stream_delegate_ = std::make_unique<QuicStreamDelegateForTesting>();
    server_stream_->SetDelegate(server_stream_delegate_.get());
    streams_setup_ = true;
  }

  void ExpectSecureConnection() {
    EXPECT_TRUE(client_peer_->quic_transport()->IsEncryptionEstablished());
    EXPECT_TRUE(client_peer_->quic_transport()->IsCryptoHandshakeConfirmed());
    EXPECT_TRUE(server_peer_->quic_transport()->IsCryptoHandshakeConfirmed());
    EXPECT_TRUE(server_peer_->quic_transport()->IsEncryptionEstablished());
  }

  void ExpectConnectionNotEstablished() {
    EXPECT_FALSE(client_peer_->quic_transport()->IsEncryptionEstablished());
    EXPECT_FALSE(client_peer_->quic_transport()->IsCryptoHandshakeConfirmed());
    EXPECT_FALSE(server_peer_->quic_transport()->IsCryptoHandshakeConfirmed());
    EXPECT_FALSE(server_peer_->quic_transport()->IsEncryptionEstablished());
  }

  // Test that the callbacks were called appropriately after a successful
  // crypto handshake.
  void ExpectSuccessfulHandshake() {
    EXPECT_EQ(1, client_peer_->quic_transport_delegate()->connected_count());
    EXPECT_EQ(0, client_peer_->quic_transport_delegate()->stopped_count());
    EXPECT_EQ(
        0, client_peer_->quic_transport_delegate()->connection_failed_count());

    EXPECT_EQ(1, server_peer_->quic_transport_delegate()->connected_count());
    EXPECT_EQ(0, server_peer_->quic_transport_delegate()->stopped_count());
    EXPECT_EQ(
        0, server_peer_->quic_transport_delegate()->connection_failed_count());
  }

  void ExpectTransportsClosed() {
    EXPECT_TRUE(client_peer_->quic_transport()->IsClosed());
    EXPECT_TRUE(server_peer_->quic_transport()->IsClosed());
  }

  void ExpectStreamsClosed() {
    ASSERT_TRUE(streams_setup_);
    EXPECT_EQ(0u, client_peer_->quic_transport()->GetNumActiveStreams());
    EXPECT_TRUE(
        client_peer_->quic_transport()->IsClosedStream(client_stream_id_));
    EXPECT_EQ(0u, server_peer_->quic_transport()->GetNumActiveStreams());
    EXPECT_TRUE(
        server_peer()->quic_transport()->IsClosedStream(server_stream_id_));
  }

  // Exposes these private functions to the test.
  bool IsClientClosed() { return client_peer_->quic_transport()->IsClosed(); }
  bool IsServerClosed() { return server_peer_->quic_transport()->IsClosed(); }

  // Tests that the callbacks were appropriately called after the client
  // stops the connection. Only the server should receive the OnRemoteStopped()
  // callback.
  void ExpectClientStopped() {
    ExpectTransportsClosed();
    EXPECT_EQ(0, client_peer_->quic_transport_delegate()->stopped_count());
    EXPECT_EQ(
        0, client_peer_->quic_transport_delegate()->connection_failed_count());
    EXPECT_EQ(1, server_peer_->quic_transport_delegate()->stopped_count());
    EXPECT_EQ(
        0, server_peer_->quic_transport_delegate()->connection_failed_count());
  }

  // Tests that the callbacks were appropriately called after the server
  // stops the connection. Only the client should receive the OnRemoteStopped()
  // callback.
  void ExpectServerStopped() {
    ExpectTransportsClosed();
    EXPECT_EQ(1, client_peer_->quic_transport_delegate()->stopped_count());
    EXPECT_EQ(
        0, client_peer_->quic_transport_delegate()->connection_failed_count());
    EXPECT_EQ(0, server_peer_->quic_transport_delegate()->stopped_count());
    EXPECT_EQ(
        0, server_peer_->quic_transport_delegate()->connection_failed_count());
  }

  QuicPeerForTest* client_peer() { return client_peer_.get(); }

  quic::QuicConnection* client_connection() {
    return client_peer_->quic_transport()->connection();
  }

  QuicPeerForTest* server_peer() { return server_peer_.get(); }

  quic::QuicConnection* server_connection() {
    return server_peer_->quic_transport()->connection();
  }

  P2PQuicStreamImpl* server_stream() { return server_stream_; }

  P2PQuicStreamImpl* client_stream() { return client_stream_; }

  quic::QuicStreamId server_stream_id() { return server_stream_id_; }

  quic::QuicStreamId client_stream_id() { return client_stream_id_; }

  QuicStreamDelegateForTesting* server_stream_delegate() {
    return server_stream_delegate_.get();
  }

  QuicStreamDelegateForTesting* client_stream_delegate() {
    return client_stream_delegate_.get();
  }

 private:
  quic::MockClock clock_;
  // The TestTaskRunner is used by the QUIC library for setting/firing alarms.
  // We are able to explicitly run these tasks ourselves with the
  // TestTaskRunner.
  scoped_refptr<net::test::TestTaskRunner> runner_;

  std::unique_ptr<P2PQuicTransportFactoryImpl> quic_transport_factory_;
  std::unique_ptr<QuicPeerForTest> client_peer_;
  std::unique_ptr<QuicPeerForTest> server_peer_;

  // Stream objects, which are created with SetupConnectedStream().
  bool streams_setup_ = false;
  std::unique_ptr<QuicStreamDelegateForTesting> client_stream_delegate_;
  std::unique_ptr<QuicStreamDelegateForTesting> server_stream_delegate_;
  // The P2PQuicStreamImpls are owned by the P2PQuicTransport.
  P2PQuicStreamImpl* client_stream_ = nullptr;
  P2PQuicStreamImpl* server_stream_ = nullptr;
  // We cache the values before the streams are potentially closed and deleted.
  quic::QuicStreamId server_stream_id_;
  quic::QuicStreamId client_stream_id_;
};

// Tests that we can connect two quic transports.
TEST_F(P2PQuicTransportTest, HandshakeConnectsPeers) {
  Initialize();
  Connect();

  ExpectSuccessfulHandshake();
}

// Tests the standard case for the server side closing the connection.
TEST_F(P2PQuicTransportTest, ServerStops) {
  Initialize();
  Connect();
  client_peer()->quic_transport_delegate()->ExpectCallback(
      TransportCallbackType::kOnRemoteStopped);
  server_peer()->quic_transport()->Stop();
  RunUntilCallbacksFired();

  ExpectServerStopped();
}

// Tests the standard case for the client side closing the connection.
TEST_F(P2PQuicTransportTest, ClientStops) {
  Initialize();
  Connect();
  server_peer()->quic_transport_delegate()->ExpectCallback(
      TransportCallbackType::kOnRemoteStopped);
  client_peer()->quic_transport()->Stop();
  RunUntilCallbacksFired();

  ExpectClientStopped();
}

// Tests that if either side tries to close the connection a second time, it
// will be ignored because the connection has already been closed.
TEST_F(P2PQuicTransportTest, StopAfterStopped) {
  Initialize();
  Connect();
  server_peer()->quic_transport_delegate()->ExpectCallback(
      TransportCallbackType::kOnRemoteStopped);
  client_peer()->quic_transport()->Stop();
  RunUntilCallbacksFired();
  client_peer()->quic_transport()->Stop();
  server_peer()->quic_transport()->Stop();
  RunCurrentTasks();

  ExpectClientStopped();
}

// Tests that when the client closes the connection the subsequent call to
// Start() will be ignored.
TEST_F(P2PQuicTransportTest, ClientStopsBeforeClientStarts) {
  Initialize();
  server_peer()->quic_transport_delegate()->ExpectCallback(
      TransportCallbackType::kOnRemoteStopped);
  client_peer()->quic_transport()->Stop();
  StartHandshake();
  RunUntilCallbacksFired();

  ExpectConnectionNotEstablished();
  ExpectClientStopped();
}

// Tests that if the server closes the connection before the client starts the
// handshake, the client side will already be closed and Start() will be
// ignored.
TEST_F(P2PQuicTransportTest, ServerStopsBeforeClientStarts) {
  Initialize();
  client_peer()->quic_transport_delegate()->ExpectCallback(
      TransportCallbackType::kOnRemoteStopped);
  server_peer()->quic_transport()->Stop();
  StartHandshake();
  RunUntilCallbacksFired();

  ExpectConnectionNotEstablished();
  ExpectServerStopped();
}

// Tests that when the server's connection fails and then a handshake is
// attempted the transports will not become connected.
TEST_F(P2PQuicTransportTest, ClientConnectionClosesBeforeHandshake) {
  Initialize();
  client_peer()->quic_transport_delegate()->ExpectCallback(
      TransportCallbackType::kOnConnectionFailed);
  server_peer()->quic_transport_delegate()->ExpectCallback(
      TransportCallbackType::kOnConnectionFailed);
  client_connection()->CloseConnection(
      quic::QuicErrorCode::QUIC_INTERNAL_ERROR, "internal error",
      quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
  StartHandshake();
  RunUntilCallbacksFired();

  ExpectConnectionNotEstablished();
}

// Tests that when the server's connection fails and then a handshake is
// attempted the transports will not become connected.
TEST_F(P2PQuicTransportTest, ServerConnectionClosesBeforeHandshake) {
  Initialize();
  client_peer()->quic_transport_delegate()->ExpectCallback(
      TransportCallbackType::kOnConnectionFailed);
  server_peer()->quic_transport_delegate()->ExpectCallback(
      TransportCallbackType::kOnConnectionFailed);
  server_connection()->CloseConnection(
      quic::QuicErrorCode::QUIC_INTERNAL_ERROR, "internal error",
      quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
  StartHandshake();
  RunUntilCallbacksFired();

  ExpectConnectionNotEstablished();
}

// Tests that the appropriate callbacks are fired when the handshake fails.
TEST_F(P2PQuicTransportTest, HandshakeFailure) {
  InitializeWithFailingProofVerification();
  client_peer()->quic_transport_delegate()->ExpectCallback(
      TransportCallbackType::kOnConnectionFailed);
  server_peer()->quic_transport_delegate()->ExpectCallback(
      TransportCallbackType::kOnConnectionFailed);
  StartHandshake();
  RunUntilCallbacksFired();

  EXPECT_EQ(
      1, client_peer()->quic_transport_delegate()->connection_failed_count());
  EXPECT_EQ(
      1, server_peer()->quic_transport_delegate()->connection_failed_count());
  ExpectConnectionNotEstablished();
  ExpectTransportsClosed();
}

// Tests that the appropriate callbacks are fired when the client's connection
// fails after the transports have connected.
TEST_F(P2PQuicTransportTest, ClientConnectionFailureAfterConnected) {
  Initialize();
  Connect();
  // Close the connection with an internal QUIC error.
  client_peer()->quic_transport_delegate()->ExpectCallback(
      TransportCallbackType::kOnConnectionFailed);
  server_peer()->quic_transport_delegate()->ExpectCallback(
      TransportCallbackType::kOnConnectionFailed);
  client_connection()->CloseConnection(
      quic::QuicErrorCode::QUIC_INTERNAL_ERROR, "internal error",
      quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
  RunUntilCallbacksFired();

  ExpectTransportsClosed();
  EXPECT_EQ(
      1, client_peer()->quic_transport_delegate()->connection_failed_count());
  EXPECT_EQ(
      1, server_peer()->quic_transport_delegate()->connection_failed_count());
}

// Tests that the appropriate callbacks are fired when the server's connection
// fails after the transports have connected.
TEST_F(P2PQuicTransportTest, ServerConnectionFailureAfterConnected) {
  Initialize();
  Connect();
  // Close the connection with an internal QUIC error.
  client_peer()->quic_transport_delegate()->ExpectCallback(
      TransportCallbackType::kOnConnectionFailed);
  server_peer()->quic_transport_delegate()->ExpectCallback(
      TransportCallbackType::kOnConnectionFailed);
  server_connection()->CloseConnection(
      quic::QuicErrorCode::QUIC_INTERNAL_ERROR, "internal error",
      quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
  RunUntilCallbacksFired();

  ExpectTransportsClosed();
  EXPECT_EQ(
      1, client_peer()->quic_transport_delegate()->connection_failed_count());
  EXPECT_EQ(
      1, server_peer()->quic_transport_delegate()->connection_failed_count());
}

// Tests that closing the connection with no ACK frame does not make any
// difference in the closing procedure.
TEST_F(P2PQuicTransportTest, ConnectionFailureNoAckFrame) {
  Initialize();
  Connect();
  client_peer()->quic_transport_delegate()->ExpectCallback(
      TransportCallbackType::kOnConnectionFailed);
  server_peer()->quic_transport_delegate()->ExpectCallback(
      TransportCallbackType::kOnConnectionFailed);
  client_connection()->CloseConnection(
      quic::QuicErrorCode::QUIC_INTERNAL_ERROR, "internal error",
      quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET_WITH_NO_ACK);
  RunUntilCallbacksFired();

  ExpectTransportsClosed();
  EXPECT_EQ(
      1, client_peer()->quic_transport_delegate()->connection_failed_count());
  EXPECT_EQ(
      1, server_peer()->quic_transport_delegate()->connection_failed_count());
}

// Tests that a silent failure will only close on one side.
TEST_F(P2PQuicTransportTest, ConnectionSilentFailure) {
  Initialize();
  Connect();
  client_peer()->quic_transport_delegate()->ExpectCallback(
      TransportCallbackType::kOnConnectionFailed);
  client_connection()->CloseConnection(
      quic::QuicErrorCode::QUIC_INTERNAL_ERROR, "internal error",
      quic::ConnectionCloseBehavior::SILENT_CLOSE);
  RunUntilCallbacksFired();

  EXPECT_TRUE(IsClientClosed());
  EXPECT_EQ(
      1, client_peer()->quic_transport_delegate()->connection_failed_count());
  EXPECT_FALSE(IsServerClosed());
  EXPECT_EQ(
      0, server_peer()->quic_transport_delegate()->connection_failed_count());
}

// Tests that the client transport can create a stream and an incoming stream
// will be created on the remote server.
TEST_F(P2PQuicTransportTest, ClientCreatesStream) {
  Initialize();
  Connect();
  P2PQuicStreamImpl* client_stream =
      client_peer()->quic_transport()->CreateStream();
  RunCurrentTasks();

  ASSERT_TRUE(client_stream);
  EXPECT_TRUE(client_peer()->quic_transport()->HasOpenDynamicStreams());
  EXPECT_EQ(0, server_peer()->quic_transport_delegate()->on_stream_count());
  EXPECT_FALSE(server_peer()->quic_transport()->HasOpenDynamicStreams());

  // After sending data across it will trigger a stream to be created on the
  // server side.
  server_peer()->quic_transport_delegate()->ExpectCallback(
      TransportCallbackType::kOnStream);
  client_stream->WriteOrBufferData("hello", false, nullptr);
  RunUntilCallbacksFired();

  EXPECT_EQ(1, server_peer()->quic_transport_delegate()->on_stream_count());
  EXPECT_TRUE(server_peer()->quic_transport()->HasOpenDynamicStreams());
}

// Tests that the server transport can create a stream and an incoming stream
// will be created on the remote client.
TEST_F(P2PQuicTransportTest, ServerCreatesStream) {
  Initialize();
  Connect();
  P2PQuicStreamImpl* server_stream =
      server_peer()->quic_transport()->CreateStream();
  RunCurrentTasks();

  ASSERT_TRUE(server_stream);
  EXPECT_TRUE(server_peer()->quic_transport()->HasOpenDynamicStreams());
  EXPECT_EQ(0, client_peer()->quic_transport_delegate()->on_stream_count());
  EXPECT_FALSE(client_peer()->quic_transport()->HasOpenDynamicStreams());

  // After sending data across it will trigger a stream to be created on the
  // client side.
  client_peer()->quic_transport_delegate()->ExpectCallback(
      TransportCallbackType::kOnStream);
  server_stream->WriteOrBufferData("hello", false, nullptr);
  RunUntilCallbacksFired();

  EXPECT_EQ(1, client_peer()->quic_transport_delegate()->on_stream_count());
  EXPECT_TRUE(client_peer()->quic_transport()->HasOpenDynamicStreams());
}

// Tests that when the client transport calls Stop() it closes its outgoing
// stream, which, in turn closes the incoming stream on the server quic
// transport.
TEST_F(P2PQuicTransportTest, ClientClosingConnectionClosesStreams) {
  Initialize();
  Connect();
  SetupConnectedStreams();

  client_peer()->quic_transport()->Stop();
  RunCurrentTasks();

  ExpectTransportsClosed();
  ExpectStreamsClosed();
}

// Tests that when the server transport calls Stop() it closes its incoming
// stream, which, in turn closes the outgoing stream on the client quic
// transport.
TEST_F(P2PQuicTransportTest, ServerClosingConnectionClosesStreams) {
  Initialize();
  Connect();
  SetupConnectedStreams();

  server_peer()->quic_transport()->Stop();
  RunCurrentTasks();

  ExpectTransportsClosed();
  ExpectStreamsClosed();
}

// Tests that calling Reset() will close both side's streams for reading and
// writing.
TEST_F(P2PQuicTransportTest, ClientStreamReset) {
  Initialize();
  Connect();
  SetupConnectedStreams();

  server_stream_delegate()->ExpectCallback(StreamCallbackType::kOnRemoteReset);
  client_stream()->Reset();
  RunUntilCallbacksFired();

  ExpectStreamsClosed();
}

// Tests that calling Reset() will close both side's streams for reading and
// writing.
TEST_F(P2PQuicTransportTest, ServerStreamReset) {
  Initialize();
  Connect();
  SetupConnectedStreams();

  client_stream_delegate()->ExpectCallback(StreamCallbackType::kOnRemoteReset);
  server_stream()->Reset();
  RunUntilCallbacksFired();

  ExpectStreamsClosed();
}

// Tests the basic case for calling Finish() on both sides.
TEST_F(P2PQuicTransportTest, StreamFinishHandshake) {
  Initialize();
  Connect();
  SetupConnectedStreams();

  server_stream_delegate()->ExpectCallback(StreamCallbackType::kOnRemoteFinish);
  client_stream()->Finish();
  RunUntilCallbacksFired();

  ASSERT_EQ(1u, server_peer()->quic_transport()->GetNumActiveStreams());
  ASSERT_EQ(1u, client_peer()->quic_transport()->GetNumActiveStreams());
  EXPECT_EQ(0, client_stream_delegate()->remote_finish_count());
  EXPECT_TRUE(client_stream()->write_side_closed());
  EXPECT_FALSE(client_stream()->reading_stopped());
  EXPECT_FALSE(server_stream()->write_side_closed());
  EXPECT_TRUE(server_stream()->reading_stopped());
  EXPECT_FALSE(
      server_peer()->quic_transport()->IsClosedStream(server_stream_id()));
  EXPECT_FALSE(
      client_peer()->quic_transport()->IsClosedStream(client_stream_id()));

  client_stream_delegate()->ExpectCallback(StreamCallbackType::kOnRemoteFinish);
  server_stream()->Finish();
  RunUntilCallbacksFired();
  // This is required so that the client acks the FIN back to the server side
  // and the server side removes its zombie streams.
  RunCurrentTasks();

  ASSERT_EQ(0u, server_peer()->quic_transport()->GetNumActiveStreams());
  ASSERT_EQ(0u, client_peer()->quic_transport()->GetNumActiveStreams());
  EXPECT_EQ(1, server_stream_delegate()->remote_finish_count());
  EXPECT_EQ(1, client_stream_delegate()->remote_finish_count());
  EXPECT_TRUE(
      server_peer()->quic_transport()->IsClosedStream(server_stream_id()));
  EXPECT_TRUE(
      client_peer()->quic_transport()->IsClosedStream(client_stream_id()));
}

// Tests that if a Reset() is called after Finish(), both sides close down
// properly.
TEST_F(P2PQuicTransportTest, StreamResetAfterFinish) {
  Initialize();
  Connect();
  SetupConnectedStreams();

  server_stream_delegate()->ExpectCallback(StreamCallbackType::kOnRemoteFinish);
  client_stream()->Finish();
  RunUntilCallbacksFired();

  server_stream_delegate()->ExpectCallback(StreamCallbackType::kOnRemoteReset);
  client_stream()->Reset();
  RunUntilCallbacksFired();

  ExpectStreamsClosed();
  EXPECT_EQ(0, client_stream_delegate()->remote_reset_count());
}

// Tests that if a Reset() is called after receiving a stream frame with the FIN
// bit set from the remote side, both sides close down properly.
TEST_F(P2PQuicTransportTest, StreamResetAfterRemoteFinish) {
  Initialize();
  Connect();
  SetupConnectedStreams();

  server_stream_delegate()->ExpectCallback(StreamCallbackType::kOnRemoteFinish);
  client_stream()->Finish();
  RunUntilCallbacksFired();

  client_stream_delegate()->ExpectCallback(StreamCallbackType::kOnRemoteReset);
  // The server stream has received its FIN bit from the remote side, and
  // responds with a Reset() to close everything down.
  server_stream()->Reset();
  RunUntilCallbacksFired();

  ExpectStreamsClosed();
  EXPECT_EQ(0, server_stream_delegate()->remote_reset_count());
}

// The following unit tests are more isolated to the P2PQuicStreamImpl
// implementation. They only test a stream's behavior on one side (not any
// interactions between two connected streams).

TEST_F(P2PQuicTransportTest, StreamFinishSendsFinAndCanNoLongerWrite) {
  Initialize();
  Connect();
  P2PQuicStreamImpl* stream = client_peer()->quic_transport()->CreateStream();

  stream->Finish();
  EXPECT_TRUE(stream->fin_sent());
  EXPECT_TRUE(stream->write_side_closed());
  EXPECT_FALSE(stream->reading_stopped());
}

TEST_F(P2PQuicTransportTest, StreamResetSendsRstAndBecomesClosed) {
  Initialize();
  Connect();

  P2PQuicStreamImpl* stream = client_peer()->quic_transport()->CreateStream();
  quic::QuicStreamId stream_id = stream->id();

  stream->Reset();

  EXPECT_TRUE(client_peer()->quic_transport()->IsClosedStream(stream_id));
}

// Tests that when a stream receives a stream frame with the FIN bit set it
// will fire the appropriate callback and close the stream for reading.
TEST_F(P2PQuicTransportTest, StreamOnStreamFrameWithFin) {
  Initialize();
  Connect();
  P2PQuicStreamImpl* stream = client_peer()->quic_transport()->CreateStream();
  QuicStreamDelegateForTesting stream_delegate;
  stream->SetDelegate(&stream_delegate);

  quic::QuicStreamFrame fin_frame(stream->id(), /*fin=*/true, 0, 0);
  stream->OnStreamFrame(fin_frame);
  EXPECT_EQ(1, stream_delegate.remote_finish_count());
  EXPECT_TRUE(stream->reading_stopped());
  EXPECT_FALSE(stream->write_side_closed());
}

// Tests that when a stream receives a stream frame with the FIN bit set after
// it has called Finish(), then the stream will close.
TEST_F(P2PQuicTransportTest, StreamClosedAfterReceivesFin) {
  Initialize();
  Connect();
  P2PQuicStreamImpl* stream = client_peer()->quic_transport()->CreateStream();
  quic::QuicStreamId stream_id = stream->id();
  QuicStreamDelegateForTesting stream_delegate;
  stream->SetDelegate(&stream_delegate);

  stream->Finish();
  EXPECT_FALSE(client_peer()->quic_transport()->IsClosedStream(stream_id));
  quic::QuicStreamFrame fin_frame(stream->id(), /*fin=*/true, 0, 0);
  stream->OnStreamFrame(fin_frame);

  EXPECT_TRUE(client_peer()->quic_transport()->IsClosedStream(stream_id));
}

// Tests that when a stream calls Finish() after receiving a stream frame with
// the FIN bit then the stream will close.
TEST_F(P2PQuicTransportTest, StreamClosedAfterFinish) {
  Initialize();
  Connect();
  P2PQuicStreamImpl* stream = client_peer()->quic_transport()->CreateStream();
  quic::QuicStreamId stream_id = stream->id();
  QuicStreamDelegateForTesting stream_delegate;
  stream->SetDelegate(&stream_delegate);

  quic::QuicStreamFrame fin_frame(stream->id(), /*fin=*/true, 0, 0);
  stream->OnStreamFrame(fin_frame);
  EXPECT_FALSE(client_peer()->quic_transport()->IsClosedStream(stream_id));
  stream->Finish();

  EXPECT_TRUE(client_peer()->quic_transport()->IsClosedStream(stream_id));
}

// Tests that when a stream receives a RST_STREAM frame it will fire the
// appropriate callback and the stream will become closed.
TEST_F(P2PQuicTransportTest, StreamClosedAfterReceivingReset) {
  Initialize();
  Connect();
  P2PQuicStreamImpl* stream = client_peer()->quic_transport()->CreateStream();
  quic::QuicStreamId stream_id = stream->id();
  QuicStreamDelegateForTesting stream_delegate;
  stream->SetDelegate(&stream_delegate);

  quic::QuicRstStreamFrame rst_frame(quic::kInvalidControlFrameId, stream_id,
                                     quic::QUIC_STREAM_CANCELLED, 0);
  stream->OnStreamReset(rst_frame);

  EXPECT_EQ(1, stream_delegate.remote_reset_count());
  EXPECT_TRUE(client_peer()->quic_transport()->IsClosedStream(stream_id));
}

}  // namespace blink
