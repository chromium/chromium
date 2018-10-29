// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/quic_versions.h"

#include "net/third_party/quic/platform/api/quic_arraysize.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_mock_log.h"
#include "net/third_party/quic/platform/api/quic_test.h"

namespace quic {
namespace test {
namespace {

using testing::_;

class QuicVersionsTest : public QuicTest {
 protected:
  QuicVersionLabel MakeVersionLabel(char a, char b, char c, char d) {
    return MakeQuicTag(d, c, b, a);
  }
};

TEST_F(QuicVersionsTest, QuicVersionToQuicVersionLabel) {
  // If you add a new version to the QuicTransportVersion enum you will need to
  // add a new case to QuicVersionToQuicVersionLabel, otherwise this test will
  // fail.

  // Any logs would indicate an unsupported version which we don't expect.
  CREATE_QUIC_MOCK_LOG(log);
  EXPECT_QUIC_LOG_CALL(log).Times(0);
  log.StartCapturingLogs();

  // Explicitly test a specific version.
  EXPECT_EQ(MakeQuicTag('5', '3', '0', 'Q'),
            QuicVersionToQuicVersionLabel(QUIC_VERSION_35));

  // Loop over all supported versions and make sure that we never hit the
  // default case (i.e. all supported versions should be successfully converted
  // to valid QuicVersionLabels).
  for (size_t i = 0; i < QUIC_ARRAYSIZE(kSupportedTransportVersions); ++i) {
    QuicTransportVersion version = kSupportedTransportVersions[i];
    EXPECT_LT(0u, QuicVersionToQuicVersionLabel(version));
  }
}

TEST_F(QuicVersionsTest, QuicVersionToQuicVersionLabelUnsupported) {
  // TODO(rjshade): Change to DFATAL once we actually support multiple versions,
  // and QuicConnectionTest::SendVersionNegotiationPacket can be changed to use
  // mis-matched versions rather than relying on QUIC_VERSION_UNSUPPORTED.
  CREATE_QUIC_MOCK_LOG(log);
  log.StartCapturingLogs();

#if QUIC_LOG_ERROR_IS_ON
  EXPECT_QUIC_LOG_CALL_CONTAINS(log, ERROR,
                                "Unsupported QuicTransportVersion: 0");
#endif

  EXPECT_EQ(0u, QuicVersionToQuicVersionLabel(QUIC_VERSION_UNSUPPORTED));
}

TEST_F(QuicVersionsTest, QuicVersionLabelToQuicTransportVersion) {
  // If you add a new version to the QuicTransportVersion enum you will need to
  // add a new case to QuicVersionLabelToQuicTransportVersion, otherwise this
  // test will fail.

  // Any logs would indicate an unsupported version which we don't expect.
  CREATE_QUIC_MOCK_LOG(log);
  EXPECT_QUIC_LOG_CALL(log).Times(0);
  log.StartCapturingLogs();

  // Explicitly test specific versions.
  EXPECT_EQ(QUIC_VERSION_35,
            QuicVersionLabelToQuicVersion(MakeQuicTag('5', '3', '0', 'Q')));

  for (size_t i = 0; i < QUIC_ARRAYSIZE(kSupportedTransportVersions); ++i) {
    QuicTransportVersion version = kSupportedTransportVersions[i];

    // Get the label from the version (we can loop over QuicVersions easily).
    QuicVersionLabel version_label = QuicVersionToQuicVersionLabel(version);
    EXPECT_LT(0u, version_label);

    // Now try converting back.
    QuicTransportVersion label_to_transport_version =
        QuicVersionLabelToQuicVersion(version_label);
    EXPECT_EQ(version, label_to_transport_version);
    EXPECT_NE(QUIC_VERSION_UNSUPPORTED, label_to_transport_version);
  }
}

TEST_F(QuicVersionsTest, QuicVersionLabelToQuicVersionUnsupported) {
  CREATE_QUIC_MOCK_LOG(log);
#if QUIC_DLOG_INFO_IS_ON
  EXPECT_QUIC_LOG_CALL_CONTAINS(log, INFO,
                                "Unsupported QuicVersionLabel version: EKAF")
      .Times(1);
#endif
  log.StartCapturingLogs();

  EXPECT_EQ(QUIC_VERSION_UNSUPPORTED,
            QuicVersionLabelToQuicVersion(MakeQuicTag('F', 'A', 'K', 'E')));
}

TEST_F(QuicVersionsTest, QuicVersionLabelToHandshakeProtocol) {
  CREATE_QUIC_MOCK_LOG(log);
  EXPECT_QUIC_LOG_CALL(log).Times(0);
  log.StartCapturingLogs();

  for (size_t i = 0; i < QUIC_ARRAYSIZE(kSupportedTransportVersions); ++i) {
    QuicVersionLabel version_label =
        QuicVersionToQuicVersionLabel(kSupportedTransportVersions[i]);
    EXPECT_EQ(PROTOCOL_QUIC_CRYPTO,
              QuicVersionLabelToHandshakeProtocol(version_label));
  }

  // Test a TLS version:
  FLAGS_quic_supports_tls_handshake = true;
  QuicTag tls_tag = MakeQuicTag('3', '4', '0', 'T');
  EXPECT_EQ(PROTOCOL_TLS1_3, QuicVersionLabelToHandshakeProtocol(tls_tag));

  FLAGS_quic_supports_tls_handshake = false;
#if QUIC_DLOG_INFO_IS_ON
  EXPECT_QUIC_LOG_CALL_CONTAINS(log, INFO,
                                "Unsupported QuicVersionLabel version: T043")
      .Times(1);
#endif
  EXPECT_EQ(PROTOCOL_UNSUPPORTED, QuicVersionLabelToHandshakeProtocol(tls_tag));
}

TEST_F(QuicVersionsTest, ParseQuicVersionLabel) {
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_35),
            ParseQuicVersionLabel(MakeVersionLabel('Q', '0', '3', '5')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_39),
            ParseQuicVersionLabel(MakeVersionLabel('Q', '0', '3', '9')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_43),
            ParseQuicVersionLabel(MakeVersionLabel('Q', '0', '4', '3')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_44),
            ParseQuicVersionLabel(MakeVersionLabel('Q', '0', '4', '4')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_45),
            ParseQuicVersionLabel(MakeVersionLabel('Q', '0', '4', '5')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_46),
            ParseQuicVersionLabel(MakeVersionLabel('Q', '0', '4', '6')));

