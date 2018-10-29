// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <cstdint>
#include <list>
#include <memory>
#include <ostream>
#include <utility>
#include <vector>

#include "net/third_party/quic/core/crypto/null_encrypter.h"
#include "net/third_party/quic/core/http/quic_spdy_client_stream.h"
#include "net/third_party/quic/core/quic_epoll_connection_helper.h"
#include "net/third_party/quic/core/quic_framer.h"
#include "net/third_party/quic/core/quic_packet_creator.h"
#include "net/third_party/quic/core/quic_packet_writer_wrapper.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/core/quic_session.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/platform/api/quic_expect_bug.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_sleep.h"
#include "net/third_party/quic/platform/api/quic_socket_address.h"
#include "net/third_party/quic/platform/api/quic_str_cat.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/platform/api/quic_test_loopback.h"
#include "net/third_party/quic/platform/api/quic_text_utils.h"
#include "net/third_party/quic/platform/impl/quic_socket_utils.h"
#include "net/third_party/quic/test_tools/bad_packet_writer.h"
#include "net/third_party/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quic/test_tools/packet_dropping_test_writer.h"
#include "net/third_party/quic/test_tools/packet_reordering_writer.h"
#include "net/third_party/quic/test_tools/quic_client_peer.h"
#include "net/third_party/quic/test_tools/quic_config_peer.h"
#include "net/third_party/quic/test_tools/quic_connection_peer.h"
#include "net/third_party/quic/test_tools/quic_dispatcher_peer.h"
#include "net/third_party/quic/test_tools/quic_flow_controller_peer.h"
#include "net/third_party/quic/test_tools/quic_sent_packet_manager_peer.h"
#include "net/third_party/quic/test_tools/quic_server_peer.h"
#include "net/third_party/quic/test_tools/quic_session_peer.h"
#include "net/third_party/quic/test_tools/quic_spdy_session_peer.h"
#include "net/third_party/quic/test_tools/quic_stream_peer.h"
#include "net/third_party/quic/test_tools/quic_stream_sequencer_peer.h"
#include "net/third_party/quic/test_tools/quic_test_client.h"
#include "net/third_party/quic/test_tools/quic_test_server.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quic/test_tools/server_thread.h"
#include "net/third_party/quic/tools/quic_backend_response.h"
#include "net/third_party/quic/tools/quic_client.h"
#include "net/third_party/quic/tools/quic_memory_cache_backend.h"
#include "net/third_party/quic/tools/quic_server.h"
#include "net/third_party/quic/tools/quic_simple_server_stream.h"
#include "net/tools/epoll_server/epoll_server.h"

using net::EpollEvent;
using net::EpollServer;
using spdy::kV3LowestPriority;
using spdy::SETTINGS_MAX_HEADER_LIST_SIZE;
using spdy::SpdyFramer;
using spdy::SpdyHeaderBlock;
using spdy::SpdySerializedFrame;
using spdy::SpdySettingsIR;

namespace quic {
namespace test {
namespace {

const char kFooResponseBody[] = "Artichoke hearts make me happy.";
const char kBarResponseBody[] = "Palm hearts are pretty delicious, also.";
const float kSessionToStreamRatio = 1.5;

// Run all tests with the cross products of all versions.
struct TestParams {
  TestParams(const ParsedQuicVersionVector& client_supported_versions,
             const ParsedQuicVersionVector& server_supported_versions,
             ParsedQuicVersion negotiated_version,
             bool client_supports_stateless_rejects,
             bool server_uses_stateless_rejects_if_peer_supported,
             QuicTag congestion_control_tag,
             bool use_cheap_stateless_reject)
      : client_supported_versions(client_supported_versions),
        server_supported_versions(server_supported_versions),
        negotiated_version(negotiated_version),
        client_supports_stateless_rejects(client_supports_stateless_rejects),
        server_uses_stateless_rejects_if_peer_supported(
            server_uses_stateless_rejects_if_peer_supported),
        congestion_control_tag(congestion_control_tag),
        use_cheap_stateless_reject(use_cheap_stateless_reject) {}

  friend std::ostream& operator<<(std::ostream& os, const TestParams& p) {
    os << "{ server_supported_versions: "
       << ParsedQuicVersionVectorToString(p.server_supported_versions);
    os << " client_supported_versions: "
       << ParsedQuicVersionVectorToString(p.client_supported_versions);
    os << " negotiated_version: "
       << ParsedQuicVersionToString(p.negotiated_version);
    os << " client_supports_stateless_rejects: "
       << p.client_supports_stateless_rejects;
    os << " server_uses_stateless_rejects_if_peer_supported: "
       << p.server_uses_stateless_rejects_if_peer_supported;
    os << " congestion_control_tag: "
       << QuicTagToString(p.congestion_control_tag);
    os << " use_cheap_stateless_reject: " << p.use_cheap_stateless_reject
       << " }";
    return os;
  }

  ParsedQuicVersionVector client_supported_versions;
  ParsedQuicVersionVector server_supported_versions;
  ParsedQuicVersion negotiated_version;
  bool client_supports_stateless_rejects;
  bool server_uses_stateless_rejects_if_peer_supported;
  QuicTag congestion_control_tag;
  bool use_cheap_stateless_reject;
};

// Constructs various test permutations.
std::vector<TestParams> GetTestParams(bool use_tls_handshake,
                                      bool test_stateless_rejects) {
  // Version 99 is default disabled, but should be exercised in EndToEnd tests
  QuicFlagSaver flags;
  FLAGS_quic_enable_version_99 = true;
  // Divide the versions into buckets in which the intra-frame format
  // is compatible. When clients encounter QUIC version negotiation
  // they simply retransmit all packets using the new version's
  // QUIC framing. However, they are unable to change the intra-frame
  // layout (for example to change HTTP/2 headers to SPDY/3, or a change in the
  // handshake protocol). So these tests need to ensure that clients are never
  // attempting to do 0-RTT across incompatible versions. Chromium only
  // supports a single version at a time anyway. :)
  FLAGS_quic_supports_tls_handshake = use_tls_handshake;
  ParsedQuicVersionVector all_supported_versions = AllSupportedVersions();
  // Buckets are separated by the handshake protocol (QUIC crypto or TLS) in
  // use, since if the handshake protocol changes, the ClientHello/CHLO must be
  // reconstructed for the correct protocol.
  ParsedQuicVersionVector version_buckets[2];

  for (const ParsedQuicVersion& version : all_supported_versions) {
    // Versions: 35+
    // QUIC_VERSION_35 allows endpoints to independently set stream limit.
    if (version.handshake_protocol == PROTOCOL_TLS1_3) {
      version_buckets[1].push_back(version);
    } else {
      version_buckets[0].push_back(version);
    }
  }

  // This must be kept in sync with the number of nested for-loops below as it
  // is used to prune the number of tests that are run.
  const int kMaxEnabledOptions = 4;
  int max_enabled_options = 0;
  std::vector<TestParams> params;
  for (const QuicTag congestion_control_tag : {kRENO, kTBBR, kQBIC, kTPCC}) {
    for (bool server_uses_stateless_rejects_if_peer_supported : {true, false}) {
      for (bool client_supports_stateless_rejects : {true, false}) {
        for (bool use_cheap_stateless_reject : {true, false}) {
          int enabled_options = 0;
          if (congestion_control_tag != kQBIC) {
            ++enabled_options;
          }
          if (client_supports_stateless_rejects) {
            ++enabled_options;
          }
          if (server_uses_stateless_rejects_if_peer_supported) {
            ++enabled_options;
          }
          if (use_cheap_stateless_reject) {
            ++enabled_options;
          }
          CHECK_GE(kMaxEnabledOptions, enabled_options);
          if (enabled_options > max_enabled_options) {
            max_enabled_options = enabled_options;
          }

          // Run tests with no options, a single option, or all the
          // options enabled to avoid a combinatorial explosion.
          if (enabled_options > 1 && enabled_options < kMaxEnabledOptions) {
            continue;
          }

          // There are many stateless reject combinations, so don't test them
          // unless requested.
          if ((server_uses_stateless_rejects_if_peer_supported ||
               client_supports_stateless_rejects ||
               use_cheap_stateless_reject) &&
              !test_stateless_rejects) {
            continue;
          }

          for (const ParsedQuicVersionVector& client_versions :
               version_buckets) {
            if (FilterSupportedVersions(client_versions).empty()) {
              continue;
            }
            // Add an entry for server and client supporting all
            // versions.
            params.push_back(TestParams(
                client_versions, all_supported_versions,
                client_versions.front(), client_supports_stateless_rejects,
                server_uses_stateless_rejects_if_peer_supported,
                congestion_control_tag, use_cheap_stateless_reject));

            // Run version negotiation tests tests with no options, or
            // all the options enabled to avoid a combinatorial
            // explosion.
            if (enabled_options > 1 && enabled_options < kMaxEnabledOptions) {
              continue;
            }

            // Test client supporting all versions and server supporting
            // 1 version. Simulate an old server and exercise version
            // downgrade in the client. Protocol negotiation should
            // occur.  Skip the i = 0 case because it is essentially the
            // same as the default case.
            for (size_t i = 1; i < client_versions.size(); ++i) {
              ParsedQuicVersionVector server_supported_versions;
              server_supported_versions.push_back(client_versions[i]);
              if (FilterSupportedVersions(server_supported_versions).empty()) {
                continue;
              }
              params.push_back(TestParams(
                  client_versions, server_supported_versions,
                  server_supported_versions.front(),
                  client_supports_stateless_rejects,
                  server_uses_stateless_rejects_if_peer_supported,
                  congestion_control_tag, use_cheap_stateless_reject));
            }  // End of inner version loop.
          }    // End of outer version loop.
        }      // End of use_cheap_stateless_reject loop.
      }        // End of client_supports_stateless_rejects loop.
    }          // End of server_uses_stateless_rejects_if_peer_supported loop.
  }            // End of congestion_control_tag loop.
  CHECK_EQ(kMaxEnabledOptions, max_enabled_options);
  return params;
}

class ServerDelegate : public PacketDroppingTestWriter::Delegate {
 public:
  explicit ServerDelegate(QuicDispatcher* dispatcher)
      : dispatcher_(dispatcher) {}
  ~ServerDelegate() override = default;
  void OnCanWrite() override { dispatcher_->OnCanWrite(); }

 private:
  QuicDispatcher* dispatcher_;
};

class ClientDelegate : public PacketDroppingTestWriter::Delegate {
 public:
  explicit ClientDelegate(QuicClient* client) : client_(client) {}
  ~ClientDelegate() override = default;
  void OnCanWrite() override {
    EpollEvent event(EPOLLOUT);
    client_->epoll_network_helper()->OnEvent(client_->GetLatestFD(), &event);
  }

 private:
  QuicClient* client_;
};

class EndToEndTest : public QuicTestWithParam<TestParams> {
 protected:
  EndToEndTest()
      : initialized_(false),
        server_address_(QuicSocketAddress(TestLoopback(), 0)),
        server_hostname_("test.example.com"),
        client_writer_(nullptr),
        server_writer_(nullptr),
        negotiated_version_(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED),
        chlo_multiplier_(0),
        stream_factory_(nullptr),
        support_server_push_(false) {
    // Version 99 is default disabled, but should be exercised in EndToEnd tests
    FLAGS_quic_enable_version_99 = true;
    FLAGS_quic_supports_tls_handshake = true;
    client_supported_versions_ = GetParam().client_supported_versions;
    server_supported_versions_ = GetParam().server_supported_versions;
    negotiated_version_ = GetParam().negotiated_version;

    QUIC_LOG(INFO) << "Using Configuration: " << GetParam();

    // Use different flow control windows for client/server.
    client_config_.SetInitialStreamFlowControlWindowToSend(
        2 * kInitialStreamFlowControlWindowForTest);
    client_config_.SetInitialSessionFlowControlWindowToSend(
        2 * kInitialSessionFlowControlWindowForTest);
    server_config_.SetInitialStreamFlowControlWindowToSend(
        3 * kInitialStreamFlowControlWindowForTest);
    server_config_.SetInitialSessionFlowControlWindowToSend(
        3 * kInitialSessionFlowControlWindowForTest);

    // The default idle timeouts can be too strict when running on a busy
    // machine.
    const QuicTime::Delta timeout = QuicTime::Delta::FromSeconds(30);
    client_config_.set_max_time_before_crypto_handshake(timeout);
    client_config_.set_max_idle_time_before_crypto_handshake(timeout);
    server_config_.set_max_time_before_crypto_handshake(timeout);
    server_config_.set_max_idle_time_before_crypto_handshake(timeout);

    AddToCache("/foo", 200, kFooResponseBody);
    AddToCache("/bar", 200, kBarResponseBody);
  }

  ~EndToEndTest() override {
    // TODO(rtenneti): port RecycleUnusedPort if needed.
    // RecycleUnusedPort(server_address_.port());
  }

  virtual void CreateClientWithWriter() {
    client_.reset(CreateQuicClient(client_writer_));
  }

  QuicTestClient* CreateQuicClient(QuicPacketWriterWrapper* writer) {
    QuicTestClient* client =
        new QuicTestClient(server_address_, server_hostname_, client_config_,
                           client_supported_versions_,
                           crypto_test_utils::ProofVerifierForTesting());
    client->UseWriter(writer);
    if (!pre_shared_key_client_.empty()) {
      client->client()->SetPreSharedKey(pre_shared_key_client_);
    }
    client->Connect();
    return client;
  }

  void set_smaller_flow_control_receive_window() {
    const uint32_t kClientIFCW = 64 * 1024;
    const uint32_t kServerIFCW = 1024 * 1024;
    set_client_initial_stream_flow_control_receive_window(kClientIFCW);
    set_client_initial_session_flow_control_receive_window(
        kSessionToStreamRatio * kClientIFCW);
    set_server_initial_stream_flow_control_receive_window(kServerIFCW);
    set_server_initial_session_flow_control_receive_window(
        kSessionToStreamRatio * kServerIFCW);
  }

  void set_client_initial_stream_flow_control_receive_window(uint32_t window) {
    CHECK(client_ == nullptr);
    QUIC_DLOG(INFO) << "Setting client initial stream flow control window: "
                    << window;
    client_config_.SetInitialStreamFlowControlWindowToSend(window);
  }

  void set_client_initial_session_flow_control_receive_window(uint32_t window) {
    CHECK(client_ == nullptr);
    QUIC_DLOG(INFO) << "Setting client initial session flow control window: "
                    << window;
    client_config_.SetInitialSessionFlowControlWindowToSend(window);
  }

  void set_server_initial_stream_flow_control_receive_window(uint32_t window) {
    CHECK(server_thread_ == nullptr);
    QUIC_DLOG(INFO) << "Setting server initial stream flow control window: "
                    << window;
    server_config_.SetInitialStreamFlowControlWindowToSend(window);
  }

  void set_server_initial_session_flow_control_receive_window(uint32_t window) {
    CHECK(server_thread_ == nullptr);
    QUIC_DLOG(INFO) << "Setting server initial session flow control window: "
                    << window;
    server_config_.SetInitialSessionFlowControlWindowToSend(window);
  }

  const QuicSentPacketManager* GetSentPacketManagerFromFirstServerSession() {
    return &GetServerConnection()->sent_packet_manager();
  }

  QuicConnection* GetServerConnection() {
    return GetServerSession()->connection();
  }

  QuicSession* GetServerSession() {
    QuicDispatcher* dispatcher =
        QuicServerPeer::GetDispatcher(server_thread_->server());
    EXPECT_EQ(1u, dispatcher->session_map().size());
    return dispatcher->session_map().begin()->second.get();
  }

