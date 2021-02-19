// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/quic_transport.h"

#include <set>
#include <vector>

#include "base/containers/contains.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/log/test_net_log.h"
#include "net/quic/crypto/proof_source_chromium.h"
#include "net/test/test_data_directory.h"
#include "net/third_party/quiche/src/quic/test_tools/crypto_test_utils.h"
#include "net/tools/quic/quic_transport_simple_server.h"
#include "net/url_request/url_request_context.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

// A clock that only mocks out WallNow(), but uses real Now() and
// ApproximateNow().  Useful for certificate verification.
class TestWallClock : public quic::QuicClock {
 public:
  quic::QuicTime Now() const override {
    return quic::QuicChromiumClock::GetInstance()->Now();
  }
  quic::QuicTime ApproximateNow() const override {
    return quic::QuicChromiumClock::GetInstance()->ApproximateNow();
  }
  quic::QuicWallTime WallNow() const override { return wall_now_; }

  void set_wall_now(quic::QuicWallTime now) { wall_now_ = now; }

 private:
  quic::QuicWallTime wall_now_ = quic::QuicWallTime::Zero();
};

class TestConnectionHelper : public quic::QuicConnectionHelperInterface {
 public:
  const quic::QuicClock* GetClock() const override { return &clock_; }
  quic::QuicRandom* GetRandomGenerator() override {
    return quic::QuicRandom::GetInstance();
  }
  quic::QuicBufferAllocator* GetStreamSendBufferAllocator() override {
    return &allocator_;
  }

  TestWallClock& clock() { return clock_; }

 private:
  TestWallClock clock_;
  quic::SimpleBufferAllocator allocator_;
};

mojom::NetworkContextParamsPtr CreateNetworkContextParams() {
  auto context_params = mojom::NetworkContextParams::New();
  // Use a dummy CertVerifier that always passes cert verification, since
  // these unittests don't need to test CertVerifier behavior.
  context_params->cert_verifier_params =
      FakeTestCertVerifierParamsFactory::GetCertVerifierParams();
  return context_params;
}

// We don't use mojo::BlockingCopyToString because it leads to deadlocks.
std::string Read(mojo::ScopedDataPipeConsumerHandle readable) {
  std::string output;
  while (true) {
    char buffer[1024];
    uint32_t size = sizeof(buffer);
    MojoResult result =
        readable->ReadData(buffer, &size, MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      base::RunLoop run_loop;
      base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                       run_loop.QuitClosure());
      run_loop.Run();
      continue;
    }
    if (result == MOJO_RESULT_FAILED_PRECONDITION) {
      return output;
    }
    DCHECK_EQ(result, MOJO_RESULT_OK);
    output.append(buffer, size);
  }
}

class TestHandshakeClient final : public mojom::QuicTransportHandshakeClient {
 public:
  TestHandshakeClient(mojo::PendingReceiver<mojom::QuicTransportHandshakeClient>
                          pending_receiver,
                      base::OnceClosure callback)
      : receiver_(this, std::move(pending_receiver)),
        callback_(std::move(callback)) {
    receiver_.set_disconnect_handler(base::BindOnce(
        &TestHandshakeClient::OnMojoConnectionError, base::Unretained(this)));
  }
  ~TestHandshakeClient() override = default;

  void OnConnectionEstablished(
      mojo::PendingRemote<mojom::QuicTransport> transport,
      mojo::PendingReceiver<mojom::QuicTransportClient> client_receiver)
      override {
    transport_ = std::move(transport);
    client_receiver_ = std::move(client_receiver);
    has_seen_connection_establishment_ = true;
    receiver_.reset();
    std::move(callback_).Run();
  }

  void OnHandshakeFailed(
      const base::Optional<net::QuicTransportError>& error) override {
    has_seen_handshake_failure_ = true;
    receiver_.reset();
    std::move(callback_).Run();
  }

  void OnMojoConnectionError() {
    has_seen_handshake_failure_ = true;
    std::move(callback_).Run();
  }

  mojo::PendingRemote<mojom::QuicTransport> PassTransport() {
    return std::move(transport_);
  }
  mojo::PendingReceiver<mojom::QuicTransportClient> PassClientReceiver() {
    return std::move(client_receiver_);
  }
  bool has_seen_connection_establishment() const {
    return has_seen_connection_establishment_;
  }
  bool has_seen_handshake_failure() const {
    return has_seen_handshake_failure_;
  }
  bool has_seen_mojo_connection_error() const {
    return has_seen_mojo_connection_error_;
  }

