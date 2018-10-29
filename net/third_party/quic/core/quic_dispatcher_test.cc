// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/quic_dispatcher.h"

#include <memory>
#include <ostream>

#include "base/macros.h"
#include "net/third_party/quic/core/chlo_extractor.h"
#include "net/third_party/quic/core/crypto/crypto_handshake.h"
#include "net/third_party/quic/core/crypto/crypto_protocol.h"
#include "net/third_party/quic/core/crypto/quic_crypto_server_config.h"
#include "net/third_party/quic/core/crypto/quic_random.h"
#include "net/third_party/quic/core/quic_crypto_stream.h"
#include "net/third_party/quic/core/quic_packet_writer_wrapper.h"
#include "net/third_party/quic/core/quic_time_wait_list_manager.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/core/stateless_rejector.h"
#include "net/third_party/quic/core/tls_server_handshaker.h"
#include "net/third_party/quic/platform/api/quic_arraysize.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_str_cat.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quic/test_tools/fake_proof_source.h"
#include "net/third_party/quic/test_tools/mock_quic_time_wait_list_manager.h"
#include "net/third_party/quic/test_tools/quic_buffered_packet_store_peer.h"
#include "net/third_party/quic/test_tools/quic_crypto_server_config_peer.h"
#include "net/third_party/quic/test_tools/quic_dispatcher_peer.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quic/test_tools/quic_time_wait_list_manager_peer.h"
#include "net/third_party/quic/tools/quic_simple_crypto_server_stream_helper.h"

using testing::_;
using testing::InSequence;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::WithArg;
using testing::WithoutArgs;

static const size_t kDefaultMaxConnectionsInStore = 100;
static const size_t kMaxConnectionsWithoutCHLO =
    kDefaultMaxConnectionsInStore / 2;
static const int16_t kMaxNumSessionsToCreate = 16;

namespace quic {
namespace test {
namespace {

class TestQuicSpdyServerSession : public QuicServerSessionBase {
 public:
  TestQuicSpdyServerSession(const QuicConfig& config,
                            QuicConnection* connection,
                            const QuicCryptoServerConfig* crypto_config,
                            QuicCompressedCertsCache* compressed_certs_cache)
      : QuicServerSessionBase(config,
                              CurrentSupportedVersions(),
                              connection,
                              nullptr,
                              nullptr,
                              crypto_config,
                              compressed_certs_cache),
        crypto_stream_(QuicServerSessionBase::GetMutableCryptoStream()) {}
  TestQuicSpdyServerSession(const TestQuicSpdyServerSession&) = delete;
  TestQuicSpdyServerSession& operator=(const TestQuicSpdyServerSession&) =
      delete;

  ~TestQuicSpdyServerSession() override { delete connection(); };

  MOCK_METHOD3(OnConnectionClosed,
               void(QuicErrorCode error,
                    const QuicString& error_details,
                    ConnectionCloseSource source));
  MOCK_METHOD1(CreateIncomingStream, QuicSpdyStream*(QuicStreamId id));
  MOCK_METHOD0(CreateOutgoingBidirectionalStream, QuicSpdyStream*());
  MOCK_METHOD0(CreateOutgoingUnidirectionalStream, QuicSpdyStream*());

  QuicCryptoServerStreamBase* CreateQuicCryptoServerStream(
      const QuicCryptoServerConfig* crypto_config,
      QuicCompressedCertsCache* compressed_certs_cache) override {
    return new QuicCryptoServerStream(
        crypto_config, compressed_certs_cache,
        GetQuicReloadableFlag(enable_quic_stateless_reject_support), this,
        stream_helper());
  }

  void SetCryptoStream(QuicCryptoServerStream* crypto_stream) {
    crypto_stream_ = crypto_stream;
  }

  QuicCryptoServerStreamBase* GetMutableCryptoStream() override {
    return crypto_stream_;
  }

  const QuicCryptoServerStreamBase* GetCryptoStream() const override {
    return crypto_stream_;
  }

  QuicCryptoServerStream::Helper* stream_helper() {
    return QuicServerSessionBase::stream_helper();
  }

 private:
  QuicCryptoServerStreamBase* crypto_stream_;
};

class TestDispatcher : public QuicDispatcher {
 public:
  TestDispatcher(const QuicConfig& config,
                 const QuicCryptoServerConfig* crypto_config,
                 QuicVersionManager* version_manager)
      : QuicDispatcher(config,
                       crypto_config,
                       version_manager,
                       QuicMakeUnique<MockQuicConnectionHelper>(),
                       std::unique_ptr<QuicCryptoServerStream::Helper>(
                           new QuicSimpleCryptoServerStreamHelper(
                               QuicRandom::GetInstance())),
                       QuicMakeUnique<MockAlarmFactory>()) {}

  MOCK_METHOD3(CreateQuicSession,
               QuicServerSessionBase*(QuicConnectionId connection_id,
                                      const QuicSocketAddress& peer_address,
                                      QuicStringPiece alpn));

  MOCK_METHOD1(ShouldCreateOrBufferPacketForConnection,
               bool(QuicConnectionId connection_id));

  struct TestQuicPerPacketContext : public PerPacketContext {
    QuicString custom_packet_context;
  };

  std::unique_ptr<PerPacketContext> GetPerPacketContext() const override {
    auto test_context = QuicMakeUnique<TestQuicPerPacketContext>();
    test_context->custom_packet_context = custom_packet_context_;
    return std::move(test_context);
  }

  void RestorePerPacketContext(
      std::unique_ptr<PerPacketContext> context) override {
    TestQuicPerPacketContext* test_context =
        static_cast<TestQuicPerPacketContext*>(context.get());
    custom_packet_context_ = test_context->custom_packet_context;
  }

  QuicString custom_packet_context_;

  using QuicDispatcher::current_client_address;
  using QuicDispatcher::current_peer_address;
  using QuicDispatcher::current_self_address;
  using QuicDispatcher::framer;
};

// A Connection class which unregisters the session from the dispatcher when
// sending connection close.
// It'd be slightly more realistic to do this from the Session but it would
// involve a lot more mocking.
class MockServerConnection : public MockQuicConnection {
 public:
  MockServerConnection(QuicConnectionId connection_id,
                       MockQuicConnectionHelper* helper,
                       MockAlarmFactory* alarm_factory,
                       QuicDispatcher* dispatcher)
      : MockQuicConnection(connection_id,
                           helper,
                           alarm_factory,
                           Perspective::IS_SERVER),
        dispatcher_(dispatcher) {}

  void UnregisterOnConnectionClosed() {
    QUIC_LOG(ERROR) << "Unregistering " << connection_id();
    dispatcher_->OnConnectionClosed(connection_id(), QUIC_NO_ERROR,
                                    "Unregistering.");
  }

 private:
  QuicDispatcher* dispatcher_;
};

class QuicDispatcherTest : public QuicTest {
 public:
  QuicDispatcherTest()
      : QuicDispatcherTest(crypto_test_utils::ProofSourceForTesting()) {}

  ParsedQuicVersionVector AllSupportedVersionsIncludingTls() {
    SetQuicFlag(&FLAGS_quic_supports_tls_handshake, true);
    return AllSupportedVersions();
  }

  explicit QuicDispatcherTest(std::unique_ptr<ProofSource> proof_source)
      :

        version_manager_(AllSupportedVersionsIncludingTls()),
        crypto_config_(QuicCryptoServerConfig::TESTING,
                       QuicRandom::GetInstance(),
                       std::move(proof_source),
                       KeyExchangeSource::Default(),
                       TlsServerHandshaker::CreateSslCtx()),
        dispatcher_(new NiceMock<TestDispatcher>(config_,
                                                 &crypto_config_,
                                                 &version_manager_)),
        time_wait_list_manager_(nullptr),
        session1_(nullptr),
        session2_(nullptr),
        store_(nullptr) {}

  void SetUp() override {
    dispatcher_->InitializeWithWriter(new MockPacketWriter());
    // Set the counter to some value to start with.
    QuicDispatcherPeer::set_new_sessions_allowed_per_event_loop(
        dispatcher_.get(), kMaxNumSessionsToCreate);
    ON_CALL(*dispatcher_, ShouldCreateOrBufferPacketForConnection(_))
        .WillByDefault(Return(true));
  }

  MockQuicConnection* connection1() {
    return reinterpret_cast<MockQuicConnection*>(session1_->connection());
  }

  MockQuicConnection* connection2() {
    return reinterpret_cast<MockQuicConnection*>(session2_->connection());
  }

  // Process a packet with an 8 byte connection id,
  // 6 byte packet number, default path id, and packet number 1,
  // using the first supported version.
  void ProcessPacket(QuicSocketAddress peer_address,
                     QuicConnectionId connection_id,
                     bool has_version_flag,
                     const QuicString& data) {
    ProcessPacket(peer_address, connection_id, has_version_flag, data,
                  PACKET_8BYTE_CONNECTION_ID, PACKET_4BYTE_PACKET_NUMBER);
  }

  // Process a packet with a default path id, and packet number 1,
  // using the first supported version.
  void ProcessPacket(QuicSocketAddress peer_address,
                     QuicConnectionId connection_id,
                     bool has_version_flag,
                     const QuicString& data,
                     QuicConnectionIdLength connection_id_length,
                     QuicPacketNumberLength packet_number_length) {
    ProcessPacket(peer_address, connection_id, has_version_flag, data,
                  connection_id_length, packet_number_length, 1);
  }

  // Process a packet using the first supported version.
  void ProcessPacket(QuicSocketAddress peer_address,
                     QuicConnectionId connection_id,
                     bool has_version_flag,
                     const QuicString& data,
                     QuicConnectionIdLength connection_id_length,
                     QuicPacketNumberLength packet_number_length,
                     QuicPacketNumber packet_number) {
    ProcessPacket(peer_address, connection_id, has_version_flag,
                  CurrentSupportedVersions().front(), data,
                  connection_id_length, packet_number_length, packet_number);
  }

  // Processes a packet.
  void ProcessPacket(QuicSocketAddress peer_address,
                     QuicConnectionId connection_id,
                     bool has_version_flag,
                     ParsedQuicVersion version,
                     const QuicString& data,
                     QuicConnectionIdLength connection_id_length,
                     QuicPacketNumberLength packet_number_length,
                     QuicPacketNumber packet_number) {
    ParsedQuicVersionVector versions(SupportedVersions(version));
    std::unique_ptr<QuicEncryptedPacket> packet(ConstructEncryptedPacket(
        connection_id, 0, has_version_flag, false, packet_number, data,
        connection_id_length, PACKET_0BYTE_CONNECTION_ID, packet_number_length,
        &versions));
    std::unique_ptr<QuicReceivedPacket> received_packet(
        ConstructReceivedPacket(*packet, mock_helper_.GetClock()->Now()));

    if (ChloExtractor::Extract(*packet, versions, {}, nullptr)) {
      // Add CHLO packet to the beginning to be verified first, because it is
      // also processed first by new session.
      data_connection_map_[connection_id].push_front(
          QuicString(packet->data(), packet->length()));
    } else {
      // For non-CHLO, always append to last.
      data_connection_map_[connection_id].push_back(
          QuicString(packet->data(), packet->length()));
    }
    dispatcher_->ProcessPacket(server_address_, peer_address, *received_packet);
  }

  void ValidatePacket(QuicConnectionId conn_id,
                      const QuicEncryptedPacket& packet) {
    EXPECT_EQ(data_connection_map_[conn_id].front().length(),
              packet.AsStringPiece().length());
    EXPECT_EQ(data_connection_map_[conn_id].front(), packet.AsStringPiece());
    data_connection_map_[conn_id].pop_front();
  }

  QuicServerSessionBase* CreateSession(
      QuicDispatcher* dispatcher,
      const QuicConfig& config,
      QuicConnectionId connection_id,
      const QuicSocketAddress& peer_address,
      MockQuicConnectionHelper* helper,
      MockAlarmFactory* alarm_factory,
      const QuicCryptoServerConfig* crypto_config,
      QuicCompressedCertsCache* compressed_certs_cache,
      TestQuicSpdyServerSession** session) {
    MockServerConnection* connection = new MockServerConnection(
        connection_id, helper, alarm_factory, dispatcher);
    *session = new TestQuicSpdyServerSession(config, connection, crypto_config,
                                             compressed_certs_cache);
    connection->set_visitor(*session);
    ON_CALL(*connection, CloseConnection(_, _, _))
        .WillByDefault(WithoutArgs(Invoke(
            connection, &MockServerConnection::UnregisterOnConnectionClosed)));
    return *session;
  }

  void CreateTimeWaitListManager() {
    time_wait_list_manager_ = new MockTimeWaitListManager(
        QuicDispatcherPeer::GetWriter(dispatcher_.get()), dispatcher_.get(),
        mock_helper_.GetClock(), &mock_alarm_factory_);
    // dispatcher_ takes the ownership of time_wait_list_manager_.
    QuicDispatcherPeer::SetTimeWaitListManager(dispatcher_.get(),
                                               time_wait_list_manager_);
  }