  bool Initialize() {
    QuicTagVector copt;
    server_config_.SetConnectionOptionsToSend(copt);
    copt = client_extra_copts_;

    // TODO(nimia): Consider setting the congestion control algorithm for the
    // client as well according to the test parameter.
    copt.push_back(GetParam().congestion_control_tag);
    if (GetParam().congestion_control_tag == kTPCC &&
        GetQuicReloadableFlag(quic_enable_pcc3)) {
      copt.push_back(kTPCC);
    }

    if (support_server_push_) {
      copt.push_back(kSPSH);
    }
    if (GetParam().client_supports_stateless_rejects) {
      copt.push_back(kSREJ);
    }
    client_config_.SetConnectionOptionsToSend(copt);

    // Start the server first, because CreateQuicClient() attempts
    // to connect to the server.
    StartServer();

    CreateClientWithWriter();
    static EpollEvent event(EPOLLOUT);
    if (client_writer_ != nullptr) {
      client_writer_->Initialize(
          QuicConnectionPeer::GetHelper(
              client_->client()->client_session()->connection()),
          QuicConnectionPeer::GetAlarmFactory(
              client_->client()->client_session()->connection()),
          std::make_unique<ClientDelegate>(client_->client()));
    }
    initialized_ = true;
    return client_->client()->connected();
  }

  void SetUp() override {
    // The ownership of these gets transferred to the QuicPacketWriterWrapper
    // when Initialize() is executed.
    client_writer_ = new PacketDroppingTestWriter();
    server_writer_ = new PacketDroppingTestWriter();
  }

  void TearDown() override {
    ASSERT_TRUE(initialized_) << "You must call Initialize() in every test "
                              << "case. Otherwise, your test will leak memory.";
    StopServer();
  }

  void StartServer() {
    SetQuicReloadableFlag(quic_use_cheap_stateless_rejects,
                          GetParam().use_cheap_stateless_reject);

    auto* test_server = new QuicTestServer(
        crypto_test_utils::ProofSourceForTesting(), server_config_,
        server_supported_versions_, &memory_cache_backend_);
    server_thread_ = QuicMakeUnique<ServerThread>(test_server, server_address_);
    if (chlo_multiplier_ != 0) {
      server_thread_->server()->SetChloMultiplier(chlo_multiplier_);
    }
    if (!pre_shared_key_server_.empty()) {
      server_thread_->server()->SetPreSharedKey(pre_shared_key_server_);
    }
    server_thread_->Initialize();
    server_address_ =
        QuicSocketAddress(server_address_.host(), server_thread_->GetPort());
    QuicDispatcher* dispatcher =
        QuicServerPeer::GetDispatcher(server_thread_->server());
    QuicDispatcherPeer::UseWriter(dispatcher, server_writer_);

    SetQuicReloadableFlag(
        enable_quic_stateless_reject_support,
        GetParam().server_uses_stateless_rejects_if_peer_supported);

    server_writer_->Initialize(QuicDispatcherPeer::GetHelper(dispatcher),
                               QuicDispatcherPeer::GetAlarmFactory(dispatcher),
                               std::make_unique<ServerDelegate>(dispatcher));
    if (stream_factory_ != nullptr) {
      static_cast<QuicTestServer*>(server_thread_->server())
          ->SetSpdyStreamFactory(stream_factory_);
    }

    server_thread_->Start();
  }

  void StopServer() {
    if (server_thread_) {
      server_thread_->Quit();
      server_thread_->Join();
    }
  }

  void AddToCache(QuicStringPiece path,
                  int response_code,
                  QuicStringPiece body) {
    memory_cache_backend_.AddSimpleResponse(server_hostname_, path,
                                            response_code, body);
  }

  void SetPacketLossPercentage(int32_t loss) {
    // TODO(rtenneti): enable when we can do random packet loss tests in
    // chrome's tree.
    if (loss != 0 && loss != 100)
      return;
    client_writer_->set_fake_packet_loss_percentage(loss);
    server_writer_->set_fake_packet_loss_percentage(loss);
  }

  void SetPacketSendDelay(QuicTime::Delta delay) {
    // TODO(rtenneti): enable when we can do random packet send delay tests in
    // chrome's tree.
    // client_writer_->set_fake_packet_delay(delay);
    // server_writer_->set_fake_packet_delay(delay);
  }

  void SetReorderPercentage(int32_t reorder) {
    // TODO(rtenneti): enable when we can do random packet reorder tests in
    // chrome's tree.
    // client_writer_->set_fake_reorder_percentage(reorder);
    // server_writer_->set_fake_reorder_percentage(reorder);
  }

  // Verifies that the client and server connections were both free of packets
  // being discarded, based on connection stats.
  // Calls server_thread_ Pause() and Resume(), which may only be called once
  // per test.
  void VerifyCleanConnection(bool had_packet_loss) {
    QuicConnectionStats client_stats =
        client_->client()->client_session()->connection()->GetStats();
    // TODO(ianswett): Determine why this becomes even more flaky with BBR
    // enabled.  b/62141144
    if (!had_packet_loss && !GetQuicReloadableFlag(quic_default_to_bbr)) {
      EXPECT_EQ(0u, client_stats.packets_lost);
    }
    EXPECT_EQ(0u, client_stats.packets_discarded);
    // When doing 0-RTT with stateless rejects, the encrypted requests cause
    // a retranmission of the SREJ packets which are dropped by the client.
    if (!BothSidesSupportStatelessRejects()) {
      EXPECT_EQ(0u, client_stats.packets_dropped);
    }
    if (!ClientSupportsIetfQuicNotSupportedByServer()) {
      // In this case, if client sends 0-RTT POST with v99, receives IETF
      // version negotiation packet and speaks a GQUIC version. Server processes
      // this connection in time wait list and keeps sending IETF version
      // negotiation packet for incoming packets. But these version negotiation
      // packets cannot be processed by the client speaking GQUIC.
      EXPECT_EQ(client_stats.packets_received, client_stats.packets_processed);
    }

    const int num_expected_stateless_rejects =
        (BothSidesSupportStatelessRejects() &&
         client_->client()->client_session()->GetNumSentClientHellos() > 0)
            ? 1
            : 0;
    EXPECT_EQ(num_expected_stateless_rejects,
              client_->client()->num_stateless_rejects_received());

    server_thread_->Pause();
    QuicConnectionStats server_stats = GetServerConnection()->GetStats();
    if (!had_packet_loss) {
      EXPECT_EQ(0u, server_stats.packets_lost);
    }
    EXPECT_EQ(0u, server_stats.packets_discarded);
    // TODO(ianswett): Restore the check for packets_dropped equals 0.
    // The expect for packets received is equal to packets processed fails
    // due to version negotiation packets.
    server_thread_->Resume();
  }

  bool BothSidesSupportStatelessRejects() {
    return (GetParam().server_uses_stateless_rejects_if_peer_supported &&
            GetParam().client_supports_stateless_rejects);
  }

  // Client supports IETF QUIC, while it is not supported by server.
  bool ClientSupportsIetfQuicNotSupportedByServer() {
    return GetParam().client_supported_versions[0].transport_version >
               QUIC_VERSION_43 &&
           FilterSupportedVersions(GetParam().server_supported_versions)[0]
                   .transport_version <= QUIC_VERSION_43;
  }

  bool SupportsIetfQuicWithTls(ParsedQuicVersion version) {
    return version.transport_version > QUIC_VERSION_43 &&
           version.handshake_protocol == PROTOCOL_TLS1_3;
  }

  void ExpectFlowControlsSynced(QuicFlowController* client,
                                QuicFlowController* server) {
    EXPECT_EQ(QuicFlowControllerPeer::SendWindowSize(client),
              QuicFlowControllerPeer::ReceiveWindowSize(server));
    EXPECT_EQ(QuicFlowControllerPeer::ReceiveWindowSize(client),
              QuicFlowControllerPeer::SendWindowSize(server));
  }

  // Must be called before Initialize to have effect.
  void SetSpdyStreamFactory(QuicTestServer::StreamFactory* factory) {
    stream_factory_ = factory;
  }

  QuicStreamId GetNthClientInitiatedId(int n) {
    return QuicSpdySessionPeer::GetNthClientInitiatedStreamId(
        *client_->client()->client_session(), n);
  }

  QuicStreamId GetNthServerInitiatedId(int n) {
    return QuicSpdySessionPeer::GetNthServerInitiatedStreamId(
        *client_->client()->client_session(), n);
  }

  ScopedEnvironmentForThreads environment_;
  bool initialized_;
  QuicSocketAddress server_address_;
  QuicString server_hostname_;
  QuicMemoryCacheBackend memory_cache_backend_;
  std::unique_ptr<ServerThread> server_thread_;
  std::unique_ptr<QuicTestClient> client_;
  PacketDroppingTestWriter* client_writer_;
  PacketDroppingTestWriter* server_writer_;
  QuicConfig client_config_;
  QuicConfig server_config_;
  ParsedQuicVersionVector client_supported_versions_;
  ParsedQuicVersionVector server_supported_versions_;
  QuicTagVector client_extra_copts_;
  ParsedQuicVersion negotiated_version_;
  size_t chlo_multiplier_;
  QuicTestServer::StreamFactory* stream_factory_;
  bool support_server_push_;
  QuicString pre_shared_key_client_;
  QuicString pre_shared_key_server_;
};

// Run all end to end tests with all supported versions.
INSTANTIATE_TEST_CASE_P(EndToEndTests,
                        EndToEndTest,
                        ::testing::ValuesIn(GetTestParams(false, false)));

class EndToEndTestWithTls : public EndToEndTest {};

INSTANTIATE_TEST_CASE_P(EndToEndTestsWithTls,
                        EndToEndTestWithTls,
                        ::testing::ValuesIn(GetTestParams(true, false)));

class EndToEndTestWithStatelessReject : public EndToEndTest {};

INSTANTIATE_TEST_CASE_P(WithStatelessReject,
                        EndToEndTestWithStatelessReject,
                        ::testing::ValuesIn(GetTestParams(false, true)));

TEST_P(EndToEndTestWithTls, HandshakeSuccessful) {
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());
  server_thread_->WaitForCryptoHandshakeConfirmed();
  QuicCryptoStream* crypto_stream = QuicSessionPeer::GetMutableCryptoStream(
      client_->client()->client_session());
  QuicStreamSequencer* sequencer = QuicStreamPeer::sequencer(crypto_stream);
  EXPECT_FALSE(QuicStreamSequencerPeer::IsUnderlyingBufferAllocated(sequencer));
  server_thread_->Pause();
  crypto_stream = QuicSessionPeer::GetMutableCryptoStream(GetServerSession());
  sequencer = QuicStreamPeer::sequencer(crypto_stream);
  EXPECT_FALSE(QuicStreamSequencerPeer::IsUnderlyingBufferAllocated(sequencer));
}

TEST_P(EndToEndTestWithStatelessReject, SimpleRequestResponseStatless) {
  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);
  EXPECT_EQ(2, client_->client()->GetNumSentClientHellos());
}

TEST_P(EndToEndTest, SimpleRequestResponse) {
  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);
  EXPECT_EQ(2, client_->client()->GetNumSentClientHellos());
}

TEST_P(EndToEndTest, SimpleRequestResponseWithLargeReject) {
  chlo_multiplier_ = 1;
  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);
  EXPECT_EQ(3, client_->client()->GetNumSentClientHellos());
}

TEST_P(EndToEndTestWithTls, SimpleRequestResponsev6) {
  server_address_ =
      QuicSocketAddress(QuicIpAddress::Loopback6(), server_address_.port());
  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);
}

TEST_P(EndToEndTestWithTls, SeparateFinPacket) {
  ASSERT_TRUE(Initialize());

  // Send a request in two parts: the request and then an empty packet with FIN.
  SpdyHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;
  client_->SendMessage(headers, "", /*fin=*/false);
  client_->SendData("", true);
  client_->WaitForResponse();
  EXPECT_EQ(kFooResponseBody, client_->response_body());
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);

  // Now do the same thing but with a content length.
  headers["content-length"] = "3";
  client_->SendMessage(headers, "", /*fin=*/false);
  client_->SendData("foo", true);
  client_->WaitForResponse();
  EXPECT_EQ(kFooResponseBody, client_->response_body());
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);
}

TEST_P(EndToEndTestWithTls, MultipleRequestResponse) {
  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);
  EXPECT_EQ(kBarResponseBody, client_->SendSynchronousRequest("/bar"));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);
}

TEST_P(EndToEndTestWithTls, MultipleStreams) {
  // Verifies quic_test_client can track responses of all active streams.
  ASSERT_TRUE(Initialize());

  const int kNumRequests = 10;

  SpdyHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;
  headers["content-length"] = "3";

  for (int i = 0; i < kNumRequests; ++i) {
    client_->SendMessage(headers, "bar", /*fin=*/true);
  }

  while (kNumRequests > client_->num_responses()) {
    client_->ClearPerRequestState();
    client_->WaitForResponse();
    EXPECT_EQ(kFooResponseBody, client_->response_body());
    EXPECT_EQ("200", client_->response_headers()->find(":status")->second);
  }
}

TEST_P(EndToEndTestWithTls, MultipleClients) {
  ASSERT_TRUE(Initialize());
  std::unique_ptr<QuicTestClient> client2(CreateQuicClient(nullptr));

  SpdyHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;
  headers["content-length"] = "3";

  client_->SendMessage(headers, "", /*fin=*/false);
  client2->SendMessage(headers, "", /*fin=*/false);

  client_->SendData("bar", true);
  client_->WaitForResponse();
  EXPECT_EQ(kFooResponseBody, client_->response_body());
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);

  client2->SendData("eep", true);
  client2->WaitForResponse();
  EXPECT_EQ(kFooResponseBody, client2->response_body());
  EXPECT_EQ("200", client2->response_headers()->find(":status")->second);
}

TEST_P(EndToEndTestWithTls, RequestOverMultiplePackets) {
  // Send a large enough request to guarantee fragmentation.
  QuicString huge_request =
      "/some/path?query=" + QuicString(kMaxPacketSize, '.');
  AddToCache(huge_request, 200, kBarResponseBody);

  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kBarResponseBody, client_->SendSynchronousRequest(huge_request));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);
}

TEST_P(EndToEndTestWithTls, MultiplePacketsRandomOrder) {
  // Send a large enough request to guarantee fragmentation.
  QuicString huge_request =
      "/some/path?query=" + QuicString(kMaxPacketSize, '.');
  AddToCache(huge_request, 200, kBarResponseBody);

  ASSERT_TRUE(Initialize());
  SetPacketSendDelay(QuicTime::Delta::FromMilliseconds(2));
  SetReorderPercentage(50);

  EXPECT_EQ(kBarResponseBody, client_->SendSynchronousRequest(huge_request));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);
}

TEST_P(EndToEndTestWithTls, PostMissingBytes) {
  ASSERT_TRUE(Initialize());

  // Add a content length header with no body.
  SpdyHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;
  headers["content-length"] = "3";

  // This should be detected as stream fin without complete request,
  // triggering an error response.
  client_->SendCustomSynchronousRequest(headers, "");
  EXPECT_EQ(QuicSimpleServerStream::kErrorResponseBody,
            client_->response_body());
  EXPECT_EQ("500", client_->response_headers()->find(":status")->second);
}

TEST_P(EndToEndTest, LargePostNoPacketLoss) {
  ASSERT_TRUE(Initialize());

  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());

  // 1 MB body.
  QuicString body(1024 * 1024, 'a');
  SpdyHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;

  EXPECT_EQ(kFooResponseBody,
            client_->SendCustomSynchronousRequest(headers, body));
  // TODO(ianswett): There should not be packet loss in this test, but on some
  // platforms the receive buffer overflows.
  VerifyCleanConnection(true);
}