 private:
  mojo::Receiver<mojom::QuicTransportHandshakeClient> receiver_;

  mojo::PendingRemote<mojom::QuicTransport> transport_;
  mojo::PendingReceiver<mojom::QuicTransportClient> client_receiver_;
  base::OnceClosure callback_;
  bool has_seen_connection_establishment_ = false;
  bool has_seen_handshake_failure_ = false;
  bool has_seen_mojo_connection_error_ = false;
};

class TestClient final : public mojom::QuicTransportClient {
 public:
  explicit TestClient(
      mojo::PendingReceiver<mojom::QuicTransportClient> pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {
    receiver_.set_disconnect_handler(base::BindOnce(
        &TestClient::OnMojoConnectionError, base::Unretained(this)));
  }

  // mojom::QuicTransportClient implementation.
  void OnDatagramReceived(base::span<const uint8_t> data) override {
    received_datagrams_.push_back(
        std::vector<uint8_t>(data.begin(), data.end()));
  }
  void OnIncomingStreamClosed(uint32_t stream_id, bool fin_received) override {
    closed_incoming_streams_.insert(std::make_pair(stream_id, fin_received));
    if (quit_closure_for_incoming_stream_closure_) {
      std::move(quit_closure_for_incoming_stream_closure_).Run();
    }
  }

  void WaitUntilMojoConnectionError() {
    base::RunLoop run_loop;

    quit_closure_for_mojo_connection_error_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void WaitUntilIncomingStreamIsClosed(uint32_t stream_id) {
    while (!stream_is_closed_as_incoming_stream(stream_id)) {
      base::RunLoop run_loop;

      quit_closure_for_incoming_stream_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }
  }

  const std::vector<std::vector<uint8_t>>& received_datagrams() const {
    return received_datagrams_;
  }
  bool has_received_fin_for(uint32_t stream_id) {
    auto it = closed_incoming_streams_.find(stream_id);
    return it != closed_incoming_streams_.end() && it->second;
  }
  bool stream_is_closed_as_incoming_stream(uint32_t stream_id) {
    return closed_incoming_streams_.find(stream_id) !=
           closed_incoming_streams_.end();
  }
  bool has_seen_mojo_connection_error() const {
    return has_seen_mojo_connection_error_;
  }

 private:
  void OnMojoConnectionError() {
    has_seen_mojo_connection_error_ = true;
    if (quit_closure_for_mojo_connection_error_) {
      std::move(quit_closure_for_mojo_connection_error_).Run();
    }
  }

  mojo::Receiver<mojom::QuicTransportClient> receiver_;

  base::OnceClosure quit_closure_for_mojo_connection_error_;
  base::OnceClosure quit_closure_for_incoming_stream_closure_;

  std::vector<std::vector<uint8_t>> received_datagrams_;
  std::map<uint32_t, bool> closed_incoming_streams_;
  bool has_seen_mojo_connection_error_ = false;
};

quic::ParsedQuicVersion GetTestVersion() {
  quic::ParsedQuicVersion version = quic::DefaultVersionForQuicTransport();
  quic::QuicEnableVersion(version);
  return version;
}

class QuicTransportTest : public testing::Test {
 public:
  QuicTransportTest()
      : QuicTransportTest(
            quic::test::crypto_test_utils::ProofSourceForTesting()) {}
  explicit QuicTransportTest(std::unique_ptr<quic::ProofSource> proof_source)
      : version_(GetTestVersion()),
        origin_(url::Origin::Create(GURL("https://example.org/"))),
        task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        network_service_(NetworkService::CreateForTesting()),
        network_context_remote_(mojo::NullRemote()),
        network_context_(network_service_.get(),
                         network_context_remote_.BindNewPipeAndPassReceiver(),
                         CreateNetworkContextParams()),
        server_(/* port= */ 0, {origin_}, std::move(proof_source)) {
    EXPECT_EQ(EXIT_SUCCESS, server_.Start());

    cert_verifier_.set_default_result(net::OK);
    host_resolver_.rules()->AddRule("test.example.com", "127.0.0.1");

    network_context_.url_request_context()->set_cert_verifier(&cert_verifier_);
    network_context_.url_request_context()->set_host_resolver(&host_resolver_);
    network_context_.url_request_context()->set_net_log(&net_log_);
    auto* quic_context = network_context_.url_request_context()->quic_context();
    quic_context->params()->supported_versions.push_back(version_);
    quic_context->params()->origins_to_force_quic_on.insert(
        net::HostPortPair("test.example.com", 0));
  }
  ~QuicTransportTest() override = default;

  void CreateQuicTransport(
      const GURL& url,
      const url::Origin& origin,
      const net::NetworkIsolationKey& key,
      std::vector<mojom::QuicTransportCertificateFingerprintPtr> fingerprints,
      mojo::PendingRemote<mojom::QuicTransportHandshakeClient>
          handshake_client) {
    network_context_.CreateQuicTransport(
        url, origin, key, std::move(fingerprints), std::move(handshake_client));
  }
  void CreateQuicTransport(
      const GURL& url,
      const url::Origin& origin,
      mojo::PendingRemote<mojom::QuicTransportHandshakeClient>
          handshake_client) {
    CreateQuicTransport(url, origin, net::NetworkIsolationKey(), {},
                        std::move(handshake_client));
  }

  void CreateQuicTransport(
      const GURL& url,
      const url::Origin& origin,
      std::vector<mojom::QuicTransportCertificateFingerprintPtr> fingerprints,
      mojo::PendingRemote<mojom::QuicTransportHandshakeClient>
          handshake_client) {
    CreateQuicTransport(url, origin, net::NetworkIsolationKey(),
                        std::move(fingerprints), std::move(handshake_client));
  }

  GURL GetURL(base::StringPiece suffix) {
    return GURL(base::StrCat(
        {"quic-transport://test.example.com:",
         base::NumberToString(server_.server_address().port()), suffix}));
  }

  const url::Origin& origin() const { return origin_; }
  const NetworkContext& network_context() const { return network_context_; }
  NetworkContext& mutable_network_context() { return network_context_; }
  net::RecordingTestNetLog& net_log() { return net_log_; }

  void RunPendingTasks() {
    base::RunLoop run_loop;
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  QuicFlagSaver flags_;  // Save/restore all QUIC flag values.
  quic::ParsedQuicVersion version_;
  const url::Origin origin_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<NetworkService> network_service_;
  mojo::Remote<mojom::NetworkContext> network_context_remote_;

  net::MockCertVerifier cert_verifier_;
  net::MockHostResolver host_resolver_;
  net::RecordingTestNetLog net_log_;

  NetworkContext network_context_;

  net::QuicTransportSimpleServer server_;
};

TEST_F(QuicTransportTest, ConnectSuccessfully) {
  base::RunLoop run_loop_for_handshake;
  mojo::PendingRemote<mojom::QuicTransportHandshakeClient> handshake_client;
  TestHandshakeClient test_handshake_client(
      handshake_client.InitWithNewPipeAndPassReceiver(),
      run_loop_for_handshake.QuitClosure());

  CreateQuicTransport(GetURL("/discard"), origin(),
                      std::move(handshake_client));

  run_loop_for_handshake.Run();

  EXPECT_TRUE(test_handshake_client.has_seen_connection_establishment());
  EXPECT_FALSE(test_handshake_client.has_seen_handshake_failure());
  EXPECT_FALSE(test_handshake_client.has_seen_mojo_connection_error());
  EXPECT_EQ(1u, network_context().NumOpenQuicTransports());
}

TEST_F(QuicTransportTest, ConnectWithWrongOrigin) {
  base::RunLoop run_loop_for_handshake;
  mojo::PendingRemote<mojom::QuicTransportHandshakeClient> handshake_client;
  TestHandshakeClient test_handshake_client(
      handshake_client.InitWithNewPipeAndPassReceiver(),
      run_loop_for_handshake.QuitClosure());

  CreateQuicTransport(GetURL("/discard"),
                      url::Origin::Create(GURL("https://evil.com")),
                      std::move(handshake_client));

  run_loop_for_handshake.Run();

  EXPECT_TRUE(test_handshake_client.has_seen_connection_establishment());
  EXPECT_FALSE(test_handshake_client.has_seen_handshake_failure());
  EXPECT_FALSE(test_handshake_client.has_seen_mojo_connection_error());

  // Server resets the connection due to origin mismatch.
  TestClient client(test_handshake_client.PassClientReceiver());
  client.WaitUntilMojoConnectionError();

  EXPECT_EQ(0u, network_context().NumOpenQuicTransports());
}

TEST_F(QuicTransportTest, SendDatagram) {
  base::RunLoop run_loop_for_handshake;
  mojo::PendingRemote<mojom::QuicTransportHandshakeClient> handshake_client;
  TestHandshakeClient test_handshake_client(
      handshake_client.InitWithNewPipeAndPassReceiver(),
      run_loop_for_handshake.QuitClosure());

  CreateQuicTransport(GetURL("/echo"),
                      url::Origin::Create(GURL("https://example.org/")),
                      std::move(handshake_client));

  run_loop_for_handshake.Run();
  mojo::Remote<mojom::QuicTransport> transport_remote(
      test_handshake_client.PassTransport());
  TestClient client(test_handshake_client.PassClientReceiver());

  std::set<std::vector<uint8_t>> sent_data;
  // Both sending and receiving datagrams are flaky due to lack of
  // retransmission, and we cannot expect a specific message to be echoed back.
  // Instead, we expect one of sent messages to be echoed back.
  while (client.received_datagrams().empty()) {
    base::RunLoop run_loop_for_datagram;
    bool result;
    std::vector<uint8_t> data = {
        static_cast<uint8_t>(base::RandInt(0, 255)),
        static_cast<uint8_t>(base::RandInt(0, 255)),
        static_cast<uint8_t>(base::RandInt(0, 255)),
        static_cast<uint8_t>(base::RandInt(0, 255)),
    };
    transport_remote->SendDatagram(base::make_span(data),
                                   base::BindLambdaForTesting([&](bool r) {
                                     result = r;
                                     run_loop_for_datagram.Quit();
                                   }));
    run_loop_for_datagram.Run();
    if (sent_data.empty()) {
      // We expect that the first data went to the network successfully.
      ASSERT_TRUE(result);
    }
    sent_data.insert(std::move(data));
  }

  EXPECT_TRUE(base::Contains(sent_data, client.received_datagrams()[0]));
}

TEST_F(QuicTransportTest, SendToolargeDatagram) {
  base::RunLoop run_loop_for_handshake;
  mojo::PendingRemote<mojom::QuicTransportHandshakeClient> handshake_client;
  TestHandshakeClient test_handshake_client(
      handshake_client.InitWithNewPipeAndPassReceiver(),
      run_loop_for_handshake.QuitClosure());

  CreateQuicTransport(GetURL("/discard"),
                      url::Origin::Create(GURL("https://example.org/")),
                      std::move(handshake_client));

  run_loop_for_handshake.Run();

  base::RunLoop run_loop_for_datagram;
  bool result;
  // The actual upper limit for one datagram is platform specific, but
  // 786kb should be large enough for any platform.
  std::vector<uint8_t> data(786 * 1024, 99);
  mojo::Remote<mojom::QuicTransport> transport_remote(
      test_handshake_client.PassTransport());

  transport_remote->SendDatagram(base::make_span(data),
                                 base::BindLambdaForTesting([&](bool r) {
                                   result = r;
                                   run_loop_for_datagram.Quit();
                                 }));
  run_loop_for_datagram.Run();
  EXPECT_FALSE(result);
}

TEST_F(QuicTransportTest, EchoOnUnidirectionalStreams) {
  base::RunLoop run_loop_for_handshake;
  mojo::PendingRemote<mojom::QuicTransportHandshakeClient> handshake_client;
  TestHandshakeClient test_handshake_client(
      handshake_client.InitWithNewPipeAndPassReceiver(),
      run_loop_for_handshake.QuitClosure());

  CreateQuicTransport(GetURL("/echo"),
                      url::Origin::Create(GURL("https://example.org/")),
                      std::move(handshake_client));

  run_loop_for_handshake.Run();

  ASSERT_TRUE(test_handshake_client.has_seen_connection_establishment());

  TestClient client(test_handshake_client.PassClientReceiver());
  mojo::Remote<mojom::QuicTransport> transport_remote(
      test_handshake_client.PassTransport());

  mojo::ScopedDataPipeConsumerHandle readable_for_outgoing;
  mojo::ScopedDataPipeProducerHandle writable_for_outgoing;
  const MojoCreateDataPipeOptions options = {
      sizeof(options), MOJO_CREATE_DATA_PIPE_FLAG_NONE, 1, 4 * 1024};
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(&options, writable_for_outgoing,
                                 readable_for_outgoing));
  uint32_t size = 5;
  ASSERT_EQ(MOJO_RESULT_OK, writable_for_outgoing->WriteData(
                                "hello", &size, MOJO_WRITE_DATA_FLAG_NONE));

  base::RunLoop run_loop_for_stream_creation;
  uint32_t stream_id;
  bool stream_created;
  transport_remote->CreateStream(
      std::move(readable_for_outgoing),
      /*writable=*/{}, base::BindLambdaForTesting([&](bool b, uint32_t id) {
        stream_created = b;
        stream_id = id;
        run_loop_for_stream_creation.Quit();
      }));
  run_loop_for_stream_creation.Run();
  ASSERT_TRUE(stream_created);

  // Signal the end-of-data.
  writable_for_outgoing.reset();
  transport_remote->SendFin(stream_id);

  mojo::ScopedDataPipeConsumerHandle readable_for_incoming;
  uint32_t incoming_stream_id = stream_id;
  base::RunLoop run_loop_for_incoming_stream;
  transport_remote->AcceptUnidirectionalStream(base::BindLambdaForTesting(
      [&](uint32_t id, mojo::ScopedDataPipeConsumerHandle readable) {
        incoming_stream_id = id;
        readable_for_incoming = std::move(readable);
        run_loop_for_incoming_stream.Quit();
      }));

  run_loop_for_incoming_stream.Run();
  ASSERT_TRUE(readable_for_incoming);
  EXPECT_NE(stream_id, incoming_stream_id);

  std::string echo_back = Read(std::move(readable_for_incoming));
  EXPECT_EQ("hello", echo_back);

  client.WaitUntilIncomingStreamIsClosed(incoming_stream_id);

  EXPECT_FALSE(client.has_received_fin_for(stream_id));
  EXPECT_TRUE(client.has_received_fin_for(incoming_stream_id));
  EXPECT_FALSE(client.has_seen_mojo_connection_error());

  std::vector<net::NetLogEntry> resets_sent = net_log().GetEntriesWithType(
      net::NetLogEventType::QUIC_SESSION_RST_STREAM_FRAME_SENT);
  EXPECT_EQ(0u, resets_sent.size());
}

// crbug.com/1129847: disabled because it is flaky.
TEST_F(QuicTransportTest, DISABLED_EchoOnBidirectionalStream) {
  base::RunLoop run_loop_for_handshake;
  mojo::PendingRemote<mojom::QuicTransportHandshakeClient> handshake_client;
  TestHandshakeClient test_handshake_client(
      handshake_client.InitWithNewPipeAndPassReceiver(),
      run_loop_for_handshake.QuitClosure());

  CreateQuicTransport(GetURL("/echo"),
                      url::Origin::Create(GURL("https://example.org/")),
                      std::move(handshake_client));

  run_loop_for_handshake.Run();

  ASSERT_TRUE(test_handshake_client.has_seen_connection_establishment());

  TestClient client(test_handshake_client.PassClientReceiver());
  mojo::Remote<mojom::QuicTransport> transport_remote(
      test_handshake_client.PassTransport());

  mojo::ScopedDataPipeConsumerHandle readable_for_outgoing;
  mojo::ScopedDataPipeProducerHandle writable_for_outgoing;
  mojo::ScopedDataPipeConsumerHandle readable_for_incoming;
  mojo::ScopedDataPipeProducerHandle writable_for_incoming;
  const MojoCreateDataPipeOptions options = {
      sizeof(options), MOJO_CREATE_DATA_PIPE_FLAG_NONE, 1, 4 * 1024};
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(&options, writable_for_outgoing,
                                 readable_for_outgoing));
  ASSERT_EQ(MOJO_RESULT_OK,
            mojo::CreateDataPipe(&options, writable_for_incoming,
                                 readable_for_incoming));
  uint32_t size = 5;
  ASSERT_EQ(MOJO_RESULT_OK, writable_for_outgoing->WriteData(
                                "hello", &size, MOJO_WRITE_DATA_FLAG_NONE));

  base::RunLoop run_loop_for_stream_creation;
  uint32_t stream_id;
  bool stream_created;
  transport_remote->CreateStream(
      std::move(readable_for_outgoing), std::move(writable_for_incoming),
      base::BindLambdaForTesting([&](bool b, uint32_t id) {
        stream_created = b;
        stream_id = id;
        run_loop_for_stream_creation.Quit();
      }));
  run_loop_for_stream_creation.Run();
  ASSERT_TRUE(stream_created);

  // Signal the end-of-data.
  writable_for_outgoing.reset();
  transport_remote->SendFin(stream_id);

  std::string echo_back = Read(std::move(readable_for_incoming));
  EXPECT_EQ("hello", echo_back);

  client.WaitUntilIncomingStreamIsClosed(stream_id);
  EXPECT_FALSE(client.has_seen_mojo_connection_error());
  EXPECT_TRUE(client.has_received_fin_for(stream_id));
  EXPECT_TRUE(client.stream_is_closed_as_incoming_stream(stream_id));
}

class QuicTransportWithCustomCertificateTest : public QuicTransportTest {
 public:
  QuicTransportWithCustomCertificateTest()
      : QuicTransportTest(CreateProofSource()) {
    auto helper = std::make_unique<TestConnectionHelper>();
    // Set clock to a time in which quic-short-lived.pem is valid
    // (2020-06-05T20:35:00.000Z).
    helper->clock().set_wall_now(
        quic::QuicWallTime::FromUNIXSeconds(1591389300));
    mutable_network_context()
        .url_request_context()
        ->quic_context()
        ->SetHelperForTesting(std::move(helper));
  }
  ~QuicTransportWithCustomCertificateTest() override = default;

