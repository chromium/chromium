// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport.h"
#include "base/bind.h"
#include "net/quic/mock_crypto_client_stream.h"
#include "net/quic/mock_crypto_client_stream_factory.h"
#include "net/quic/quic_chromium_alarm_factory.h"
#include "net/quic/test_task_runner.h"
#include "net/test/gtest_util.h"
#include "net/third_party/quiche/src/quic/core/crypto/proof_verifier.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_compressed_certs_cache.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_crypto_server_config.h"
#include "net/third_party/quiche/src/quic/core/quic_server_id.h"
#include "net/third_party/quiche/src/quic/core/quic_session.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_mem_slice_span.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quiche/src/quic/test_tools/mock_clock.h"
#include "net/third_party/quiche/src/quic/test_tools/quic_test_utils.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_crypto_config_factory_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_crypto_stream_factory_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_packet_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport_factory_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/test/mock_p2p_quic_packet_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/test/mock_p2p_quic_stream_delegate.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/test/mock_p2p_quic_transport_delegate.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/webrtc/rtc_base/rtc_certificate.h"
#include "third_party/webrtc/rtc_base/ssl_fingerprint.h"
#include "third_party/webrtc/rtc_base/ssl_identity.h"

namespace blink {

namespace {

using testing::_;
using testing::ElementsAreArray;
using testing::Eq;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using ::testing::MakePolymorphicAction;
using ::testing::PolymorphicAction;
using testing::Property;
using testing::ResultOf;
using testing::Return;

const uint8_t kTriggerRemoteStreamPhrase[] = {'o', 'p', 'e', 'n', ' ', 's',
                                              'e', 's', 'a', 'm', 'e'};
const uint8_t kMessage[] = {'h', 'o', 'w', 'd', 'y'};
const uint8_t kMessage2[] = {'p', 'a', 'r', 't', 'n', 'e', 'r'};
const uint8_t kClientMessage[] = {'h', 'o', 'w', 'd', 'y'};
const uint8_t kServerMessage[] = {'p', 'a', 'r', 't', 'n', 'e', 'r'};
const uint32_t kTransportWriteBufferSize = 100 * 1024;
const uint32_t kTransportDelegateReadBufferSize = 100 * 1024;

template <wtf_size_t Size>
static Vector<uint8_t> VectorFromArray(const uint8_t (&array)[Size]) {
  Vector<uint8_t> vector;
  vector.Append(array, Size);
  return vector;
}

// A custom gmock Action that fires the given callback. This is used in
// conjuction with the CallbackRunLoop in order to drive the TestTaskRunner
// until callbacks are fired. For example:
//   CallbackRunLoop run_loop(runner());
//   EXPECT_CALL(&object, foo())
//       .WillOnce(FireCallback(run_loop.CreateCallback()));
//   run_loop.RunUntilCallbacksFired(task_runner);
class FireCallbackAction {
  STACK_ALLOCATED();

 public:
  FireCallbackAction(base::RepeatingCallback<void()> callback)
      : callback_(callback) {}

  template <typename Result, typename ArgumentTuple>
  Result Perform(const ArgumentTuple& args) const {
    callback_.Run();
  }

 private:
  base::RepeatingCallback<void()> callback_;
};

// Returns the custom gmock PolymorphicAction created from the
// FireCallbackAction above.
PolymorphicAction<FireCallbackAction> FireCallback(
    base::RepeatingCallback<void()> callback) {
  return MakePolymorphicAction(FireCallbackAction(callback));
}

// A helper object that can drive a TestTaskRunner's tasks, until
// callbacks are fired.
//
// TODO(https://crbug.com/874296): If the test files get moved to the platform
// directory we will run the tests in a different test environment. In that
// case it will make more sense to use the TestCompletionCallback and the
// RunLoop for driving the test.
class CallbackRunLoop {
  STACK_ALLOCATED();

 public:
  CallbackRunLoop(scoped_refptr<net::test::TestTaskRunner> task_runner)
      : task_runner_(task_runner) {}

  // Drives the run loop until all created callbacks have been fired.
  // This is done using the |task_runner_|, which runs the tasks
  // in the correct order and then advances the quic::MockClock to the time the
  // task is run.
  void RunUntilCallbacksFired() {
    while (callback_counter_ != 0) {
      ASSERT_GT(task_runner_->GetPostedTasks().size(), 0u);
      task_runner_->RunNextTask();
    }
  }

  // Creates a callback and increments the |callback_counter_|. The callback,
  // when fired, will decrement the counter. This callback must only
  // be Run() once (it is a RepeatingCallback because MakePolymorphicAction()
  // requires that the action is COPYABLE).
  base::RepeatingCallback<void()> CreateCallback() {
    callback_counter_++;
    return base::BindRepeating(&CallbackRunLoop::OnCallbackFired,
                               base::Unretained(this));
  }

 private:
  void OnCallbackFired() { callback_counter_--; }

  scoped_refptr<net::test::TestTaskRunner> task_runner_;
  // Incremented when a callback is created and decremented when the returned
  // callback is later Run().
  size_t callback_counter_ = 0;
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
  }

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

  uint64_t last_packet_num() { return last_packet_num_; }

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
  quic::QuicDeque<std::string> packet_queue_;
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
  uint64_t last_packet_num_;
  quic::MockClock* clock_;
};

// A helper class to bundle test objects together. It keeps track of the
// P2PQuicTransport, P2PQuicStream and the associated delegate objects. This
// also keeps track of when callbacks are expected on the delegate objects,
// which allows running the TestTaskRunner tasks until they have been fired.
class QuicPeerForTest {
  USING_FAST_MALLOC(QuicPeerForTest);

 public:
  QuicPeerForTest(
      std::unique_ptr<FakePacketTransport> packet_transport,
      std::unique_ptr<MockP2PQuicTransportDelegate> quic_transport_delegate,
      std::unique_ptr<P2PQuicTransportImpl> quic_transport,
      rtc::scoped_refptr<rtc::RTCCertificate> certificate)
      : packet_transport_(std::move(packet_transport)),
        quic_transport_delegate_(std::move(quic_transport_delegate)),
        quic_transport_(std::move(quic_transport)),
        certificate_(certificate) {}

  // A helper that creates a stream and creates and attaches a delegate.
  void CreateStreamWithDelegate() {
    stream_ = quic_transport_->CreateStream();
    stream_delegate_ = std::make_unique<MockP2PQuicStreamDelegate>();
    stream_->SetDelegate(stream_delegate_.get());
    stream_id_ = stream_->id();
  }

  // When a remote stream is created via P2PQuicTransport::Delegate::OnStream,
  // this is called to set the stream.
  void SetStreamAndDelegate(
      P2PQuicStreamImpl* stream,
      std::unique_ptr<MockP2PQuicStreamDelegate> stream_delegate) {
    DCHECK(stream);
    stream_ = stream;
    stream_id_ = stream->id();
    stream_delegate_ = std::move(stream_delegate);
  }

  FakePacketTransport* packet_transport() { return packet_transport_.get(); }

  MockP2PQuicTransportDelegate* quic_transport_delegate() {
    return quic_transport_delegate_.get();
  }