TEST_P(EndToEndTest, LargePostNoPacketLoss1sRTT) {
  ASSERT_TRUE(Initialize());
  SetPacketSendDelay(QuicTime::Delta::FromMilliseconds(1000));

  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());

  // 100 KB body.
  QuicString body(100 * 1024, 'a');
  SpdyHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;

  EXPECT_EQ(kFooResponseBody,
            client_->SendCustomSynchronousRequest(headers, body));
  VerifyCleanConnection(false);
}

TEST_P(EndToEndTest, LargePostWithPacketLoss) {
  if (!BothSidesSupportStatelessRejects()) {
    // Connect with lower fake packet loss than we'd like to test.
    // Until b/10126687 is fixed, losing handshake packets is pretty
    // brutal.
    // TODO(jokulik): Until we support redundant SREJ packets, don't
    // drop handshake packets for stateless rejects.
    SetPacketLossPercentage(5);
  }
  ASSERT_TRUE(Initialize());

  // Wait for the server SHLO before upping the packet loss.
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());
  SetPacketLossPercentage(30);

  // 10 KB body.
  QuicString body(1024 * 10, 'a');
  SpdyHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;

  EXPECT_EQ(kFooResponseBody,
            client_->SendCustomSynchronousRequest(headers, body));
  VerifyCleanConnection(true);
}

// Regression test for b/80090281.
TEST_P(EndToEndTest, LargePostWithPacketLossAndAlwaysBundleWindowUpdates) {
  ASSERT_TRUE(Initialize());

  // Wait for the server SHLO before upping the packet loss.
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());
  server_thread_->WaitForCryptoHandshakeConfirmed();

  // Normally server only bundles a retransmittable frame once every other
  // kMaxConsecutiveNonRetransmittablePackets ack-only packets. Setting the max
  // to 0 to reliably reproduce b/80090281.
  server_thread_->Schedule([this]() {
    QuicConnectionPeer::SetMaxConsecutiveNumPacketsWithNoRetransmittableFrames(
        GetServerConnection(), 0);
  });

  SetPacketLossPercentage(30);

  // 10 KB body.
  QuicString body(1024 * 10, 'a');
  SpdyHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;

  EXPECT_EQ(kFooResponseBody,
            client_->SendCustomSynchronousRequest(headers, body));
  VerifyCleanConnection(true);
}

TEST_P(EndToEndTest, LargePostWithPacketLossAndBlockedSocket) {
  if (!BothSidesSupportStatelessRejects()) {
    // Connect with lower fake packet loss than we'd like to test.  Until
    // b/10126687 is fixed, losing handshake packets is pretty brutal.
    // TODO(jokulik): Until we support redundant SREJ packets, don't
    // drop handshake packets for stateless rejects.
    SetPacketLossPercentage(5);
  }
  ASSERT_TRUE(Initialize());

  // Wait for the server SHLO before upping the packet loss.
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());
  SetPacketLossPercentage(10);
  client_writer_->set_fake_blocked_socket_percentage(10);

  // 10 KB body.
  QuicString body(1024 * 10, 'a');
  SpdyHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;

  EXPECT_EQ(kFooResponseBody,
            client_->SendCustomSynchronousRequest(headers, body));
}

TEST_P(EndToEndTest, LargePostNoPacketLossWithDelayAndReordering) {
  ASSERT_TRUE(Initialize());

  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());
  // Both of these must be called when the writer is not actively used.
  SetPacketSendDelay(QuicTime::Delta::FromMilliseconds(2));
  SetReorderPercentage(30);

  // 1 MB body.
  QuicString body(1024 * 1024, 'a');
  SpdyHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;

  EXPECT_EQ(kFooResponseBody,
            client_->SendCustomSynchronousRequest(headers, body));
}

TEST_P(EndToEndTest, LargePostZeroRTTFailure) {
  // Send a request and then disconnect. This prepares the client to attempt
  // a 0-RTT handshake for the next request.
  ASSERT_TRUE(Initialize());

  QuicString body(20480, 'a');
  SpdyHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;

  EXPECT_EQ(kFooResponseBody,
            client_->SendCustomSynchronousRequest(headers, body));
  // In the non-stateless case, the same session is used for both
  // hellos, so the number of hellos sent on that session is 2.  In
  // the stateless case, the first client session will be completely
  // torn down after the reject.  The number of hellos on the latest
  // session is 1.
  const int expected_num_hellos_latest_session =
      BothSidesSupportStatelessRejects() ? 1 : 2;
  EXPECT_EQ(expected_num_hellos_latest_session,
            client_->client()->client_session()->GetNumSentClientHellos());
  EXPECT_EQ(2, client_->client()->GetNumSentClientHellos());

  client_->Disconnect();

  // The 0-RTT handshake should succeed.
  client_->Connect();
  client_->WaitForInitialResponse();
  ASSERT_TRUE(client_->client()->connected());
  EXPECT_EQ(kFooResponseBody,
            client_->SendCustomSynchronousRequest(headers, body));

  EXPECT_EQ(1, client_->client()->client_session()->GetNumSentClientHellos());
  EXPECT_EQ(1, client_->client()->GetNumSentClientHellos());

  client_->Disconnect();

  // Restart the server so that the 0-RTT handshake will take 1 RTT.
  StopServer();
  server_writer_ = new PacketDroppingTestWriter();
  StartServer();

  client_->Connect();
  ASSERT_TRUE(client_->client()->connected());
  EXPECT_EQ(kFooResponseBody,
            client_->SendCustomSynchronousRequest(headers, body));
  // In the non-stateless case, the same session is used for both
  // hellos, so the number of hellos sent on that session is 2.  In
  // the stateless case, the first client session will be completely
  // torn down after the reject.  The number of hellos sent on the
  // latest session is 1.
  EXPECT_EQ(expected_num_hellos_latest_session,
            client_->client()->client_session()->GetNumSentClientHellos());
  EXPECT_EQ(2, client_->client()->GetNumSentClientHellos());

  VerifyCleanConnection(false);
}

TEST_P(EndToEndTest, SynchronousRequestZeroRTTFailure) {
  // Send a request and then disconnect. This prepares the client to attempt
  // a 0-RTT handshake for the next request.
  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  // In the non-stateless case, the same session is used for both
  // hellos, so the number of hellos sent on that session is 2.  In
  // the stateless case, the first client session will be completely
  // torn down after the reject.  The number of hellos on that second
  // latest session is 1.
  const int expected_num_hellos_latest_session =
      BothSidesSupportStatelessRejects() ? 1 : 2;
  EXPECT_EQ(expected_num_hellos_latest_session,
            client_->client()->client_session()->GetNumSentClientHellos());
  EXPECT_EQ(2, client_->client()->GetNumSentClientHellos());

  client_->Disconnect();

  // The 0-RTT handshake should succeed.
  client_->Connect();
  client_->WaitForInitialResponse();
  ASSERT_TRUE(client_->client()->connected());
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));

  EXPECT_EQ(1, client_->client()->client_session()->GetNumSentClientHellos());
  EXPECT_EQ(1, client_->client()->GetNumSentClientHellos());

  client_->Disconnect();

  // Restart the server so that the 0-RTT handshake will take 1 RTT.
  StopServer();
  server_writer_ = new PacketDroppingTestWriter();
  StartServer();

  client_->Connect();
  ASSERT_TRUE(client_->client()->connected());
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  // In the non-stateless case, the same session is used for both
  // hellos, so the number of hellos sent on that session is 2.  In
  // the stateless case, the first client session will be completely
  // torn down after the reject.  The number of hellos sent on the
  // latest session is 1.
  EXPECT_EQ(expected_num_hellos_latest_session,
            client_->client()->client_session()->GetNumSentClientHellos());
  EXPECT_EQ(2, client_->client()->GetNumSentClientHellos());

  VerifyCleanConnection(false);
}

TEST_P(EndToEndTest, LargePostSynchronousRequest) {
  // Send a request and then disconnect. This prepares the client to attempt
  // a 0-RTT handshake for the next request.
  ASSERT_TRUE(Initialize());

  QuicString body(20480, 'a');
  SpdyHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;

  EXPECT_EQ(kFooResponseBody,
            client_->SendCustomSynchronousRequest(headers, body));
  // In the non-stateless case, the same session is used for both
  // hellos, so the number of hellos sent on that session is 2.  In
  // the stateless case, the first client session will be completely
  // torn down after the reject.  The number of hellos on the latest
  // session is 1.
  const int expected_num_hellos_latest_session =
      BothSidesSupportStatelessRejects() ? 1 : 2;
  EXPECT_EQ(expected_num_hellos_latest_session,
            client_->client()->client_session()->GetNumSentClientHellos());
  EXPECT_EQ(2, client_->client()->GetNumSentClientHellos());

  client_->Disconnect();

  // The 0-RTT handshake should succeed.
  client_->Connect();
  client_->WaitForInitialResponse();
  ASSERT_TRUE(client_->client()->connected());
  EXPECT_EQ(kFooResponseBody,
            client_->SendCustomSynchronousRequest(headers, body));

  EXPECT_EQ(1, client_->client()->client_session()->GetNumSentClientHellos());
  EXPECT_EQ(1, client_->client()->GetNumSentClientHellos());

  client_->Disconnect();

  // Restart the server so that the 0-RTT handshake will take 1 RTT.
  StopServer();
  server_writer_ = new PacketDroppingTestWriter();
  StartServer();

  client_->Connect();
  ASSERT_TRUE(client_->client()->connected());
  EXPECT_EQ(kFooResponseBody,
            client_->SendCustomSynchronousRequest(headers, body));
  // In the non-stateless case, the same session is used for both
  // hellos, so the number of hellos sent on that session is 2.  In
  // the stateless case, the first client session will be completely
  // torn down after the reject.  The number of hellos sent on the
  // latest session is 1.
  EXPECT_EQ(expected_num_hellos_latest_session,
            client_->client()->client_session()->GetNumSentClientHellos());
  EXPECT_EQ(2, client_->client()->GetNumSentClientHellos());

  VerifyCleanConnection(false);
}

TEST_P(EndToEndTest, StatelessRejectWithPacketLoss) {
  // In this test, we intentionally drop the first packet from the
  // server, which corresponds with the initial REJ/SREJ response from
  // the server.
  server_writer_->set_fake_drop_first_n_packets(1);
  ASSERT_TRUE(Initialize());
}

TEST_P(EndToEndTest, SetInitialReceivedConnectionOptions) {
  QuicTagVector initial_received_options;
  initial_received_options.push_back(kTBBR);
  initial_received_options.push_back(kIW10);
  initial_received_options.push_back(kPRST);
  EXPECT_TRUE(server_config_.SetInitialReceivedConnectionOptions(
      initial_received_options));

  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());
  server_thread_->WaitForCryptoHandshakeConfirmed();

  EXPECT_FALSE(server_config_.SetInitialReceivedConnectionOptions(
      initial_received_options));

  // Verify that server's configuration is correct.
  server_thread_->Pause();
  EXPECT_TRUE(server_config_.HasReceivedConnectionOptions());
  EXPECT_TRUE(
      ContainsQuicTag(server_config_.ReceivedConnectionOptions(), kTBBR));
  EXPECT_TRUE(
      ContainsQuicTag(server_config_.ReceivedConnectionOptions(), kIW10));
  EXPECT_TRUE(
      ContainsQuicTag(server_config_.ReceivedConnectionOptions(), kPRST));
}

TEST_P(EndToEndTest, LargePostSmallBandwidthLargeBuffer) {
  ASSERT_TRUE(Initialize());
  SetPacketSendDelay(QuicTime::Delta::FromMicroseconds(1));
  // 256KB per second with a 256KB buffer from server to client.  Wireless
  // clients commonly have larger buffers, but our max CWND is 200.
  server_writer_->set_max_bandwidth_and_buffer_size(
      QuicBandwidth::FromBytesPerSecond(256 * 1024), 256 * 1024);

  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());

  // 1 MB body.
  QuicString body(1024 * 1024, 'a');
  SpdyHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;

  EXPECT_EQ(kFooResponseBody,
            client_->SendCustomSynchronousRequest(headers, body));
  // This connection may drop packets, because the buffer is smaller than the
  // max CWND.
  VerifyCleanConnection(true);
}

TEST_P(EndToEndTestWithTls, DoNotSetSendAlarmIfConnectionFlowControlBlocked) {
  // Regression test for b/14677858.
  // Test that the resume write alarm is not set in QuicConnection::OnCanWrite
  // if currently connection level flow control blocked. If set, this results in
  // an infinite loop in the EpollServer, as the alarm fires and is immediately
  // rescheduled.
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());

  // Ensure both stream and connection level are flow control blocked by setting
  // the send window offset to 0.
  const uint64_t flow_control_window =
      server_config_.GetInitialStreamFlowControlWindowToSend();
  QuicSpdyClientStream* stream = client_->GetOrCreateStream();
  QuicSession* session = client_->client()->client_session();
  QuicFlowControllerPeer::SetSendWindowOffset(stream->flow_controller(), 0);
  QuicFlowControllerPeer::SetSendWindowOffset(session->flow_controller(), 0);
  EXPECT_TRUE(stream->flow_controller()->IsBlocked());
  EXPECT_TRUE(session->flow_controller()->IsBlocked());

  // Make sure that the stream has data pending so that it will be marked as
  // write blocked when it receives a stream level WINDOW_UPDATE.
  stream->WriteOrBufferBody("hello", false, nullptr);

  // The stream now attempts to write, fails because it is still connection
  // level flow control blocked, and is added to the write blocked list.
  QuicWindowUpdateFrame window_update(kInvalidControlFrameId, stream->id(),
                                      2 * flow_control_window);
  stream->OnWindowUpdateFrame(window_update);

  // Prior to fixing b/14677858 this call would result in an infinite loop in
  // Chromium. As a proxy for detecting this, we now check whether the
  // send alarm is set after OnCanWrite. It should not be, as the
  // connection is still flow control blocked.
  session->connection()->OnCanWrite();

  QuicAlarm* send_alarm =
      QuicConnectionPeer::GetSendAlarm(session->connection());
  EXPECT_FALSE(send_alarm->IsSet());
}

// TODO(nharper): Needs to get turned back to EndToEndTestWithTls
// when we figure out why the test doesn't work on chrome.
TEST_P(EndToEndTest, InvalidStream) {
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());

  QuicString body(kMaxPacketSize, 'a');
  SpdyHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;

  // Force the client to write with a stream ID belonging to a nonexistent
  // server-side stream.
  QuicSpdySession* session = client_->client()->client_session();
  QuicSessionPeer::SetNextOutgoingStreamId(session, GetNthServerInitiatedId(0));

  client_->SendCustomSynchronousRequest(headers, body);
  EXPECT_EQ(QUIC_STREAM_CONNECTION_ERROR, client_->stream_error());
  EXPECT_EQ(QUIC_INVALID_STREAM_ID, client_->connection_error());
}

// Test that if the server will close the connection if the client attempts
// to send a request with overly large headers.
TEST_P(EndToEndTest, LargeHeaders) {
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());

  QuicString body(kMaxPacketSize, 'a');
  SpdyHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;
  headers["key1"] = QuicString(15 * 1024, 'a');
  headers["key2"] = QuicString(15 * 1024, 'a');
  headers["key3"] = QuicString(15 * 1024, 'a');

  client_->SendCustomSynchronousRequest(headers, body);
  EXPECT_EQ(QUIC_HEADERS_TOO_LARGE, client_->stream_error());
  EXPECT_EQ(QUIC_NO_ERROR, client_->connection_error());
}

TEST_P(EndToEndTest, EarlyResponseWithQuicStreamNoError) {
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());

  QuicString large_body(1024 * 1024, 'a');
  SpdyHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;
  // Insert an invalid content_length field in request to trigger an early
  // response from server.
  headers["content-length"] = "-3";

  client_->SendCustomSynchronousRequest(headers, large_body);
  EXPECT_EQ("bad", client_->response_body());
  EXPECT_EQ("500", client_->response_headers()->find(":status")->second);
  EXPECT_EQ(QUIC_STREAM_NO_ERROR, client_->stream_error());
  EXPECT_EQ(QUIC_NO_ERROR, client_->connection_error());
}

