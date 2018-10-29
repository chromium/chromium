// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/chlo_extractor.h"

#include <memory>

#include "net/third_party/quic/core/quic_framer.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/platform/api/quic_arraysize.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/test_tools/crypto_test_utils.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"

namespace quic {
namespace test {
namespace {

class TestDelegate : public ChloExtractor::Delegate {
 public:
  TestDelegate() = default;
  ~TestDelegate() override = default;

  // ChloExtractor::Delegate implementation
  void OnChlo(QuicTransportVersion version,
              QuicConnectionId connection_id,
              const CryptoHandshakeMessage& chlo) override {
    version_ = version;
    connection_id_ = connection_id;
    chlo_ = chlo.DebugString();
  }

  QuicConnectionId connection_id() const { return connection_id_; }
  QuicTransportVersion transport_version() const { return version_; }
  const QuicString& chlo() const { return chlo_; }

 private:
  QuicConnectionId connection_id_;
  QuicTransportVersion version_;
  QuicString chlo_;
};

class ChloExtractorTest : public QuicTest {
 public:
  ChloExtractorTest() {
    header_.destination_connection_id = 42;
    header_.destination_connection_id_length = PACKET_8BYTE_CONNECTION_ID;
    header_.version_flag = true;
    header_.version = AllSupportedVersions().front();
    header_.reset_flag = false;
    header_.packet_number_length = PACKET_4BYTE_PACKET_NUMBER;
    header_.packet_number = 1;
  }

  void MakePacket(const QuicStreamFrame& stream_frame) {
    QuicFrame frame(stream_frame);
    QuicFrames frames;
    frames.push_back(frame);
    QuicFramer framer(SupportedVersions(header_.version), QuicTime::Zero(),
                      Perspective::IS_CLIENT);
    std::unique_ptr<QuicPacket> packet(
        BuildUnsizedDataPacket(&framer, header_, frames));
    EXPECT_TRUE(packet != nullptr);
    size_t encrypted_length =
        framer.EncryptPayload(ENCRYPTION_NONE, header_.packet_number, *packet,
                              buffer_, QUIC_ARRAYSIZE(buffer_));
    ASSERT_NE(0u, encrypted_length);
    packet_ = QuicMakeUnique<QuicEncryptedPacket>(buffer_, encrypted_length);
    EXPECT_TRUE(packet_ != nullptr);
  }

 protected:
  TestDelegate delegate_;
  QuicPacketHeader header_;
  std::unique_ptr<QuicEncryptedPacket> packet_;
  char buffer_[kMaxPacketSize];
};

TEST_F(ChloExtractorTest, FindsValidChlo) {
  CryptoHandshakeMessage client_hello;
  client_hello.set_tag(kCHLO);

  QuicString client_hello_str(client_hello.GetSerialized().AsStringPiece());
  // Construct a CHLO with each supported version
  for (ParsedQuicVersion version : AllSupportedVersions()) {
    ParsedQuicVersionVector versions(SupportedVersions(version));
    header_.version = version;
    MakePacket(
        QuicStreamFrame(QuicUtils::GetCryptoStreamId(version.transport_version),
                        false, 0, client_hello_str));
    EXPECT_TRUE(ChloExtractor::Extract(*packet_, versions, {}, &delegate_))
        << ParsedQuicVersionToString(version);
    EXPECT_EQ(version.transport_version, delegate_.transport_version());
    EXPECT_EQ(header_.destination_connection_id, delegate_.connection_id());
    EXPECT_EQ(client_hello.DebugString(), delegate_.chlo())
        << ParsedQuicVersionToString(version);
  }
}

TEST_F(ChloExtractorTest, DoesNotFindValidChloOnWrongStream) {
  CryptoHandshakeMessage client_hello;
  client_hello.set_tag(kCHLO);

  QuicString client_hello_str(client_hello.GetSerialized().AsStringPiece());
  MakePacket(QuicStreamFrame(QuicUtils::GetCryptoStreamId(
                                 AllSupportedVersions()[0].transport_version) +
                                 1,
                             false, 0, client_hello_str));
  EXPECT_FALSE(
      ChloExtractor::Extract(*packet_, AllSupportedVersions(), {}, &delegate_));
}

TEST_F(ChloExtractorTest, DoesNotFindValidChloOnWrongOffset) {
  CryptoHandshakeMessage client_hello;
  client_hello.set_tag(kCHLO);

  QuicString client_hello_str(client_hello.GetSerialized().AsStringPiece());
  MakePacket(QuicStreamFrame(
      QuicUtils::GetCryptoStreamId(AllSupportedVersions()[0].transport_version),
      false, 1, client_hello_str));
  EXPECT_FALSE(
      ChloExtractor::Extract(*packet_, AllSupportedVersions(), {}, &delegate_));
}

TEST_F(ChloExtractorTest, DoesNotFindInvalidChlo) {
  MakePacket(QuicStreamFrame(
      QuicUtils::GetCryptoStreamId(AllSupportedVersions()[0].transport_version),
      false, 0, "foo"));
  EXPECT_FALSE(
      ChloExtractor::Extract(*packet_, AllSupportedVersions(), {}, &delegate_));
}

}  // namespace
}  // namespace test
}  // namespace quic