  QuicString SerializeCHLO() {
    CryptoHandshakeMessage client_hello;
    client_hello.set_tag(kCHLO);
    client_hello.SetStringPiece(kALPN, "hq");
    return QuicString(client_hello.GetSerialized().AsStringPiece());
  }

  QuicString SerializeTlsClientHello() { return ""; }

  MockQuicConnectionHelper mock_helper_;
  MockAlarmFactory mock_alarm_factory_;
  QuicConfig config_;
  QuicVersionManager version_manager_;
  QuicCryptoServerConfig crypto_config_;
  QuicSocketAddress server_address_;
  std::unique_ptr<NiceMock<TestDispatcher>> dispatcher_;
  MockTimeWaitListManager* time_wait_list_manager_;
  TestQuicSpdyServerSession* session1_;
  TestQuicSpdyServerSession* session2_;
  std::map<QuicConnectionId, std::list<QuicString>> data_connection_map_;
  QuicBufferedPacketStore* store_;
};

TEST_F(QuicDispatcherTest, TlsClientHelloCreatesSession) {
  FLAGS_quic_supports_tls_handshake = true;
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  server_address_ = QuicSocketAddress(QuicIpAddress::Any4(), 5);

  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(1, client_address, QuicStringPiece("")))
      .WillOnce(testing::Return(CreateSession(
          dispatcher_.get(), config_, 1, client_address, &mock_helper_,
          &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_)));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(1, packet);
      })));
  EXPECT_CALL(*dispatcher_, ShouldCreateOrBufferPacketForConnection(1));
  ProcessPacket(
      client_address, 1, true,
      ParsedQuicVersion(PROTOCOL_TLS1_3,
                        CurrentSupportedVersions().front().transport_version),
      SerializeCHLO(), PACKET_8BYTE_CONNECTION_ID, PACKET_4BYTE_PACKET_NUMBER,
      1);
  EXPECT_EQ(client_address, dispatcher_->current_peer_address());
  EXPECT_EQ(server_address_, dispatcher_->current_self_address());
}

TEST_F(QuicDispatcherTest, ProcessPackets) {
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  server_address_ = QuicSocketAddress(QuicIpAddress::Any4(), 5);

  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(1, client_address, QuicStringPiece("hq")))
      .WillOnce(testing::Return(CreateSession(
          dispatcher_.get(), config_, 1, client_address, &mock_helper_,
          &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_)));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(1, packet);
      })));
  EXPECT_CALL(*dispatcher_, ShouldCreateOrBufferPacketForConnection(1));
  ProcessPacket(client_address, 1, true, SerializeCHLO());
  EXPECT_EQ(client_address, dispatcher_->current_peer_address());
  EXPECT_EQ(server_address_, dispatcher_->current_self_address());

  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(2, client_address, QuicStringPiece("hq")))
      .WillOnce(testing::Return(CreateSession(
          dispatcher_.get(), config_, 2, client_address, &mock_helper_,
          &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session2_)));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session2_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(2, packet);
      })));
  EXPECT_CALL(*dispatcher_, ShouldCreateOrBufferPacketForConnection(2));
  ProcessPacket(client_address, 2, true, SerializeCHLO());

  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .Times(1)
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(1, packet);
      })));
  ProcessPacket(client_address, 1, false, "data");
}

// Regression test of b/93325907.
TEST_F(QuicDispatcherTest, DispatcherDoesNotRejectPacketNumberZero) {
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  server_address_ = QuicSocketAddress(QuicIpAddress::Any4(), 5);

  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(1, client_address, QuicStringPiece("hq")))
      .WillOnce(testing::Return(CreateSession(
          dispatcher_.get(), config_, 1, client_address, &mock_helper_,
          &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_)));
  // Verify both packets 1 and 2 are processed by connection 1.
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .Times(2)
      .WillRepeatedly(
          WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
            ValidatePacket(1, packet);
          })));
  EXPECT_CALL(*dispatcher_, ShouldCreateOrBufferPacketForConnection(1));
  ProcessPacket(
      client_address, 1, true,
      ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO,
                        CurrentSupportedVersions().front().transport_version),
      SerializeCHLO(), PACKET_8BYTE_CONNECTION_ID, PACKET_4BYTE_PACKET_NUMBER,
      1);
  // Packet number 256 with packet number length 1 would be considered as 0 in
  // dispatcher.
  ProcessPacket(
      client_address, 1, false,
      ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO,
                        CurrentSupportedVersions().front().transport_version),
      "", PACKET_8BYTE_CONNECTION_ID, PACKET_1BYTE_PACKET_NUMBER, 256);
  EXPECT_EQ(client_address, dispatcher_->current_peer_address());
  EXPECT_EQ(server_address_, dispatcher_->current_self_address());
}

TEST_F(QuicDispatcherTest, StatelessVersionNegotiation) {
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  server_address_ = QuicSocketAddress(QuicIpAddress::Any4(), 5);

  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(1, client_address, QuicStringPiece("hq")))
      .Times(0);
  QuicTransportVersion version =
      static_cast<QuicTransportVersion>(QuicTransportVersionMin() - 1);
  ParsedQuicVersion parsed_version(PROTOCOL_QUIC_CRYPTO, version);
  ProcessPacket(client_address, 1, true, parsed_version, SerializeCHLO(),
                PACKET_8BYTE_CONNECTION_ID, PACKET_4BYTE_PACKET_NUMBER, 1);
}

TEST_F(QuicDispatcherTest, Shutdown) {
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(_, client_address, QuicStringPiece("hq")))
      .WillOnce(testing::Return(CreateSession(
          dispatcher_.get(), config_, 1, client_address, &mock_helper_,
          &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_)));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(1, packet);
      })));

  EXPECT_CALL(*dispatcher_, ShouldCreateOrBufferPacketForConnection(1));
  ProcessPacket(client_address, 1, true, SerializeCHLO());

  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              CloseConnection(QUIC_PEER_GOING_AWAY, _, _));

  dispatcher_->Shutdown();
}

TEST_F(QuicDispatcherTest, TimeWaitListManager) {
  CreateTimeWaitListManager();

  // Create a new session.
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  QuicConnectionId connection_id = 1;
  EXPECT_CALL(*dispatcher_, CreateQuicSession(connection_id, client_address,
                                              QuicStringPiece("hq")))
      .WillOnce(testing::Return(CreateSession(
          dispatcher_.get(), config_, connection_id, client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_)));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(1, packet);
      })));

  EXPECT_CALL(*dispatcher_, ShouldCreateOrBufferPacketForConnection(1));
  ProcessPacket(client_address, connection_id, true, SerializeCHLO());

  // Close the connection by sending public reset packet.
  QuicPublicResetPacket packet;
  packet.connection_id = connection_id;
  packet.nonce_proof = 132232;
  std::unique_ptr<QuicEncryptedPacket> encrypted(
      QuicFramer::BuildPublicResetPacket(packet));
  std::unique_ptr<QuicReceivedPacket> received(ConstructReceivedPacket(
      *encrypted, session1_->connection()->clock()->Now()));
  EXPECT_CALL(*session1_, OnConnectionClosed(QUIC_PUBLIC_RESET, _,
                                             ConnectionCloseSource::FROM_PEER))
      .Times(1)
      .WillOnce(WithoutArgs(Invoke(
          reinterpret_cast<MockServerConnection*>(session1_->connection()),
          &MockServerConnection::UnregisterOnConnectionClosed)));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(
          Invoke(reinterpret_cast<MockQuicConnection*>(session1_->connection()),
                 &MockQuicConnection::ReallyProcessUdpPacket));
  dispatcher_->ProcessPacket(QuicSocketAddress(), client_address, *received);
  EXPECT_TRUE(time_wait_list_manager_->IsConnectionIdInTimeWait(connection_id));

  // Dispatcher forwards subsequent packets for this connection_id to the time
  // wait list manager.
  EXPECT_CALL(*time_wait_list_manager_, ProcessPacket(_, _, connection_id))
      .Times(1);
  EXPECT_CALL(*time_wait_list_manager_, AddConnectionIdToTimeWait(_, _, _, _))
      .Times(0);
  ProcessPacket(client_address, connection_id, true, "data");
}

TEST_F(QuicDispatcherTest, NoVersionPacketToTimeWaitListManager) {
  CreateTimeWaitListManager();

  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  QuicConnectionId connection_id = 1;
  // Dispatcher forwards all packets for this connection_id to the time wait
  // list manager.
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, QuicStringPiece("hq")))
      .Times(0);
  EXPECT_CALL(*time_wait_list_manager_, ProcessPacket(_, _, connection_id))
      .Times(1);
  EXPECT_CALL(*time_wait_list_manager_, AddConnectionIdToTimeWait(_, _, _, _))
      .Times(1);
  ProcessPacket(client_address, connection_id, false, SerializeCHLO());
}

TEST_F(QuicDispatcherTest, ProcessPacketWithZeroPort) {
  CreateTimeWaitListManager();

  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 0);
  server_address_ = QuicSocketAddress(QuicIpAddress::Any4(), 5);

  // dispatcher_ should drop this packet.
  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(1, client_address, QuicStringPiece("hq")))
      .Times(0);
  EXPECT_CALL(*time_wait_list_manager_, ProcessPacket(_, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_, AddConnectionIdToTimeWait(_, _, _, _))
      .Times(0);
  ProcessPacket(client_address, 1, true, SerializeCHLO());
}

TEST_F(QuicDispatcherTest, OKSeqNoPacketProcessed) {
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  QuicConnectionId connection_id = 1;
  server_address_ = QuicSocketAddress(QuicIpAddress::Any4(), 5);

  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(1, client_address, QuicStringPiece("hq")))
      .WillOnce(testing::Return(CreateSession(
          dispatcher_.get(), config_, 1, client_address, &mock_helper_,
          &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_)));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(1, packet);
      })));

  // A packet whose packet number is the largest that is allowed to start a
  // connection.
  EXPECT_CALL(*dispatcher_,
              ShouldCreateOrBufferPacketForConnection(connection_id));
  ProcessPacket(client_address, connection_id, true, SerializeCHLO(),
                PACKET_8BYTE_CONNECTION_ID, PACKET_4BYTE_PACKET_NUMBER,
                QuicDispatcher::kMaxReasonableInitialPacketNumber);
  EXPECT_EQ(client_address, dispatcher_->current_peer_address());
  EXPECT_EQ(server_address_, dispatcher_->current_self_address());
}

TEST_F(QuicDispatcherTest, TooBigSeqNoPacketToTimeWaitListManager) {
  CreateTimeWaitListManager();
  SetQuicRestartFlag(quic_enable_accept_random_ipn, false);
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  QuicConnectionId connection_id = 1;

  // Dispatcher forwards this packet for this connection_id to the time wait
  // list manager.
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, QuicStringPiece("hq")))
      .Times(0);
  EXPECT_CALL(*time_wait_list_manager_, ProcessPacket(_, _, 1)).Times(1);
  EXPECT_CALL(*time_wait_list_manager_, ProcessPacket(_, _, 2)).Times(1);
  EXPECT_CALL(*time_wait_list_manager_, AddConnectionIdToTimeWait(_, _, _, _))
      .Times(2);
  // A packet whose packet number is one to large to be allowed to start a
  // connection.
  ProcessPacket(client_address, connection_id, true, SerializeCHLO(),
                PACKET_8BYTE_CONNECTION_ID, PACKET_4BYTE_PACKET_NUMBER,
                QuicDispatcher::kMaxReasonableInitialPacketNumber + 1);
  connection_id = 2;
  SetQuicRestartFlag(quic_enable_accept_random_ipn, true);
  ProcessPacket(client_address, connection_id, true, SerializeCHLO(),
                PACKET_8BYTE_CONNECTION_ID, PACKET_4BYTE_PACKET_NUMBER,
                kMaxRandomInitialPacketNumber +
                    QuicDispatcher::kMaxReasonableInitialPacketNumber + 1);
}