// TODO(rch): this test seems to cause net_unittests timeouts :|
TEST_P(EndToEndTestWithTls, DISABLED_MultipleTermination) {
  ASSERT_TRUE(Initialize());

  // Set the offset so we won't frame.  Otherwise when we pick up termination
  // before HTTP framing is complete, we send an error and close the stream,
  // and the second write is picked up as writing on a closed stream.
  QuicSpdyClientStream* stream = client_->GetOrCreateStream();
  ASSERT_TRUE(stream != nullptr);
  QuicStreamPeer::SetStreamBytesWritten(3, stream);

  client_->SendData("bar", true);
  client_->WaitForWriteToFlush();

  // By default the stream protects itself from writes after terminte is set.
  // Override this to test the server handling buggy clients.
  QuicStreamPeer::SetWriteSideClosed(false, client_->GetOrCreateStream());

  EXPECT_QUIC_BUG(client_->SendData("eep", true), "Fin already buffered");
}

// TODO(nharper): Needs to get turned back to EndToEndTestWithTls
// when we figure out why the test doesn't work on chrome.
TEST_P(EndToEndTest, Timeout) {
  client_config_.SetIdleNetworkTimeout(QuicTime::Delta::FromMicroseconds(500),
                                       QuicTime::Delta::FromMicroseconds(500));
  // Note: we do NOT ASSERT_TRUE: we may time out during initial handshake:
  // that's enough to validate timeout in this case.
  Initialize();
  while (client_->client()->connected()) {
    client_->client()->WaitForEvents();
  }
}

TEST_P(EndToEndTestWithTls, MaxIncomingDynamicStreamsLimitRespected) {
  // Set a limit on maximum number of incoming dynamic streams.
  // Make sure the limit is respected.
  const uint32_t kServerMaxIncomingDynamicStreams = 1;
  server_config_.SetMaxIncomingDynamicStreamsToSend(
      kServerMaxIncomingDynamicStreams);
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());

  // Make the client misbehave after negotiation.
  const int kServerMaxStreams = kMaxStreamsMinimumIncrement + 1;
  QuicSessionPeer::SetMaxOpenOutgoingStreams(
      client_->client()->client_session(), kServerMaxStreams + 1);

  SpdyHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;
  headers["content-length"] = "3";

  // The server supports a small number of additional streams beyond the
  // negotiated limit. Open enough streams to go beyond that limit.
  for (int i = 0; i < kServerMaxStreams + 1; ++i) {
    client_->SendMessage(headers, "", /*fin=*/false);
  }
  client_->WaitForResponse();

  EXPECT_TRUE(client_->connected());
  EXPECT_EQ(QUIC_REFUSED_STREAM, client_->stream_error());
  EXPECT_EQ(QUIC_NO_ERROR, client_->connection_error());
}

TEST_P(EndToEndTest, SetIndependentMaxIncomingDynamicStreamsLimits) {
  // Each endpoint can set max incoming dynamic streams independently.
  const uint32_t kClientMaxIncomingDynamicStreams = 2;
  const uint32_t kServerMaxIncomingDynamicStreams = 1;
  client_config_.SetMaxIncomingDynamicStreamsToSend(
      kClientMaxIncomingDynamicStreams);
  server_config_.SetMaxIncomingDynamicStreamsToSend(
      kServerMaxIncomingDynamicStreams);
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());

  // The client has received the server's limit and vice versa.
  EXPECT_EQ(kServerMaxIncomingDynamicStreams,
            client_->client()->client_session()->max_open_outgoing_streams());
  server_thread_->Pause();
  EXPECT_EQ(kClientMaxIncomingDynamicStreams,
            GetServerSession()->max_open_outgoing_streams());
  server_thread_->Resume();
}

TEST_P(EndToEndTest, NegotiateCongestionControl) {
  ASSERT_TRUE(Initialize());

  // For PCC, the underlying implementation may be a stub with a
  // different name-tag.  Skip the rest of this test.
  if (GetParam().congestion_control_tag == kTPCC) {
    return;
  }

  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());

  CongestionControlType expected_congestion_control_type = kRenoBytes;
  switch (GetParam().congestion_control_tag) {
    case kRENO:
      expected_congestion_control_type = kRenoBytes;
      break;
    case kTBBR:
      expected_congestion_control_type = kBBR;
      break;
    case kQBIC:
      expected_congestion_control_type = kCubicBytes;
      break;
    default:
      QUIC_DLOG(FATAL) << "Unexpected congestion control tag";
  }

  server_thread_->Pause();
  EXPECT_EQ(expected_congestion_control_type,
            QuicSentPacketManagerPeer::GetSendAlgorithm(
                *GetSentPacketManagerFromFirstServerSession())
                ->GetCongestionControlType());
  server_thread_->Resume();
}

TEST_P(EndToEndTest, ClientSuggestsRTT) {
  // Client suggests initial RTT, verify it is used.
  const QuicTime::Delta kInitialRTT = QuicTime::Delta::FromMicroseconds(20000);
  client_config_.SetInitialRoundTripTimeUsToSend(kInitialRTT.ToMicroseconds());

  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());
  server_thread_->WaitForCryptoHandshakeConfirmed();

  // Pause the server so we can access the server's internals without races.
  server_thread_->Pause();
  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  ASSERT_EQ(1u, dispatcher->session_map().size());
  const QuicSentPacketManager& client_sent_packet_manager =
      client_->client()->client_session()->connection()->sent_packet_manager();
  const QuicSentPacketManager* server_sent_packet_manager =
      GetSentPacketManagerFromFirstServerSession();

  EXPECT_EQ(kInitialRTT,
            client_sent_packet_manager.GetRttStats()->initial_rtt());
  EXPECT_EQ(kInitialRTT,
            server_sent_packet_manager->GetRttStats()->initial_rtt());
  server_thread_->Resume();
}

TEST_P(EndToEndTest, ClientSuggestsIgnoredRTT) {
  // Client suggests initial RTT, but also specifies NRTT, so it's not used.
  const QuicTime::Delta kInitialRTT = QuicTime::Delta::FromMicroseconds(20000);
  client_config_.SetInitialRoundTripTimeUsToSend(kInitialRTT.ToMicroseconds());
  QuicTagVector options;
  options.push_back(kNRTT);
  client_config_.SetConnectionOptionsToSend(options);

  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());
  server_thread_->WaitForCryptoHandshakeConfirmed();

  // Pause the server so we can access the server's internals without races.
  server_thread_->Pause();
  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  ASSERT_EQ(1u, dispatcher->session_map().size());
  const QuicSentPacketManager& client_sent_packet_manager =
      client_->client()->client_session()->connection()->sent_packet_manager();
  const QuicSentPacketManager* server_sent_packet_manager =
      GetSentPacketManagerFromFirstServerSession();

  EXPECT_EQ(kInitialRTT,
            client_sent_packet_manager.GetRttStats()->initial_rtt());
  EXPECT_EQ(kInitialRTT,
            server_sent_packet_manager->GetRttStats()->initial_rtt());
  server_thread_->Resume();
}

TEST_P(EndToEndTest, MaxInitialRTT) {
  // Client tries to suggest twice the server's max initial rtt and the server
  // uses the max.
  client_config_.SetInitialRoundTripTimeUsToSend(2 *
                                                 kMaxInitialRoundTripTimeUs);

  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());
  server_thread_->WaitForCryptoHandshakeConfirmed();

  // Pause the server so we can access the server's internals without races.
  server_thread_->Pause();
  const QuicSentPacketManager& client_sent_packet_manager =
      client_->client()->client_session()->connection()->sent_packet_manager();

  // Now that acks have been exchanged, the RTT estimate has decreased on the
  // server and is not infinite on the client.
  EXPECT_FALSE(
      client_sent_packet_manager.GetRttStats()->smoothed_rtt().IsInfinite());
  const RttStats& server_rtt_stats =
      *GetServerConnection()->sent_packet_manager().GetRttStats();
  EXPECT_EQ(static_cast<int64_t>(kMaxInitialRoundTripTimeUs),
            server_rtt_stats.initial_rtt().ToMicroseconds());
  EXPECT_GE(static_cast<int64_t>(kMaxInitialRoundTripTimeUs),
            server_rtt_stats.smoothed_rtt().ToMicroseconds());
  server_thread_->Resume();
}

TEST_P(EndToEndTest, MinInitialRTT) {
  // Client tries to suggest 0 and the server uses the default.
  client_config_.SetInitialRoundTripTimeUsToSend(0);

  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());
  server_thread_->WaitForCryptoHandshakeConfirmed();

  // Pause the server so we can access the server's internals without races.
  server_thread_->Pause();
  const QuicSentPacketManager& client_sent_packet_manager =
      client_->client()->client_session()->connection()->sent_packet_manager();
  const QuicSentPacketManager& server_sent_packet_manager =
      GetServerConnection()->sent_packet_manager();

  // Now that acks have been exchanged, the RTT estimate has decreased on the
  // server and is not infinite on the client.
  EXPECT_FALSE(
      client_sent_packet_manager.GetRttStats()->smoothed_rtt().IsInfinite());
  // Expect the default rtt of 100ms.
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(100),
            server_sent_packet_manager.GetRttStats()->initial_rtt());
  // Ensure the bandwidth is valid.
  client_sent_packet_manager.BandwidthEstimate();
  server_sent_packet_manager.BandwidthEstimate();
  server_thread_->Resume();
}

TEST_P(EndToEndTest, 0ByteConnectionId) {
  client_config_.SetBytesForConnectionIdToSend(0);
  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);
  QuicConnection* client_connection =
      client_->client()->client_session()->connection();
  QuicPacketHeader* header =
      QuicConnectionPeer::GetLastHeader(client_connection);
  EXPECT_EQ(PACKET_0BYTE_CONNECTION_ID,
            header->destination_connection_id_length);
}

TEST_P(EndToEndTestWithTls, 8ByteConnectionId) {
  client_config_.SetBytesForConnectionIdToSend(8);
  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);
  QuicConnection* client_connection =
      client_->client()->client_session()->connection();
  QuicPacketHeader* header =
      QuicConnectionPeer::GetLastHeader(client_connection);
  if (client_connection->transport_version() > QUIC_VERSION_43) {
    EXPECT_EQ(PACKET_0BYTE_CONNECTION_ID,
              header->destination_connection_id_length);
  } else {
    EXPECT_EQ(PACKET_8BYTE_CONNECTION_ID,
              header->destination_connection_id_length);
  }
}

TEST_P(EndToEndTestWithTls, 15ByteConnectionId) {
  client_config_.SetBytesForConnectionIdToSend(15);
  ASSERT_TRUE(Initialize());

  // Our server is permissive and allows for out of bounds values.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);
  QuicConnection* client_connection =
      client_->client()->client_session()->connection();
  QuicPacketHeader* header =
      QuicConnectionPeer::GetLastHeader(client_connection);
  if (client_connection->transport_version() > QUIC_VERSION_43) {
    EXPECT_EQ(PACKET_0BYTE_CONNECTION_ID,
              header->destination_connection_id_length);
  } else {
    EXPECT_EQ(PACKET_8BYTE_CONNECTION_ID,
              header->destination_connection_id_length);
  }
}

TEST_P(EndToEndTestWithTls, ResetConnection) {
  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);
  client_->ResetConnection();
  EXPECT_EQ(kBarResponseBody, client_->SendSynchronousRequest("/bar"));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);
}

// TODO(nharper): Needs to get turned back to EndToEndTestWithTls
// when we figure out why the test doesn't work on chrome.
TEST_P(EndToEndTest, MaxStreamsUberTest) {
  if (!BothSidesSupportStatelessRejects()) {
    // Connect with lower fake packet loss than we'd like to test.  Until
    // b/10126687 is fixed, losing handshake packets is pretty brutal.
    // TODO(jokulik): Until we support redundant SREJ packets, don't
    // drop handshake packets for stateless rejects.
    SetPacketLossPercentage(1);
  }
  ASSERT_TRUE(Initialize());
  QuicString large_body(10240, 'a');
  int max_streams = 100;

  AddToCache("/large_response", 200, large_body);

  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());
  SetPacketLossPercentage(10);

  for (int i = 0; i < max_streams; ++i) {
    EXPECT_LT(0, client_->SendRequest("/large_response"));
  }

  // WaitForEvents waits 50ms and returns true if there are outstanding
  // requests.
  while (client_->client()->WaitForEvents() == true) {
  }
}

TEST_P(EndToEndTestWithTls, StreamCancelErrorTest) {
  ASSERT_TRUE(Initialize());
  QuicString small_body(256, 'a');

  AddToCache("/small_response", 200, small_body);

  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());

  QuicSession* session = client_->client()->client_session();
  // Lose the request.
  SetPacketLossPercentage(100);
  EXPECT_LT(0, client_->SendRequest("/small_response"));
  client_->client()->WaitForEvents();
  // Transmit the cancel, and ensure the connection is torn down properly.
  SetPacketLossPercentage(0);
  QuicStreamId stream_id = GetNthClientInitiatedId(0);
  session->SendRstStream(stream_id, QUIC_STREAM_CANCELLED, 0);

  // WaitForEvents waits 50ms and returns true if there are outstanding
  // requests.
  while (client_->client()->WaitForEvents() == true) {
  }
  // It should be completely fine to RST a stream before any data has been
  // received for that stream.
  EXPECT_EQ(QUIC_NO_ERROR, client_->connection_error());
}

TEST_P(EndToEndTest, ConnectionMigrationClientIPChanged) {
  ASSERT_TRUE(Initialize());
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);

  // Store the client IP address which was used to send the first request.
  QuicIpAddress old_host =
      client_->client()->network_helper()->GetLatestClientAddress().host();

  // Migrate socket to the new IP address.
  QuicIpAddress new_host = TestLoopback(2);
  EXPECT_NE(old_host, new_host);
  ASSERT_TRUE(client_->client()->MigrateSocket(new_host));

  // Send a request using the new socket.
  EXPECT_EQ(kBarResponseBody, client_->SendSynchronousRequest("/bar"));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);
}

TEST_P(EndToEndTest, ConnectionMigrationClientPortChanged) {
  // Tests that the client's port can change during an established QUIC
  // connection, and that doing so does not result in the connection being
  // closed by the server.
  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);

  // Store the client address which was used to send the first request.
  QuicSocketAddress old_address =
      client_->client()->network_helper()->GetLatestClientAddress();
  int old_fd = client_->client()->GetLatestFD();

  // Create a new socket before closing the old one, which will result in a new
  // ephemeral port.
  QuicClientPeer::CreateUDPSocketAndBind(client_->client());

  // Stop listening and close the old FD.
  QuicClientPeer::CleanUpUDPSocket(client_->client(), old_fd);

  // The packet writer needs to be updated to use the new FD.
  client_->client()->network_helper()->CreateQuicPacketWriter();

  // Change the internal state of the client and connection to use the new port,
  // this is done because in a real NAT rebinding the client wouldn't see any
  // port change, and so expects no change to incoming port.
  // This is kind of ugly, but needed as we are simply swapping out the client
  // FD rather than any more complex NAT rebinding simulation.
  int new_port =
      client_->client()->network_helper()->GetLatestClientAddress().port();
  QuicClientPeer::SetClientPort(client_->client(), new_port);
  QuicConnectionPeer::SetSelfAddress(
      client_->client()->client_session()->connection(),
      QuicSocketAddress(client_->client()
                            ->client_session()
                            ->connection()
                            ->self_address()
                            .host(),
                        new_port));

  // Register the new FD for epoll events.
  int new_fd = client_->client()->GetLatestFD();
  EpollServer* eps = client_->epoll_server();
  eps->RegisterFD(new_fd, client_->client()->epoll_network_helper(),
                  EPOLLIN | EPOLLOUT | EPOLLET);

  // Send a second request, using the new FD.
  EXPECT_EQ(kBarResponseBody, client_->SendSynchronousRequest("/bar"));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);

  // Verify that the client's ephemeral port is different.
  QuicSocketAddress new_address =
      client_->client()->network_helper()->GetLatestClientAddress();
  EXPECT_EQ(old_address.host(), new_address.host());
  EXPECT_NE(old_address.port(), new_address.port());
}