  P2PQuicTransportImpl* quic_transport() { return quic_transport_.get(); }

  rtc::scoped_refptr<rtc::RTCCertificate> certificate() { return certificate_; }

  P2PQuicStreamImpl* stream() const { return stream_; }

  MockP2PQuicStreamDelegate* stream_delegate() const {
    return stream_delegate_.get();
  }

  quic::QuicStreamId stream_id() const { return stream_id_; }

 private:
  std::unique_ptr<FakePacketTransport> packet_transport_;
  std::unique_ptr<MockP2PQuicTransportDelegate> quic_transport_delegate_;
  // The corresponding delegate to |stream_|.
  std::unique_ptr<MockP2PQuicStreamDelegate> stream_delegate_ = nullptr;
  // Created as a result of CreateStreamWithDelegate() or RemoteStreamCreated().
  // Owned by the |quic_transport_|.
  P2PQuicStreamImpl* stream_ = nullptr;
  // The corresponding ID for |stream_|. This can be used to check if the stream
  // is closed at the transport level (after the stream object could be
  // deleted).
  quic::QuicStreamId stream_id_;
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
class FailingProofVerifierStub : public quic::ProofVerifier {
 public:
  FailingProofVerifierStub() {}
  ~FailingProofVerifierStub() override {}

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
    return quic::QUIC_FAILURE;
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
    return quic::QUIC_FAILURE;
  }

  std::unique_ptr<quic::ProofVerifyContext> CreateDefaultContext() override {
    return nullptr;
  }
};

// A dummy implementation of a quic::ProofSource.
class ProofSourceStub : public quic::ProofSource {
 public:
  ProofSourceStub() {}
  ~ProofSourceStub() override {}

  // ProofSource override.
  void GetProof(const quic::QuicSocketAddress& server_addr,
                const std::string& hostname,
                const std::string& server_config,
                quic::QuicTransportVersion transport_version,
                quic::QuicStringPiece chlo_hash,
                std::unique_ptr<Callback> callback) override {
    quic::QuicCryptoProof proof;
    proof.signature = "Test signature";
    proof.leaf_cert_scts = "Test timestamp";
    callback->Run(true, GetCertChain(server_addr, hostname), proof,
                  nullptr /* details */);
  }

  quic::QuicReferenceCountedPointer<Chain> GetCertChain(
      const quic::QuicSocketAddress& server_address,
      const std::string& hostname) override {
    WebVector<std::string> certs;
    certs.emplace_back("Test cert");
    return quic::QuicReferenceCountedPointer<Chain>(
        new ProofSource::Chain(certs.ReleaseVector()));
  }
  void ComputeTlsSignature(
      const quic::QuicSocketAddress& server_address,
      const std::string& hostname,
      uint16_t signature_algorithm,
      quic::QuicStringPiece in,
      std::unique_ptr<SignatureCallback> callback) override {
    callback->Run(true, "Test signature");
  }
};

// Creates crypto configs that will fail a QUIC handshake.
class FailingQuicCryptoConfigFactory final : public P2PQuicCryptoConfigFactory {
 public:
  FailingQuicCryptoConfigFactory(quic::QuicRandom* quic_random)
      : quic_random_(quic_random) {}

  std::unique_ptr<quic::QuicCryptoClientConfig> CreateClientCryptoConfig()
      override {
    return std::make_unique<quic::QuicCryptoClientConfig>(
        std::make_unique<FailingProofVerifierStub>());
  }

  std::unique_ptr<quic::QuicCryptoServerConfig> CreateServerCryptoConfig()
      override {
    return std::make_unique<quic::QuicCryptoServerConfig>(
        quic::QuicCryptoServerConfig::TESTING, quic_random_,
        std::make_unique<ProofSourceStub>(),
        quic::KeyExchangeSource::Default());
  }

 private:
  quic::QuicRandom* quic_random_;
};

// A CryptoClientStream that bypasses the QUIC handshake and becomes connected.
class ConnectedCryptoClientStream final : public quic::QuicCryptoClientStream {
 public:
  ConnectedCryptoClientStream(
      const quic::QuicServerId& server_id,
      quic::QuicSession* session,
      std::unique_ptr<quic::ProofVerifyContext> verify_context,
      quic::QuicCryptoClientConfig* crypto_config,
      quic::QuicCryptoClientStream::ProofHandler* proof_handler)
      : quic::QuicCryptoClientStream(server_id,
                                     session,
                                     std::move(verify_context),
                                     crypto_config,
                                     proof_handler),
        session_(session) {}
  ~ConnectedCryptoClientStream() override {}

  bool CryptoConnect() override {
    encryption_established_ = true;
    handshake_confirmed_ = true;
    // quic::QuicSession checks that its config has been negotiated after the
    // handshake has been confirmed. The easiest way to fake negotiated values
    // is to have the config object process a hello message.
    quic::QuicConfig config;
    config.SetIdleNetworkTimeout(
        quic::QuicTime::Delta::FromSeconds(2 * quic::kMaximumIdleTimeoutSecs),
        quic::QuicTime::Delta::FromSeconds(quic::kMaximumIdleTimeoutSecs));
    config.SetBytesForConnectionIdToSend(quic::PACKET_8BYTE_CONNECTION_ID);
    config.SetMaxIncomingBidirectionalStreamsToSend(
        quic::kDefaultMaxStreamsPerConnection / 2);
    config.SetMaxIncomingUnidirectionalStreamsToSend(
        quic::kDefaultMaxStreamsPerConnection / 2);
    quic::CryptoHandshakeMessage message;
    config.ToHandshakeMessage(&message,
                              session()->connection()->transport_version());
    std::string error_details;
    session()->config()->ProcessPeerHello(message, quic::CLIENT,
                                          &error_details);
    session()->OnConfigNegotiated();
    session()->OnCryptoHandshakeEvent(quic::QuicSession::HANDSHAKE_CONFIRMED);
    return true;
  }

  quic::QuicSession* session() { return session_; }

  bool encryption_established() const override {
    return encryption_established_;
  }

  bool handshake_confirmed() const override { return handshake_confirmed_; }