TEST_F(QuicDispatcherTest, SupportedTransportVersionsChangeInFlight) {
  static_assert(QUIC_ARRAYSIZE(kSupportedTransportVersions) == 7u,
                "Supported versions out of sync");
  SetQuicReloadableFlag(quic_disable_version_35, false);
  SetQuicReloadableFlag(quic_enable_version_43, true);
  SetQuicReloadableFlag(quic_enable_version_44, true);
  SetQuicReloadableFlag(quic_enable_version_45, true);
  SetQuicReloadableFlag(quic_enable_version_46, true);
  SetQuicFlag(&FLAGS_quic_enable_version_99, true);
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  server_address_ = QuicSocketAddress(QuicIpAddress::Any4(), 5);
  QuicConnectionId connection_id = 1;

  EXPECT_CALL(*dispatcher_, CreateQuicSession(connection_id, client_address,
                                              QuicStringPiece("hq")))
      .Times(0);
  ParsedQuicVersion version(
      PROTOCOL_QUIC_CRYPTO,
      static_cast<QuicTransportVersion>(QuicTransportVersionMin() - 1));
  ProcessPacket(client_address, connection_id, true, version, SerializeCHLO(),
                PACKET_8BYTE_CONNECTION_ID, PACKET_4BYTE_PACKET_NUMBER, 1);
  ++connection_id;
  EXPECT_CALL(*dispatcher_, CreateQuicSession(connection_id, client_address,
                                              QuicStringPiece("hq")))
      .WillOnce(testing::Return(CreateSession(
          dispatcher_.get(), config_, connection_id, client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_)));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(
          Invoke([this, connection_id](const QuicEncryptedPacket& packet) {
            ValidatePacket(connection_id, packet);
          })));
  EXPECT_CALL(*dispatcher_,
              ShouldCreateOrBufferPacketForConnection(connection_id));
  ProcessPacket(client_address, connection_id, true,
                ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO,
                                  QuicVersionMin().transport_version),
                SerializeCHLO(), PACKET_8BYTE_CONNECTION_ID,
                PACKET_4BYTE_PACKET_NUMBER, 1);
  ++connection_id;
  EXPECT_CALL(*dispatcher_, CreateQuicSession(connection_id, client_address,
                                              QuicStringPiece("hq")))
      .WillOnce(testing::Return(CreateSession(
          dispatcher_.get(), config_, connection_id, client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_)));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(
          Invoke([this, connection_id](const QuicEncryptedPacket& packet) {
            ValidatePacket(connection_id, packet);
          })));
  EXPECT_CALL(*dispatcher_,
              ShouldCreateOrBufferPacketForConnection(connection_id));
  ProcessPacket(client_address, connection_id, true, QuicVersionMax(),
                SerializeCHLO(), PACKET_8BYTE_CONNECTION_ID,
                PACKET_4BYTE_PACKET_NUMBER, 1);

  // Turn off version 46.
  SetQuicReloadableFlag(quic_enable_version_46, false);
  ++connection_id;
  EXPECT_CALL(*dispatcher_, CreateQuicSession(connection_id, client_address,
                                              QuicStringPiece("hq")))
      .Times(0);
  ProcessPacket(client_address, connection_id, true,
                ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_46),
                SerializeCHLO(), PACKET_8BYTE_CONNECTION_ID,
                PACKET_4BYTE_PACKET_NUMBER, 1);

  // Turn on version 46.
  SetQuicReloadableFlag(quic_enable_version_46, true);
  ++connection_id;
  EXPECT_CALL(*dispatcher_, CreateQuicSession(connection_id, client_address,
                                              QuicStringPiece("hq")))
      .WillOnce(testing::Return(CreateSession(
          dispatcher_.get(), config_, connection_id, client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_)));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(
          Invoke([this, connection_id](const QuicEncryptedPacket& packet) {
            ValidatePacket(connection_id, packet);
          })));
  EXPECT_CALL(*dispatcher_,
              ShouldCreateOrBufferPacketForConnection(connection_id));
  ProcessPacket(client_address, connection_id, true,
                ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_46),
                SerializeCHLO(), PACKET_8BYTE_CONNECTION_ID,
                PACKET_4BYTE_PACKET_NUMBER, 1);

  // Turn off version 45.
  SetQuicReloadableFlag(quic_enable_version_45, false);
  ++connection_id;
  EXPECT_CALL(*dispatcher_, CreateQuicSession(connection_id, client_address,
                                              QuicStringPiece("hq")))
      .Times(0);
  ProcessPacket(client_address, connection_id, true,
                ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_45),
                SerializeCHLO(), PACKET_8BYTE_CONNECTION_ID,
                PACKET_4BYTE_PACKET_NUMBER, 1);

  // Turn on version 45.
  SetQuicReloadableFlag(quic_enable_version_45, true);
  ++connection_id;
  EXPECT_CALL(*dispatcher_, CreateQuicSession(connection_id, client_address,
                                              QuicStringPiece("hq")))
      .WillOnce(testing::Return(CreateSession(
          dispatcher_.get(), config_, connection_id, client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_)));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(
          Invoke([this, connection_id](const QuicEncryptedPacket& packet) {
            ValidatePacket(connection_id, packet);
          })));
  EXPECT_CALL(*dispatcher_,
              ShouldCreateOrBufferPacketForConnection(connection_id));
  ProcessPacket(client_address, connection_id, true,
                ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_45),
                SerializeCHLO(), PACKET_8BYTE_CONNECTION_ID,
                PACKET_4BYTE_PACKET_NUMBER, 1);

  // Turn off version 44.
  SetQuicReloadableFlag(quic_enable_version_44, false);
  ++connection_id;
  EXPECT_CALL(*dispatcher_, CreateQuicSession(connection_id, client_address,
                                              QuicStringPiece("hq")))
      .Times(0);
  ProcessPacket(client_address, connection_id, true,
                ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_44),
                SerializeCHLO(), PACKET_8BYTE_CONNECTION_ID,
                PACKET_4BYTE_PACKET_NUMBER, 1);

  // Turn on version 44.
  SetQuicReloadableFlag(quic_enable_version_44, true);
  ++connection_id;
  EXPECT_CALL(*dispatcher_, CreateQuicSession(connection_id, client_address,
                                              QuicStringPiece("hq")))
      .WillOnce(testing::Return(CreateSession(
          dispatcher_.get(), config_, connection_id, client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_)));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(
          Invoke([this, connection_id](const QuicEncryptedPacket& packet) {
            ValidatePacket(connection_id, packet);
          })));
  EXPECT_CALL(*dispatcher_,
              ShouldCreateOrBufferPacketForConnection(connection_id));
  ProcessPacket(client_address, connection_id, true,
                ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_44),
                SerializeCHLO(), PACKET_8BYTE_CONNECTION_ID,
                PACKET_4BYTE_PACKET_NUMBER, 1);

  // Turn off version 43.
  SetQuicReloadableFlag(quic_enable_version_43, false);
  ++connection_id;
  EXPECT_CALL(*dispatcher_, CreateQuicSession(connection_id, client_address,
                                              QuicStringPiece("hq")))
      .Times(0);
  ProcessPacket(client_address, connection_id, true,
                ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_43),
                SerializeCHLO(), PACKET_8BYTE_CONNECTION_ID,
                PACKET_4BYTE_PACKET_NUMBER, 1);

  // Turn on version 43.
  SetQuicReloadableFlag(quic_enable_version_43, true);
  ++connection_id;
  EXPECT_CALL(*dispatcher_, CreateQuicSession(connection_id, client_address,
                                              QuicStringPiece("hq")))
      .WillOnce(testing::Return(CreateSession(
          dispatcher_.get(), config_, connection_id, client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_)));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(
          Invoke([this, connection_id](const QuicEncryptedPacket& packet) {
            ValidatePacket(connection_id, packet);
          })));
  EXPECT_CALL(*dispatcher_,
              ShouldCreateOrBufferPacketForConnection(connection_id));
  ProcessPacket(client_address, connection_id, true,
                ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_43),
                SerializeCHLO(), PACKET_8BYTE_CONNECTION_ID,
                PACKET_4BYTE_PACKET_NUMBER, 1);

  // Turn off version 35.
  SetQuicReloadableFlag(quic_disable_version_35, true);
  ++connection_id;
  EXPECT_CALL(*dispatcher_, CreateQuicSession(connection_id, client_address,
                                              QuicStringPiece("hq")))
      .Times(0);
  ProcessPacket(client_address, connection_id, true,
                ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_35),
                SerializeCHLO(), PACKET_8BYTE_CONNECTION_ID,
                PACKET_4BYTE_PACKET_NUMBER, 1);

  // Turn on version 35.
  SetQuicReloadableFlag(quic_disable_version_35, false);
  ++connection_id;
  EXPECT_CALL(*dispatcher_, CreateQuicSession(connection_id, client_address,
                                              QuicStringPiece("hq")))
      .WillOnce(testing::Return(CreateSession(
          dispatcher_.get(), config_, connection_id, client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_)));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(
          Invoke([this, connection_id](const QuicEncryptedPacket& packet) {
            ValidatePacket(connection_id, packet);
          })));
  EXPECT_CALL(*dispatcher_,
              ShouldCreateOrBufferPacketForConnection(connection_id));
  ProcessPacket(client_address, connection_id, true,
                ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_35),
                SerializeCHLO(), PACKET_8BYTE_CONNECTION_ID,
                PACKET_4BYTE_PACKET_NUMBER, 1);
}

// Enables mocking of the handshake-confirmation for stateless rejects.
class MockQuicCryptoServerStream : public QuicCryptoServerStream {
 public:
  MockQuicCryptoServerStream(const QuicCryptoServerConfig& crypto_config,
                             QuicCompressedCertsCache* compressed_certs_cache,
                             QuicServerSessionBase* session,
                             QuicCryptoServerStream::Helper* helper)
      : QuicCryptoServerStream(
            &crypto_config,
            compressed_certs_cache,
            GetQuicReloadableFlag(enable_quic_stateless_reject_support),
            session,
            helper),
        handshake_confirmed_(false) {}
  MockQuicCryptoServerStream(const MockQuicCryptoServerStream&) = delete;
  MockQuicCryptoServerStream& operator=(const MockQuicCryptoServerStream&) =
      delete;

  void set_handshake_confirmed_for_testing(bool handshake_confirmed) {
    handshake_confirmed_ = handshake_confirmed;
  }

  bool handshake_confirmed() const override { return handshake_confirmed_; }

 private:
  bool handshake_confirmed_;
};

struct StatelessRejectTestParams {
  StatelessRejectTestParams(bool enable_stateless_rejects_via_flag,
                            bool client_supports_statelesss_rejects,
                            bool crypto_handshake_successful)
      : enable_stateless_rejects_via_flag(enable_stateless_rejects_via_flag),
        client_supports_statelesss_rejects(client_supports_statelesss_rejects),
        crypto_handshake_successful(crypto_handshake_successful) {}

  friend std::ostream& operator<<(std::ostream& os,
                                  const StatelessRejectTestParams& p) {
    os << "{  enable_stateless_rejects_via_flag: "
       << p.enable_stateless_rejects_via_flag << std::endl;
    os << " client_supports_statelesss_rejects: "
       << p.client_supports_statelesss_rejects << std::endl;
    os << " crypto_handshake_successful: " << p.crypto_handshake_successful
       << " }";
    return os;
  }

  // This only enables the stateless reject feature via the feature-flag.
  // This should be a no-op if the peer does not support them.
  bool enable_stateless_rejects_via_flag;
  // Whether or not the client supports stateless rejects.
  bool client_supports_statelesss_rejects;
  // Should the initial crypto handshake succeed or not.
  bool crypto_handshake_successful;
};

// Constructs various test permutations for stateless rejects.
std::vector<StatelessRejectTestParams> GetStatelessRejectTestParams() {
  std::vector<StatelessRejectTestParams> params;
  for (bool enable_stateless_rejects_via_flag : {true, false}) {
    for (bool client_supports_statelesss_rejects : {true, false}) {
      for (bool crypto_handshake_successful : {true, false}) {
        params.push_back(StatelessRejectTestParams(
            enable_stateless_rejects_via_flag,
            client_supports_statelesss_rejects, crypto_handshake_successful));
      }
    }
  }
  return params;
}

class QuicDispatcherStatelessRejectTest
    : public QuicDispatcherTest,
      public testing::WithParamInterface<StatelessRejectTestParams> {
 public:
  QuicDispatcherStatelessRejectTest()
      : QuicDispatcherTest(), crypto_stream1_(nullptr) {}

  ~QuicDispatcherStatelessRejectTest() override {
    if (crypto_stream1_) {
      delete crypto_stream1_;
    }
  }

  // This test setup assumes that all testing will be done using
  // crypto_stream1_.
  void SetUp() override {
    QuicDispatcherTest::SetUp();
    SetQuicReloadableFlag(enable_quic_stateless_reject_support,
                          GetParam().enable_stateless_rejects_via_flag);
  }

  // Returns true or false, depending on whether the server will emit
  // a stateless reject, depending upon the parameters of the test.
  bool ExpectStatelessReject() {
    return GetParam().enable_stateless_rejects_via_flag &&
           !GetParam().crypto_handshake_successful &&
           GetParam().client_supports_statelesss_rejects;
  }

  // Sets up dispatcher_, session1_, and crypto_stream1_ based on
  // the test parameters.
  QuicServerSessionBase* CreateSessionBasedOnTestParams(
      QuicConnectionId connection_id,
      const QuicSocketAddress& client_address) {
    CreateSession(dispatcher_.get(), config_, connection_id, client_address,
                  &mock_helper_, &mock_alarm_factory_, &crypto_config_,
                  QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_);

    crypto_stream1_ = new MockQuicCryptoServerStream(
        crypto_config_, QuicDispatcherPeer::GetCache(dispatcher_.get()),
        session1_, session1_->stream_helper());
    session1_->SetCryptoStream(crypto_stream1_);
    crypto_stream1_->set_handshake_confirmed_for_testing(
        GetParam().crypto_handshake_successful);
    crypto_stream1_->SetPeerSupportsStatelessRejects(
        GetParam().client_supports_statelesss_rejects);
    return session1_;
  }

  MockQuicCryptoServerStream* crypto_stream1_;
};