TEST_P(EndToEndTest, NegotiatedInitialCongestionWindow) {
  SetQuicReloadableFlag(quic_unified_iw_options, true);
  client_extra_copts_.push_back(kIW03);

  ASSERT_TRUE(Initialize());

  // Values are exchanged during crypto handshake, so wait for that to finish.
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());
  server_thread_->WaitForCryptoHandshakeConfirmed();
  server_thread_->Pause();

  QuicPacketCount cwnd =
      GetServerConnection()->sent_packet_manager().initial_congestion_window();
  EXPECT_EQ(3u, cwnd);
}

TEST_P(EndToEndTest, DifferentFlowControlWindows) {
  // Client and server can set different initial flow control receive windows.
  // These are sent in CHLO/SHLO. Tests that these values are exchanged properly
  // in the crypto handshake.
  const uint32_t kClientStreamIFCW = 123456;
  const uint32_t kClientSessionIFCW = 234567;
  set_client_initial_stream_flow_control_receive_window(kClientStreamIFCW);
  set_client_initial_session_flow_control_receive_window(kClientSessionIFCW);

  uint32_t kServerStreamIFCW = 32 * 1024;
  uint32_t kServerSessionIFCW = 48 * 1024;
  set_server_initial_stream_flow_control_receive_window(kServerStreamIFCW);
  set_server_initial_session_flow_control_receive_window(kServerSessionIFCW);

  ASSERT_TRUE(Initialize());

  // Values are exchanged during crypto handshake, so wait for that to finish.
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());
  server_thread_->WaitForCryptoHandshakeConfirmed();

  // Open a data stream to make sure the stream level flow control is updated.
  QuicSpdyClientStream* stream = client_->GetOrCreateStream();
  stream->WriteOrBufferBody("hello", false, nullptr);

  // Client should have the right values for server's receive window.
  EXPECT_EQ(kServerStreamIFCW,
            client_->client()
                ->client_session()
                ->config()
                ->ReceivedInitialStreamFlowControlWindowBytes());
  EXPECT_EQ(kServerSessionIFCW,
            client_->client()
                ->client_session()
                ->config()
                ->ReceivedInitialSessionFlowControlWindowBytes());
  EXPECT_EQ(kServerStreamIFCW, QuicFlowControllerPeer::SendWindowOffset(
                                   stream->flow_controller()));
  EXPECT_EQ(kServerSessionIFCW,
            QuicFlowControllerPeer::SendWindowOffset(
                client_->client()->client_session()->flow_controller()));

  // Server should have the right values for client's receive window.
  server_thread_->Pause();
  QuicSession* session = GetServerSession();
  EXPECT_EQ(kClientStreamIFCW,
            session->config()->ReceivedInitialStreamFlowControlWindowBytes());
  EXPECT_EQ(kClientSessionIFCW,
            session->config()->ReceivedInitialSessionFlowControlWindowBytes());
  EXPECT_EQ(kClientSessionIFCW, QuicFlowControllerPeer::SendWindowOffset(
                                    session->flow_controller()));
  server_thread_->Resume();
}

// Test negotiation of IFWA connection option.
TEST_P(EndToEndTest, NegotiatedServerInitialFlowControlWindow) {
  const uint32_t kClientStreamIFCW = 123456;
  const uint32_t kClientSessionIFCW = 234567;
  set_client_initial_stream_flow_control_receive_window(kClientStreamIFCW);
  set_client_initial_session_flow_control_receive_window(kClientSessionIFCW);

  uint32_t kServerStreamIFCW = 32 * 1024;
  uint32_t kServerSessionIFCW = 48 * 1024;
  set_server_initial_stream_flow_control_receive_window(kServerStreamIFCW);
  set_server_initial_session_flow_control_receive_window(kServerSessionIFCW);

  // Bump the window.
  const uint32_t kExpectedStreamIFCW = 1024 * 1024;
  const uint32_t kExpectedSessionIFCW = 1.5 * 1024 * 1024;
  client_extra_copts_.push_back(kIFWA);

  ASSERT_TRUE(Initialize());

  // Values are exchanged during crypto handshake, so wait for that to finish.
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());
  server_thread_->WaitForCryptoHandshakeConfirmed();

  // Open a data stream to make sure the stream level flow control is updated.
  QuicSpdyClientStream* stream = client_->GetOrCreateStream();
  stream->WriteOrBufferBody("hello", false, nullptr);

  // Client should have the right values for server's receive window.
  EXPECT_EQ(kExpectedStreamIFCW,
            client_->client()
                ->client_session()
                ->config()
                ->ReceivedInitialStreamFlowControlWindowBytes());
  EXPECT_EQ(kExpectedSessionIFCW,
            client_->client()
                ->client_session()
                ->config()
                ->ReceivedInitialSessionFlowControlWindowBytes());
  EXPECT_EQ(kExpectedStreamIFCW, QuicFlowControllerPeer::SendWindowOffset(
                                     stream->flow_controller()));
  EXPECT_EQ(kExpectedSessionIFCW,
            QuicFlowControllerPeer::SendWindowOffset(
                client_->client()->client_session()->flow_controller()));
}

TEST_P(EndToEndTest, HeadersAndCryptoStreamsNoConnectionFlowControl) {
  // The special headers and crypto streams should be subject to per-stream flow
  // control limits, but should not be subject to connection level flow control
  const uint32_t kStreamIFCW = 32 * 1024;
  const uint32_t kSessionIFCW = 48 * 1024;
  set_client_initial_stream_flow_control_receive_window(kStreamIFCW);
  set_client_initial_session_flow_control_receive_window(kSessionIFCW);
  set_server_initial_stream_flow_control_receive_window(kStreamIFCW);
  set_server_initial_session_flow_control_receive_window(kSessionIFCW);

  ASSERT_TRUE(Initialize());

  // Wait for crypto handshake to finish. This should have contributed to the
  // crypto stream flow control window, but not affected the session flow
  // control window.
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());
  server_thread_->WaitForCryptoHandshakeConfirmed();

  QuicCryptoStream* crypto_stream = QuicSessionPeer::GetMutableCryptoStream(
      client_->client()->client_session());
  EXPECT_LT(
      QuicFlowControllerPeer::SendWindowSize(crypto_stream->flow_controller()),
      kStreamIFCW);
  EXPECT_EQ(kSessionIFCW,
            QuicFlowControllerPeer::SendWindowSize(
                client_->client()->client_session()->flow_controller()));

  // Send a request with no body, and verify that the connection level window
  // has not been affected.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));

  QuicHeadersStream* headers_stream = QuicSpdySessionPeer::GetHeadersStream(
      client_->client()->client_session());
  EXPECT_LT(
      QuicFlowControllerPeer::SendWindowSize(headers_stream->flow_controller()),
      kStreamIFCW);
  EXPECT_EQ(kSessionIFCW,
            QuicFlowControllerPeer::SendWindowSize(
                client_->client()->client_session()->flow_controller()));

  // Server should be in a similar state: connection flow control window should
  // not have any bytes marked as received.
  server_thread_->Pause();
  QuicSession* session = GetServerSession();
  QuicFlowController* server_connection_flow_controller =
      session->flow_controller();
  EXPECT_EQ(kSessionIFCW, QuicFlowControllerPeer::ReceiveWindowSize(
                              server_connection_flow_controller));
  server_thread_->Resume();
}

TEST_P(EndToEndTest, FlowControlsSynced) {
  set_smaller_flow_control_receive_window();

  ASSERT_TRUE(Initialize());

  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());
  server_thread_->WaitForCryptoHandshakeConfirmed();

  server_thread_->Pause();
  QuicSpdySession* const client_session = client_->client()->client_session();
  auto* server_session = static_cast<QuicSpdySession*>(GetServerSession());
  ExpectFlowControlsSynced(client_session->flow_controller(),
                           server_session->flow_controller());
  ExpectFlowControlsSynced(
      QuicSessionPeer::GetMutableCryptoStream(client_session)
          ->flow_controller(),
      QuicSessionPeer::GetMutableCryptoStream(server_session)
          ->flow_controller());
  SpdyFramer spdy_framer(SpdyFramer::ENABLE_COMPRESSION);
  SpdySettingsIR settings_frame;
  settings_frame.AddSetting(SETTINGS_MAX_HEADER_LIST_SIZE,
                            kDefaultMaxUncompressedHeaderSize);
  SpdySerializedFrame frame(spdy_framer.SerializeFrame(settings_frame));
  QuicFlowController* client_header_stream_flow_controller =
      QuicSpdySessionPeer::GetHeadersStream(client_session)->flow_controller();
  QuicFlowController* server_header_stream_flow_controller =
      QuicSpdySessionPeer::GetHeadersStream(server_session)->flow_controller();
  // Both client and server are sending this SETTINGS frame, and the send
  // window is consumed. But because of timing issue, the server may send or
  // not send the frame, and the client may send/ not send / receive / not
  // receive the frame.
  // TODO(fayang): Rewrite this part because it is hacky.
  QuicByteCount win_difference1 = QuicFlowControllerPeer::ReceiveWindowSize(
                                      server_header_stream_flow_controller) -
                                  QuicFlowControllerPeer::SendWindowSize(
                                      client_header_stream_flow_controller);
  QuicByteCount win_difference2 = QuicFlowControllerPeer::ReceiveWindowSize(
                                      client_header_stream_flow_controller) -
                                  QuicFlowControllerPeer::SendWindowSize(
                                      server_header_stream_flow_controller);
  EXPECT_TRUE(win_difference1 == 0 || win_difference1 == frame.size());
  EXPECT_TRUE(win_difference2 == 0 || win_difference2 == frame.size());

  // Client *may* have received the SETTINGs frame.
  // TODO(fayang): Rewrite this part because it is hacky.
  float ratio1 = static_cast<float>(QuicFlowControllerPeer::ReceiveWindowSize(
                     client_session->flow_controller())) /
                 QuicFlowControllerPeer::ReceiveWindowSize(
                     QuicSpdySessionPeer::GetHeadersStream(client_session)
                         ->flow_controller());
  float ratio2 = static_cast<float>(QuicFlowControllerPeer::ReceiveWindowSize(
                     client_session->flow_controller())) /
                 (QuicFlowControllerPeer::ReceiveWindowSize(
                      QuicSpdySessionPeer::GetHeadersStream(client_session)
                          ->flow_controller()) +
                  frame.size());
  EXPECT_TRUE(ratio1 == kSessionToStreamRatio ||
              ratio2 == kSessionToStreamRatio);

  server_thread_->Resume();
}

TEST_P(EndToEndTestWithTls, RequestWithNoBodyWillNeverSendStreamFrameWithFIN) {
  // A stream created on receipt of a simple request with no body will never get
  // a stream frame with a FIN. Verify that we don't keep track of the stream in
  // the locally closed streams map: it will never be removed if so.
  ASSERT_TRUE(Initialize());

  // Send a simple headers only request, and receive response.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);

  // Now verify that the server is not waiting for a final FIN or RST.
  server_thread_->Pause();
  QuicSession* session = GetServerSession();
  EXPECT_EQ(
      0u,
      QuicSessionPeer::GetLocallyClosedStreamsHighestOffset(session).size());
  server_thread_->Resume();
}

// A TestAckListener verifies that its OnAckNotification method has been
// called exactly once on destruction.
class TestAckListener : public QuicAckListenerInterface {
 public:
  explicit TestAckListener(int bytes_to_ack) : bytes_to_ack_(bytes_to_ack) {}

  void OnPacketAcked(int acked_bytes,
                     QuicTime::Delta /*delta_largest_observed*/) override {
    ASSERT_LE(acked_bytes, bytes_to_ack_);
    bytes_to_ack_ -= acked_bytes;
  }

  void OnPacketRetransmitted(int /*retransmitted_bytes*/) override {}

  bool has_been_notified() const { return bytes_to_ack_ == 0; }

 protected:
  // Object is ref counted.
  ~TestAckListener() override { EXPECT_EQ(0, bytes_to_ack_); }

 private:
  int bytes_to_ack_;
};

class TestResponseListener : public QuicSpdyClientBase::ResponseListener {
 public:
  void OnCompleteResponse(QuicStreamId id,
                          const SpdyHeaderBlock& response_headers,
                          const QuicString& response_body) override {
    QUIC_DVLOG(1) << "response for stream " << id << " "
                  << response_headers.DebugString() << "\n"
                  << response_body;
  }
};

TEST_P(EndToEndTest, AckNotifierWithPacketLossAndBlockedSocket) {
  // Verify that even in the presence of packet loss and occasionally blocked
  // socket,  an AckNotifierDelegate will get informed that the data it is
  // interested in has been ACKed. This tests end-to-end ACK notification, and
  // demonstrates that retransmissions do not break this functionality.
  if (!BothSidesSupportStatelessRejects()) {
    // TODO(jokulik): Until we support redundant SREJ packets, don't
    // drop handshake packets for stateless rejects.
    SetPacketLossPercentage(5);
  }
  ASSERT_TRUE(Initialize());

  // Wait for the server SHLO before upping the packet loss.
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());
  SetPacketLossPercentage(30);
  client_writer_->set_fake_blocked_socket_percentage(10);

  // Create a POST request and send the headers only.
  SpdyHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;

  client_->SendMessage(headers, "", /*fin=*/false);

  // Test the AckNotifier's ability to track multiple packets by making the
  // request body exceed the size of a single packet.
  QuicString request_string =
      "a request body bigger than one packet" + QuicString(kMaxPacketSize, '.');

  // The TestAckListener will cause a failure if not notified.
  QuicReferenceCountedPointer<TestAckListener> ack_listener(
      new TestAckListener(request_string.length()));

  // Send the request, and register the delegate for ACKs.
  client_->SendData(request_string, true, ack_listener);
  client_->WaitForResponse();
  EXPECT_EQ(kFooResponseBody, client_->response_body());
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);

  // Send another request to flush out any pending ACKs on the server.
  client_->SendSynchronousRequest("/bar");

  // Make sure the delegate does get the notification it expects.
  while (!ack_listener->has_been_notified()) {
    // Waits for up to 50 ms.
    client_->client()->WaitForEvents();
  }
}

// Send a public reset from the server.
TEST_P(EndToEndTestWithTls, ServerSendPublicReset) {
  ASSERT_TRUE(Initialize());

  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());
  QuicConnection* client_connection =
      client_->client()->client_session()->connection();
  if (SupportsIetfQuicWithTls(client_connection->version())) {
    // TLS handshake does not support stateless reset token yet.
    return;
  }
  QuicUint128 stateless_reset_token = 0;
  if (client_connection->version().handshake_protocol == PROTOCOL_QUIC_CRYPTO) {
    QuicConfig* config = client_->client()->session()->config();
    EXPECT_TRUE(config->HasReceivedStatelessResetToken());
    stateless_reset_token = config->ReceivedStatelessResetToken();
  }

  // Send the public reset.
  QuicConnectionId connection_id = client_connection->connection_id();
  QuicPublicResetPacket header;
  header.connection_id = connection_id;
  QuicFramer framer(server_supported_versions_, QuicTime::Zero(),
                    Perspective::IS_SERVER);
  std::unique_ptr<QuicEncryptedPacket> packet;
  if (client_connection->transport_version() > QUIC_VERSION_43) {
    packet = framer.BuildIetfStatelessResetPacket(connection_id,
                                                  stateless_reset_token);
  } else {
    packet = framer.BuildPublicResetPacket(header);
  }
  // We must pause the server's thread in order to call WritePacket without
  // race conditions.
  server_thread_->Pause();
  server_writer_->WritePacket(
      packet->data(), packet->length(), server_address_.host(),
      client_->client()->network_helper()->GetLatestClientAddress(), nullptr);
  server_thread_->Resume();

  // The request should fail.
  EXPECT_EQ("", client_->SendSynchronousRequest("/foo"));
  EXPECT_TRUE(client_->response_headers()->empty());
  EXPECT_EQ(QUIC_PUBLIC_RESET, client_->connection_error());
}