 private:
  bool encryption_established_ = false;
  bool handshake_confirmed_ = false;
  // Outlives this object.
  quic::QuicSession* session_;
};

// A P2PQuicCryptoStream factory that uses a ConnectedCryptoClientStream
// test object that can fake a successful connection.
class ConnectedCryptoClientStreamFactory final
    : public P2PQuicCryptoStreamFactory {
 public:
  ~ConnectedCryptoClientStreamFactory() override {}

  std::unique_ptr<quic::QuicCryptoClientStream> CreateClientCryptoStream(
      quic::QuicSession* session,
      quic::QuicCryptoClientConfig* crypto_config,
      quic::QuicCryptoClientStream::ProofHandler* proof_handler) override {
    quic::QuicServerId server_id("dummy_host", 12345);
    return std::make_unique<ConnectedCryptoClientStream>(
        server_id, session,
        crypto_config->proof_verifier()->CreateDefaultContext(), crypto_config,
        proof_handler);
  }

  // Creates a real quic::QuiCryptoServerStream.
  std::unique_ptr<quic::QuicCryptoServerStream> CreateServerCryptoStream(
      const quic::QuicCryptoServerConfig* crypto_config,
      quic::QuicCompressedCertsCache* compressed_certs_cache,
      quic::QuicSession* session,
      quic::QuicCryptoServerStream::Helper* helper) override {
    return std::make_unique<quic::QuicCryptoServerStream>(
        crypto_config, compressed_certs_cache, session, helper);
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
  P2PQuicTransportTest() {
    // Quic crashes if packets are sent at time 0, and the clock defaults to 0.
    clock_.AdvanceTime(quic::QuicTime::Delta::FromMilliseconds(1000));
    quic_random_ = quic::QuicRandom::GetInstance();
    runner_ = base::MakeRefCounted<net::test::TestTaskRunner>(&clock_);
    alarm_factory_ =
        std::make_unique<net::QuicChromiumAlarmFactory>(runner_.get(), &clock_);
  }

  ~P2PQuicTransportTest() override {
    // This must be done before desctructing the transports so that we don't
    // have any dangling pointers.
    client_peer_->packet_transport()->DisconnectPeerTransport(
        server_peer_->packet_transport());
    server_peer_->packet_transport()->DisconnectPeerTransport(
        client_peer_->packet_transport());
  }

  // Supplying the |client_crypto_factory| and |server_crypto_factory| allows
  // testing a failing QUIC handshake. The |client_certificate| and
  // |server_certificate| must be the same certificates used in the crypto
  // factories.
  void Initialize(
      std::unique_ptr<P2PQuicCryptoConfigFactory> client_crypto_factory,
      std::unique_ptr<P2PQuicCryptoConfigFactory> server_crypto_factory) {
    auto client_packet_transport =
        std::make_unique<FakePacketTransport>(alarm_factory_.get(), &clock_);
    auto server_packet_transport =
        std::make_unique<FakePacketTransport>(alarm_factory_.get(), &clock_);
    // Connect the transports so that they can speak to each other.
    client_packet_transport->ConnectPeerTransport(
        server_packet_transport.get());
    server_packet_transport->ConnectPeerTransport(
        client_packet_transport.get());

    rtc::scoped_refptr<rtc::RTCCertificate> client_certificate =
        CreateTestCertificate();
    auto client_quic_transport_delegate =
        std::make_unique<MockP2PQuicTransportDelegate>();
    P2PQuicTransportConfig client_config(
        quic::Perspective::IS_CLIENT, {client_certificate},
        kTransportDelegateReadBufferSize, kTransportWriteBufferSize);

    std::unique_ptr<P2PQuicTransportImpl> client_quic_transport =
        P2PQuicTransportImpl::Create(
            &clock_, alarm_factory_.get(), quic_random_,
            client_quic_transport_delegate.get(), client_packet_transport.get(),
            client_config, std::move(client_crypto_factory),
            std::make_unique<P2PQuicCryptoStreamFactoryImpl>());

    client_peer_ = std::make_unique<QuicPeerForTest>(
        std::move(client_packet_transport),
        std::move(client_quic_transport_delegate),
        std::move(client_quic_transport), client_certificate);

    auto server_quic_transport_delegate =
        std::make_unique<MockP2PQuicTransportDelegate>();

    rtc::scoped_refptr<rtc::RTCCertificate> server_certificate =
        CreateTestCertificate();
    P2PQuicTransportConfig server_config(
        quic::Perspective::IS_SERVER, {server_certificate},
        kTransportDelegateReadBufferSize, kTransportWriteBufferSize);

    std::unique_ptr<P2PQuicTransportImpl> server_quic_transport =
        P2PQuicTransportImpl::Create(
            &clock_, alarm_factory_.get(), quic_random_,
            server_quic_transport_delegate.get(), server_packet_transport.get(),
            server_config, std::move(server_crypto_factory),
            std::make_unique<P2PQuicCryptoStreamFactoryImpl>());

    server_peer_ = std::make_unique<QuicPeerForTest>(
        std::move(server_packet_transport),
        std::move(server_quic_transport_delegate),
        std::move(server_quic_transport), server_certificate);
  }

  // Connects both peer's underlying packet transports and creates both
  // P2PQuicTransportImpls.
  void Initialize() {
    Initialize(std::make_unique<P2PQuicCryptoConfigFactoryImpl>(quic_random_),
               std::make_unique<P2PQuicCryptoConfigFactoryImpl>(quic_random_));
  }

  // Uses a crypto config factory that returns a client configuration that
  // will reject the QUIC handshake. This lets us simulate a failng handshake.
  void InitializeWithFailingProofVerification() {
    Initialize(std::make_unique<FailingQuicCryptoConfigFactory>(quic_random_),
               std::make_unique<FailingQuicCryptoConfigFactory>(quic_random_));
  }

  // Drives the test by running the current tasks that are posted.
  void RunCurrentTasks() {
    size_t posted_tasks_size = runner_->GetPostedTasks().size();
    for (size_t i = 0; i < posted_tasks_size; ++i) {
      runner_->RunNextTask();
    }
  }

  // Sets up an initial handshake and connection between peers.
  // This is done using a pre shared key.
  void Connect() {
    CallbackRunLoop run_loop(runner());
    EXPECT_CALL(*client_peer_->quic_transport_delegate(), OnConnected(_))
        .WillOnce(FireCallback(run_loop.CreateCallback()));
    EXPECT_CALL(*server_peer_->quic_transport_delegate(), OnConnected(_))
        .WillOnce(FireCallback(run_loop.CreateCallback()));

    server_peer_->quic_transport()->Start(
        P2PQuicTransport::StartConfig("foobar"));
    client_peer_->quic_transport()->Start(
        P2PQuicTransport::StartConfig("foobar"));
    run_loop.RunUntilCallbacksFired();
  }

  // Creates a P2PQuicStreamImpl on both the client and server side that are
  // connected to each other. The client's stream is created with
  // P2PQuicTransport::CreateStream, while the server's stream is initiated from
  // the remote (client) side, with P2PQuicStream::Delegate::OnStream. This
  // allows us to test at an integration level with connected streams.
  void SetupConnectedStreams() {
    CallbackRunLoop run_loop(runner());
    // We must already have a secure connection before streams are created.
    ASSERT_TRUE(client_peer_->quic_transport()->IsEncryptionEstablished());
    ASSERT_TRUE(server_peer_->quic_transport()->IsEncryptionEstablished());

    client_peer_->CreateStreamWithDelegate();
    ASSERT_TRUE(client_peer_->stream());
    ASSERT_TRUE(client_peer_->stream_delegate());

    // Send some data to trigger the remote side (server side) to get an
    // incoming stream. We capture the stream and set it's delegate when
    // OnStream gets called on the mock object.
    base::RepeatingCallback<void()> callback = run_loop.CreateCallback();
    QuicPeerForTest* server_peer_ptr = server_peer_.get();
    MockP2PQuicStreamDelegate* stream_delegate =
        new MockP2PQuicStreamDelegate();
    P2PQuicStream* server_stream;
    EXPECT_CALL(*server_peer_->quic_transport_delegate(), OnStream(_))
        .WillOnce(Invoke([&callback, &server_stream,
                          &stream_delegate](P2PQuicStream* stream) {
          stream->SetDelegate(stream_delegate);
          server_stream = stream;
          callback.Run();
        }));

    client_peer_->stream()->WriteData(
        VectorFromArray(kTriggerRemoteStreamPhrase),
        /*fin=*/false);
    run_loop.RunUntilCallbacksFired();
    // Set the stream and delegate to the |server_peer_|, so that it can be
    // accessed by tests later.
    server_peer_ptr->SetStreamAndDelegate(
        static_cast<P2PQuicStreamImpl*>(server_stream),
        std::unique_ptr<MockP2PQuicStreamDelegate>(stream_delegate));
    ASSERT_TRUE(client_peer_->stream());
    ASSERT_TRUE(client_peer_->stream_delegate());
  }

  void ExpectConnectionNotEstablished() {
    EXPECT_FALSE(client_peer_->quic_transport()->IsEncryptionEstablished());
    EXPECT_FALSE(client_peer_->quic_transport()->IsCryptoHandshakeConfirmed());
    EXPECT_FALSE(server_peer_->quic_transport()->IsCryptoHandshakeConfirmed());
    EXPECT_FALSE(server_peer_->quic_transport()->IsEncryptionEstablished());
  }

  void ExpectTransportsClosed() {
    EXPECT_TRUE(client_peer_->quic_transport()->IsClosed());
    EXPECT_TRUE(server_peer_->quic_transport()->IsClosed());
  }

  // Expects that streams of both the server and client transports are
  // closed.
  void ExpectStreamsClosed() {
    EXPECT_EQ(0u, client_peer_->quic_transport()->GetNumActiveStreams());
    EXPECT_TRUE(client_peer_->quic_transport()->IsClosedStream(
        client_peer()->stream_id()));

    EXPECT_EQ(0u, server_peer_->quic_transport()->GetNumActiveStreams());
    EXPECT_TRUE(server_peer()->quic_transport()->IsClosedStream(
        server_peer()->stream_id()));
  }

  // Exposes these private functions to the test.
  bool IsClientClosed() { return client_peer_->quic_transport()->IsClosed(); }
  bool IsServerClosed() { return server_peer_->quic_transport()->IsClosed(); }

  QuicPeerForTest* client_peer() { return client_peer_.get(); }

  quic::QuicConnection* client_connection() {
    return client_peer_->quic_transport()->connection();
  }

  QuicPeerForTest* server_peer() { return server_peer_.get(); }

  quic::QuicConnection* server_connection() {
    return server_peer_->quic_transport()->connection();
  }

  scoped_refptr<net::test::TestTaskRunner> runner() { return runner_; }

 private:
  quic::MockClock clock_;
  quic::QuicRandom* quic_random_;
  // The TestTaskRunner is used by the QUIC library for setting/firing alarms.
  // We are able to explicitly run these tasks ourselves with the
  // TestTaskRunner.
  scoped_refptr<net::test::TestTaskRunner> runner_;
  // This is eventually passed down to the QUIC library.
  std::unique_ptr<net::QuicChromiumAlarmFactory> alarm_factory_;

  std::unique_ptr<P2PQuicTransportFactoryImpl> quic_transport_factory_;
  std::unique_ptr<QuicPeerForTest> client_peer_;
  std::unique_ptr<QuicPeerForTest> server_peer_;
};

// Tests that we can connect two quic transports using pre shared keys.
TEST_F(P2PQuicTransportTest, HandshakeConnectsPeersWithPreSharedKeys) {
  Initialize();

  CallbackRunLoop run_loop(runner());
  // Datagrams should be supported.
  EXPECT_CALL(*client_peer()->quic_transport_delegate(),
              OnConnected(Property(
                  &P2PQuicNegotiatedParams::datagrams_supported, Eq(true))))
      .WillOnce(FireCallback(run_loop.CreateCallback()));
  EXPECT_CALL(*server_peer()->quic_transport_delegate(),
              OnConnected(Property(
                  &P2PQuicNegotiatedParams::datagrams_supported, Eq(true))))
      .WillOnce(FireCallback(run_loop.CreateCallback()));

  server_peer()->quic_transport()->Start(
      P2PQuicTransport::StartConfig("foobar"));
  client_peer()->quic_transport()->Start(
      P2PQuicTransport::StartConfig("foobar"));
  run_loop.RunUntilCallbacksFired();

  EXPECT_TRUE(client_peer()->quic_transport()->IsEncryptionEstablished());
  EXPECT_TRUE(client_peer()->quic_transport()->IsCryptoHandshakeConfirmed());
  EXPECT_TRUE(server_peer()->quic_transport()->IsCryptoHandshakeConfirmed());
  EXPECT_TRUE(server_peer()->quic_transport()->IsEncryptionEstablished());
}

// Tests that we can connect two quic transports using remote certificate
// fingerprints. Note that the fingerprints aren't currently used for
// verification.
TEST_F(P2PQuicTransportTest, HandshakeConnectsPeersWithRemoteCertificates) {
  Initialize();

  CallbackRunLoop run_loop(runner());
  // Datagrams should be supported.
  EXPECT_CALL(*client_peer()->quic_transport_delegate(),
              OnConnected(Property(
                  &P2PQuicNegotiatedParams::datagrams_supported, Eq(true))))
      .WillOnce(FireCallback(run_loop.CreateCallback()));
  EXPECT_CALL(*server_peer()->quic_transport_delegate(),
              OnConnected(Property(
                  &P2PQuicNegotiatedParams::datagrams_supported, Eq(true))))
      .WillOnce(FireCallback(run_loop.CreateCallback()));

  // Start the handshake with the remote fingerprints.
  Vector<std::unique_ptr<rtc::SSLFingerprint>> server_fingerprints;
  server_fingerprints.push_back(rtc::SSLFingerprint::CreateUnique(
      "sha-256", *server_peer()->certificate()->identity()));
  server_peer()->quic_transport()->Start(
      P2PQuicTransport::StartConfig(std::move(server_fingerprints)));

  Vector<std::unique_ptr<rtc::SSLFingerprint>> client_fingerprints;
  client_fingerprints.push_back(rtc::SSLFingerprint::CreateUnique(
      "sha-256", *client_peer()->certificate()->identity()));
  client_peer()->quic_transport()->Start(
      P2PQuicTransport::StartConfig(std::move(client_fingerprints)));

  run_loop.RunUntilCallbacksFired();

  EXPECT_TRUE(client_peer()->quic_transport()->IsEncryptionEstablished());
  EXPECT_TRUE(client_peer()->quic_transport()->IsCryptoHandshakeConfirmed());
  EXPECT_TRUE(server_peer()->quic_transport()->IsCryptoHandshakeConfirmed());
  EXPECT_TRUE(server_peer()->quic_transport()->IsEncryptionEstablished());
}

// Tests the standard case for the server side closing the connection.
TEST_F(P2PQuicTransportTest, ServerStops) {
  Initialize();
  Connect();
  CallbackRunLoop run_loop(runner());
  EXPECT_CALL(*client_peer()->quic_transport_delegate(), OnRemoteStopped())
      .WillOnce(FireCallback(run_loop.CreateCallback()));
  EXPECT_CALL(*server_peer()->quic_transport_delegate(), OnRemoteStopped())
      .Times(0);

  server_peer()->quic_transport()->Stop();
  run_loop.RunUntilCallbacksFired();

  ExpectTransportsClosed();
}

// Tests the standard case for the client side closing the connection.
TEST_F(P2PQuicTransportTest, ClientStops) {
  Initialize();
  Connect();
  CallbackRunLoop run_loop(runner());
  EXPECT_CALL(*server_peer()->quic_transport_delegate(), OnRemoteStopped())
      .WillOnce(FireCallback(run_loop.CreateCallback()));
  EXPECT_CALL(*client_peer()->quic_transport_delegate(), OnRemoteStopped())
      .Times(0);

  client_peer()->quic_transport()->Stop();
  run_loop.RunUntilCallbacksFired();

  ExpectTransportsClosed();
}

// Tests that if either side tries to close the connection a second time, it
// will be ignored because the connection has already been closed.
TEST_F(P2PQuicTransportTest, StopAfterStopped) {
  Initialize();
  Connect();
  CallbackRunLoop run_loop(runner());
  EXPECT_CALL(*server_peer()->quic_transport_delegate(), OnRemoteStopped())
      .WillOnce(FireCallback(run_loop.CreateCallback()));
  client_peer()->quic_transport()->Stop();
  run_loop.RunUntilCallbacksFired();

  EXPECT_CALL(*server_peer()->quic_transport_delegate(), OnRemoteStopped())
      .Times(0);
  EXPECT_CALL(*client_peer()->quic_transport_delegate(), OnRemoteStopped())
      .Times(0);

  client_peer()->quic_transport()->Stop();
  server_peer()->quic_transport()->Stop();
  RunCurrentTasks();

  ExpectTransportsClosed();
}

// Tests that the appropriate callbacks are fired when the handshake fails.
TEST_F(P2PQuicTransportTest, HandshakeFailure) {
  InitializeWithFailingProofVerification();
  CallbackRunLoop run_loop(runner());
  EXPECT_CALL(*client_peer()->quic_transport_delegate(),
              OnConnectionFailed(_, _))
      .WillOnce(FireCallback(run_loop.CreateCallback()));
  EXPECT_CALL(*server_peer()->quic_transport_delegate(),
              OnConnectionFailed(_, _))
      .WillOnce(FireCallback(run_loop.CreateCallback()));

  server_peer()->quic_transport()->Start(
      P2PQuicTransport::StartConfig("foobar"));
  client_peer()->quic_transport()->Start(
      P2PQuicTransport::StartConfig("foobar"));
  run_loop.RunUntilCallbacksFired();

  ExpectConnectionNotEstablished();
  ExpectTransportsClosed();
}

// Tests that the handshake fails if the pre shared keys don't match.
// In this case the handshake finishes, but the connection fails because packets
// can't be decrypted.
TEST_F(P2PQuicTransportTest, HandshakeFailsBecauseKeysDontMatch) {
  Initialize();
  CallbackRunLoop run_loop(runner());
  EXPECT_CALL(*client_peer()->quic_transport_delegate(),
              OnConnectionFailed(_, _))
      .WillOnce(FireCallback(run_loop.CreateCallback()));
  EXPECT_CALL(*server_peer()->quic_transport_delegate(),
              OnConnectionFailed(_, _))
      .WillOnce(FireCallback(run_loop.CreateCallback()));

  server_peer()->quic_transport()->Start(
      P2PQuicTransport::StartConfig("foobar"));
  client_peer()->quic_transport()->Start(
      P2PQuicTransport::StartConfig("barfoo"));
  run_loop.RunUntilCallbacksFired();

  ExpectTransportsClosed();
}

// Tests that the appropriate callbacks are fired when the client's connection
// fails after the transports have connected.
TEST_F(P2PQuicTransportTest, ClientConnectionFailureAfterConnected) {
  Initialize();
  Connect();
  CallbackRunLoop run_loop(runner());
  EXPECT_CALL(*client_peer()->quic_transport_delegate(),
              OnConnectionFailed(_, /*from_remote=*/false))
      .WillOnce(FireCallback(run_loop.CreateCallback()));
  EXPECT_CALL(*server_peer()->quic_transport_delegate(),
              OnConnectionFailed(_, /*from_remote=*/true))
      .WillOnce(FireCallback(run_loop.CreateCallback()));

  // Close the connection with an internal QUIC error.
  client_connection()->CloseConnection(
      quic::QuicErrorCode::QUIC_INTERNAL_ERROR, "internal error",
      quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
  run_loop.RunUntilCallbacksFired();

  ExpectTransportsClosed();
}

// Tests that the appropriate callbacks are fired when the server's connection
// fails after the transports have connected.
TEST_F(P2PQuicTransportTest, ServerConnectionFailureAfterConnected) {
  Initialize();
  Connect();
  CallbackRunLoop run_loop(runner());
  EXPECT_CALL(*client_peer()->quic_transport_delegate(),
              OnConnectionFailed(_, /*from_remote=*/true))
      .WillOnce(FireCallback(run_loop.CreateCallback()));
  EXPECT_CALL(*server_peer()->quic_transport_delegate(),
              OnConnectionFailed(_, /*from_remote=*/false))
      .WillOnce(FireCallback(run_loop.CreateCallback()));

  server_connection()->CloseConnection(
      quic::QuicErrorCode::QUIC_INTERNAL_ERROR, "internal error",
      quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
  run_loop.RunUntilCallbacksFired();

  ExpectTransportsClosed();
}

// Tests that a silent failure will only close on one side.
TEST_F(P2PQuicTransportTest, ConnectionSilentFailure) {
  Initialize();
  Connect();
  CallbackRunLoop run_loop(runner());
  EXPECT_CALL(*client_peer()->quic_transport_delegate(),
              OnConnectionFailed(_, _))
      .WillOnce(FireCallback(run_loop.CreateCallback()));
  EXPECT_CALL(*server_peer()->quic_transport_delegate(),
              OnConnectionFailed(_, _))
      .Times(0);

  client_connection()->CloseConnection(
      quic::QuicErrorCode::QUIC_INTERNAL_ERROR, "internal error",
      quic::ConnectionCloseBehavior::SILENT_CLOSE);
  run_loop.RunUntilCallbacksFired();

  EXPECT_TRUE(IsClientClosed());
  EXPECT_FALSE(IsServerClosed());
}

// Tests that the client transport can create a stream and an incoming stream
// will be created on the remote server.
TEST_F(P2PQuicTransportTest, ClientCreatesStream) {
  Initialize();
  Connect();
  CallbackRunLoop run_loop(runner());
  client_peer()->CreateStreamWithDelegate();
  ASSERT_TRUE(client_peer()->stream());

  RunCurrentTasks();

  EXPECT_TRUE(client_peer()->quic_transport()->ShouldKeepConnectionAlive());
  EXPECT_FALSE(server_peer()->quic_transport()->ShouldKeepConnectionAlive());

  // After sending data across it will trigger a stream to be created on the
  // server side.
  MockP2PQuicStreamDelegate server_stream_delegate;
  base::RepeatingCallback<void()> callback = run_loop.CreateCallback();
  EXPECT_CALL(*server_peer()->quic_transport_delegate(), OnStream(_))
      .WillOnce(
          Invoke([&callback, &server_stream_delegate](P2PQuicStream* stream) {
            ASSERT_TRUE(stream);
            // The Delegate must get immediately set to a new incoming stream.
            stream->SetDelegate(&server_stream_delegate);
            // Allows the run loop to run until this is fired.
            callback.Run();
          }));

  client_peer()->stream()->WriteData(
      VectorFromArray(kTriggerRemoteStreamPhrase),
      /*fin=*/false);
  run_loop.RunUntilCallbacksFired();

  EXPECT_TRUE(server_peer()->quic_transport()->ShouldKeepConnectionAlive());
}

// Tests that the server transport can create a stream and an incoming stream
// will be created on the remote client.
TEST_F(P2PQuicTransportTest, ServerCreatesStream) {
  Initialize();
  Connect();
  CallbackRunLoop run_loop(runner());
  server_peer()->CreateStreamWithDelegate();
  ASSERT_TRUE(server_peer()->stream());

  RunCurrentTasks();

  EXPECT_TRUE(server_peer()->quic_transport()->ShouldKeepConnectionAlive());
  EXPECT_FALSE(client_peer()->quic_transport()->ShouldKeepConnectionAlive());

  // After sending data across it will trigger a stream to be created on the
  // server side.
  MockP2PQuicStreamDelegate client_stream_delegate;
  base::RepeatingCallback<void()> callback = run_loop.CreateCallback();
  EXPECT_CALL(*client_peer()->quic_transport_delegate(), OnStream(_))
      .WillOnce(
          Invoke([&callback, &client_stream_delegate](P2PQuicStream* stream) {
            ASSERT_TRUE(stream);
            // The Delegate must get immediately set to a new incoming stream.
            stream->SetDelegate(&client_stream_delegate);
            // Allows the run loop to run until this is fired.
            callback.Run();
          }));

  server_peer()->stream()->WriteData(
      VectorFromArray(kTriggerRemoteStreamPhrase),
      /*fin=*/false);
  run_loop.RunUntilCallbacksFired();

  EXPECT_TRUE(client_peer()->quic_transport()->ShouldKeepConnectionAlive());
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
  CallbackRunLoop run_loop(runner());

  EXPECT_CALL(*server_peer()->stream_delegate(), OnRemoteReset())
      .WillOnce(FireCallback(run_loop.CreateCallback()));

  client_peer()->stream()->Reset();
  run_loop.RunUntilCallbacksFired();
  ExpectStreamsClosed();
}

// Tests that calling Reset() will close both side's streams for reading and
// writing.
TEST_F(P2PQuicTransportTest, ServerStreamReset) {
  Initialize();
  Connect();
  SetupConnectedStreams();
  CallbackRunLoop run_loop(runner());

  EXPECT_CALL(*client_peer()->stream_delegate(), OnRemoteReset())
      .WillOnce(FireCallback(run_loop.CreateCallback()));

  server_peer()->stream()->Reset();
  run_loop.RunUntilCallbacksFired();

  ExpectStreamsClosed();
}

// Tests the basic case for sending a FIN bit on both sides.
TEST_F(P2PQuicTransportTest, StreamClosedAfterSendingAndReceivingFin) {
  Initialize();
  Connect();
  SetupConnectedStreams();
  CallbackRunLoop run_loop(runner());

  EXPECT_CALL(*server_peer()->stream_delegate(),
              OnDataReceived(_, /*fin=*/true))
      .WillOnce(FireCallback(run_loop.CreateCallback()));

  client_peer()->stream()->WriteData({}, /*fin=*/true);
  run_loop.RunUntilCallbacksFired();

  ASSERT_EQ(1u, server_peer()->quic_transport()->GetNumActiveStreams());
  ASSERT_EQ(1u, client_peer()->quic_transport()->GetNumActiveStreams());
  EXPECT_TRUE(client_peer()->stream()->write_side_closed());
  EXPECT_FALSE(client_peer()->stream()->reading_stopped());
  EXPECT_FALSE(server_peer()->stream()->write_side_closed());
  EXPECT_TRUE(server_peer()->stream()->reading_stopped());
  EXPECT_FALSE(server_peer()->quic_transport()->IsClosedStream(
      server_peer()->stream_id()));
  EXPECT_FALSE(client_peer()->quic_transport()->IsClosedStream(
      client_peer()->stream_id()));

  EXPECT_CALL(*client_peer()->stream_delegate(),
              OnDataReceived(_, /*fin=*/true))
      .WillOnce(FireCallback(run_loop.CreateCallback()));

  server_peer()->stream()->WriteData({}, /*fin=*/true);
  run_loop.RunUntilCallbacksFired();

  // This is required so that the client acks the FIN back to the server side
  // and the server side removes its zombie streams.
  RunCurrentTasks();

  ASSERT_EQ(0u, server_peer()->quic_transport()->GetNumActiveStreams());
  ASSERT_EQ(0u, client_peer()->quic_transport()->GetNumActiveStreams());
  EXPECT_TRUE(server_peer()->quic_transport()->IsClosedStream(
      server_peer()->stream_id()));
  EXPECT_TRUE(client_peer()->quic_transport()->IsClosedStream(
      client_peer()->stream_id()));
}

// Tests that if a Reset() is called after sending a FIN bit, both sides close
// down properly.
TEST_F(P2PQuicTransportTest, StreamResetAfterSendingFin) {
  Initialize();
  Connect();
  SetupConnectedStreams();
  CallbackRunLoop run_loop(runner());

  EXPECT_CALL(*server_peer()->stream_delegate(),
              OnDataReceived(_, /*fin=*/true))
      .WillOnce(FireCallback(run_loop.CreateCallback()));

  client_peer()->stream()->WriteData({}, /*fin=*/true);
  run_loop.RunUntilCallbacksFired();

  EXPECT_CALL(*server_peer()->stream_delegate(), OnRemoteReset())
      .WillOnce(FireCallback(run_loop.CreateCallback()));
  EXPECT_CALL(*client_peer()->stream_delegate(), OnRemoteReset()).Times(0);

  client_peer()->stream()->Reset();
  run_loop.RunUntilCallbacksFired();

  ExpectStreamsClosed();
}

// Tests that if a Reset() is called after receiving a stream frame with the FIN
// bit set from the remote side, both sides close down properly.
TEST_F(P2PQuicTransportTest, StreamResetAfterReceivingFin) {
  Initialize();
  Connect();
  SetupConnectedStreams();
  CallbackRunLoop run_loop(runner());

  EXPECT_CALL(*server_peer()->stream_delegate(),
              OnDataReceived(_, /*fin=*/true))
      .WillOnce(FireCallback(run_loop.CreateCallback()));

  client_peer()->stream()->WriteData({}, /*fin=*/true);
  run_loop.RunUntilCallbacksFired();

  EXPECT_CALL(*client_peer()->stream_delegate(), OnRemoteReset())
      .WillOnce(FireCallback(run_loop.CreateCallback()));
  EXPECT_CALL(*server_peer()->stream_delegate(), OnRemoteReset()).Times(0);

  // The server stream has received its FIN bit from the remote side, and
  // responds with a Reset() to close everything down.
  server_peer()->stream()->Reset();
  run_loop.RunUntilCallbacksFired();

  ExpectStreamsClosed();
}

// Tests that when datagrams are sent from each side they are received on the
// other end.
TEST_F(P2PQuicTransportTest, DatagramsSentReceivedOnRemoteSide) {
  Initialize();
  Connect();
  CallbackRunLoop run_loop(runner());

  // We should get the appropriate message on each end.
  EXPECT_CALL(*server_peer()->quic_transport_delegate(),
              OnDatagramReceived(ElementsAreArray(kClientMessage)))
      .WillOnce(FireCallback(run_loop.CreateCallback()));
  EXPECT_CALL(*client_peer()->quic_transport_delegate(),
              OnDatagramReceived(ElementsAreArray(kServerMessage)))
      .WillOnce(FireCallback(run_loop.CreateCallback()));
  // The OnDatagramSent callback should fire for each datagram being sent.
  EXPECT_CALL(*server_peer()->quic_transport_delegate(), OnDatagramSent())
      .Times(1)
      .WillOnce(FireCallback(run_loop.CreateCallback()));
  EXPECT_CALL(*client_peer()->quic_transport_delegate(), OnDatagramSent())
      .Times(1)
      .WillOnce(FireCallback(run_loop.CreateCallback()));

  server_peer()->quic_transport()->SendDatagram(
      VectorFromArray(kServerMessage));
  client_peer()->quic_transport()->SendDatagram(
      VectorFromArray(kClientMessage));

  run_loop.RunUntilCallbacksFired();
}

// Tests that when data is sent on a stream it is received on the other end.
TEST_F(P2PQuicTransportTest, StreamDataSentThenReceivedOnRemoteSide) {
  Initialize();
  Connect();
  SetupConnectedStreams();
  CallbackRunLoop run_loop(runner());

  EXPECT_CALL(*server_peer()->stream_delegate(),
              OnDataReceived(ElementsAreArray(kMessage), false))
      .WillOnce(FireCallback(run_loop.CreateCallback()));
  EXPECT_CALL(*client_peer()->stream_delegate(),
              OnWriteDataConsumed(base::size(kMessage)))
      .WillOnce(FireCallback(run_loop.CreateCallback()));

  client_peer()->stream()->WriteData(VectorFromArray(kMessage),
                                     /* fin= */ false);
  run_loop.RunUntilCallbacksFired();
}

// Tests that if both sides have a stream that sends data and FIN bit
// they both close down for reading and writing properly.
TEST_F(P2PQuicTransportTest, StreamDataSentWithFinClosesStreams) {
  Initialize();
  Connect();
  SetupConnectedStreams();
  CallbackRunLoop run_loop(runner());

  EXPECT_CALL(*server_peer()->stream_delegate(),
              OnDataReceived(VectorFromArray(kClientMessage), true))
      .WillOnce(FireCallback(run_loop.CreateCallback()));
  EXPECT_CALL(*server_peer()->stream_delegate(),
              OnWriteDataConsumed(base::size(kServerMessage)))
      .WillOnce(FireCallback(run_loop.CreateCallback()));

  EXPECT_CALL(*client_peer()->stream_delegate(),
              OnDataReceived(ElementsAreArray(kServerMessage), true))
      .WillOnce(FireCallback(run_loop.CreateCallback()));
  EXPECT_CALL(*client_peer()->stream_delegate(),
              OnWriteDataConsumed(base::size(kClientMessage)))
      .WillOnce(FireCallback(run_loop.CreateCallback()));

  client_peer()->stream()->WriteData(VectorFromArray(kClientMessage),
                                     /*fin=*/true);
  server_peer()->stream()->WriteData(VectorFromArray(kServerMessage),
                                     /*fin=*/true);
  run_loop.RunUntilCallbacksFired();

  ExpectStreamsClosed();
}

// Tests that the stats returned by the P2PQuicTransportImpl have the correct
// number of incoming and outgoing streams.
TEST_F(P2PQuicTransportTest, GetStatsForNumberOfStreams) {
  Initialize();
  Connect();

  P2PQuicTransportStats client_stats_1 =
      client_peer()->quic_transport()->GetStats();
  EXPECT_EQ(0u, client_stats_1.num_incoming_streams_created);
  EXPECT_EQ(0u, client_stats_1.num_outgoing_streams_created);
  P2PQuicTransportStats server_stats_1 =
      server_peer()->quic_transport()->GetStats();
  EXPECT_EQ(0u, server_stats_1.num_incoming_streams_created);
  EXPECT_EQ(0u, server_stats_1.num_outgoing_streams_created);

  // Create a stream on the client side and send some data to trigger a stream
  // creation on the remote side.
  client_peer()->CreateStreamWithDelegate();
  CallbackRunLoop run_loop(runner());
  base::RepeatingCallback<void()> callback = run_loop.CreateCallback();
  MockP2PQuicStreamDelegate server_stream_delegate;
  EXPECT_CALL(*server_peer()->quic_transport_delegate(), OnStream(_))
      .WillOnce(
          Invoke([&callback, &server_stream_delegate](P2PQuicStream* stream) {
            stream->SetDelegate(&server_stream_delegate);
            callback.Run();
          }));
  client_peer()->stream()->WriteData(
      VectorFromArray(kTriggerRemoteStreamPhrase),
      /*fin=*/false);
  run_loop.RunUntilCallbacksFired();

  P2PQuicTransportStats client_stats_2 =
      client_peer()->quic_transport()->GetStats();
  EXPECT_EQ(0u, client_stats_2.num_incoming_streams_created);
  EXPECT_EQ(1u, client_stats_2.num_outgoing_streams_created);
  EXPECT_GT(client_stats_2.timestamp, client_stats_1.timestamp);
  P2PQuicTransportStats server_stats_2 =
      server_peer()->quic_transport()->GetStats();
  EXPECT_EQ(1u, server_stats_2.num_incoming_streams_created);
  EXPECT_EQ(0u, server_stats_2.num_outgoing_streams_created);
  EXPECT_GT(server_stats_2.timestamp, server_stats_1.timestamp);
}

// P2PQuicTransport tests that use a fake quic::QuicConnection.
class P2PQuicTransportMockConnectionTest : public testing::Test {
 public:
  P2PQuicTransportMockConnectionTest() {
    connection_helper_ = new quic::test::MockQuicConnectionHelper();
    connection_ = new quic::test::MockQuicConnection(
        connection_helper_, &alarm_factory_, quic::Perspective::IS_CLIENT);

    rtc::scoped_refptr<rtc::RTCCertificate> certificate =
        CreateTestCertificate();
    P2PQuicTransportConfig config(quic::Perspective::IS_CLIENT, {certificate},
                                  kTransportDelegateReadBufferSize,
                                  kTransportWriteBufferSize);
    quic::QuicConfig quic_config;
    transport_ = std::make_unique<P2PQuicTransportImpl>(
        &delegate_, &packet_transport_, config,
        std::unique_ptr<quic::test::MockQuicConnectionHelper>(
            connection_helper_),
        std::unique_ptr<quic::test::MockQuicConnection>(connection_),
        quic_config,
        std::make_unique<P2PQuicCryptoConfigFactoryImpl>(&quic_random_),
        std::make_unique<ConnectedCryptoClientStreamFactory>(), &clock_);
    // Called once in P2PQuicTransportImpl::Start and once in the destructor.
    EXPECT_CALL(packet_transport_, SetReceiveDelegate(transport())).Times(1);
    EXPECT_CALL(packet_transport_, SetReceiveDelegate(nullptr)).Times(1);
    // DCHECKS get hit when the clock is at 0.
    connection_helper_->AdvanceTime(quic::QuicTime::Delta::FromSeconds(1));
    transport_->Start(P2PQuicTransport::StartConfig("foobar"));
  }

  ~P2PQuicTransportMockConnectionTest() override {}

  P2PQuicTransportImpl* transport() { return transport_.get(); }

  MockP2PQuicTransportDelegate* delegate() { return &delegate_; }

  quic::test::MockQuicConnection* connection() { return connection_; }

 private:
  quic::MockClock clock_;
  MockP2PQuicPacketTransport packet_transport_;
  quic::test::MockRandom quic_random_;
  quic::test::MockAlarmFactory alarm_factory_;
  MockP2PQuicTransportDelegate delegate_;
  std::unique_ptr<P2PQuicTransportImpl> transport_;
  // Owned by the |transport_|.
  quic::test::MockQuicConnection* connection_;
  quic::test::MockQuicConnectionHelper* connection_helper_;
};

// Test that when a datagram is received it properly fires the
// OnDatagramReceived function on the delegate.
TEST_F(P2PQuicTransportMockConnectionTest, OnDatagramReceived) {
  EXPECT_TRUE(transport()->CanSendDatagram());
  EXPECT_CALL(*delegate(), OnDatagramReceived(ElementsAreArray(kMessage)));
  transport()->OnMessageReceived(quic::QuicStringPiece(
      reinterpret_cast<const char*>(kMessage), sizeof(kMessage)));
}

// Test that when a datagram is sent that is properly fires the OnDatagramSent
// function on the delegate.
TEST_F(P2PQuicTransportMockConnectionTest, OnDatagramSent) {
  EXPECT_CALL(*connection(), SendMessage(_, _))
      .WillOnce(Invoke(
          [](quic::QuicMessageId message_id, quic::QuicMemSliceSpan message) {
            EXPECT_THAT(message.GetData(0), ElementsAreArray(kMessage));
            return quic::MESSAGE_STATUS_SUCCESS;
          }));
  EXPECT_CALL(*delegate(), OnDatagramSent());

  transport()->SendDatagram(VectorFromArray(kMessage));
}

// Test that when the quic::QuicConnection is congestion control blocked that
// the datagram gets buffered and not sent.
TEST_F(P2PQuicTransportMockConnectionTest, DatagramNotSent) {
  EXPECT_CALL(*connection(), SendMessage(_, _))
      .WillOnce(Return(quic::MESSAGE_STATUS_BLOCKED));
  EXPECT_CALL(*delegate(), OnDatagramSent()).Times(0);

  transport()->SendDatagram(VectorFromArray(kMessage));
}

// Test that when datagrams are buffered they are later sent when the transport
// is no longer congestion control blocked.
TEST_F(P2PQuicTransportMockConnectionTest, BufferedDatagramsSent) {
  EXPECT_CALL(*connection(), SendMessage(_, _))
      .WillOnce(Return(quic::MESSAGE_STATUS_BLOCKED));
  transport()->SendDatagram(VectorFromArray(kMessage));
  transport()->SendDatagram(VectorFromArray(kMessage2));

  EXPECT_CALL(*delegate(), OnDatagramSent()).Times(2);
  // Need to check equality with the function call matcher, instead of
  // passing a lamda that checks equality in an Invoke as done in other tests.
  EXPECT_CALL(*connection(),
              SendMessage(_, ResultOf(
                                 [](quic::QuicMemSliceSpan message) {
                                   return message.GetData(0);
                                 },
                                 ElementsAreArray(kMessage))))
      .WillOnce(Return(quic::MESSAGE_STATUS_SUCCESS));
  EXPECT_CALL(*connection(),
              SendMessage(_, ResultOf(
                                 [](quic::QuicMemSliceSpan message) {
                                   return message.GetData(0);
                                 },
                                 ElementsAreArray(kMessage2))))
      .WillOnce(Return(quic::MESSAGE_STATUS_SUCCESS));

  transport()->OnCanWrite();
}

// Tests the following scenario:
// -Write blocked - datagrams are buffered.
// -Write unblocked - send buffered datagrams.
// -Write blocked - keep datagrams buffered.
TEST_F(P2PQuicTransportMockConnectionTest, BufferedDatagramRemainBuffered) {
  EXPECT_CALL(*connection(), SendMessage(_, _))
      .WillOnce(Return(quic::MESSAGE_STATUS_BLOCKED));
  transport()->SendDatagram(VectorFromArray(kMessage));
  transport()->SendDatagram(VectorFromArray(kMessage2));

  // The first datagram gets sent off after becoming write unblocked, while the
  // second datagram is buffered.
  EXPECT_CALL(*connection(),
              SendMessage(_, ResultOf(
                                 [](quic::QuicMemSliceSpan message) {
                                   return message.GetData(0);
                                 },
                                 ElementsAreArray(kMessage))))
      .WillOnce(Return(quic::MESSAGE_STATUS_SUCCESS));
  EXPECT_CALL(*connection(),
              SendMessage(_, ResultOf(
                                 [](quic::QuicMemSliceSpan message) {
                                   return message.GetData(0);
                                 },
                                 ElementsAreArray(kMessage2))))
      .WillOnce(Return(quic::MESSAGE_STATUS_BLOCKED));
  // No callback for the second datagram, as it is still buffered.
  EXPECT_CALL(*delegate(), OnDatagramSent()).Times(1);

  transport()->OnCanWrite();

  // Sending another datagram at this point should just buffer it.
  EXPECT_CALL(*connection(), SendMessage(_, _)).Times(0);
  transport()->SendDatagram(VectorFromArray(kMessage));
}

TEST_F(P2PQuicTransportMockConnectionTest, LostDatagramUpdatesStats) {
  // The ID the quic::QuicSession will assign to the datagram that is used for
  // callbacks, like OnDatagramLost.
  uint32_t datagram_id;
  EXPECT_CALL(*connection(), SendMessage(_, _))
      .WillOnce(Invoke([&datagram_id](quic::QuicMessageId message_id,
                                      quic::QuicMemSliceSpan message) {
        datagram_id = message_id;
        return quic::MESSAGE_STATUS_SUCCESS;
      }));
  EXPECT_CALL(*delegate(), OnDatagramSent()).Times(1);
  transport()->SendDatagram(VectorFromArray(kMessage));

  EXPECT_EQ(0u, transport()->GetStats().num_datagrams_lost);
  transport()->OnMessageLost(datagram_id);

  EXPECT_EQ(1u, transport()->GetStats().num_datagrams_lost);
}
}  // namespace blink
