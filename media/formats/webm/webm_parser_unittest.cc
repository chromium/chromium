// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/webm/webm_parser.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "media/formats/webm/cluster_builder.h"
#include "media/formats/webm/webm_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::Return;
using ::testing::ReturnNull;
using ::testing::StrictMock;
using ::testing::_;

namespace media {

enum { kBlockCount = 5 };

class MockWebMParserClient : public WebMParserClient {
 public:
  ~MockWebMParserClient() override = default;

  // WebMParserClient methods.
  MOCK_METHOD1(OnListStart, WebMParserClient*(int));
  MOCK_METHOD1(OnListEnd, bool(int));
  MOCK_METHOD2(OnUInt, bool(int, int64_t));
  MOCK_METHOD2(OnFloat, bool(int, double));
  MOCK_METHOD3(OnBinary, bool(int, const uint8_t*, int));
  MOCK_METHOD2(OnString, bool(int, const std::string&));
};

class WebMParserTest : public testing::Test {
 protected:
  StrictMock<MockWebMParserClient> client_;
};

static std::unique_ptr<Cluster> CreateCluster(int block_count) {
  ClusterBuilder cb;
  cb.SetClusterTimecode(0);

  for (int i = 0; i < block_count; i++) {
    uint8_t data[] = {0x00};
    cb.AddSimpleBlock(0, i, 0, data, sizeof(data));
  }

  return cb.Finish();
}

static void CreateClusterExpectations(int block_count,
                                      bool is_complete_cluster,
                                      MockWebMParserClient* client) {

  InSequence s;
  EXPECT_CALL(*client, OnListStart(kWebMIdCluster)).WillOnce(Return(client));
  EXPECT_CALL(*client, OnUInt(kWebMIdTimecode, 0))
      .WillOnce(Return(true));

  for (int i = 0; i < block_count; i++) {
    EXPECT_CALL(*client, OnBinary(kWebMIdSimpleBlock, _, _))
        .WillOnce(Return(true));
  }

  if (is_complete_cluster)
    EXPECT_CALL(*client, OnListEnd(kWebMIdCluster)).WillOnce(Return(true));
}

TEST_F(WebMParserTest, EmptyCluster) {
  const uint8_t kEmptyCluster[] = {
      0x1F, 0x43, 0xB6, 0x75, 0x80  // CLUSTER (size = 0)
  };
  int size = sizeof(kEmptyCluster);

  InSequence s;
  EXPECT_CALL(client_, OnListStart(kWebMIdCluster)).WillOnce(Return(&client_));
  EXPECT_CALL(client_, OnListEnd(kWebMIdCluster)).WillOnce(Return(true));

  WebMListParser parser(kWebMIdCluster, &client_);
  EXPECT_EQ(size, parser.Parse(kEmptyCluster, size));
  EXPECT_TRUE(parser.IsParsingComplete());
}

TEST_F(WebMParserTest, EmptyClusterInSegment) {
  const uint8_t kBuffer[] = {
      0x18, 0x53, 0x80, 0x67, 0x85,  // SEGMENT (size = 5)
      0x1F, 0x43, 0xB6, 0x75, 0x80,  // CLUSTER (size = 0)
  };
  int size = sizeof(kBuffer);

  InSequence s;
  EXPECT_CALL(client_, OnListStart(kWebMIdSegment)).WillOnce(Return(&client_));
  EXPECT_CALL(client_, OnListStart(kWebMIdCluster)).WillOnce(Return(&client_));
  EXPECT_CALL(client_, OnListEnd(kWebMIdCluster)).WillOnce(Return(true));
  EXPECT_CALL(client_, OnListEnd(kWebMIdSegment)).WillOnce(Return(true));

  WebMListParser parser(kWebMIdSegment, &client_);
  EXPECT_EQ(size, parser.Parse(kBuffer, size));
  EXPECT_TRUE(parser.IsParsingComplete());
}

// Test the case where a non-list child element has a size
// that is beyond the end of the parent.
TEST_F(WebMParserTest, ChildNonListLargerThanParent) {
  const uint8_t kBuffer[] = {
      0x1F, 0x43, 0xB6, 0x75, 0x81,  // CLUSTER (size = 1)
      0xE7, 0x81, 0x01,              // Timecode (size=1, value=1)
  };

  InSequence s;
  EXPECT_CALL(client_, OnListStart(kWebMIdCluster)).WillOnce(Return(&client_));

  WebMListParser parser(kWebMIdCluster, &client_);
  EXPECT_EQ(-1, parser.Parse(kBuffer, sizeof(kBuffer)));
  EXPECT_FALSE(parser.IsParsingComplete());
}

// Test the case where a list child element has a size
// that is beyond the end of the parent.
TEST_F(WebMParserTest, ChildListLargerThanParent) {
  const uint8_t kBuffer[] = {
      0x18, 0x53, 0x80, 0x67, 0x85,  // SEGMENT (size = 5)
      0x1F, 0x43, 0xB6, 0x75, 0x81,
      0x11  // CLUSTER (size = 1)
  };

  InSequence s;
  EXPECT_CALL(client_, OnListStart(kWebMIdSegment)).WillOnce(Return(&client_));

  WebMListParser parser(kWebMIdSegment, &client_);
  EXPECT_EQ(-1, parser.Parse(kBuffer, sizeof(kBuffer)));
  EXPECT_FALSE(parser.IsParsingComplete());
}

// Expecting to parse a Cluster, but get a Segment.
TEST_F(WebMParserTest, ListIdDoesNotMatch) {
  const uint8_t kBuffer[] = {
      0x18, 0x53, 0x80, 0x67, 0x80,  // SEGMENT (size = 0)
  };

  WebMListParser parser(kWebMIdCluster, &client_);
  EXPECT_EQ(-1, parser.Parse(kBuffer, sizeof(kBuffer)));
  EXPECT_FALSE(parser.IsParsingComplete());
}

TEST_F(WebMParserTest, InvalidElementInList) {
  const uint8_t kBuffer[] = {
      0x18, 0x53, 0x80, 0x67, 0x82,  // SEGMENT (size = 2)
      0xAE, 0x80,                    // TrackEntry (size = 0)
  };

  InSequence s;
  EXPECT_CALL(client_, OnListStart(kWebMIdSegment)).WillOnce(Return(&client_));

  WebMListParser parser(kWebMIdSegment, &client_);
  EXPECT_EQ(-1, parser.Parse(kBuffer, sizeof(kBuffer)));
  EXPECT_FALSE(parser.IsParsingComplete());
}

// Test specific case of InvalidElementInList to verify EBMLHEADER within
// known-sized cluster causes parse error.
TEST_F(WebMParserTest, InvalidEBMLHeaderInCluster) {
  const uint8_t kBuffer[] = {
      0x1F, 0x43, 0xB6, 0x75, 0x85,  // CLUSTER (size = 5)
      0x1A, 0x45, 0xDF, 0xA3, 0x80,  // EBMLHEADER (size = 0)
  };

  InSequence s;
  EXPECT_CALL(client_, OnListStart(kWebMIdCluster)).WillOnce(Return(&client_));

  WebMListParser parser(kWebMIdCluster, &client_);
  EXPECT_EQ(-1, parser.Parse(kBuffer, sizeof(kBuffer)));
  EXPECT_FALSE(parser.IsParsingComplete());
}

// Verify that EBMLHEADER ends a preceding "unknown"-sized CLUSTER.
TEST_F(WebMParserTest, UnknownSizeClusterFollowedByEBMLHeader) {
  const uint8_t kBuffer[] = {
      0x1F, 0x43, 0xB6,
      0x75, 0xFF,  // CLUSTER (size = unknown; really 0 due to:)
      0x1A, 0x45, 0xDF,
      0xA3, 0x80,  // EBMLHEADER (size = 0)
  };

  InSequence s;
  EXPECT_CALL(client_, OnListStart(kWebMIdCluster)).WillOnce(Return(&client_));
  EXPECT_CALL(client_, OnListEnd(kWebMIdCluster)).WillOnce(Return(true));

  WebMListParser parser(kWebMIdCluster, &client_);

  // List parse should consume the CLUSTER but not the EBMLHEADER.
  EXPECT_EQ(5, parser.Parse(kBuffer, sizeof(kBuffer)));
  EXPECT_TRUE(parser.IsParsingComplete());
}

TEST_F(WebMParserTest, VoidAndCRC32InList) {
  const uint8_t kBuffer[] = {
      0x18, 0x53, 0x80, 0x67, 0x99,  // SEGMENT (size = 25)
      0xEC, 0x83, 0x00, 0x00, 0x00,  // Void (size = 3)
      0xBF, 0x83, 0x00, 0x00, 0x00,  // CRC32 (size = 3)
      0x1F, 0x43, 0xB6, 0x75, 0x8A,  // CLUSTER (size = 10)
      0xEC, 0x83, 0x00, 0x00, 0x00,  // Void (size = 3)
      0xBF, 0x83, 0x00, 0x00, 0x00,  // CRC32 (size = 3)
  };
  int size = sizeof(kBuffer);

  InSequence s;
  EXPECT_CALL(client_, OnListStart(kWebMIdSegment)).WillOnce(Return(&client_));
  EXPECT_CALL(client_, OnListStart(kWebMIdCluster)).WillOnce(Return(&client_));
  EXPECT_CALL(client_, OnListEnd(kWebMIdCluster)).WillOnce(Return(true));
  EXPECT_CALL(client_, OnListEnd(kWebMIdSegment)).WillOnce(Return(true));

  WebMListParser parser(kWebMIdSegment, &client_);
  EXPECT_EQ(size, parser.Parse(kBuffer, size));
  EXPECT_TRUE(parser.IsParsingComplete());
}


TEST_F(WebMParserTest, ParseListElementWithSingleCall) {
  std::unique_ptr<Cluster> cluster(CreateCluster(kBlockCount));
  CreateClusterExpectations(kBlockCount, true, &client_);

  WebMListParser parser(kWebMIdCluster, &client_);
  EXPECT_EQ(cluster->bytes_used(),
            parser.Parse(cluster->data(), cluster->bytes_used()));
  EXPECT_TRUE(parser.IsParsingComplete());
}

TEST_F(WebMParserTest, ParseListElementWithMultipleCalls) {
  std::unique_ptr<Cluster> cluster(CreateCluster(kBlockCount));
  CreateClusterExpectations(kBlockCount, true, &client_);

  const uint8_t* data = cluster->data();
  int size = cluster->bytes_used();
  int default_parse_size = 3;
  WebMListParser parser(kWebMIdCluster, &client_);
  int parse_size = std::min(default_parse_size, size);

  while (size > 0) {
    int result = parser.Parse(data, parse_size);
    ASSERT_GE(result, 0);
    ASSERT_LE(result, parse_size);

    if (result == 0) {
      // The parser needs more data so increase the parse_size a little.
      EXPECT_FALSE(parser.IsParsingComplete());
      parse_size += default_parse_size;
      parse_size = std::min(parse_size, size);
      continue;
    }

    parse_size = default_parse_size;

    data += result;
    size -= result;

    EXPECT_EQ((size == 0), parser.IsParsingComplete());
  }
  EXPECT_TRUE(parser.IsParsingComplete());
}

TEST_F(WebMParserTest, Reset) {
  InSequence s;
  std::unique_ptr<Cluster> cluster(CreateCluster(kBlockCount));

  // First expect all but the last block.
  CreateClusterExpectations(kBlockCount - 1, false, &client_);

  // Now expect all blocks.
  CreateClusterExpectations(kBlockCount, true, &client_);

  WebMListParser parser(kWebMIdCluster, &client_);

  // Send slightly less than the full cluster so all but the last block is
  // parsed.
  int result = parser.Parse(cluster->data(), cluster->bytes_used() - 1);
  EXPECT_GT(result, 0);
  EXPECT_LT(result, cluster->bytes_used());
  EXPECT_FALSE(parser.IsParsingComplete());

  parser.Reset();

  // Now parse a whole cluster to verify that all the blocks will get parsed.
  EXPECT_EQ(cluster->bytes_used(),
            parser.Parse(cluster->data(), cluster->bytes_used()));
  EXPECT_TRUE(parser.IsParsingComplete());
}

// Test the case where multiple clients are used for different lists.
TEST_F(WebMParserTest, MultipleClients) {
  const uint8_t kBuffer[] = {
      0x18, 0x53, 0x80, 0x67, 0x94,  // SEGMENT (size = 20)
      0x16, 0x54, 0xAE, 0x6B, 0x85,  //   TRACKS (size = 5)
      0xAE, 0x83,                    //     TRACKENTRY (size = 3)
      0xD7, 0x81, 0x01,              //       TRACKNUMBER (size = 1)
      0x1F, 0x43, 0xB6, 0x75, 0x85,  //   CLUSTER (size = 5)
      0xEC, 0x83, 0x00, 0x00, 0x00,  //     Void (size = 3)
  };
  int size = sizeof(kBuffer);

  StrictMock<MockWebMParserClient> c1_;
  StrictMock<MockWebMParserClient> c2_;
  StrictMock<MockWebMParserClient> c3_;

  InSequence s;
  EXPECT_CALL(client_, OnListStart(kWebMIdSegment)).WillOnce(Return(&c1_));
  EXPECT_CALL(c1_, OnListStart(kWebMIdTracks)).WillOnce(Return(&c2_));
  EXPECT_CALL(c2_, OnListStart(kWebMIdTrackEntry)).WillOnce(Return(&c3_));
  EXPECT_CALL(c3_, OnUInt(kWebMIdTrackNumber, 1)).WillOnce(Return(true));
  EXPECT_CALL(c2_, OnListEnd(kWebMIdTrackEntry)).WillOnce(Return(true));
  EXPECT_CALL(c1_, OnListEnd(kWebMIdTracks)).WillOnce(Return(true));
  EXPECT_CALL(c1_, OnListStart(kWebMIdCluster)).WillOnce(Return(&c2_));
  EXPECT_CALL(c1_, OnListEnd(kWebMIdCluster)).WillOnce(Return(true));
  EXPECT_CALL(client_, OnListEnd(kWebMIdSegment)).WillOnce(Return(true));

  WebMListParser parser(kWebMIdSegment, &client_);
  EXPECT_EQ(size, parser.Parse(kBuffer, size));
  EXPECT_TRUE(parser.IsParsingComplete());
}

// Test the case where multiple clients are used for different lists.
TEST_F(WebMParserTest, InvalidClient) {
  const uint8_t kBuffer[] = {
      0x18, 0x53, 0x80, 0x67, 0x85,  // SEGMENT (size = 20)
      0x16, 0x54, 0xAE, 0x6B, 0x80,  //   TRACKS (size = 5)
  };

  InSequence s;
  EXPECT_CALL(client_, OnListStart(kWebMIdSegment)).WillOnce(ReturnNull());

  WebMListParser parser(kWebMIdSegment, &client_);
  EXPECT_EQ(-1, parser.Parse(kBuffer, sizeof(kBuffer)));
  EXPECT_FALSE(parser.IsParsingComplete());
}

TEST_F(WebMParserTest, ReservedIds) {
  const uint8_t k1ByteReservedId[] = {0xFF, 0x81};
  const uint8_t k2ByteReservedId[] = {0x7F, 0xFF, 0x81};
  const uint8_t k3ByteReservedId[] = {0x3F, 0xFF, 0xFF, 0x81};
  const uint8_t k4ByteReservedId[] = {0x1F, 0xFF, 0xFF, 0xFF, 0x81};
  const uint8_t* kBuffers[] = {k1ByteReservedId, k2ByteReservedId,
                               k3ByteReservedId, k4ByteReservedId};

  for (size_t i = 0; i < std::size(kBuffers); i++) {
    int id;
    int64_t element_size;
    int buffer_size = 2 + i;
    EXPECT_EQ(buffer_size, WebMParseElementHeader(kBuffers[i], buffer_size,
                                                  &id, &element_size));
    EXPECT_EQ(id, kWebMReservedId);
    EXPECT_EQ(element_size, 1);
  }
}

TEST_F(WebMParserTest, ReservedSizes) {
  const uint8_t k1ByteReservedSize[] = {0xA3, 0xFF};
  const uint8_t k2ByteReservedSize[] = {0xA3, 0x7F, 0xFF};
  const uint8_t k3ByteReservedSize[] = {0xA3, 0x3F, 0xFF, 0xFF};
  const uint8_t k4ByteReservedSize[] = {0xA3, 0x1F, 0xFF, 0xFF, 0xFF};
  const uint8_t k5ByteReservedSize[] = {0xA3, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF};
  const uint8_t k6ByteReservedSize[] = {0xA3, 0x07, 0xFF, 0xFF,
                                        0xFF, 0xFF, 0xFF};
  const uint8_t k7ByteReservedSize[] = {0xA3, 0x03, 0xFF, 0xFF,
                                        0xFF, 0xFF, 0xFF, 0xFF};
  const uint8_t k8ByteReservedSize[] = {0xA3, 0x01, 0xFF, 0xFF, 0xFF,
                                        0xFF, 0xFF, 0xFF, 0xFF};
  const uint8_t* kBuffers[] = {k1ByteReservedSize, k2ByteReservedSize,
                               k3ByteReservedSize, k4ByteReservedSize,
                               k5ByteReservedSize, k6ByteReservedSize,
                               k7ByteReservedSize, k8ByteReservedSize};

  for (size_t i = 0; i < std::size(kBuffers); i++) {
    int id;
    int64_t element_size;
    int buffer_size = 2 + i;
    EXPECT_EQ(buffer_size, WebMParseElementHeader(kBuffers[i], buffer_size,
                                                  &id, &element_size));
    EXPECT_EQ(id, 0xA3);
    EXPECT_EQ(element_size, kWebMUnknownSize);
  }
}

TEST_F(WebMParserTest, ZeroPaddedStrings) {
  const uint8_t kBuffer[] = {
      0x1A, 0x45, 0xDF, 0xA3, 0x91,  // EBMLHEADER (size = 17)
      0x42, 0x82, 0x80,              // DocType (size = 0)
      0x42, 0x82, 0x81, 0x00,        // DocType (size = 1) ""
      0x42, 0x82, 0x81, 'a',         // DocType (size = 1) "a"
      0x42, 0x82, 0x83, 'a',  0x00,
      0x00  // DocType (size = 3) "a"
  };
  int size = sizeof(kBuffer);

  InSequence s;
  EXPECT_CALL(client_, OnListStart(kWebMIdEBMLHeader))
      .WillOnce(Return(&client_));
  EXPECT_CALL(client_, OnString(kWebMIdDocType, "")).WillOnce(Return(true));
  EXPECT_CALL(client_, OnString(kWebMIdDocType, "")).WillOnce(Return(true));
  EXPECT_CALL(client_, OnString(kWebMIdDocType, "a")).WillOnce(Return(true));
  EXPECT_CALL(client_, OnString(kWebMIdDocType, "a")).WillOnce(Return(true));
  EXPECT_CALL(client_, OnListEnd(kWebMIdEBMLHeader)).WillOnce(Return(true));

  WebMListParser parser(kWebMIdEBMLHeader, &client_);
  EXPECT_EQ(size, parser.Parse(kBuffer, size));
  EXPECT_TRUE(parser.IsParsingComplete());
}

}  // namespace media