// Send a public reset from the server for a different connection ID.
// It should be ignored.
TEST_P(EndToEndTestWithTls, ServerSendPublicResetWithDifferentConnectionId) {
  ASSERT_TRUE(Initialize());

  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());
  QuicConnection* client_connection =
      client_->client()->client_session()->connection();
  if (SupportsIetfQuicWithTls(client_connection->version())) {
    // TLS handshake does not support stateless reset token yet.
    return;
  }
  QuicUint128 stateless_reset_token = 0;
  if (client_connection->version().handshake_protocol == PROTOCOL_QUIC_CRYPTO) {
    QuicConfig* config = client_->client()->session()->config();
    EXPECT_TRUE(config->HasReceivedStatelessResetToken());
    stateless_reset_token = config->ReceivedStatelessResetToken();
  }
  // Send the public reset.
  QuicConnectionId incorrect_connection_id =
      client_connection->connection_id() + 1;
  QuicPublicResetPacket header;
  header.connection_id = incorrect_connection_id;
  QuicFramer framer(server_supported_versions_, QuicTime::Zero(),
                    Perspective::IS_SERVER);
  std::unique_ptr<QuicEncryptedPacket> packet;
  testing::NiceMock<MockQuicConnectionDebugVisitor> visitor;
  client_->client()->client_session()->connection()->set_debug_visitor(
      &visitor);
  if (client_connection->transport_version() > QUIC_VERSION_43) {
    packet = framer.BuildIetfStatelessResetPacket(incorrect_connection_id,
                                                  stateless_reset_token);
    EXPECT_CALL(visitor, OnIncorrectConnectionId(incorrect_connection_id))
        .Times(0);
  } else {
    packet = framer.BuildPublicResetPacket(header);
    EXPECT_CALL(visitor, OnIncorrectConnectionId(incorrect_connection_id))
        .Times(1);
  }
  // We must pause the server's thread in order to call WritePacket without
  // race conditions.
  server_thread_->Pause();
  server_writer_->WritePacket(
      packet->data(), packet->length(), server_address_.host(),
      client_->client()->network_helper()->GetLatestClientAddress(), nullptr);
  server_thread_->Resume();

  if (client_connection->transport_version() > QUIC_VERSION_43) {
    // The request should fail. IETF stateless reset does not include connection
    // ID.
    EXPECT_EQ("", client_->SendSynchronousRequest("/foo"));
    EXPECT_TRUE(client_->response_headers()->empty());
    EXPECT_EQ(QUIC_PUBLIC_RESET, client_->connection_error());
    return;
  }
  // The connection should be unaffected.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);

  client_->client()->client_session()->connection()->set_debug_visitor(nullptr);
}

// Send a public reset from the client for a different connection ID.
// It should be ignored.
TEST_P(EndToEndTestWithTls, ClientSendPublicResetWithDifferentConnectionId) {
  ASSERT_TRUE(Initialize());

  // Send the public reset.
  QuicConnectionId incorrect_connection_id =
      client_->client()->client_session()->connection()->connection_id() + 1;
  QuicPublicResetPacket header;
  header.connection_id = incorrect_connection_id;
  QuicFramer framer(server_supported_versions_, QuicTime::Zero(),
                    Perspective::IS_CLIENT);
  std::unique_ptr<QuicEncryptedPacket> packet(
      framer.BuildPublicResetPacket(header));
  client_writer_->WritePacket(
      packet->data(), packet->length(),
      client_->client()->network_helper()->GetLatestClientAddress().host(),
      server_address_, nullptr);

  // The connection should be unaffected.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);
}

// Send a version negotiation packet from the server for a different
// connection ID.  It should be ignored.
TEST_P(EndToEndTestWithTls,
       ServerSendVersionNegotiationWithDifferentConnectionId) {
  ASSERT_TRUE(Initialize());

  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());

  // Send the version negotiation packet.
  QuicConnection* client_connection =
      client_->client()->client_session()->connection();
  QuicConnectionId incorrect_connection_id =
      client_connection->connection_id() + 1;
  std::unique_ptr<QuicEncryptedPacket> packet(
      QuicFramer::BuildVersionNegotiationPacket(
          incorrect_connection_id,
          client_connection->transport_version() > QUIC_VERSION_43,
          server_supported_versions_));
  testing::NiceMock<MockQuicConnectionDebugVisitor> visitor;
  client_connection->set_debug_visitor(&visitor);
  EXPECT_CALL(visitor, OnIncorrectConnectionId(incorrect_connection_id))
      .Times(1);
  // We must pause the server's thread in order to call WritePacket without
  // race conditions.
  server_thread_->Pause();
  server_writer_->WritePacket(
      packet->data(), packet->length(), server_address_.host(),
      client_->client()->network_helper()->GetLatestClientAddress(), nullptr);
  server_thread_->Resume();

  // The connection should be unaffected.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);

  client_connection->set_debug_visitor(nullptr);
}

// A bad header shouldn't tear down the connection, because the receiver can't
// tell the connection ID.
TEST_P(EndToEndTestWithTls, BadPacketHeaderTruncated) {
  ASSERT_TRUE(Initialize());

  // Start the connection.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);

  // Packet with invalid public flags.
  char packet[] = {// public flags (8 byte connection_id)
                   0x3C,
                   // truncated connection ID
                   0x11};
  client_writer_->WritePacket(
      &packet[0], sizeof(packet),
      client_->client()->network_helper()->GetLatestClientAddress().host(),
      server_address_, nullptr);
  // Give the server time to process the packet.
  QuicSleep(QuicTime::Delta::FromMilliseconds(100));
  // Pause the server so we can access the server's internals without races.
  server_thread_->Pause();
  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  EXPECT_EQ(QUIC_INVALID_PACKET_HEADER,
            QuicDispatcherPeer::GetAndClearLastError(dispatcher));
  server_thread_->Resume();

  // The connection should not be terminated.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);
}

// A bad header shouldn't tear down the connection, because the receiver can't
// tell the connection ID.
TEST_P(EndToEndTestWithTls, BadPacketHeaderFlags) {
  ASSERT_TRUE(Initialize());

  // Start the connection.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);

  // Packet with invalid public flags.
  char packet[] = {
      // invalid public flags
      0xFF,
      // connection_id
      0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
      // packet sequence number
      0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12,
      // private flags
      0x00,
  };
  client_writer_->WritePacket(
      &packet[0], sizeof(packet),
      client_->client()->network_helper()->GetLatestClientAddress().host(),
      server_address_, nullptr);
  // Give the server time to process the packet.
  QuicSleep(QuicTime::Delta::FromMilliseconds(100));
  // Pause the server so we can access the server's internals without races.
  server_thread_->Pause();
  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  EXPECT_EQ(QUIC_INVALID_PACKET_HEADER,
            QuicDispatcherPeer::GetAndClearLastError(dispatcher));
  server_thread_->Resume();

  // The connection should not be terminated.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);
}

// Send a packet from the client with bad encrypted data.  The server should not
// tear down the connection.
TEST_P(EndToEndTestWithTls, BadEncryptedData) {
  ASSERT_TRUE(Initialize());

  // Start the connection.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);

  std::unique_ptr<QuicEncryptedPacket> packet(ConstructEncryptedPacket(
      client_->client()->client_session()->connection()->connection_id(), 0,
      false, false, 1, "At least 20 characters.", PACKET_8BYTE_CONNECTION_ID,
      PACKET_0BYTE_CONNECTION_ID, PACKET_4BYTE_PACKET_NUMBER));
  // Damage the encrypted data.
  QuicString damaged_packet(packet->data(), packet->length());
  damaged_packet[30] ^= 0x01;
  QUIC_DLOG(INFO) << "Sending bad packet.";
  client_writer_->WritePacket(
      damaged_packet.data(), damaged_packet.length(),
      client_->client()->network_helper()->GetLatestClientAddress().host(),
      server_address_, nullptr);
  // Give the server time to process the packet.
  QuicSleep(QuicTime::Delta::FromMilliseconds(100));
  // This error is sent to the connection's OnError (which ignores it), so the
  // dispatcher doesn't see it.
  // Pause the server so we can access the server's internals without races.
  server_thread_->Pause();
  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  EXPECT_EQ(QUIC_NO_ERROR,
            QuicDispatcherPeer::GetAndClearLastError(dispatcher));
  server_thread_->Resume();

  // The connection should not be terminated.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);
}

TEST_P(EndToEndTestWithTls, CanceledStreamDoesNotBecomeZombie) {
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());
  // Lose the request.
  SetPacketLossPercentage(100);
  SpdyHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;
  client_->SendMessage(headers, "test_body", /*fin=*/false);
  QuicSpdyClientStream* stream = client_->GetOrCreateStream();

  // Cancel the stream.
  stream->Reset(QUIC_STREAM_CANCELLED);
  QuicSession* session = client_->client()->client_session();
  // Verify canceled stream does not become zombie.
  EXPECT_TRUE(QuicSessionPeer::zombie_streams(session).empty());
  EXPECT_EQ(1u, QuicSessionPeer::closed_streams(session).size());
}

// A test stream that gives |response_body_| as an error response body.
class ServerStreamWithErrorResponseBody : public QuicSimpleServerStream {
 public:
  ServerStreamWithErrorResponseBody(
      QuicStreamId id,
      QuicSpdySession* session,
      QuicSimpleServerBackend* quic_simple_server_backend,
      QuicString response_body)
      : QuicSimpleServerStream(id,
                               session,
                               BIDIRECTIONAL,
                               quic_simple_server_backend),
        response_body_(std::move(response_body)) {}

  ~ServerStreamWithErrorResponseBody() override = default;

 protected:
  void SendErrorResponse() override {
    QUIC_DLOG(INFO) << "Sending error response for stream " << id();
    SpdyHeaderBlock headers;
    headers[":status"] = "500";
    headers["content-length"] =
        QuicTextUtils::Uint64ToString(response_body_.size());
    // This method must call CloseReadSide to cause the test case, StopReading
    // is not sufficient.
    QuicStreamPeer::CloseReadSide(this);
    SendHeadersAndBody(std::move(headers), response_body_);
  }

  QuicString response_body_;
};

class StreamWithErrorFactory : public QuicTestServer::StreamFactory {
 public:
  explicit StreamWithErrorFactory(QuicString response_body)
      : response_body_(std::move(response_body)) {}

  ~StreamWithErrorFactory() override = default;

  QuicSimpleServerStream* CreateStream(
      QuicStreamId id,
      QuicSpdySession* session,
      QuicSimpleServerBackend* quic_simple_server_backend) override {
    return new ServerStreamWithErrorResponseBody(
        id, session, quic_simple_server_backend, response_body_);
  }

 private:
  QuicString response_body_;
};

// A test server stream that drops all received body.
class ServerStreamThatDropsBody : public QuicSimpleServerStream {
 public:
  ServerStreamThatDropsBody(QuicStreamId id,
                            QuicSpdySession* session,
                            QuicSimpleServerBackend* quic_simple_server_backend)
      : QuicSimpleServerStream(id,
                               session,
                               BIDIRECTIONAL,
                               quic_simple_server_backend) {}

  ~ServerStreamThatDropsBody() override = default;

 protected:
  void OnDataAvailable() override {
    while (HasBytesToRead()) {
      struct iovec iov;
      if (GetReadableRegions(&iov, 1) == 0) {
        // No more data to read.
        break;
      }
      QUIC_DVLOG(1) << "Processed " << iov.iov_len << " bytes for stream "
                    << id();
      MarkConsumed(iov.iov_len);
    }

    if (!sequencer()->IsClosed()) {
      sequencer()->SetUnblocked();
      return;
    }

    // If the sequencer is closed, then all the body, including the fin, has
    // been consumed.
    OnFinRead();

    if (write_side_closed() || fin_buffered()) {
      return;
    }

    SendResponse();
  }
};

class ServerStreamThatDropsBodyFactory : public QuicTestServer::StreamFactory {
 public:
  ServerStreamThatDropsBodyFactory() = default;

  ~ServerStreamThatDropsBodyFactory() override = default;

  QuicSimpleServerStream* CreateStream(
      QuicStreamId id,
      QuicSpdySession* session,
      QuicSimpleServerBackend* quic_simple_server_backend) override {
    return new ServerStreamThatDropsBody(id, session,
                                         quic_simple_server_backend);
  }
};

// A test server stream that sends response with body size greater than 4GB.
class ServerStreamThatSendsHugeResponse : public QuicSimpleServerStream {
 public:
  ServerStreamThatSendsHugeResponse(
      QuicStreamId id,
      QuicSpdySession* session,
      QuicSimpleServerBackend* quic_simple_server_backend,
      int64_t body_bytes)
      : QuicSimpleServerStream(id,
                               session,
                               BIDIRECTIONAL,
                               quic_simple_server_backend),
        body_bytes_(body_bytes) {}

  ~ServerStreamThatSendsHugeResponse() override = default;

 protected:
  void SendResponse() override {
    QuicBackendResponse response;
    QuicString body(body_bytes_, 'a');
    response.set_body(body);
    SendHeadersAndBodyAndTrailers(response.headers().Clone(), response.body(),
                                  response.trailers().Clone());
  }

 private:
  // Use a explicit int64_t rather than size_t to simulate a 64-bit server
  // talking to a 32-bit client.
  int64_t body_bytes_;
};

class ServerStreamThatSendsHugeResponseFactory
    : public QuicTestServer::StreamFactory {
 public:
  explicit ServerStreamThatSendsHugeResponseFactory(int64_t body_bytes)
      : body_bytes_(body_bytes) {}

  ~ServerStreamThatSendsHugeResponseFactory() override = default;

  QuicSimpleServerStream* CreateStream(
      QuicStreamId id,
      QuicSpdySession* session,
      QuicSimpleServerBackend* quic_simple_server_backend) override {
    return new ServerStreamThatSendsHugeResponse(
        id, session, quic_simple_server_backend, body_bytes_);
  }

  int64_t body_bytes_;
};

TEST_P(EndToEndTest, EarlyResponseFinRecording) {
  set_smaller_flow_control_receive_window();

  // Verify that an incoming FIN is recorded in a stream object even if the read
  // side has been closed.  This prevents an entry from being made in
  // locally_close_streams_highest_offset_ (which will never be deleted).
  // To set up the test condition, the server must do the following in order:
  // start sending the response and call CloseReadSide
  // receive the FIN of the request
  // send the FIN of the response

  // The response body must be larger than the flow control window so the server
  // must receive a window update from the client before it can finish sending
  // it.
  uint32_t response_body_size =
      2 * client_config_.GetInitialStreamFlowControlWindowToSend();
  QuicString response_body(response_body_size, 'a');

  StreamWithErrorFactory stream_factory(response_body);
  SetSpdyStreamFactory(&stream_factory);

  ASSERT_TRUE(Initialize());

  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());

  // A POST that gets an early error response, after the headers are received
  // and before the body is received, due to invalid content-length.
  // Set an invalid content-length, so the request will receive an early 500
  // response.
  SpdyHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/garbage";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;
  headers["content-length"] = "-1";

  // The body must be large enough that the FIN will be in a different packet
  // than the end of the headers, but short enough to not require a flow control
  // update.  This allows headers processing to trigger the error response
  // before the request FIN is processed but receive the request FIN before the
  // response is sent completely.
  const uint32_t kRequestBodySize = kMaxPacketSize + 10;
  QuicString request_body(kRequestBodySize, 'a');

  // Send the request.
  client_->SendMessage(headers, request_body);
  client_->WaitForResponse();
  EXPECT_EQ("500", client_->response_headers()->find(":status")->second);

  // Pause the server so we can access the server's internals without races.
  server_thread_->Pause();

  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  QuicDispatcher::SessionMap const& map =
      QuicDispatcherPeer::session_map(dispatcher);
  auto it = map.begin();
  EXPECT_TRUE(it != map.end());
  QuicSession* server_session = it->second.get();

  // The stream is not waiting for the arrival of the peer's final offset.
  EXPECT_EQ(
      0u, QuicSessionPeer::GetLocallyClosedStreamsHighestOffset(server_session)
              .size());

  server_thread_->Resume();
}