// Parameterized test for stateless rejects.  Should test all
// combinations of enabling/disabling, reject/no-reject for stateless
// rejects.
INSTANTIATE_TEST_CASE_P(QuicDispatcherStatelessRejectTests,
                        QuicDispatcherStatelessRejectTest,
                        ::testing::ValuesIn(GetStatelessRejectTestParams()));

TEST_P(QuicDispatcherStatelessRejectTest, ParameterizedBasicTest) {
  CreateTimeWaitListManager();

  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  QuicConnectionId connection_id = 1;
  EXPECT_CALL(*dispatcher_, CreateQuicSession(connection_id, client_address,
                                              QuicStringPiece("hq")))
      .WillOnce(testing::Return(
          CreateSessionBasedOnTestParams(connection_id, client_address)));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(
          Invoke([this, connection_id](const QuicEncryptedPacket& packet) {
            ValidatePacket(connection_id, packet);
          })));
  EXPECT_CALL(*dispatcher_,
              ShouldCreateOrBufferPacketForConnection(connection_id))
      .Times(1);

  // Process the first packet for the connection.
  ProcessPacket(client_address, connection_id, true, SerializeCHLO());
  if (ExpectStatelessReject()) {
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
                CloseConnection(QUIC_CRYPTO_HANDSHAKE_STATELESS_REJECT, _, _));
    // If this is a stateless reject, the crypto stream will close the
    // connection.
    session1_->connection()->CloseConnection(
        QUIC_CRYPTO_HANDSHAKE_STATELESS_REJECT, "stateless reject",
        ConnectionCloseBehavior::SILENT_CLOSE);
  }

  // Send a second packet and check the results.  If this is a stateless reject,
  // the existing connection_id will go on the time-wait list.
  EXPECT_EQ(ExpectStatelessReject(),
            time_wait_list_manager_->IsConnectionIdInTimeWait(connection_id));
  if (ExpectStatelessReject()) {
    // The second packet will be processed on the time-wait list.
    EXPECT_CALL(*time_wait_list_manager_, ProcessPacket(_, _, connection_id))
        .Times(1);
  } else {
    // The second packet will trigger a packet-validation
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
                ProcessUdpPacket(_, _, _))
        .Times(1)
        .WillOnce(WithArg<2>(
            Invoke([this, connection_id](const QuicEncryptedPacket& packet) {
              ValidatePacket(connection_id, packet);
            })));
  }
  ProcessPacket(client_address, connection_id, true, "data");
}

TEST_P(QuicDispatcherStatelessRejectTest, CheapRejects) {
  SetQuicReloadableFlag(quic_use_cheap_stateless_rejects, true);
  CreateTimeWaitListManager();

  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  QuicConnectionId connection_id = 1;
  if (GetParam().enable_stateless_rejects_via_flag) {
    EXPECT_CALL(*dispatcher_,
                CreateQuicSession(connection_id, client_address, _))
        .Times(0);
  } else {
    EXPECT_CALL(*dispatcher_, CreateQuicSession(connection_id, client_address,
                                                QuicStringPiece("h2")))
        .WillOnce(testing::Return(
            CreateSessionBasedOnTestParams(connection_id, client_address)));
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
                ProcessUdpPacket(_, _, _))
        .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
          ValidatePacket(1, packet);
        })));
  }

  QUIC_LOG(INFO) << "ExpectStatelessReject: " << ExpectStatelessReject();
  QUIC_LOG(INFO) << "Params: " << GetParam();
  // Process the first packet for the connection.
  CryptoHandshakeMessage client_hello =
      crypto_test_utils::CreateCHLO({{"AEAD", "AESG"},
                                     {"KEXS", "C255"},
                                     {"COPT", "SREJ"},
                                     {"NONC", "1234567890123456789012"},
                                     {"ALPN", "h2"},
                                     {"VER\0", "Q025"}},
                                    kClientHelloMinimumSize);

  if (GetParam().enable_stateless_rejects_via_flag) {
    EXPECT_CALL(*time_wait_list_manager_, ProcessPacket(_, _, connection_id))
        .Times(1);
  } else {
    EXPECT_CALL(*dispatcher_,
                ShouldCreateOrBufferPacketForConnection(connection_id))
        .Times(1);
  }
  ProcessPacket(client_address, connection_id, true,
                QuicString(client_hello.GetSerialized().AsStringPiece()));

  if (GetParam().enable_stateless_rejects_via_flag) {
    EXPECT_EQ(true,
              time_wait_list_manager_->IsConnectionIdInTimeWait(connection_id));
  }
}

TEST_P(QuicDispatcherStatelessRejectTest, BufferNonChlo) {
  SetQuicReloadableFlag(quic_use_cheap_stateless_rejects, true);
  CreateTimeWaitListManager();

  const QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  const QuicConnectionId connection_id = 1;

  EXPECT_CALL(*dispatcher_,
              ShouldCreateOrBufferPacketForConnection(connection_id))
      .Times(1);
  ProcessPacket(client_address, connection_id, true, "NOT DATA FOR A CHLO");

  // Process the first packet for the connection.
  CryptoHandshakeMessage client_hello =
      crypto_test_utils::CreateCHLO({{"AEAD", "AESG"},
                                     {"KEXS", "C255"},
                                     {"NONC", "1234567890123456789012"},
                                     {"ALPN", "h3"},
                                     {"VER\0", "Q025"}},
                                    kClientHelloMinimumSize);

  // If stateless rejects are enabled then a connection will be created now
  // and the buffered packet will be processed
  EXPECT_CALL(*dispatcher_, CreateQuicSession(connection_id, client_address,
                                              QuicStringPiece("h3")))
      .WillOnce(testing::Return(
          CreateSessionBasedOnTestParams(connection_id, client_address)));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, client_address, _))
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(connection_id, packet);
      })));
  // Expect both packets to be passed to ProcessUdpPacket(). And one of them
  // is already expected in CreateSessionBasedOnTestParams().
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, client_address, _))
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(connection_id, packet);
      })))
      .RetiresOnSaturation();
  ProcessPacket(client_address, connection_id, true,
                QuicString(client_hello.GetSerialized().AsStringPiece()));
  EXPECT_FALSE(
      time_wait_list_manager_->IsConnectionIdInTimeWait(connection_id));
}

// Verify the stopgap test: Packets with truncated connection IDs should be
// dropped.
class QuicDispatcherTestStrayPacketConnectionId : public QuicDispatcherTest {};

// Packets with truncated connection IDs should be dropped.
TEST_F(QuicDispatcherTestStrayPacketConnectionId,
       StrayPacketTruncatedConnectionId) {
  CreateTimeWaitListManager();

  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  QuicConnectionId connection_id = 1;
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, QuicStringPiece("hq")))
      .Times(0);
  if (CurrentSupportedVersions()[0].transport_version > QUIC_VERSION_43) {
    // This IETF packet has invalid connection ID length.
    EXPECT_CALL(*time_wait_list_manager_, ProcessPacket(_, _, _)).Times(0);
    EXPECT_CALL(*time_wait_list_manager_, AddConnectionIdToTimeWait(_, _, _, _))
        .Times(0);
  } else {
    // This GQUIC packet is considered as IETF QUIC packet with short header
    // with unacceptable packet number.
    EXPECT_CALL(*time_wait_list_manager_, ProcessPacket(_, _, _)).Times(1);
    EXPECT_CALL(*time_wait_list_manager_, AddConnectionIdToTimeWait(_, _, _, _))
        .Times(1);
  }
  ProcessPacket(client_address, connection_id, true, "data",
                PACKET_0BYTE_CONNECTION_ID, PACKET_4BYTE_PACKET_NUMBER);
}

class BlockingWriter : public QuicPacketWriterWrapper {
 public:
  BlockingWriter() : write_blocked_(false) {}

  bool IsWriteBlocked() const override { return write_blocked_; }
  void SetWritable() override { write_blocked_ = false; }

  WriteResult WritePacket(const char* buffer,
                          size_t buf_len,
                          const QuicIpAddress& self_client_address,
                          const QuicSocketAddress& peer_client_address,
                          PerPacketOptions* options) override {
    // It would be quite possible to actually implement this method here with
    // the fake blocked status, but it would be significantly more work in
    // Chromium, and since it's not called anyway, don't bother.
    QUIC_LOG(DFATAL) << "Not supported";
    return WriteResult();
  }

  bool write_blocked_;
};

class QuicDispatcherWriteBlockedListTest : public QuicDispatcherTest {
 public:
  void SetUp() override {
    QuicDispatcherTest::SetUp();
    writer_ = new BlockingWriter;
    QuicDispatcherPeer::UseWriter(dispatcher_.get(), writer_);

    QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

    EXPECT_CALL(*dispatcher_,
                CreateQuicSession(_, client_address, QuicStringPiece("hq")))
        .WillOnce(testing::Return(CreateSession(
            dispatcher_.get(), config_, 1, client_address, &helper_,
            &alarm_factory_, &crypto_config_,
            QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_)));
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
                ProcessUdpPacket(_, _, _))
        .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
          ValidatePacket(1, packet);
        })));
    EXPECT_CALL(*dispatcher_, ShouldCreateOrBufferPacketForConnection(1));
    ProcessPacket(client_address, 1, true, SerializeCHLO());

    EXPECT_CALL(*dispatcher_,
                CreateQuicSession(_, client_address, QuicStringPiece("hq")))
        .WillOnce(testing::Return(CreateSession(
            dispatcher_.get(), config_, 2, client_address, &helper_,
            &alarm_factory_, &crypto_config_,
            QuicDispatcherPeer::GetCache(dispatcher_.get()), &session2_)));
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session2_->connection()),
                ProcessUdpPacket(_, _, _))
        .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
          ValidatePacket(2, packet);
        })));
    EXPECT_CALL(*dispatcher_, ShouldCreateOrBufferPacketForConnection(2));
    ProcessPacket(client_address, 2, true, SerializeCHLO());

    blocked_list_ = QuicDispatcherPeer::GetWriteBlockedList(dispatcher_.get());
  }

  void TearDown() override {
    EXPECT_CALL(*connection1(), CloseConnection(QUIC_PEER_GOING_AWAY, _, _));
    EXPECT_CALL(*connection2(), CloseConnection(QUIC_PEER_GOING_AWAY, _, _));
    dispatcher_->Shutdown();
  }

  void SetBlocked() { writer_->write_blocked_ = true; }

  void BlockConnection2() {
    writer_->write_blocked_ = true;
    dispatcher_->OnWriteBlocked(connection2());
  }

 protected:
  MockQuicConnectionHelper helper_;
  MockAlarmFactory alarm_factory_;
  BlockingWriter* writer_;
  QuicDispatcher::WriteBlockedList* blocked_list_;
};

TEST_F(QuicDispatcherWriteBlockedListTest, BasicOnCanWrite) {
  // No OnCanWrite calls because no connections are blocked.
  dispatcher_->OnCanWrite();

  // Register connection 1 for events, and make sure it's notified.
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  EXPECT_CALL(*connection1(), OnCanWrite());
  dispatcher_->OnCanWrite();

  // It should get only one notification.
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(0);
  dispatcher_->OnCanWrite();
  EXPECT_FALSE(dispatcher_->HasPendingWrites());
}

TEST_F(QuicDispatcherWriteBlockedListTest, OnCanWriteOrder) {
  // Make sure we handle events in order.
  InSequence s;
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  dispatcher_->OnWriteBlocked(connection2());
  EXPECT_CALL(*connection1(), OnCanWrite());
  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_->OnCanWrite();

  // Check the other ordering.
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection2());
  dispatcher_->OnWriteBlocked(connection1());
  EXPECT_CALL(*connection2(), OnCanWrite());
  EXPECT_CALL(*connection1(), OnCanWrite());
  dispatcher_->OnCanWrite();
}

TEST_F(QuicDispatcherWriteBlockedListTest, OnCanWriteRemove) {
  // Add and remove one connction.
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  blocked_list_->erase(connection1());
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(0);
  dispatcher_->OnCanWrite();

  // Add and remove one connction and make sure it doesn't affect others.
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  dispatcher_->OnWriteBlocked(connection2());
  blocked_list_->erase(connection1());
  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_->OnCanWrite();

  // Add it, remove it, and add it back and make sure things are OK.
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  blocked_list_->erase(connection1());
  dispatcher_->OnWriteBlocked(connection1());
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(1);
  dispatcher_->OnCanWrite();
}