  // Test a TLS version:
  FLAGS_quic_supports_tls_handshake = true;
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_TLS1_3, QUIC_VERSION_35),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '3', '5')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_TLS1_3, QUIC_VERSION_39),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '3', '9')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_TLS1_3, QUIC_VERSION_43),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '4', '3')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_TLS1_3, QUIC_VERSION_44),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '4', '4')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_TLS1_3, QUIC_VERSION_45),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '4', '5')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_TLS1_3, QUIC_VERSION_46),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '4', '6')));

  FLAGS_quic_supports_tls_handshake = false;
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '3', '5')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '3', '9')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '4', '3')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '4', '4')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '4', '5')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '4', '6')));
}

TEST_F(QuicVersionsTest, CreateQuicVersionLabel) {
  EXPECT_EQ(MakeVersionLabel('Q', '0', '3', '5'),
            CreateQuicVersionLabel(
                ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_35)));
  EXPECT_EQ(MakeVersionLabel('Q', '0', '3', '9'),
            CreateQuicVersionLabel(
                ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_39)));
  EXPECT_EQ(MakeVersionLabel('Q', '0', '4', '3'),
            CreateQuicVersionLabel(
                ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_43)));
  EXPECT_EQ(MakeVersionLabel('Q', '0', '4', '4'),
            CreateQuicVersionLabel(
                ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_44)));
  EXPECT_EQ(MakeVersionLabel('Q', '0', '4', '5'),
            CreateQuicVersionLabel(
                ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_45)));
  EXPECT_EQ(MakeVersionLabel('Q', '0', '4', '6'),
            CreateQuicVersionLabel(
                ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_46)));

  // Test a TLS version:
  FLAGS_quic_supports_tls_handshake = true;
  EXPECT_EQ(MakeVersionLabel('T', '0', '3', '5'),
            CreateQuicVersionLabel(
                ParsedQuicVersion(PROTOCOL_TLS1_3, QUIC_VERSION_35)));
  EXPECT_EQ(MakeVersionLabel('T', '0', '3', '9'),
            CreateQuicVersionLabel(
                ParsedQuicVersion(PROTOCOL_TLS1_3, QUIC_VERSION_39)));
  EXPECT_EQ(MakeVersionLabel('T', '0', '4', '3'),
            CreateQuicVersionLabel(
                ParsedQuicVersion(PROTOCOL_TLS1_3, QUIC_VERSION_43)));
  EXPECT_EQ(MakeVersionLabel('T', '0', '4', '4'),
            CreateQuicVersionLabel(
                ParsedQuicVersion(PROTOCOL_TLS1_3, QUIC_VERSION_44)));
  EXPECT_EQ(MakeVersionLabel('T', '0', '4', '5'),
            CreateQuicVersionLabel(
                ParsedQuicVersion(PROTOCOL_TLS1_3, QUIC_VERSION_45)));
  EXPECT_EQ(MakeVersionLabel('T', '0', '4', '6'),
            CreateQuicVersionLabel(
                ParsedQuicVersion(PROTOCOL_TLS1_3, QUIC_VERSION_46)));

  FLAGS_quic_supports_tls_handshake = false;
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '3', '5')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '3', '9')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '4', '3')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '4', '4')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '4', '5')));
  EXPECT_EQ(ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED),
            ParseQuicVersionLabel(MakeVersionLabel('T', '0', '4', '6')));
}

