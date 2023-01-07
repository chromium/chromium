// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_simple_server.h"

#include <memory>

#include "net/quic/address_utils.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_random.h"
#include "net/third_party/quiche/src/quiche/quic/core/deterministic_connection_id_generator.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_crypto_stream.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_test.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_quic_dispatcher.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/quic_test_utils.h"
#include "net/third_party/quiche/src/quiche/quic/tools/quic_memory_cache_backend.h"
#include "net/tools/quic/quic_simple_server_session_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace net::test {

// TODO(dmz) Remove "Chrome" part of name once net/tools/quic is deleted.
class QuicChromeServerDispatchPacketTest : public ::testing::Test {
 public:
  QuicChromeServerDispatchPacketTest()
      : crypto_config_("blah",
                       quic::QuicRandom::GetInstance(),
                       quic::test::crypto_test_utils::ProofSourceForTesting(),
                       quic::KeyExchangeSource::Default()),
        version_manager_(quic::AllSupportedVersions()),
        connection_id_generator_(quic::kQuicDefaultConnectionIdLength),
        dispatcher_(std::make_unique<quic::test::MockQuicDispatcher>(
            &config_,
            &crypto_config_,
            &version_manager_,
            std::make_unique<quic::test::MockQuicConnectionHelper>(),
            std::make_unique<QuicSimpleServerSessionHelper>(
                quic::QuicRandom::GetInstance()),
            std::make_unique<quic::test::MockAlarmFactory>(),
            &memory_cache_backend_,
            connection_id_generator_)) {
    dispatcher_->InitializeWithWriter(nullptr);
  }

  void DispatchPacket(const quic::QuicReceivedPacket& packet) {
    IPEndPoint client_addr, server_addr;
    dispatcher_->ProcessPacket(ToQuicSocketAddress(server_addr),
                               ToQuicSocketAddress(client_addr), packet);
  }

 protected:
  quic::QuicConfig config_;
  quic::QuicCryptoServerConfig crypto_config_;
  quic::QuicVersionManager version_manager_;
  quic::DeterministicConnectionIdGenerator connection_id_generator_;
  std::unique_ptr<quic::test::MockQuicDispatcher> dispatcher_;
  quic::QuicMemoryCacheBackend memory_cache_backend_;
};

TEST_F(QuicChromeServerDispatchPacketTest, DispatchPacket) {
  unsigned char valid_packet[] = {// public flags (8 byte connection_id)
                                  0x3C,
                                  // connection_id
                                  0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC,
                                  0xFE,
                                  // packet sequence number
                                  0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12,
                                  // private flags
                                  0x00};
  quic::QuicReceivedPacket encrypted_valid_packet(
      reinterpret_cast<char*>(valid_packet), std::size(valid_packet),
      quic::QuicTime::Zero(), false);

  EXPECT_CALL(*dispatcher_, ProcessPacket(_, _, _)).Times(1);
  DispatchPacket(encrypted_valid_packet);
}

}  // namespace net::test