TEST_P(EndToEndTestWithTls, Trailers) {
  // Test sending and receiving HTTP/2 Trailers (trailing HEADERS frames).
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());

  // Set reordering to ensure that Trailers arriving before body is ok.
  SetPacketSendDelay(QuicTime::Delta::FromMilliseconds(2));
  SetReorderPercentage(30);

  // Add a response with headers, body, and trailers.
  const QuicString kBody = "body content";

  SpdyHeaderBlock headers;
  headers[":status"] = "200";
  headers[":version"] = "HTTP/1.1";
  headers["content-length"] = QuicTextUtils::Uint64ToString(kBody.size());

  SpdyHeaderBlock trailers;
  trailers["some-trailing-header"] = "trailing-header-value";

  memory_cache_backend_.AddResponse(server_hostname_, "/trailer_url",
                                    std::move(headers), kBody,
                                    trailers.Clone());

  EXPECT_EQ(kBody, client_->SendSynchronousRequest("/trailer_url"));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);
  EXPECT_EQ(trailers, client_->response_trailers());
}

class EndToEndTestServerPush : public EndToEndTest {
 protected:
  const size_t kNumMaxStreams = 10;

  EndToEndTestServerPush() : EndToEndTest() {
    client_config_.SetMaxIncomingDynamicStreamsToSend(kNumMaxStreams);
    server_config_.SetMaxIncomingDynamicStreamsToSend(kNumMaxStreams);
    support_server_push_ = true;
  }

  // Add a request with its response and |num_resources| push resources into
  // cache.
  // If |resource_size| == 0, response body of push resources use default string
  // concatenating with resource url. Otherwise, generate a string of
  // |resource_size| as body.
  void AddRequestAndResponseWithServerPush(QuicString host,
                                           QuicString path,
                                           QuicString response_body,
                                           QuicString* push_urls,
                                           const size_t num_resources,
                                           const size_t resource_size) {
    bool use_large_response = resource_size != 0;
    QuicString large_resource;
    if (use_large_response) {
      // Generate a response common body larger than flow control window for
      // push response.
      large_resource = QuicString(resource_size, 'a');
    }
    std::list<QuicBackendResponse::ServerPushInfo> push_resources;
    for (size_t i = 0; i < num_resources; ++i) {
      QuicString url = push_urls[i];
      QuicUrl resource_url(url);
      QuicString body =
          use_large_response
              ? large_resource
              : QuicStrCat("This is server push response body for ", url);
      SpdyHeaderBlock response_headers;
      response_headers[":version"] = "HTTP/1.1";
      response_headers[":status"] = "200";
      response_headers["content-length"] =
          QuicTextUtils::Uint64ToString(body.size());
      push_resources.push_back(QuicBackendResponse::ServerPushInfo(
          resource_url, std::move(response_headers), kV3LowestPriority, body));
    }

    memory_cache_backend_.AddSimpleResponseWithServerPushResources(
        host, path, 200, response_body, push_resources);
  }
};

// Run all server push end to end tests with all supported versions.
INSTANTIATE_TEST_CASE_P(EndToEndTestsServerPush,
                        EndToEndTestServerPush,
                        ::testing::ValuesIn(GetTestParams(false, false)));

TEST_P(EndToEndTestServerPush, ServerPush) {
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());

  // Set reordering to ensure that body arriving before PUSH_PROMISE is ok.
  SetPacketSendDelay(QuicTime::Delta::FromMilliseconds(2));
  SetReorderPercentage(30);

  // Add a response with headers, body, and push resources.
  const QuicString kBody = "body content";
  size_t kNumResources = 4;
  QuicString push_urls[] = {"https://example.com/font.woff",
                            "https://example.com/script.js",
                            "https://fonts.example.com/font.woff",
                            "https://example.com/logo-hires.jpg"};
  AddRequestAndResponseWithServerPush("example.com", "/push_example", kBody,
                                      push_urls, kNumResources, 0);

  client_->client()->set_response_listener(
      std::unique_ptr<QuicSpdyClientBase::ResponseListener>(
          new TestResponseListener));

  QUIC_DVLOG(1) << "send request for /push_example";
  EXPECT_EQ(kBody, client_->SendSynchronousRequest(
                       "https://example.com/push_example"));
  QuicHeadersStream* headers_stream = QuicSpdySessionPeer::GetHeadersStream(
      client_->client()->client_session());
  QuicStreamSequencer* sequencer = QuicStreamPeer::sequencer(headers_stream);
  // Headers stream's sequencer buffer shouldn't be released because server push
  // hasn't finished yet.
  EXPECT_TRUE(QuicStreamSequencerPeer::IsUnderlyingBufferAllocated(sequencer));

  for (const QuicString& url : push_urls) {
    QUIC_DVLOG(1) << "send request for pushed stream on url " << url;
    QuicString expected_body =
        QuicStrCat("This is server push response body for ", url);
    QuicString response_body = client_->SendSynchronousRequest(url);
    QUIC_DVLOG(1) << "response body " << response_body;
    EXPECT_EQ(expected_body, response_body);
  }
  EXPECT_FALSE(QuicStreamSequencerPeer::IsUnderlyingBufferAllocated(sequencer));
}

TEST_P(EndToEndTestServerPush, ServerPushUnderLimit) {
  // Tests that sending a request which has 4 push resources will trigger server
  // to push those 4 resources and client can handle pushed resources and match
  // them with requests later.
  ASSERT_TRUE(Initialize());

  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());

  // Set reordering to ensure that body arriving before PUSH_PROMISE is ok.
  SetPacketSendDelay(QuicTime::Delta::FromMilliseconds(2));
  SetReorderPercentage(30);

  // Add a response with headers, body, and push resources.
  const QuicString kBody = "body content";
  size_t const kNumResources = 4;
  QuicString push_urls[] = {
      "https://example.com/font.woff", "https://example.com/script.js",
      "https://fonts.example.com/font.woff",
      "https://example.com/logo-hires.jpg",
  };
  AddRequestAndResponseWithServerPush("example.com", "/push_example", kBody,
                                      push_urls, kNumResources, 0);
  client_->client()->set_response_listener(
      std::unique_ptr<QuicSpdyClientBase::ResponseListener>(
          new TestResponseListener));

  // Send the first request: this will trigger the server to send all the push
  // resources associated with this request, and these will be cached by the
  // client.
  EXPECT_EQ(kBody, client_->SendSynchronousRequest(
                       "https://example.com/push_example"));

  for (const QuicString& url : push_urls) {
    // Sending subsequent requesets will not actually send anything on the wire,
    // as the responses are already in the client's cache.
    QUIC_DVLOG(1) << "send request for pushed stream on url " << url;
    QuicString expected_body =
        QuicStrCat("This is server push response body for ", url);
    QuicString response_body = client_->SendSynchronousRequest(url);
    QUIC_DVLOG(1) << "response body " << response_body;
    EXPECT_EQ(expected_body, response_body);
  }
  // Expect only original request has been sent and push responses have been
  // received as normal response.
  EXPECT_EQ(1u, client_->num_requests());
  EXPECT_EQ(1u + kNumResources, client_->num_responses());
}

TEST_P(EndToEndTestServerPush, ServerPushOverLimitNonBlocking) {
  // Tests that when streams are not blocked by flow control or congestion
  // control, pushing even more resources than max number of open outgoing
  // streams should still work because all response streams get closed
  // immediately after pushing resources.
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());

  // Set reordering to ensure that body arriving before PUSH_PROMISE is ok.
  SetPacketSendDelay(QuicTime::Delta::FromMilliseconds(2));
  SetReorderPercentage(30);

  // Add a response with headers, body, and push resources.
  const QuicString kBody = "body content";

  // One more resource than max number of outgoing stream of this session.
  const size_t kNumResources = 1 + kNumMaxStreams;  // 11.
  QuicString push_urls[11];
  for (size_t i = 0; i < kNumResources; ++i) {
    push_urls[i] = QuicStrCat("https://example.com/push_resources", i);
  }
  AddRequestAndResponseWithServerPush("example.com", "/push_example", kBody,
                                      push_urls, kNumResources, 0);
  client_->client()->set_response_listener(
      std::unique_ptr<QuicSpdyClientBase::ResponseListener>(
          new TestResponseListener));

  // Send the first request: this will trigger the server to send all the push
  // resources associated with this request, and these will be cached by the
  // client.
  EXPECT_EQ(kBody, client_->SendSynchronousRequest(
                       "https://example.com/push_example"));

  for (const QuicString& url : push_urls) {
    // Sending subsequent requesets will not actually send anything on the wire,
    // as the responses are already in the client's cache.
    EXPECT_EQ(QuicStrCat("This is server push response body for ", url),
              client_->SendSynchronousRequest(url));
  }

  // Only 1 request should have been sent.
  EXPECT_EQ(1u, client_->num_requests());
  // The responses to the original request and all the promised resources
  // should have been received.
  EXPECT_EQ(12u, client_->num_responses());
}

TEST_P(EndToEndTestServerPush, ServerPushOverLimitWithBlocking) {
  // Tests that when server tries to send more large resources(large enough to
  // be blocked by flow control window or congestion control window) than max
  // open outgoing streams , server can open upto max number of outgoing
  // streams for them, and the rest will be queued up.

  // Reset flow control windows.
  size_t kFlowControlWnd = 20 * 1024;  // 20KB.
  // Response body is larger than 1 flow controlblock window.
  size_t kBodySize = kFlowControlWnd * 2;
  set_client_initial_stream_flow_control_receive_window(kFlowControlWnd);
  // Make sure conntection level flow control window is large enough not to
  // block data being sent out though they will be blocked by stream level one.
  set_client_initial_session_flow_control_receive_window(
      kBodySize * kNumMaxStreams + 1024);

  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());

  // Set reordering to ensure that body arriving before PUSH_PROMISE is ok.
  SetPacketSendDelay(QuicTime::Delta::FromMilliseconds(2));
  SetReorderPercentage(30);

  // Add a response with headers, body, and push resources.
  const QuicString kBody = "body content";

  const size_t kNumResources = kNumMaxStreams + 1;
  QuicString push_urls[11];
  for (size_t i = 0; i < kNumResources; ++i) {
    push_urls[i] = QuicStrCat("http://example.com/push_resources", i);
  }
  AddRequestAndResponseWithServerPush("example.com", "/push_example", kBody,
                                      push_urls, kNumResources, kBodySize);

  client_->client()->set_response_listener(
      std::unique_ptr<QuicSpdyClientBase::ResponseListener>(
          new TestResponseListener));

  client_->SendRequest("https://example.com/push_example");

  // Pause after the first response arrives.
  while (!client_->response_complete()) {
    // Because of priority, the first response arrived should be to original
    // request.
    client_->WaitForResponse();
  }

  // Check server session to see if it has max number of outgoing streams opened
  // though more resources need to be pushed.
  server_thread_->Pause();
  EXPECT_EQ(kNumMaxStreams, GetServerSession()->GetNumOpenOutgoingStreams());
  server_thread_->Resume();

  EXPECT_EQ(1u, client_->num_requests());
  EXPECT_EQ(1u, client_->num_responses());
  EXPECT_EQ(kBody, client_->response_body());

  // "Send" request for a promised resources will not really send out it because
  // its response is being pushed(but blocked). And the following ack and
  // flow control behavior of SendSynchronousRequests()
  // will unblock the stream to finish receiving response.
  client_->SendSynchronousRequest(push_urls[0]);
  EXPECT_EQ(1u, client_->num_requests());
  EXPECT_EQ(2u, client_->num_responses());

  // Do same thing for the rest 10 resources.
  for (size_t i = 1; i < kNumResources; ++i) {
    client_->SendSynchronousRequest(push_urls[i]);
  }

  // Because of server push, client gets all pushed resources without actually
  // sending requests for them.
  EXPECT_EQ(1u, client_->num_requests());
  // Including response to original request, 12 responses in total were
  // received.
  EXPECT_EQ(12u, client_->num_responses());
}

// TODO(fayang): this test seems to cause net_unittests timeouts :|
TEST_P(EndToEndTest, DISABLED_TestHugePostWithPacketLoss) {
  // This test tests a huge post with introduced packet loss from client to
  // server and body size greater than 4GB, making sure QUIC code does not break
  // for 32-bit builds.
  ServerStreamThatDropsBodyFactory stream_factory;
  SetSpdyStreamFactory(&stream_factory);
  ASSERT_TRUE(Initialize());
  // Set client's epoll server's time out to 0 to make this test be finished
  // within a short time.
  client_->epoll_server()->set_timeout_in_us(0);

  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());
  SetPacketLossPercentage(1);
  // To avoid storing the whole request body in memory, use a loop to repeatedly
  // send body size of kSizeBytes until the whole request body size is reached.
  const int kSizeBytes = 128 * 1024;
  // Request body size is 4G plus one more kSizeBytes.
  int64_t request_body_size_bytes = pow(2, 32) + kSizeBytes;
  ASSERT_LT(INT64_C(4294967296), request_body_size_bytes);
  QuicString body(kSizeBytes, 'a');

  SpdyHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;
  headers["content-length"] =
      QuicTextUtils::Uint64ToString(request_body_size_bytes);

  client_->SendMessage(headers, "", /*fin=*/false);

  for (int i = 0; i < request_body_size_bytes / kSizeBytes; ++i) {
    bool fin = (i == request_body_size_bytes - 1);
    client_->SendData(QuicString(body.data(), kSizeBytes), fin);
    client_->client()->WaitForEvents();
  }
  VerifyCleanConnection(true);
}

// TODO(fayang): this test seems to cause net_unittests timeouts :|
TEST_P(EndToEndTest, DISABLED_TestHugeResponseWithPacketLoss) {
  // This test tests a huge response with introduced loss from server to client
  // and body size greater than 4GB, making sure QUIC code does not break for
  // 32-bit builds.
  const int kSizeBytes = 128 * 1024;
  int64_t response_body_size_bytes = pow(2, 32) + kSizeBytes;
  ASSERT_LT(4294967296, response_body_size_bytes);
  ServerStreamThatSendsHugeResponseFactory stream_factory(
      response_body_size_bytes);
  SetSpdyStreamFactory(&stream_factory);

  StartServer();

  // Use a quic client that drops received body.
  QuicTestClient* client =
      new QuicTestClient(server_address_, server_hostname_, client_config_,
                         client_supported_versions_);
  client->client()->set_drop_response_body(true);
  client->UseWriter(client_writer_);
  client->Connect();
  client_.reset(client);
  static EpollEvent event(EPOLLOUT);
  client_writer_->Initialize(
      QuicConnectionPeer::GetHelper(
          client_->client()->client_session()->connection()),
      QuicConnectionPeer::GetAlarmFactory(
          client_->client()->client_session()->connection()),
      std::make_unique<ClientDelegate>(client_->client()));
  initialized_ = true;
  ASSERT_TRUE(client_->client()->connected());

  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());
  SetPacketLossPercentage(1);
  client_->SendRequest("/huge_response");
  client_->WaitForResponse();
  // TODO(fayang): Fix this test to work with stateless rejects.
  if (!BothSidesSupportStatelessRejects()) {
    VerifyCleanConnection(true);
  }
}