TEST_F(QuicVersionsTest, QuicVersionLabelToString) {
  QuicVersionLabelVector version_labels = {
      MakeVersionLabel('Q', '0', '3', '5'),
      MakeVersionLabel('Q', '0', '3', '7'),
      MakeVersionLabel('T', '0', '3', '8'),
  };

  EXPECT_EQ("Q035", QuicVersionLabelToString(version_labels[0]));
  EXPECT_EQ("T038", QuicVersionLabelToString(version_labels[2]));

  EXPECT_EQ("Q035,Q037,T038", QuicVersionLabelVectorToString(version_labels));
  EXPECT_EQ("Q035:Q037:T038",
            QuicVersionLabelVectorToString(version_labels, ":", 2));
  EXPECT_EQ("Q035|Q037|...",
            QuicVersionLabelVectorToString(version_labels, "|", 1));
}

TEST_F(QuicVersionsTest, QuicVersionToString) {
  EXPECT_EQ("QUIC_VERSION_35", QuicVersionToString(QUIC_VERSION_35));
  EXPECT_EQ("QUIC_VERSION_UNSUPPORTED",
            QuicVersionToString(QUIC_VERSION_UNSUPPORTED));

  QuicTransportVersion single_version[] = {QUIC_VERSION_35};
  QuicTransportVersionVector versions_vector;
  for (size_t i = 0; i < QUIC_ARRAYSIZE(single_version); ++i) {
    versions_vector.push_back(single_version[i]);
  }
  EXPECT_EQ("QUIC_VERSION_35",
            QuicTransportVersionVectorToString(versions_vector));

  QuicTransportVersion multiple_versions[] = {QUIC_VERSION_UNSUPPORTED,
                                              QUIC_VERSION_35};
  versions_vector.clear();
  for (size_t i = 0; i < QUIC_ARRAYSIZE(multiple_versions); ++i) {
    versions_vector.push_back(multiple_versions[i]);
  }
  EXPECT_EQ("QUIC_VERSION_UNSUPPORTED,QUIC_VERSION_35",
            QuicTransportVersionVectorToString(versions_vector));

  // Make sure that all supported versions are present in QuicVersionToString.
  for (size_t i = 0; i < QUIC_ARRAYSIZE(kSupportedTransportVersions); ++i) {
    QuicTransportVersion version = kSupportedTransportVersions[i];
    EXPECT_NE("QUIC_VERSION_UNSUPPORTED", QuicVersionToString(version));
  }
}