TEST_F(QuicDispatcherWriteBlockedListTest, DoubleAdd) {
  // Make sure a double add does not necessitate a double remove.
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  dispatcher_->OnWriteBlocked(connection1());
  blocked_list_->erase(connection1());
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(0);
  dispatcher_->OnCanWrite();

  // Make sure a double add does not result in two OnCanWrite calls.
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  dispatcher_->OnWriteBlocked(connection1());
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(1);
  dispatcher_->OnCanWrite();
}

TEST_F(QuicDispatcherWriteBlockedListTest, OnCanWriteHandleBlock) {
  // Finally make sure if we write block on a write call, we stop calling.
  InSequence s;
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  dispatcher_->OnWriteBlocked(connection2());
  EXPECT_CALL(*connection1(), OnCanWrite())
      .WillOnce(Invoke(this, &QuicDispatcherWriteBlockedListTest::SetBlocked));
  EXPECT_CALL(*connection2(), OnCanWrite()).Times(0);
  dispatcher_->OnCanWrite();

  // And we'll resume where we left off when we get another call.
  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_->OnCanWrite();
}

TEST_F(QuicDispatcherWriteBlockedListTest, LimitedWrites) {
  // Make sure we call both writers.  The first will register for more writing
  // but should not be immediately called due to limits.
  InSequence s;
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  dispatcher_->OnWriteBlocked(connection2());
  EXPECT_CALL(*connection1(), OnCanWrite());
  EXPECT_CALL(*connection2(), OnCanWrite())
      .WillOnce(
          Invoke(this, &QuicDispatcherWriteBlockedListTest::BlockConnection2));
  dispatcher_->OnCanWrite();
  EXPECT_TRUE(dispatcher_->HasPendingWrites());

  // Now call OnCanWrite again, and connection1 should get its second chance
  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_->OnCanWrite();
  EXPECT_FALSE(dispatcher_->HasPendingWrites());
}

TEST_F(QuicDispatcherWriteBlockedListTest, TestWriteLimits) {
  // Finally make sure if we write block on a write call, we stop calling.
  InSequence s;
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  dispatcher_->OnWriteBlocked(connection2());
  EXPECT_CALL(*connection1(), OnCanWrite())
      .WillOnce(Invoke(this, &QuicDispatcherWriteBlockedListTest::SetBlocked));
  EXPECT_CALL(*connection2(), OnCanWrite()).Times(0);
  dispatcher_->OnCanWrite();
  EXPECT_TRUE(dispatcher_->HasPendingWrites());

  // And we'll resume where we left off when we get another call.
  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_->OnCanWrite();
  EXPECT_FALSE(dispatcher_->HasPendingWrites());
}

// Tests that bufferring packets works in stateful reject, expensive stateless
// reject and cheap stateless reject.
struct BufferedPacketStoreTestParams {
  BufferedPacketStoreTestParams(bool enable_stateless_rejects_via_flag,
                                bool support_cheap_stateless_reject)
      : enable_stateless_rejects_via_flag(enable_stateless_rejects_via_flag),
        support_cheap_stateless_reject(support_cheap_stateless_reject) {}

  friend std::ostream& operator<<(std::ostream& os,
                                  const BufferedPacketStoreTestParams& p) {
    os << "{  enable_stateless_rejects_via_flag: "
       << p.enable_stateless_rejects_via_flag << std::endl;
    os << "  support_cheap_stateless_reject: "
       << p.support_cheap_stateless_reject << " }";
    return os;
  }

  // This only enables the stateless reject feature via the feature-flag.
  // This should be a no-op if the peer does not support them.
  bool enable_stateless_rejects_via_flag;
  // Whether to do cheap stateless or not.
  bool support_cheap_stateless_reject;
};

std::vector<BufferedPacketStoreTestParams> GetBufferedPacketStoreTestParams() {
  std::vector<BufferedPacketStoreTestParams> params;
  for (bool enable_stateless_rejects_via_flag : {true, false}) {
    for (bool support_cheap_stateless_reject : {true, false}) {
      params.push_back(BufferedPacketStoreTestParams(
          enable_stateless_rejects_via_flag, support_cheap_stateless_reject));
    }
  }
  return params;
}

// A dispatcher whose stateless rejector will always ACCEPTs CHLO.
class BufferedPacketStoreTest
    : public QuicDispatcherTest,
      public testing::WithParamInterface<BufferedPacketStoreTestParams> {
 public:
  BufferedPacketStoreTest()
      : QuicDispatcherTest(),
        client_addr_(QuicIpAddress::Loopback4(), 1234),
        signed_config_(new QuicSignedServerConfig) {
    SetQuicReloadableFlag(quic_use_cheap_stateless_rejects,
                          GetParam().support_cheap_stateless_reject);
    SetQuicReloadableFlag(enable_quic_stateless_reject_support,
                          GetParam().enable_stateless_rejects_via_flag);
  }

  void SetUp() override {
    QuicDispatcherTest::SetUp();
    clock_ = QuicDispatcherPeer::GetHelper(dispatcher_.get())->GetClock();

    QuicTransportVersion version = AllSupportedTransportVersions().front();
    CryptoHandshakeMessage chlo =
        crypto_test_utils::GenerateDefaultInchoateCHLO(clock_, version,
                                                       &crypto_config_);
    chlo.SetVector(kCOPT, QuicTagVector{kSREJ});
    // Pass an inchoate CHLO.
    crypto_test_utils::GenerateFullCHLO(
        chlo, &crypto_config_, server_addr_, client_addr_, version, clock_,
        signed_config_, QuicDispatcherPeer::GetCache(dispatcher_.get()),
        &full_chlo_);
  }

  QuicString SerializeFullCHLO() {
    return QuicString(full_chlo_.GetSerialized().AsStringPiece());
  }

 protected:
  QuicSocketAddress server_addr_;
  QuicSocketAddress client_addr_;
  QuicReferenceCountedPointer<QuicSignedServerConfig> signed_config_;
  const QuicClock* clock_;
  CryptoHandshakeMessage full_chlo_;
};

INSTANTIATE_TEST_CASE_P(
    BufferedPacketStoreTests,
    BufferedPacketStoreTest,
    ::testing::ValuesIn(GetBufferedPacketStoreTestParams()));

TEST_P(BufferedPacketStoreTest, ProcessNonChloPacketsUptoLimitAndProcessChlo) {
  InSequence s;
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  server_address_ = QuicSocketAddress(QuicIpAddress::Any4(), 5);
  QuicConnectionId conn_id = 1;
  // A bunch of non-CHLO should be buffered upon arrival, and the first one
  // should trigger ShouldCreateOrBufferPacketForConnection().
  EXPECT_CALL(*dispatcher_, ShouldCreateOrBufferPacketForConnection(conn_id))
      .Times(1);
  for (size_t i = 1; i <= kDefaultMaxUndecryptablePackets + 1; ++i) {
    ProcessPacket(client_address, conn_id, true,
                  QuicStrCat("data packet ", i + 1), PACKET_8BYTE_CONNECTION_ID,
                  PACKET_4BYTE_PACKET_NUMBER, /*packet_number=*/i + 1);
  }
  EXPECT_EQ(0u, dispatcher_->session_map().size())
      << "No session should be created before CHLO arrives.";

  // Pop out the last packet as it is also be dropped by the store.
  data_connection_map_[conn_id].pop_back();
  // When CHLO arrives, a new session should be created, and all packets
  // buffered should be delivered to the session.
  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(conn_id, client_address, QuicStringPiece()))
      .WillOnce(testing::Return(CreateSession(
          dispatcher_.get(), config_, conn_id, client_address, &mock_helper_,
          &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_)));

  // Only |kDefaultMaxUndecryptablePackets| packets were buffered, and they
  // should be delivered in arrival order.
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .Times(kDefaultMaxUndecryptablePackets + 1)  // + 1 for CHLO.
      .WillRepeatedly(
          WithArg<2>(Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
            ValidatePacket(conn_id, packet);
          })));
  ProcessPacket(client_address, conn_id, true, SerializeFullCHLO());
}

TEST_P(BufferedPacketStoreTest,
       ProcessNonChloPacketsForDifferentConnectionsUptoLimit) {
  InSequence s;
  server_address_ = QuicSocketAddress(QuicIpAddress::Any4(), 5);
  // A bunch of non-CHLO should be buffered upon arrival.
  size_t kNumConnections = kMaxConnectionsWithoutCHLO + 1;
  for (size_t i = 1; i <= kNumConnections; ++i) {
    QuicSocketAddress client_address(QuicIpAddress::Loopback4(), i);
    QuicConnectionId conn_id = i;
    EXPECT_CALL(*dispatcher_, ShouldCreateOrBufferPacketForConnection(conn_id));
    ProcessPacket(client_address, conn_id, true,
                  QuicStrCat("data packet on connection ", i),
                  PACKET_8BYTE_CONNECTION_ID, PACKET_4BYTE_PACKET_NUMBER,
                  /*packet_number=*/2);
  }

  // Pop out the packet on last connection as it shouldn't be enqueued in store
  // as well.
  data_connection_map_[kNumConnections].pop_front();

  // Reset session creation counter to ensure processing CHLO can always
  // create session.
  QuicDispatcherPeer::set_new_sessions_allowed_per_event_loop(dispatcher_.get(),
                                                              kNumConnections);
  // Process CHLOs to create session for these connections.
  for (size_t i = 1; i <= kNumConnections; ++i) {
    QuicSocketAddress client_address(QuicIpAddress::Loopback4(), i);
    QuicConnectionId conn_id = i;
    if (conn_id == kNumConnections) {
      EXPECT_CALL(*dispatcher_,
                  ShouldCreateOrBufferPacketForConnection(conn_id));
    }
    EXPECT_CALL(*dispatcher_,
                CreateQuicSession(conn_id, client_address, QuicStringPiece()))
        .WillOnce(testing::Return(CreateSession(
            dispatcher_.get(), config_, conn_id, client_address, &mock_helper_,
            &mock_alarm_factory_, &crypto_config_,
            QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_)));
    // First |kNumConnections| - 1 connections should have buffered
    // a packet in store. The rest should have been dropped.
    size_t num_packet_to_process = i <= kMaxConnectionsWithoutCHLO ? 2u : 1u;
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
                ProcessUdpPacket(_, client_address, _))
        .Times(num_packet_to_process)
        .WillRepeatedly(WithArg<2>(
            Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
              ValidatePacket(conn_id, packet);
            })));

    ProcessPacket(client_address, conn_id, true, SerializeFullCHLO());
  }
}

// Tests that store delivers empty packet list if CHLO arrives firstly.
TEST_P(BufferedPacketStoreTest, DeliverEmptyPackets) {
  QuicConnectionId conn_id = 1;
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  EXPECT_CALL(*dispatcher_, ShouldCreateOrBufferPacketForConnection(conn_id));
  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(conn_id, client_address, QuicStringPiece()))
      .WillOnce(testing::Return(CreateSession(
          dispatcher_.get(), config_, conn_id, client_address, &mock_helper_,
          &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_)));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, client_address, _));
  ProcessPacket(client_address, conn_id, true, SerializeFullCHLO());
}

// Tests that a retransmitted CHLO arrives after a connection for the
// CHLO has been created.
TEST_P(BufferedPacketStoreTest, ReceiveRetransmittedCHLO) {
  InSequence s;
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  server_address_ = QuicSocketAddress(QuicIpAddress::Any4(), 5);
  QuicConnectionId conn_id = 1;
  ProcessPacket(client_address, conn_id, true, QuicStrCat("data packet ", 2),
                PACKET_8BYTE_CONNECTION_ID, PACKET_4BYTE_PACKET_NUMBER,
                /*packet_number=*/2);

  // When CHLO arrives, a new session should be created, and all packets
  // buffered should be delivered to the session.
  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(conn_id, client_address, QuicStringPiece()))
      .Times(1)  // Only triggered by 1st CHLO.
      .WillOnce(testing::Return(CreateSession(
          dispatcher_.get(), config_, conn_id, client_address, &mock_helper_,
          &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_)));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .Times(3)  // Triggered by 1 data packet and 2 CHLOs.
      .WillRepeatedly(
          WithArg<2>(Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
            ValidatePacket(conn_id, packet);
          })));
  ProcessPacket(client_address, conn_id, true, SerializeFullCHLO());

  ProcessPacket(client_address, conn_id, true, SerializeFullCHLO());
}