  static std::unique_ptr<quic::ProofSource> CreateProofSource() {
    auto proof_source = std::make_unique<net::ProofSourceChromium>();
    base::FilePath certs_dir = net::GetTestCertsDirectory();
    EXPECT_TRUE(proof_source->Initialize(
        certs_dir.AppendASCII("quic-short-lived.pem"),
        certs_dir.AppendASCII("quic-leaf-cert.key"),
        certs_dir.AppendASCII("quic-leaf-cert.key.sct")));
    return proof_source;
  }
};

TEST_F(QuicTransportWithCustomCertificateTest, WithValidFingerprint) {
  base::RunLoop run_loop_for_handshake;
  mojo::PendingRemote<mojom::QuicTransportHandshakeClient> handshake_client;
  TestHandshakeClient test_handshake_client(
      handshake_client.InitWithNewPipeAndPassReceiver(),
      run_loop_for_handshake.QuitClosure());

  auto fingerprint = mojom::QuicTransportCertificateFingerprint::New(
      "sha-256",
      "ED:3D:D7:C3:67:10:94:68:D1:DC:D1:26:5C:B2:74:D7:1C:"
      "A2:63:3E:94:94:C0:84:39:D6:64:FA:08:B9:77:37");
  std::vector<mojom::QuicTransportCertificateFingerprintPtr> fingerprints;
  fingerprints.push_back(std::move(fingerprint));

  CreateQuicTransport(GetURL("/discard"), origin(), std::move(fingerprints),
                      std::move(handshake_client));

  run_loop_for_handshake.Run();

  EXPECT_TRUE(test_handshake_client.has_seen_connection_establishment());
  EXPECT_FALSE(test_handshake_client.has_seen_handshake_failure());
  EXPECT_FALSE(test_handshake_client.has_seen_mojo_connection_error());
  EXPECT_EQ(1u, network_context().NumOpenQuicTransports());
}

TEST_F(QuicTransportWithCustomCertificateTest, WithInvalidFingerprint) {
  base::RunLoop run_loop_for_handshake;
  mojo::PendingRemote<mojom::QuicTransportHandshakeClient> handshake_client;
  TestHandshakeClient test_handshake_client(
      handshake_client.InitWithNewPipeAndPassReceiver(),
      run_loop_for_handshake.QuitClosure());

  auto fingerprint = network::mojom::QuicTransportCertificateFingerprint::New(
      "sha-256",
      "00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:"
      "00:00:00:00:00:00:00:00:00:00:00:00:00:00:00");

  std::vector<mojom::QuicTransportCertificateFingerprintPtr> fingerprints;
  fingerprints.push_back(std::move(fingerprint));

  CreateQuicTransport(GetURL("/discard"), origin(), std::move(fingerprints),
                      std::move(handshake_client));

  run_loop_for_handshake.Run();

  EXPECT_FALSE(test_handshake_client.has_seen_connection_establishment());
  EXPECT_TRUE(test_handshake_client.has_seen_handshake_failure());
  EXPECT_FALSE(test_handshake_client.has_seen_mojo_connection_error());
  EXPECT_EQ(0u, network_context().NumOpenQuicTransports());
}

}  // namespace
}  // namespace network