TEST_F(QuicVersionsTest, ParsedQuicVersionToString) {
  ParsedQuicVersion unsupported(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED);
  ParsedQuicVersion version35(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_35);
  EXPECT_EQ("Q035", ParsedQuicVersionToString(version35));
  EXPECT_EQ("0", ParsedQuicVersionToString(unsupported));

  ParsedQuicVersionVector versions_vector = {version35};
  EXPECT_EQ("Q035", ParsedQuicVersionVectorToString(versions_vector));

  versions_vector = {unsupported, version35};
  EXPECT_EQ("0,Q035", ParsedQuicVersionVectorToString(versions_vector));
  EXPECT_EQ("0:Q035", ParsedQuicVersionVectorToString(versions_vector, ":",
                                                      versions_vector.size()));
  EXPECT_EQ("0|...", ParsedQuicVersionVectorToString(versions_vector, "|", 0));

  // Make sure that all supported versions are present in
  // ParsedQuicVersionToString.
  FLAGS_quic_supports_tls_handshake = true;
  for (QuicTransportVersion transport_version : kSupportedTransportVersions) {
    for (HandshakeProtocol protocol : kSupportedHandshakeProtocols) {
      EXPECT_NE("0", ParsedQuicVersionToString(
                         ParsedQuicVersion(protocol, transport_version)));
    }
  }
}
TEST_F(QuicVersionsTest, AllSupportedTransportVersions) {
  QuicTransportVersionVector all_versions = AllSupportedTransportVersions();
  ASSERT_EQ(QUIC_ARRAYSIZE(kSupportedTransportVersions), all_versions.size());
  for (size_t i = 0; i < all_versions.size(); ++i) {
    EXPECT_EQ(kSupportedTransportVersions[i], all_versions[i]);
  }
}

TEST_F(QuicVersionsTest, FilterSupportedTransportVersionsAllVersions) {
  QuicTransportVersionVector all_versions = AllSupportedTransportVersions();
  SetQuicReloadableFlag(quic_disable_version_35, false);
  SetQuicReloadableFlag(quic_enable_version_43, true);
  SetQuicReloadableFlag(quic_enable_version_44, true);
  SetQuicReloadableFlag(quic_enable_version_45, true);
  SetQuicReloadableFlag(quic_enable_version_46, true);
  SetQuicFlag(&FLAGS_quic_enable_version_99, true);
  ParsedQuicVersionVector parsed_versions;
  for (QuicTransportVersion version : all_versions) {
    parsed_versions.push_back(ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, version));
  }
  QuicTransportVersionVector expected_versions = {
      QUIC_VERSION_99, QUIC_VERSION_46, QUIC_VERSION_45, QUIC_VERSION_44,
      QUIC_VERSION_43, QUIC_VERSION_39, QUIC_VERSION_35};
  ParsedQuicVersionVector expected_parsed_versions;
  for (QuicTransportVersion version : expected_versions) {
    expected_parsed_versions.push_back(
        ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, version));
  }

  ASSERT_EQ(expected_versions, FilterSupportedTransportVersions(all_versions));
  ASSERT_EQ(expected_parsed_versions, FilterSupportedVersions(parsed_versions));
}

TEST_F(QuicVersionsTest, FilterSupportedTransportVersionsNo99) {
  QuicTransportVersionVector all_versions = AllSupportedTransportVersions();
  SetQuicReloadableFlag(quic_disable_version_35, false);
  SetQuicReloadableFlag(quic_enable_version_43, true);
  SetQuicReloadableFlag(quic_enable_version_44, true);
  SetQuicReloadableFlag(quic_enable_version_45, true);
  SetQuicReloadableFlag(quic_enable_version_46, true);
  SetQuicFlag(&FLAGS_quic_enable_version_99, false);
  ParsedQuicVersionVector parsed_versions;
  for (QuicTransportVersion version : all_versions) {
    parsed_versions.push_back(ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, version));
  }
  QuicTransportVersionVector expected_versions = {
      QUIC_VERSION_46, QUIC_VERSION_45, QUIC_VERSION_44,
      QUIC_VERSION_43, QUIC_VERSION_39, QUIC_VERSION_35};
  ParsedQuicVersionVector expected_parsed_versions;
  for (QuicTransportVersion version : expected_versions) {
    expected_parsed_versions.push_back(
        ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, version));
  }

  ASSERT_EQ(expected_versions, FilterSupportedTransportVersions(all_versions));
  ASSERT_EQ(expected_parsed_versions, FilterSupportedVersions(parsed_versions));
}