// Tests that expiration of a connection add connection id to time wait list.
TEST_P(BufferedPacketStoreTest, ReceiveCHLOAfterExpiration) {
  InSequence s;
  CreateTimeWaitListManager();
  QuicBufferedPacketStore* store =
      QuicDispatcherPeer::GetBufferedPackets(dispatcher_.get());
  QuicBufferedPacketStorePeer::set_clock(store, mock_helper_.GetClock());

  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  server_address_ = QuicSocketAddress(QuicIpAddress::Any4(), 5);
  QuicConnectionId conn_id = 1;
  ProcessPacket(client_address, conn_id, true, QuicStrCat("data packet ", 2),
                PACKET_8BYTE_CONNECTION_ID, PACKET_4BYTE_PACKET_NUMBER,
                /*packet_number=*/2);

  mock_helper_.AdvanceTime(
      QuicTime::Delta::FromSeconds(kInitialIdleTimeoutSecs));
  QuicAlarm* alarm = QuicBufferedPacketStorePeer::expiration_alarm(store);
  // Cancel alarm as if it had been fired.
  alarm->Cancel();
  store->OnExpirationTimeout();
  // New arrived CHLO will be dropped because this connection is in time wait
  // list.
  ASSERT_TRUE(time_wait_list_manager_->IsConnectionIdInTimeWait(conn_id));
  EXPECT_CALL(*time_wait_list_manager_, ProcessPacket(_, _, conn_id));
  ProcessPacket(client_address, conn_id, true, SerializeFullCHLO());
}

TEST_P(BufferedPacketStoreTest, ProcessCHLOsUptoLimitAndBufferTheRest) {
  // Process more than (|kMaxNumSessionsToCreate| +
  // |kDefaultMaxConnectionsInStore|) CHLOs,
  // the first |kMaxNumSessionsToCreate| should create connections immediately,
  // the next |kDefaultMaxConnectionsInStore| should be buffered,
  // the rest should be dropped.
  QuicBufferedPacketStore* store =
      QuicDispatcherPeer::GetBufferedPackets(dispatcher_.get());
  const size_t kNumCHLOs =
      kMaxNumSessionsToCreate + kDefaultMaxConnectionsInStore + 1;
  for (size_t conn_id = 1; conn_id <= kNumCHLOs; ++conn_id) {
    EXPECT_CALL(*dispatcher_, ShouldCreateOrBufferPacketForConnection(conn_id));
    if (conn_id <= kMaxNumSessionsToCreate) {
      EXPECT_CALL(*dispatcher_,
                  CreateQuicSession(conn_id, client_addr_, QuicStringPiece()))
          .WillOnce(testing::Return(CreateSession(
              dispatcher_.get(), config_, conn_id, client_addr_, &mock_helper_,
              &mock_alarm_factory_, &crypto_config_,
              QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_)));
      EXPECT_CALL(
          *reinterpret_cast<MockQuicConnection*>(session1_->connection()),
          ProcessUdpPacket(_, _, _))
          .WillOnce(WithArg<2>(
              Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
                ValidatePacket(conn_id, packet);
              })));
    }
    ProcessPacket(client_addr_, conn_id, true, SerializeFullCHLO());
    if (conn_id <= kMaxNumSessionsToCreate + kDefaultMaxConnectionsInStore &&
        conn_id > kMaxNumSessionsToCreate) {
      EXPECT_TRUE(store->HasChloForConnection(conn_id));
    } else {
      // First |kMaxNumSessionsToCreate| CHLOs should be passed to new
      // connections immediately, and the last CHLO should be dropped as the
      // store is full.
      EXPECT_FALSE(store->HasChloForConnection(conn_id));
    }
  }

  // Graduately consume buffered CHLOs. The buffered connections should be
  // created but the dropped one shouldn't.
  for (size_t conn_id = kMaxNumSessionsToCreate + 1;
       conn_id <= kMaxNumSessionsToCreate + kDefaultMaxConnectionsInStore;
       ++conn_id) {
    EXPECT_CALL(*dispatcher_,
                CreateQuicSession(conn_id, client_addr_, QuicStringPiece()))
        .WillOnce(testing::Return(CreateSession(
            dispatcher_.get(), config_, conn_id, client_addr_, &mock_helper_,
            &mock_alarm_factory_, &crypto_config_,
            QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_)));
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
                ProcessUdpPacket(_, _, _))
        .WillOnce(WithArg<2>(
            Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
              ValidatePacket(conn_id, packet);
            })));
  }
  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(kNumCHLOs, client_addr_, QuicStringPiece()))
      .Times(0);

  while (store->HasChlosBuffered()) {
    dispatcher_->ProcessBufferedChlos(kMaxNumSessionsToCreate);
  }

  EXPECT_EQ(static_cast<size_t>(kMaxNumSessionsToCreate) +
                kDefaultMaxConnectionsInStore,
            session1_->connection_id());
}

// Duplicated CHLO shouldn't be buffered.
TEST_P(BufferedPacketStoreTest, BufferDuplicatedCHLO) {
  for (QuicConnectionId conn_id = 1; conn_id <= kMaxNumSessionsToCreate + 1;
       ++conn_id) {
    // Last CHLO will be buffered. Others will create connection right away.
    if (conn_id <= kMaxNumSessionsToCreate) {
      EXPECT_CALL(*dispatcher_,
                  CreateQuicSession(conn_id, client_addr_, QuicStringPiece()))
          .WillOnce(testing::Return(CreateSession(
              dispatcher_.get(), config_, conn_id, client_addr_, &mock_helper_,
              &mock_alarm_factory_, &crypto_config_,
              QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_)));
      EXPECT_CALL(
          *reinterpret_cast<MockQuicConnection*>(session1_->connection()),
          ProcessUdpPacket(_, _, _))
          .WillOnce(WithArg<2>(
              Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
                ValidatePacket(conn_id, packet);
              })));
    }
    ProcessPacket(client_addr_, conn_id, true, SerializeFullCHLO());
  }
  // Retransmit CHLO on last connection should be dropped.
  QuicConnectionId last_connection = kMaxNumSessionsToCreate + 1;
  ProcessPacket(client_addr_, last_connection, true, SerializeFullCHLO());

  size_t packets_buffered = 2;

  // Reset counter and process buffered CHLO.
  EXPECT_CALL(*dispatcher_, CreateQuicSession(last_connection, client_addr_,
                                              QuicStringPiece()))
      .WillOnce(testing::Return(CreateSession(
          dispatcher_.get(), config_, last_connection, client_addr_,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_)));
  // Only one packet(CHLO) should be process.
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .Times(packets_buffered)
      .WillRepeatedly(WithArg<2>(
          Invoke([this, last_connection](const QuicEncryptedPacket& packet) {
            ValidatePacket(last_connection, packet);
          })));
  dispatcher_->ProcessBufferedChlos(kMaxNumSessionsToCreate);
}

TEST_P(BufferedPacketStoreTest, BufferNonChloPacketsUptoLimitWithChloBuffered) {
  QuicConnectionId last_connection_id = kMaxNumSessionsToCreate + 1;
  for (QuicConnectionId conn_id = 1; conn_id <= last_connection_id; ++conn_id) {
    // Last CHLO will be buffered. Others will create connection right away.
    if (conn_id <= kMaxNumSessionsToCreate) {
      EXPECT_CALL(*dispatcher_,
                  CreateQuicSession(conn_id, client_addr_, QuicStringPiece()))
          .WillOnce(testing::Return(CreateSession(
              dispatcher_.get(), config_, conn_id, client_addr_, &mock_helper_,
              &mock_alarm_factory_, &crypto_config_,
              QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_)));
      EXPECT_CALL(
          *reinterpret_cast<MockQuicConnection*>(session1_->connection()),
          ProcessUdpPacket(_, _, _))
          .WillRepeatedly(WithArg<2>(
              Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
                ValidatePacket(conn_id, packet);
              })));
    }
    ProcessPacket(client_addr_, conn_id, true, SerializeFullCHLO());
  }

  // Process another |kDefaultMaxUndecryptablePackets| + 1 data packets. The
  // last one should be dropped.
  for (QuicPacketNumber packet_number = 2;
       packet_number <= kDefaultMaxUndecryptablePackets + 2; ++packet_number) {
    ProcessPacket(client_addr_, last_connection_id, true, "data packet");
  }

  // Reset counter and process buffered CHLO.
  EXPECT_CALL(*dispatcher_, CreateQuicSession(last_connection_id, client_addr_,
                                              QuicStringPiece()))
      .WillOnce(testing::Return(CreateSession(
          dispatcher_.get(), config_, last_connection_id, client_addr_,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_)));
  // Only CHLO and following |kDefaultMaxUndecryptablePackets| data packets
  // should be process.
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .Times(kDefaultMaxUndecryptablePackets + 1)
      .WillRepeatedly(WithArg<2>(
          Invoke([this, last_connection_id](const QuicEncryptedPacket& packet) {
            ValidatePacket(last_connection_id, packet);
          })));
  dispatcher_->ProcessBufferedChlos(kMaxNumSessionsToCreate);
}

// Tests that when dispatcher's packet buffer is full, a CHLO on connection
// which doesn't have buffered CHLO should be buffered.
TEST_P(BufferedPacketStoreTest, ReceiveCHLOForBufferedConnection) {
  QuicBufferedPacketStore* store =
      QuicDispatcherPeer::GetBufferedPackets(dispatcher_.get());

  QuicConnectionId conn_id = 1;
  ProcessPacket(client_addr_, conn_id, true, "data packet",
                PACKET_8BYTE_CONNECTION_ID, PACKET_4BYTE_PACKET_NUMBER,
                /*packet_number=*/1);
  // Fill packet buffer to full with CHLOs on other connections. Need to feed
  // extra CHLOs because the first |kMaxNumSessionsToCreate| are going to create
  // session directly.
  for (conn_id = 2;
       conn_id <= kDefaultMaxConnectionsInStore + kMaxNumSessionsToCreate;
       ++conn_id) {
    if (conn_id <= kMaxNumSessionsToCreate + 1) {
      EXPECT_CALL(*dispatcher_,
                  CreateQuicSession(conn_id, client_addr_, QuicStringPiece()))
          .WillOnce(testing::Return(CreateSession(
              dispatcher_.get(), config_, conn_id, client_addr_, &mock_helper_,
              &mock_alarm_factory_, &crypto_config_,
              QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_)));
      EXPECT_CALL(
          *reinterpret_cast<MockQuicConnection*>(session1_->connection()),
          ProcessUdpPacket(_, _, _))
          .WillOnce(WithArg<2>(
              Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
                ValidatePacket(conn_id, packet);
              })));
    }
    ProcessPacket(client_addr_, conn_id, true, SerializeFullCHLO());
  }
  EXPECT_FALSE(store->HasChloForConnection(/*connection_id=*/1));

  // CHLO on connection 1 should still be buffered.
  ProcessPacket(client_addr_, /*connection_id=*/1, true, SerializeFullCHLO());
  EXPECT_TRUE(store->HasChloForConnection(/*connection_id=*/1));
}

// Test which exercises the async GetProof codepaths, especially in the context
// of stateless rejection.
class AsyncGetProofTest : public QuicDispatcherTest {
 public:
  AsyncGetProofTest()
      : QuicDispatcherTest(
            std::unique_ptr<FakeProofSource>(new FakeProofSource())),
        client_addr_(QuicIpAddress::Loopback4(), 1234),
        client_addr_2_(QuicIpAddress::Loopback4(), 1357),
        crypto_config_peer_(&crypto_config_),
        signed_config_(new QuicSignedServerConfig) {
    SetQuicReloadableFlag(enable_quic_stateless_reject_support, true);
    SetQuicReloadableFlag(quic_use_cheap_stateless_rejects, true);
  }

  void SetUp() override {
    QuicDispatcherTest::SetUp();

    clock_ = QuicDispatcherPeer::GetHelper(dispatcher_.get())->GetClock();
    QuicTransportVersion version = AllSupportedTransportVersions().front();
    chlo_ = crypto_test_utils::GenerateDefaultInchoateCHLO(clock_, version,
                                                           &crypto_config_);
    chlo_.SetVector(kCOPT, QuicTagVector{kSREJ});
    chlo_.SetStringPiece(kALPN, "HTTP/1");
    // Pass an inchoate CHLO.
    crypto_test_utils::GenerateFullCHLO(
        chlo_, &crypto_config_, server_addr_, client_addr_, version, clock_,
        signed_config_, QuicDispatcherPeer::GetCache(dispatcher_.get()),
        &full_chlo_);

    crypto_test_utils::GenerateFullCHLO(
        chlo_, &crypto_config_, server_addr_, client_addr_2_, version, clock_,
        signed_config_, QuicDispatcherPeer::GetCache(dispatcher_.get()),
        &full_chlo_2_);

    GetFakeProofSource()->Activate();
  }

  FakeProofSource* GetFakeProofSource() const {
    return static_cast<FakeProofSource*>(crypto_config_peer_.GetProofSource());
  }

  QuicString SerializeFullCHLO() {
    return QuicString(full_chlo_.GetSerialized().AsStringPiece());
  }