// Regression test for b/111515567
TEST_P(EndToEndTest, AgreeOnStopWaiting) {
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());

  QuicConnection* client_connection =
      client_->client()->client_session()->connection();
  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  // Verify client and server connections agree on the value of
  // no_stop_waiting_frames.
  EXPECT_EQ(QuicConnectionPeer::GetNoStopWaitingFrames(client_connection),
            QuicConnectionPeer::GetNoStopWaitingFrames(server_connection));
  server_thread_->Resume();
}

// Regression test for b/111515567
TEST_P(EndToEndTest, AgreeOnStopWaitingWithNoStopWaitingOption) {
  QuicTagVector options;
  options.push_back(kNSTP);
  client_config_.SetConnectionOptionsToSend(options);
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());

  QuicConnection* client_connection =
      client_->client()->client_session()->connection();
  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  // Verify client and server connections agree on the value of
  // no_stop_waiting_frames.
  EXPECT_EQ(QuicConnectionPeer::GetNoStopWaitingFrames(client_connection),
            QuicConnectionPeer::GetNoStopWaitingFrames(server_connection));
  server_thread_->Resume();
}

TEST_P(EndToEndTest, ReleaseHeadersStreamBufferWhenIdle) {
  // Tests that when client side has no active request and no waiting
  // PUSH_PROMISE, its headers stream's sequencer buffer should be released.
  ASSERT_TRUE(Initialize());
  client_->SendSynchronousRequest("/foo");
  QuicHeadersStream* headers_stream = QuicSpdySessionPeer::GetHeadersStream(
      client_->client()->client_session());
  QuicStreamSequencer* sequencer = QuicStreamPeer::sequencer(headers_stream);
  EXPECT_FALSE(QuicStreamSequencerPeer::IsUnderlyingBufferAllocated(sequencer));
}

TEST_P(EndToEndTest, WayTooLongRequestHeaders) {
  ASSERT_TRUE(Initialize());
  SpdyHeaderBlock headers;
  headers[":method"] = "GET";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;
  headers["key"] = QuicString(64 * 1024, 'a');

  client_->SendMessage(headers, "");
  client_->WaitForResponse();
  EXPECT_EQ(QUIC_HEADERS_STREAM_DATA_DECOMPRESS_FAILURE,
            client_->connection_error());
}

class WindowUpdateObserver : public QuicConnectionDebugVisitor {
 public:
  WindowUpdateObserver() : num_window_update_frames_(0), num_ping_frames_(0) {}

  size_t num_window_update_frames() const { return num_window_update_frames_; }

  size_t num_ping_frames() const { return num_ping_frames_; }

  void OnWindowUpdateFrame(const QuicWindowUpdateFrame& frame,
                           const QuicTime& receive_time) override {
    ++num_window_update_frames_;
  }

  void OnPingFrame(const QuicPingFrame& frame) override { ++num_ping_frames_; }

 private:
  size_t num_window_update_frames_;
  size_t num_ping_frames_;
};

TEST_P(EndToEndTest, WindowUpdateInAck) {
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());
  WindowUpdateObserver observer;
  QuicConnection* client_connection =
      client_->client()->client_session()->connection();
  client_connection->set_debug_visitor(&observer);
  QuicTransportVersion version = client_connection->transport_version();
  // 100KB body.
  QuicString body(100 * 1024, 'a');
  SpdyHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;

  EXPECT_EQ(kFooResponseBody,
            client_->SendCustomSynchronousRequest(headers, body));
  client_->Disconnect();
  if (version != QUIC_VERSION_35) {
    EXPECT_LT(0u, observer.num_window_update_frames());
    EXPECT_EQ(0u, observer.num_ping_frames());
  } else {
    EXPECT_EQ(0u, observer.num_window_update_frames());
  }
}

TEST_P(EndToEndTest, SendStatelessResetTokenInShlo) {
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());
  QuicConfig* config = client_->client()->session()->config();
  EXPECT_TRUE(config->HasReceivedStatelessResetToken());
  EXPECT_EQ(client_->client()->session()->connection()->connection_id(),
            config->ReceivedStatelessResetToken());
  client_->Disconnect();
}

// Regression test of b/70782529.
TEST_P(EndToEndTest, DoNotCrashOnPacketWriteError) {
  ASSERT_TRUE(Initialize());
  BadPacketWriter* bad_writer =
      new BadPacketWriter(/*packet_causing_write_error=*/5,
                          /*error_code=*/90);
  std::unique_ptr<QuicTestClient> client(CreateQuicClient(bad_writer));

  // 1 MB body.
  QuicString body(1024 * 1024, 'a');
  SpdyHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;

  client->SendCustomSynchronousRequest(headers, body);
}

// Regression test for b/71711996. This test sends a connectivity probing packet
// as its last sent packet, and makes sure the server's ACK of that packet does
// not cause the client to fail.
TEST_P(EndToEndTest, LastPacketSentIsConnectivityProbing) {
  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);

  // Wait for the client's ACK (of the response) to be received by the server.
  client_->WaitForDelayedAcks();

  // We are sending a connectivity probing packet from an unchanged client
  // address, so the server will not respond to us with a connectivity probing
  // packet, however the server should send an ack-only packet to us.
  client_->SendConnectivityProbing();

  // Wait for the server's last ACK to be received by the client.
  client_->WaitForDelayedAcks();
}

TEST_P(EndToEndTest, PreSharedKey) {
  client_config_.set_max_time_before_crypto_handshake(
      QuicTime::Delta::FromSeconds(1));
  client_config_.set_max_idle_time_before_crypto_handshake(
      QuicTime::Delta::FromSeconds(1));
  pre_shared_key_client_ = "foobar";
  pre_shared_key_server_ = "foobar";
  ASSERT_TRUE(Initialize());

  ASSERT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  EXPECT_EQ("200", client_->response_headers()->find(":status")->second);
}

// TODO: reenable once we have a way to make this run faster.
TEST_P(EndToEndTest, DISABLED_PreSharedKeyMismatch) {
  client_config_.set_max_time_before_crypto_handshake(
      QuicTime::Delta::FromSeconds(1));
  client_config_.set_max_idle_time_before_crypto_handshake(
      QuicTime::Delta::FromSeconds(1));
  pre_shared_key_client_ = "foo";
  pre_shared_key_server_ = "bar";
  // One of two things happens when Initialize() returns:
  // 1. Crypto handshake has completed, and it is unsuccessful. Initialize()
  //    returns false.
  // 2. Crypto handshake has not completed, Initialize() returns true. The call
  //    to WaitForCryptoHandshakeConfirmed() will wait for the handshake and
  //    return whether it is successful.
  ASSERT_FALSE(Initialize() &&
               client_->client()->WaitForCryptoHandshakeConfirmed());
  EXPECT_EQ(QUIC_HANDSHAKE_TIMEOUT, client_->connection_error());
}

// TODO: reenable once we have a way to make this run faster.
TEST_P(EndToEndTest, DISABLED_PreSharedKeyNoClient) {
  client_config_.set_max_time_before_crypto_handshake(
      QuicTime::Delta::FromSeconds(1));
  client_config_.set_max_idle_time_before_crypto_handshake(
      QuicTime::Delta::FromSeconds(1));
  pre_shared_key_server_ = "foobar";
  ASSERT_FALSE(Initialize() &&
               client_->client()->WaitForCryptoHandshakeConfirmed());
  EXPECT_EQ(QUIC_HANDSHAKE_TIMEOUT, client_->connection_error());
}

// TODO: reenable once we have a way to make this run faster.
TEST_P(EndToEndTest, DISABLED_PreSharedKeyNoServer) {
  client_config_.set_max_time_before_crypto_handshake(
      QuicTime::Delta::FromSeconds(1));
  client_config_.set_max_idle_time_before_crypto_handshake(
      QuicTime::Delta::FromSeconds(1));
  pre_shared_key_client_ = "foobar";
  ASSERT_FALSE(Initialize() &&
               client_->client()->WaitForCryptoHandshakeConfirmed());
  EXPECT_EQ(QUIC_HANDSHAKE_TIMEOUT, client_->connection_error());
}

TEST_P(EndToEndTest, RequestAndStreamRstInOnePacket) {
  // Regression test for b/80234898.
  ASSERT_TRUE(Initialize());

  // INCOMPLETE_RESPONSE will cause the server to not to send the trailer
  // (and the FIN) after the response body.
  QuicString response_body(1305, 'a');
  SpdyHeaderBlock response_headers;
  response_headers[":status"] = QuicTextUtils::Uint64ToString(200);
  response_headers["content-length"] =
      QuicTextUtils::Uint64ToString(response_body.length());
  memory_cache_backend_.AddSpecialResponse(
      server_hostname_, "/test_url", std::move(response_headers), response_body,
      QuicBackendResponse::INCOMPLETE_RESPONSE);

  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());
  client_->WaitForDelayedAcks();

  QuicSession* session = client_->client()->client_session();
  const QuicPacketCount packets_sent_before =
      session->connection()->GetStats().packets_sent;

  client_->SendRequestAndRstTogether("/test_url");

  // Expect exactly one packet is sent from the block above.
  ASSERT_EQ(packets_sent_before + 1,
            session->connection()->GetStats().packets_sent);

  // Wait for the connection to become idle.
  client_->WaitForDelayedAcks();

  // The real expectation is the test does not crash or timeout.
  EXPECT_EQ(QUIC_NO_ERROR, client_->connection_error());
}

TEST_P(EndToEndTest, ResetStreamOnTtlExpires) {
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());
  SetPacketLossPercentage(30);

  QuicSpdyClientStream* stream = client_->GetOrCreateStream();
  // Set a TTL which expires immediately.
  stream->MaybeSetTtl(QuicTime::Delta::FromMicroseconds(1));

  // 1 MB body.
  QuicString body(1024 * 1024, 'a');
  stream->WriteOrBufferBody(body, true, nullptr);
  client_->WaitForResponse();
  EXPECT_EQ(QUIC_STREAM_TTL_EXPIRED, client_->stream_error());
}

TEST_P(EndToEndTest, SendMessages) {
  SetQuicReloadableFlag(quic_fix_mark_for_loss_retransmission, true);
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForCryptoHandshakeConfirmed());
  QuicSession* client_session = client_->client()->client_session();
  QuicConnection* client_connection = client_session->connection();
  if (client_connection->transport_version() <= QUIC_VERSION_44) {
    return;
  }

  SetPacketLossPercentage(30);
  ASSERT_GT(kMaxPacketSize, client_session->GetLargestMessagePayload());
  ASSERT_LT(0, client_session->GetLargestMessagePayload());

  QuicString message_string(kMaxPacketSize, 'a');
  QuicStringPiece message_buffer(message_string);
  QuicRandom* random =
      QuicConnectionPeer::GetHelper(client_connection)->GetRandomGenerator();
  {
    QuicConnection::ScopedPacketFlusher flusher(
        client_session->connection(), QuicConnection::SEND_ACK_IF_PENDING);
    // Verify the largest message gets successfully sent.
    EXPECT_EQ(MessageResult(MESSAGE_STATUS_SUCCESS, 1),
              client_session->SendMessage(
                  QuicStringPiece(message_buffer.data(),
                                  client_session->GetLargestMessagePayload())));
    // Send more messages with size (0, largest_payload] until connection is
    // write blocked.
    const int kTestMaxNumberOfMessages = 100;
    for (size_t i = 2; i <= kTestMaxNumberOfMessages; ++i) {
      size_t message_length =
          random->RandUint64() % client_session->GetLargestMessagePayload() + 1;
      MessageResult result = client_session->SendMessage(
          QuicStringPiece(message_buffer.data(), message_length));
      if (result.status == MESSAGE_STATUS_BLOCKED) {
        // Connection is write blocked.
        break;
      }
      EXPECT_EQ(MessageResult(MESSAGE_STATUS_SUCCESS, i), result);
    }
  }

  client_->WaitForDelayedAcks();
  EXPECT_EQ(MESSAGE_STATUS_TOO_LARGE,
            client_session
                ->SendMessage(QuicStringPiece(
                    message_buffer.data(),
                    client_session->GetLargestMessagePayload() + 1))
                .status);
  EXPECT_EQ(QUIC_NO_ERROR, client_->connection_error());
}

class EndToEndPacketReorderingTest : public EndToEndTest {
 public:
  void CreateClientWithWriter() override {
    QUIC_LOG(ERROR) << "create client with reorder_writer_";
    reorder_writer_ = new PacketReorderingWriter();
    client_.reset(EndToEndTest::CreateQuicClient(reorder_writer_));
  }

  void SetUp() override {
    // Don't initialize client writer in base class.
    server_writer_ = new PacketDroppingTestWriter();
  }

 protected:
  PacketReorderingWriter* reorder_writer_;
};

INSTANTIATE_TEST_CASE_P(EndToEndPacketReorderingTests,
                        EndToEndPacketReorderingTest,
                        testing::ValuesIn(GetTestParams(false, false)));

TEST_P(EndToEndPacketReorderingTest, ReorderedConnectivityProbing) {
  ASSERT_TRUE(Initialize());

  // Finish one request to make sure handshake established.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));

  // Wait for the connection to become idle, to make sure the packet gets
  // delayed is the connectivity probing packet.
  client_->WaitForDelayedAcks();

  QuicSocketAddress old_addr =
      client_->client()->network_helper()->GetLatestClientAddress();

  // Migrate socket to the new IP address.
  QuicIpAddress new_host = TestLoopback(2);
  EXPECT_NE(old_addr.host(), new_host);
  ASSERT_TRUE(client_->client()->MigrateSocket(new_host));

  // Write a connectivity probing after the next /foo request.
  reorder_writer_->SetDelay(1);
  client_->SendConnectivityProbing();

  ASSERT_TRUE(client_->MigrateSocketWithSpecifiedPort(old_addr.host(),
                                                      old_addr.port()));

  // The (delayed) connectivity probing will be sent after this request.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));

  // Send yet another request after the connectivity probing, when this request
  // returns, the probing is guaranteed to have been received by the server, and
  // the server's response to probing is guaranteed to have been received by the
  // client.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));

  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  EXPECT_EQ(1u,
            server_connection->GetStats().num_connectivity_probing_received);
  server_thread_->Resume();

  QuicConnection* client_connection =
      client_->client()->client_session()->connection();
  EXPECT_EQ(1u,
            client_connection->GetStats().num_connectivity_probing_received);
}

TEST_P(EndToEndPacketReorderingTest, Buffer0RttRequest) {
  ASSERT_TRUE(Initialize());
  // Finish one request to make sure handshake established.
  client_->SendSynchronousRequest("/foo");
  // Disconnect for next 0-rtt request.
  client_->Disconnect();

  // Client get valid STK now. Do a 0-rtt request.
  // Buffer a CHLO till another packets sent out.
  reorder_writer_->SetDelay(1);
  // Only send out a CHLO.
  client_->client()->Initialize();
  client_->client()->StartConnect();
  ASSERT_TRUE(client_->client()->connected());

  // Send a request before handshake finishes.
  SpdyHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/bar";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;

  client_->SendMessage(headers, "");
  client_->WaitForResponse();
  EXPECT_EQ(kBarResponseBody, client_->response_body());
  QuicConnectionStats client_stats =
      client_->client()->client_session()->connection()->GetStats();
  EXPECT_EQ(0u, client_stats.packets_lost);
  EXPECT_EQ(1, client_->client()->GetNumSentClientHellos());
}

}  // namespace
}  // namespace test
}  // namespace quic