TEST_F(QuicVersionsTest, FilterSupportedTransportVersionsNo46) {
  QuicTransportVersionVector all_versions = AllSupportedTransportVersions();
  SetQuicReloadableFlag(quic_disable_version_35, false);
  SetQuicReloadableFlag(quic_enable_version_43, true);
  SetQuicReloadableFlag(quic_enable_version_44, true);
  SetQuicReloadableFlag(quic_enable_version_45, true);
  SetQuicReloadableFlag(quic_enable_version_46, false);
  SetQuicFlag(&FLAGS_quic_enable_version_99, false);
  ParsedQuicVersionVector parsed_versions;
  for (QuicTransportVersion version : all_versions) {
    parsed_versions.push_back(ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, version));
  }
  QuicTransportVersionVector expected_versions = {
      QUIC_VERSION_45, QUIC_VERSION_44, QUIC_VERSION_43, QUIC_VERSION_39,
      QUIC_VERSION_35};
  ParsedQuicVersionVector expected_parsed_versions;
  for (QuicTransportVersion version : expected_versions) {
    expected_parsed_versions.push_back(
        ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, version));
  }

  ASSERT_EQ(expected_versions, FilterSupportedTransportVersions(all_versions));
  ASSERT_EQ(expected_parsed_versions, FilterSupportedVersions(parsed_versions));
}

TEST_F(QuicVersionsTest, FilterSupportedTransportVersionsNo45) {
  QuicTransportVersionVector all_versions = AllSupportedTransportVersions();
  SetQuicReloadableFlag(quic_disable_version_35, false);
  SetQuicReloadableFlag(quic_enable_version_43, true);
  SetQuicReloadableFlag(quic_enable_version_44, true);
  SetQuicReloadableFlag(quic_enable_version_45, false);
  SetQuicReloadableFlag(quic_enable_version_46, false);
  SetQuicFlag(&FLAGS_quic_enable_version_99, false);
  ParsedQuicVersionVector parsed_versions;
  for (QuicTransportVersion version : all_versions) {
    parsed_versions.push_back(ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, version));
  }
  QuicTransportVersionVector expected_versions = {
      QUIC_VERSION_44, QUIC_VERSION_43, QUIC_VERSION_39, QUIC_VERSION_35};
  ParsedQuicVersionVector expected_parsed_versions;
  for (QuicTransportVersion version : expected_versions) {
    expected_parsed_versions.push_back(
        ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, version));
  }

  ASSERT_EQ(expected_versions, FilterSupportedTransportVersions(all_versions));
  ASSERT_EQ(expected_parsed_versions, FilterSupportedVersions(parsed_versions));
}

TEST_F(QuicVersionsTest, FilterSupportedTransportVersionsNo44) {
  QuicTransportVersionVector all_versions = AllSupportedTransportVersions();
  SetQuicReloadableFlag(quic_disable_version_35, false);
  SetQuicReloadableFlag(quic_enable_version_43, true);
  SetQuicReloadableFlag(quic_enable_version_44, false);
  SetQuicReloadableFlag(quic_enable_version_45, false);
  SetQuicReloadableFlag(quic_enable_version_46, false);
  SetQuicFlag(&FLAGS_quic_enable_version_99, false);
  ParsedQuicVersionVector parsed_versions;
  for (QuicTransportVersion version : all_versions) {
    parsed_versions.push_back(ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, version));
  }
  QuicTransportVersionVector expected_versions = {
      QUIC_VERSION_43, QUIC_VERSION_39, QUIC_VERSION_35};
  ParsedQuicVersionVector expected_parsed_versions;
  for (QuicTransportVersion version : expected_versions) {
    expected_parsed_versions.push_back(
        ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, version));
  }

  ASSERT_EQ(expected_versions, FilterSupportedTransportVersions(all_versions));
  ASSERT_EQ(expected_parsed_versions, FilterSupportedVersions(parsed_versions));
}

TEST_F(QuicVersionsTest, FilterSupportedTransportVersionsNo43) {
  QuicTransportVersionVector all_versions = AllSupportedTransportVersions();
  SetQuicReloadableFlag(quic_disable_version_35, false);
  SetQuicReloadableFlag(quic_enable_version_43, false);
  SetQuicReloadableFlag(quic_enable_version_44, false);
  SetQuicReloadableFlag(quic_enable_version_45, false);
  SetQuicReloadableFlag(quic_enable_version_46, false);
  SetQuicFlag(&FLAGS_quic_enable_version_99, false);
  ParsedQuicVersionVector parsed_versions;
  for (QuicTransportVersion version : all_versions) {
    parsed_versions.push_back(ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, version));
  }
  QuicTransportVersionVector expected_versions = {QUIC_VERSION_39,
                                                  QUIC_VERSION_35};
  ParsedQuicVersionVector expected_parsed_versions;
  for (QuicTransportVersion version : expected_versions) {
    expected_parsed_versions.push_back(
        ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, version));
  }

  ASSERT_EQ(expected_versions, FilterSupportedTransportVersions(all_versions));
  ASSERT_EQ(expected_parsed_versions, FilterSupportedVersions(parsed_versions));
}