  QuicString SerializeFullCHLOForClient2() {
    return QuicString(full_chlo_2_.GetSerialized().AsStringPiece());
  }

  QuicString SerializeCHLO() {
    return QuicString(chlo_.GetSerialized().AsStringPiece());
  }

  // Sets up a session, and crypto stream based on the test parameters.
  QuicServerSessionBase* GetSession(QuicConnectionId connection_id,
                                    QuicSocketAddress client_address) {
    auto it = sessions_.find(connection_id);
    if (it != sessions_.end()) {
      return it->second.session;
    }

    TestQuicSpdyServerSession* session;
    CreateSession(dispatcher_.get(), config_, connection_id, client_address,
                  &mock_helper_, &mock_alarm_factory_, &crypto_config_,
                  QuicDispatcherPeer::GetCache(dispatcher_.get()), &session);

    std::unique_ptr<MockQuicCryptoServerStream> crypto_stream(
        new MockQuicCryptoServerStream(
            crypto_config_, QuicDispatcherPeer::GetCache(dispatcher_.get()),
            session, session->stream_helper()));
    session->SetCryptoStream(crypto_stream.get());
    crypto_stream->SetPeerSupportsStatelessRejects(true);
    const bool ok =
        sessions_
            .insert(std::make_pair(
                connection_id, SessionInfo{session, std::move(crypto_stream)}))
            .second;
    CHECK(ok);
    return session;
  }

 protected:
  const QuicSocketAddress client_addr_;
  const QuicSocketAddress client_addr_2_;

 private:
  QuicCryptoServerConfigPeer crypto_config_peer_;
  QuicSocketAddress server_addr_;
  QuicReferenceCountedPointer<QuicSignedServerConfig> signed_config_;
  const QuicClock* clock_;
  CryptoHandshakeMessage chlo_;
  CryptoHandshakeMessage full_chlo_;    // CHLO for client_addr_
  CryptoHandshakeMessage full_chlo_2_;  // CHLO for client_addr_2_

  struct SessionInfo {
    TestQuicSpdyServerSession* session;
    std::unique_ptr<MockQuicCryptoServerStream> crypto_stream;
  };
  std::map<QuicConnectionId, SessionInfo> sessions_;
};

// Test a simple situation of connections which the StatelessRejector will
// accept.
TEST_F(AsyncGetProofTest, BasicAccept) {
  QuicConnectionId conn_id = 1;

  testing::MockFunction<void(int check_point)> check;
  {
    InSequence s;

    EXPECT_CALL(check, Call(1));
    EXPECT_CALL(*dispatcher_, ShouldCreateOrBufferPacketForConnection(conn_id));
    EXPECT_CALL(*dispatcher_, CreateQuicSession(conn_id, client_addr_,
                                                QuicStringPiece("HTTP/1")))
        .WillOnce(testing::Return(GetSession(conn_id, client_addr_)));
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(
                    GetSession(conn_id, client_addr_)->connection()),
                ProcessUdpPacket(_, _, _))
        .WillOnce(WithArg<2>(
            Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
              ValidatePacket(conn_id, packet);
            })));

    EXPECT_CALL(check, Call(2));
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(
                    GetSession(conn_id, client_addr_)->connection()),
                ProcessUdpPacket(_, _, _))
        .WillOnce(WithArg<2>(
            Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
              ValidatePacket(conn_id, packet);
            })));
  }

  // Send a CHLO that the StatelessRejector will accept.
  ProcessPacket(client_addr_, conn_id, true, SerializeFullCHLO());
  ASSERT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 1);

  check.Call(1);
  // Complete the ProofSource::GetProof call and verify that a session is
  // created.
  GetFakeProofSource()->InvokePendingCallback(0);
  ASSERT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 0);

  check.Call(2);
  // Verify that a data packet gets processed immediately.
  ProcessPacket(client_addr_, conn_id, true, "My name is Data");
}

TEST_F(AsyncGetProofTest, RestorePacketContext) {
  QuicConnectionId conn_id_1 = 1;
  QuicConnectionId conn_id_2 = 2;

  testing::MockFunction<void(int check_point)> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call(1));
    EXPECT_CALL(*dispatcher_,
                ShouldCreateOrBufferPacketForConnection(conn_id_1));

    EXPECT_CALL(*dispatcher_, CreateQuicSession(conn_id_1, client_addr_,
                                                QuicStringPiece("HTTP/1")))
        .WillOnce(testing::Return(GetSession(conn_id_1, client_addr_)));
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(
                    GetSession(conn_id_1, client_addr_)->connection()),
                ProcessUdpPacket(_, _, _))
        .WillRepeatedly(WithArg<2>(
            Invoke([this, conn_id_1](const QuicEncryptedPacket& packet) {
              ValidatePacket(conn_id_1, packet);
            })));

    EXPECT_CALL(check, Call(2));

    EXPECT_CALL(*dispatcher_,
                ShouldCreateOrBufferPacketForConnection(conn_id_2));
    EXPECT_CALL(*dispatcher_, CreateQuicSession(conn_id_2, client_addr_2_,
                                                QuicStringPiece("HTTP/1")))
        .WillOnce(testing::Return(GetSession(conn_id_2, client_addr_2_)));
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(
                    GetSession(conn_id_2, client_addr_2_)->connection()),
                ProcessUdpPacket(_, _, _))
        .WillOnce(WithArg<2>(
            Invoke([this, conn_id_2](const QuicEncryptedPacket& packet) {
              ValidatePacket(conn_id_2, packet);
            })));
  }

  // Send a CHLO that the StatelessRejector will accept.
  dispatcher_->custom_packet_context_ = "connection 1";
  ProcessPacket(client_addr_, conn_id_1, true, SerializeFullCHLO());
  ASSERT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 1);

  // Send another CHLO that the StatelessRejector will accept.
  dispatcher_->custom_packet_context_ = "connection 2";
  ProcessPacket(client_addr_2_, conn_id_2, true, SerializeFullCHLOForClient2());
  ASSERT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 2);

  // Complete the first ProofSource::GetProof call and verify that a session is
  // created.
  check.Call(1);

  EXPECT_EQ(client_addr_2_, dispatcher_->current_client_address());
  EXPECT_EQ(client_addr_2_, dispatcher_->current_peer_address());
  EXPECT_EQ("connection 2", dispatcher_->custom_packet_context_);

  // Runs the async proof callback for conn_id_1 from client_addr_.
  GetFakeProofSource()->InvokePendingCallback(0);

  EXPECT_EQ(client_addr_, dispatcher_->current_client_address());
  EXPECT_EQ(client_addr_, dispatcher_->current_peer_address());
  EXPECT_EQ("connection 1", dispatcher_->custom_packet_context_);

  ASSERT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 1);

  // Complete the second ProofSource::GetProof call and verify that a session is
  // created.
  check.Call(2);

  EXPECT_EQ(client_addr_, dispatcher_->current_client_address());
  EXPECT_EQ(client_addr_, dispatcher_->current_peer_address());
  EXPECT_EQ("connection 1", dispatcher_->custom_packet_context_);

  // Runs the async proof callback for conn_id_2 from client_addr_2_.
  GetFakeProofSource()->InvokePendingCallback(0);

  EXPECT_EQ(client_addr_2_, dispatcher_->current_client_address());
  EXPECT_EQ(client_addr_2_, dispatcher_->current_peer_address());
  EXPECT_EQ("connection 2", dispatcher_->custom_packet_context_);

  ASSERT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 0);
}

// Test a simple situation of connections which the StatelessRejector will
// reject.
TEST_F(AsyncGetProofTest, BasicReject) {
  CreateTimeWaitListManager();

  QuicConnectionId conn_id = 1;

  testing::MockFunction<void(int check_point)> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call(1));
    EXPECT_CALL(*time_wait_list_manager_,
                AddConnectionIdToTimeWait(conn_id, _, _, _));
    EXPECT_CALL(*time_wait_list_manager_,
                ProcessPacket(_, client_addr_, conn_id));

    EXPECT_CALL(check, Call(2));
    EXPECT_CALL(*dispatcher_,
                CreateQuicSession(conn_id, client_addr_, QuicStringPiece("hq")))
        .Times(0);
    EXPECT_CALL(*time_wait_list_manager_,
                ProcessPacket(_, client_addr_, conn_id));
  }

  // Send a CHLO that the StatelessRejector will reject.
  ProcessPacket(client_addr_, conn_id, true, SerializeCHLO());
  ASSERT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 1);

  // Complete the ProofSource::GetProof call and verify that the connection and
  // packet are processed by the time wait list manager.
  check.Call(1);
  GetFakeProofSource()->InvokePendingCallback(0);
  ASSERT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 0);

  // Verify that a data packet is passed to the time wait list manager.
  check.Call(2);
  ProcessPacket(client_addr_, conn_id, true, "My name is Data");
}

// Test a situation with multiple interleaved connections which the
// StatelessRejector will accept.
TEST_F(AsyncGetProofTest, MultipleAccept) {
  QuicConnectionId conn_id_1 = 1;
  QuicConnectionId conn_id_2 = 2;
  QuicBufferedPacketStore* store =
      QuicDispatcherPeer::GetBufferedPackets(dispatcher_.get());

  testing::MockFunction<void(int check_point)> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call(1));
    EXPECT_CALL(*dispatcher_,
                ShouldCreateOrBufferPacketForConnection(conn_id_2));
    EXPECT_CALL(*dispatcher_, CreateQuicSession(conn_id_2, client_addr_,
                                                QuicStringPiece("HTTP/1")))
        .WillOnce(testing::Return(GetSession(conn_id_2, client_addr_)));
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(
                    GetSession(conn_id_2, client_addr_)->connection()),
                ProcessUdpPacket(_, _, _))
        .WillOnce(WithArg<2>(
            Invoke([this, conn_id_2](const QuicEncryptedPacket& packet) {
              ValidatePacket(conn_id_2, packet);
            })));

    EXPECT_CALL(check, Call(2));
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(
                    GetSession(conn_id_2, client_addr_)->connection()),
                ProcessUdpPacket(_, _, _))
        .WillOnce(WithArg<2>(
            Invoke([this, conn_id_2](const QuicEncryptedPacket& packet) {
              ValidatePacket(conn_id_2, packet);
            })));

    EXPECT_CALL(check, Call(3));
    EXPECT_CALL(*dispatcher_,
                ShouldCreateOrBufferPacketForConnection(conn_id_1));

    EXPECT_CALL(check, Call(4));
    EXPECT_CALL(*dispatcher_, CreateQuicSession(conn_id_1, client_addr_,
                                                QuicStringPiece("HTTP/1")))
        .WillOnce(testing::Return(GetSession(conn_id_1, client_addr_)));
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(
                    GetSession(conn_id_1, client_addr_)->connection()),
                ProcessUdpPacket(_, _, _))
        .WillRepeatedly(WithArg<2>(
            Invoke([this, conn_id_1](const QuicEncryptedPacket& packet) {
              ValidatePacket(conn_id_1, packet);
            })));
  }

  // Send a CHLO that the StatelessRejector will accept.
  ProcessPacket(client_addr_, conn_id_1, true, SerializeFullCHLO());
  ASSERT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 1);

  // Send another CHLO that the StatelessRejector will accept.
  ProcessPacket(client_addr_, conn_id_2, true, SerializeFullCHLO());
  ASSERT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 2);

  // Complete the second ProofSource::GetProof call and verify that a session is
  // created.
  check.Call(1);
  GetFakeProofSource()->InvokePendingCallback(1);
  ASSERT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 1);

  // Verify that a data packet on that connection gets processed immediately.
  check.Call(2);
  ProcessPacket(client_addr_, conn_id_2, true, "My name is Data");

  // Verify that a data packet on the other connection does not get processed
  // yet.
  check.Call(3);
  ProcessPacket(client_addr_, conn_id_1, true, "My name is Data");
  EXPECT_TRUE(store->HasBufferedPackets(conn_id_1));
  EXPECT_FALSE(store->HasBufferedPackets(conn_id_2));

  // Complete the first ProofSource::GetProof call and verify that a session is
  // created and the buffered packet is processed.
  check.Call(4);
  GetFakeProofSource()->InvokePendingCallback(0);
  ASSERT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 0);
}