TEST_F(QuicVersionsTest, FilterSupportedTransportVersionsNo35) {
  QuicTransportVersionVector all_versions = AllSupportedTransportVersions();
  SetQuicReloadableFlag(quic_disable_version_35, true);
  SetQuicReloadableFlag(quic_enable_version_43, false);
  SetQuicReloadableFlag(quic_enable_version_44, false);
  SetQuicReloadableFlag(quic_enable_version_45, false);
  SetQuicReloadableFlag(quic_enable_version_46, false);
  SetQuicFlag(&FLAGS_quic_enable_version_99, false);
  ParsedQuicVersionVector parsed_versions;
  for (QuicTransportVersion version : all_versions) {
    parsed_versions.push_back(ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, version));
  }
  QuicTransportVersionVector expected_versions = {QUIC_VERSION_39};
  ParsedQuicVersionVector expected_parsed_versions;
  for (QuicTransportVersion version : expected_versions) {
    expected_parsed_versions.push_back(
        ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, version));
  }

  ASSERT_EQ(expected_versions, FilterSupportedTransportVersions(all_versions));
  ASSERT_EQ(expected_parsed_versions, FilterSupportedVersions(parsed_versions));
}

TEST_F(QuicVersionsTest, LookUpVersionByIndex) {
  QuicTransportVersionVector all_versions = {QUIC_VERSION_35, QUIC_VERSION_39};
  int version_count = all_versions.size();
  for (int i = -5; i <= version_count + 1; ++i) {
    if (i >= 0 && i < version_count) {
      EXPECT_EQ(all_versions[i], VersionOfIndex(all_versions, i)[0]);
    } else {
      EXPECT_EQ(QUIC_VERSION_UNSUPPORTED, VersionOfIndex(all_versions, i)[0]);
    }
  }
}

TEST_F(QuicVersionsTest, LookUpParsedVersionByIndex) {
  ParsedQuicVersionVector all_versions = AllSupportedVersions();
  int version_count = all_versions.size();
  for (int i = -5; i <= version_count + 1; ++i) {
    if (i >= 0 && i < version_count) {
      EXPECT_EQ(all_versions[i], ParsedVersionOfIndex(all_versions, i)[0]);
    } else {
      EXPECT_EQ(
          ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED),
          ParsedVersionOfIndex(all_versions, i)[0]);
    }
  }
}

TEST_F(QuicVersionsTest, ParsedVersionsToTransportVersions) {
  ParsedQuicVersionVector all_versions = AllSupportedVersions();
  QuicTransportVersionVector transport_versions =
      ParsedVersionsToTransportVersions(all_versions);
  ASSERT_EQ(all_versions.size(), transport_versions.size());
  for (size_t i = 0; i < all_versions.size(); ++i) {
    EXPECT_EQ(transport_versions[i], all_versions[i].transport_version);
  }
}

// This test may appear to be so simplistic as to be unnecessary,
// yet a typo was made in doing the #defines and it was caught
// only in some test far removed from here... Better safe than sorry.
TEST_F(QuicVersionsTest, CheckVersionNumbersForTypos) {
  static_assert(QUIC_ARRAYSIZE(kSupportedTransportVersions) == 7u,
                "Supported versions out of sync");
  EXPECT_EQ(QUIC_VERSION_35, 35);
  EXPECT_EQ(QUIC_VERSION_39, 39);
  EXPECT_EQ(QUIC_VERSION_43, 43);
  EXPECT_EQ(QUIC_VERSION_44, 44);
  EXPECT_EQ(QUIC_VERSION_45, 45);
  EXPECT_EQ(QUIC_VERSION_46, 46);
  EXPECT_EQ(QUIC_VERSION_99, 99);
}
}  // namespace
}  // namespace test
}  // namespace quic