// Test a situation with multiple interleaved connections which the
// StatelessRejector will reject.
TEST_F(AsyncGetProofTest, MultipleReject) {
  CreateTimeWaitListManager();

  QuicConnectionId conn_id_1 = 1;
  QuicConnectionId conn_id_2 = 2;
  QuicBufferedPacketStore* store =
      QuicDispatcherPeer::GetBufferedPackets(dispatcher_.get());

  testing::MockFunction<void(int check_point)> check;
  {
    InSequence s;

    EXPECT_CALL(check, Call(1));
    EXPECT_CALL(*dispatcher_, CreateQuicSession(conn_id_2, client_addr_, _))
        .Times(0);
    EXPECT_CALL(*time_wait_list_manager_,
                AddConnectionIdToTimeWait(conn_id_2, _, _, _));
    EXPECT_CALL(*time_wait_list_manager_,
                ProcessPacket(_, client_addr_, conn_id_2));

    EXPECT_CALL(check, Call(2));
    EXPECT_CALL(*time_wait_list_manager_,
                ProcessPacket(_, client_addr_, conn_id_2));

    EXPECT_CALL(check, Call(3));
    EXPECT_CALL(*dispatcher_,
                ShouldCreateOrBufferPacketForConnection(conn_id_1));

    EXPECT_CALL(check, Call(4));
    EXPECT_CALL(*time_wait_list_manager_,
                AddConnectionIdToTimeWait(conn_id_1, _, _, _));
    EXPECT_CALL(*time_wait_list_manager_,
                ProcessPacket(_, client_addr_, conn_id_1));
  }

  // Send a CHLO that the StatelessRejector will reject.
  ProcessPacket(client_addr_, conn_id_1, true, SerializeCHLO());
  ASSERT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 1);

  // Send another CHLO that the StatelessRejector will reject.
  ProcessPacket(client_addr_, conn_id_2, true, SerializeCHLO());
  ASSERT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 2);

  // Complete the second ProofSource::GetProof call and verify that the
  // connection and packet are processed by the time wait manager.
  check.Call(1);
  GetFakeProofSource()->InvokePendingCallback(1);
  ASSERT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 1);

  // Verify that a data packet on that connection gets processed immediately by
  // the time wait manager.
  check.Call(2);
  ProcessPacket(client_addr_, conn_id_2, true, "My name is Data");

  // Verify that a data packet on the first connection gets buffered.
  check.Call(3);
  ProcessPacket(client_addr_, conn_id_1, true, "My name is Data");
  EXPECT_TRUE(store->HasBufferedPackets(conn_id_1));
  EXPECT_FALSE(store->HasBufferedPackets(conn_id_2));

  // Complete the first ProofSource::GetProof call and verify that the CHLO is
  // processed by the time wait manager and the remaining packets are discarded.
  check.Call(4);
  GetFakeProofSource()->InvokePendingCallback(0);
  ASSERT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 0);
  EXPECT_FALSE(store->HasBufferedPackets(conn_id_1));
  EXPECT_FALSE(store->HasBufferedPackets(conn_id_2));
}

// Test a situation with multiple identical CHLOs which the StatelessRejector
// will reject.
TEST_F(AsyncGetProofTest, MultipleIdenticalReject) {
  CreateTimeWaitListManager();

  QuicConnectionId conn_id_1 = 1;
  QuicBufferedPacketStore* store =
      QuicDispatcherPeer::GetBufferedPackets(dispatcher_.get());

  testing::MockFunction<void(int check_point)> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call(1));
    EXPECT_CALL(*dispatcher_,
                ShouldCreateOrBufferPacketForConnection(conn_id_1));

    EXPECT_CALL(check, Call(2));
    EXPECT_CALL(*dispatcher_,
                CreateQuicSession(conn_id_1, client_addr_, QuicStringPiece()))
        .Times(0);
    EXPECT_CALL(*time_wait_list_manager_,
                AddConnectionIdToTimeWait(conn_id_1, _, _, _));
    EXPECT_CALL(*time_wait_list_manager_,
                ProcessPacket(_, client_addr_, conn_id_1));
  }

  // Send a CHLO that the StatelessRejector will reject.
  ProcessPacket(client_addr_, conn_id_1, true, SerializeCHLO());
  ASSERT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 1);
  EXPECT_FALSE(store->HasBufferedPackets(conn_id_1));

  // Send an identical CHLO which should get buffered.
  check.Call(1);
  ProcessPacket(client_addr_, conn_id_1, true, SerializeCHLO());
  ASSERT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 1);
  EXPECT_TRUE(store->HasBufferedPackets(conn_id_1));

  // Complete the ProofSource::GetProof call and verify that the CHLO is
  // rejected and the copy is discarded.
  check.Call(2);
  GetFakeProofSource()->InvokePendingCallback(0);
  ASSERT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 0);
  EXPECT_FALSE(store->HasBufferedPackets(conn_id_1));
}

// Test dispatcher behavior when packets time out of the buffer while CHLO
// validation is still pending.
TEST_F(AsyncGetProofTest, BufferTimeout) {
  CreateTimeWaitListManager();

  QuicConnectionId conn_id = 1;
  QuicBufferedPacketStore* store =
      QuicDispatcherPeer::GetBufferedPackets(dispatcher_.get());
  QuicBufferedPacketStorePeer::set_clock(store, mock_helper_.GetClock());

  testing::MockFunction<void(int check_point)> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call(1));
    EXPECT_CALL(*dispatcher_, ShouldCreateOrBufferPacketForConnection(conn_id));

    EXPECT_CALL(check, Call(2));
    EXPECT_CALL(*time_wait_list_manager_,
                ProcessPacket(_, client_addr_, conn_id));
    EXPECT_CALL(*dispatcher_,
                CreateQuicSession(conn_id, client_addr_, QuicStringPiece()))
        .Times(0);
  }

  // Send a CHLO that the StatelessRejector will accept.
  ProcessPacket(client_addr_, conn_id, true, SerializeFullCHLO());
  ASSERT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 1);
  EXPECT_FALSE(store->HasBufferedPackets(conn_id));

  // Send a data packet that will get buffered
  check.Call(1);
  ProcessPacket(client_addr_, conn_id, true, "My name is Data");
  EXPECT_TRUE(store->HasBufferedPackets(conn_id));

  // Pretend that enough time has gone by for the packets to get expired out of
  // the buffer
  mock_helper_.AdvanceTime(
      QuicTime::Delta::FromSeconds(kInitialIdleTimeoutSecs));
  QuicBufferedPacketStorePeer::expiration_alarm(store)->Cancel();
  store->OnExpirationTimeout();
  EXPECT_FALSE(store->HasBufferedPackets(conn_id));
  EXPECT_TRUE(time_wait_list_manager_->IsConnectionIdInTimeWait(conn_id));

  // Now allow the CHLO validation to complete, and verify that no connection
  // gets created.
  check.Call(2);
  GetFakeProofSource()->InvokePendingCallback(0);
  ASSERT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 0);
  EXPECT_FALSE(store->HasBufferedPackets(conn_id));
  EXPECT_TRUE(time_wait_list_manager_->IsConnectionIdInTimeWait(conn_id));
}

// Test behavior when packets time out of the buffer *and* the connection times
// out of the time wait manager while CHLO validation is still pending.  This
// *should* be impossible, but anything can happen with timing conditions.
TEST_F(AsyncGetProofTest, TimeWaitTimeout) {
  QuicConnectionId conn_id = 1;
  QuicBufferedPacketStore* store =
      QuicDispatcherPeer::GetBufferedPackets(dispatcher_.get());
  QuicBufferedPacketStorePeer::set_clock(store, mock_helper_.GetClock());
  CreateTimeWaitListManager();
  QuicTimeWaitListManagerPeer::set_clock(time_wait_list_manager_,
                                         mock_helper_.GetClock());

  testing::MockFunction<void(int check_point)> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call(1));
    EXPECT_CALL(*dispatcher_, ShouldCreateOrBufferPacketForConnection(conn_id));

    EXPECT_CALL(check, Call(2));
    EXPECT_CALL(*dispatcher_, ShouldCreateOrBufferPacketForConnection(conn_id));
    EXPECT_CALL(*dispatcher_, CreateQuicSession(conn_id, client_addr_,
                                                QuicStringPiece("HTTP/1")))
        .WillOnce(testing::Return(GetSession(conn_id, client_addr_)));
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(
                    GetSession(conn_id, client_addr_)->connection()),
                ProcessUdpPacket(_, _, _))
        .WillOnce(WithArg<2>(
            Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
              ValidatePacket(conn_id, packet);
            })));
  }

  // Send a CHLO that the StatelessRejector will accept.
  ProcessPacket(client_addr_, conn_id, true, SerializeFullCHLO());
  ASSERT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 1);
  EXPECT_FALSE(store->HasBufferedPackets(conn_id));

  // Send a data packet that will get buffered
  check.Call(1);
  ProcessPacket(client_addr_, conn_id, true, "My name is Data");
  EXPECT_TRUE(store->HasBufferedPackets(conn_id));

  // Pretend that enough time has gone by for the packets to get expired out of
  // the buffer
  mock_helper_.AdvanceTime(
      QuicTime::Delta::FromSeconds(kInitialIdleTimeoutSecs));
  QuicBufferedPacketStorePeer::expiration_alarm(store)->Cancel();
  store->OnExpirationTimeout();
  EXPECT_FALSE(store->HasBufferedPackets(conn_id));
  EXPECT_TRUE(time_wait_list_manager_->IsConnectionIdInTimeWait(conn_id));

  // Pretend that enough time has gone by for the connection ID to be removed
  // from the time wait manager
  mock_helper_.AdvanceTime(
      QuicTimeWaitListManagerPeer::time_wait_period(time_wait_list_manager_));
  QuicTimeWaitListManagerPeer::expiration_alarm(time_wait_list_manager_)
      ->Cancel();
  time_wait_list_manager_->CleanUpOldConnectionIds();
  EXPECT_FALSE(time_wait_list_manager_->IsConnectionIdInTimeWait(conn_id));

  // Now allow the CHLO validation to complete.  Expect that a connection is
  // indeed created, since QUIC has forgotten that this connection ever existed.
  // This is a miniscule corner case which should never happen in the wild, so
  // really we are just verifying that the dispatcher does not explode in this
  // situation.
  check.Call(2);
  GetFakeProofSource()->InvokePendingCallback(0);
  ASSERT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 0);
  EXPECT_FALSE(store->HasBufferedPackets(conn_id));
  EXPECT_FALSE(time_wait_list_manager_->IsConnectionIdInTimeWait(conn_id));
}

// Regression test for
// https://bugs.chromium.org/p/chromium/issues/detail?id=748289
TEST_F(AsyncGetProofTest, DispatcherFailedToPickUpVersionForAsyncProof) {
  // This test mimics the scenario that dispatcher's framer can have different
  // version when async proof returns.
  // When dispatcher sends SREJ, the SREJ frame can be serialized in
  // different endianness which causes the client to close the connection
  // because of QUIC_INVALID_STREAM_DATA.

  SetQuicReloadableFlag(quic_disable_version_35, false);

  // Send a CHLO with v39. Dispatcher framer's version is set to v39.
  ProcessPacket(client_addr_, 1, true,
                ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_39),
                SerializeCHLO(), PACKET_8BYTE_CONNECTION_ID,
                PACKET_4BYTE_PACKET_NUMBER, 1);

  // Send another CHLO with v35. Dispatcher framer's version is set to v35.
  ProcessPacket(client_addr_, 2, true,
                ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_35),
                SerializeCHLO(), PACKET_8BYTE_CONNECTION_ID,
                PACKET_4BYTE_PACKET_NUMBER, 1);
  ASSERT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 2);

  // Complete the ProofSource::GetProof call for v39. This would cause the
  // version mismatch between the CHLO packet and the dispatcher.
  GetFakeProofSource()->InvokePendingCallback(0);
  ASSERT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 1);
}

// Regression test for b/116200989.
TEST_F(AsyncGetProofTest, DispatcherHasWrongLastPacketIsIetfQuic) {
  SetQuicReloadableFlag(quic_fix_last_packet_is_ietf_quic, true);

  // Process a packet of v44.
  ProcessPacket(client_addr_, 1, true,
                ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_44),
                SerializeCHLO(), PACKET_8BYTE_CONNECTION_ID,
                PACKET_4BYTE_PACKET_NUMBER, 1);
  EXPECT_TRUE(dispatcher_->framer()->last_packet_is_ietf_quic());

  // Process another packet of v43.
  ProcessPacket(client_addr_, 2, true,
                ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_43),
                SerializeCHLO(), PACKET_8BYTE_CONNECTION_ID,
                PACKET_4BYTE_PACKET_NUMBER, 1);
  EXPECT_FALSE(dispatcher_->framer()->last_packet_is_ietf_quic());
  ASSERT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 2);

  // Complete the ProofSource::GetProof call for v44.
  GetFakeProofSource()->InvokePendingCallback(0);
  // Verify the last_packet_is_ietf_quic gets reset properly.
  EXPECT_TRUE(dispatcher_->framer()->last_packet_is_ietf_quic());
  ASSERT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 1);

  // Complete the ProofSource::GetProof call for v43.
  GetFakeProofSource()->InvokePendingCallback(0);
  EXPECT_FALSE(dispatcher_->framer()->last_packet_is_ietf_quic());
  ASSERT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 0);
}

}  // namespace
}  // namespace test
}  // namespace quic
