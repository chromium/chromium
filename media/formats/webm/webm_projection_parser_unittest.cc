// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/webm/webm_projection_parser.h"

#include <array>

#include "base/containers/span.h"
#include "media/base/mock_media_log.h"
#include "media/formats/webm/webm_constants.h"
#include "media/formats/webm/webm_video_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::StrictMock;

namespace media {

// Matchers for verifying common media log entry strings.
MATCHER(UnexpectedProjectionId, "") {
  return CONTAINS_STRING(arg, "Unexpected id in Projection: 0x");
}

MATCHER(UnexpectedProjectionType, "") {
  return CONTAINS_STRING(arg, "Unexpected value for ProjectionType: 0x");
}

MATCHER(UnexpectedMultipleValuesForProjectionType, "") {
  return CONTAINS_STRING(arg, "Multiple values for id: 0x7671");
}

MATCHER(UnexpectedMultipleValuesForProjectionPrivate, "") {
  return CONTAINS_STRING(arg, "Multiple values for id: 0x7672");
}

MATCHER(UnexpectedProjectionPrivateSize, "") {
  return CONTAINS_STRING(arg, "ProjectionPrivate element has unexpected size:");
}

MATCHER(UnexpectedMultipleValuesForYaw, "") {
  return CONTAINS_STRING(arg, "Multiple values for id: 0x7673");
}


MATCHER(UnexpectedMultipleValuesForRoll, "") {
  return CONTAINS_STRING(arg, "Multiple values for id: 0x7675");
}

MATCHER(ProjectionPrivateMustNotBePresent, "") {
  return CONTAINS_STRING(arg,
                         "ProjectionPrivate must not be present when "
                         "ProjectionType is Rectangular (0).");
}

MATCHER(ProjectionPrivateRequiredForEquirect, "") {
  return CONTAINS_STRING(arg,
                         "ProjectionPrivate element required when "
                         "ProjectionType is Equirectangular (1).");
}

MATCHER(ProjectionPrivateRequiredForCubemapOrMesh, "") {
  return CONTAINS_STRING(arg,
                         "ProjectionPrivate element required when "
                         "ProjectionType is Cubemap (2) or Mesh (3).");
}

MATCHER(UnexpectedProjectionYaw, "") {
  return CONTAINS_STRING(arg, "Value not within valid range. id: 0x7673 val:");
}


MATCHER(UnexpectedProjectionRoll, "") {
  return CONTAINS_STRING(arg, "Value not within valid range. id: 0x7675 val:");
}

MATCHER(MissingProjectionType, "") {
  return CONTAINS_STRING(
      arg, "Projection element is incomplete; ProjectionType required.");
}


constexpr auto kEquirectPrivateData = std::to_array<uint8_t>({
    0x00, 0x00, 0x00, 0x00,  // top
    0x00, 0x00, 0x00, 0x00,  // bottom
    0x00, 0x00, 0x00, 0x00,  // left
    0x00, 0x00, 0x00, 0x00   // right
});

constexpr auto kEquirect180PrivateData = std::to_array<uint8_t>({
    0x00, 0x00, 0x00, 0x00,  // top
    0x00, 0x00, 0x00, 0x00,  // bottom
    0x30, 0x00, 0x00, 0x00,  // left
    0x30, 0x00, 0x00, 0x00   // right
});

class WebMProjectionParserTest : public testing::Test {
 public:
  WebMProjectionParserTest()
      : projection_parser_(&media_log_), webm_video_client_(&media_log_) {}

  bool OnUInt(int id, int64_t val) {
    return projection_parser_.OnUInt(id, val);
  }

  bool OnFloat(int id, double val) {
    return projection_parser_.OnFloat(id, val);
  }

  bool OnBinary(int id, base::span<const uint8_t> data) {
    return projection_parser_.OnBinary(id, data);
  }

  WebMParserClient* VideoClientOnListStart(int id) {
    return webm_video_client_.OnListStart(id);
  }

  void VideoClientOnListEnd(int id) { webm_video_client_.OnListEnd(id); }

  bool VideoClientOnUInt(int id, int64_t val) {
    return webm_video_client_.OnUInt(id, val);
  }

  StrictMock<MockMediaLog> media_log_;
  WebMProjectionParser projection_parser_;
  WebMVideoClient webm_video_client_;
};

TEST_F(WebMProjectionParserTest, UnexpectedInt) {
  EXPECT_MEDIA_LOG(UnexpectedProjectionId());
  OnUInt(kWebMIdPrimaryBChromaticityX, 1);
}

TEST_F(WebMProjectionParserTest, UnexpectedFloat) {
  EXPECT_MEDIA_LOG(UnexpectedProjectionId());
  OnFloat(kWebMIdPrimaryBChromaticityX, 0);
}

TEST_F(WebMProjectionParserTest, InvalidProjectionType) {
  EXPECT_MEDIA_LOG(UnexpectedProjectionType());
  OnUInt(kWebMIdProjectionType, 4);
}

TEST_F(WebMProjectionParserTest, MultipleProjectionType) {
  OnUInt(kWebMIdProjectionType, 1);
  EXPECT_MEDIA_LOG(UnexpectedMultipleValuesForProjectionType());
  OnUInt(kWebMIdProjectionType, 1);
}

TEST_F(WebMProjectionParserTest, InvalidProjectionYaw) {
  EXPECT_MEDIA_LOG(UnexpectedProjectionYaw());
  OnFloat(kWebMIdProjectionPoseYaw, 181);
}


TEST_F(WebMProjectionParserTest, InvalidProjectionRoll) {
  EXPECT_MEDIA_LOG(UnexpectedProjectionRoll());
  OnFloat(kWebMIdProjectionPoseRoll, 181);
}

TEST_F(WebMProjectionParserTest, MultipleProjectionYaw) {
  OnFloat(kWebMIdProjectionPoseYaw, 180);
  EXPECT_MEDIA_LOG(UnexpectedMultipleValuesForYaw());
  OnFloat(kWebMIdProjectionPoseYaw, 180);
}


TEST_F(WebMProjectionParserTest, MultipleProjectionRoll) {
  OnFloat(kWebMIdProjectionPoseRoll, 180);
  EXPECT_MEDIA_LOG(UnexpectedMultipleValuesForRoll());
  OnFloat(kWebMIdProjectionPoseRoll, 180);
}

TEST_F(WebMProjectionParserTest, MissingProjectionType) {
  VideoClientOnListStart(kWebMIdProjection);
  EXPECT_MEDIA_LOG(MissingProjectionType());
  VideoClientOnListEnd(kWebMIdProjection);
}

TEST_F(WebMProjectionParserTest, PartialProjectionPose) {
  auto* parser = VideoClientOnListStart(kWebMIdProjection);
  EXPECT_TRUE(parser->OnBinary(kWebMIdProjectionPrivate, kEquirectPrivateData));
  parser->OnUInt(kWebMIdProjectionType, 1);
  parser->OnFloat(kWebMIdProjectionPoseYaw, 90);
  VideoClientOnListEnd(kWebMIdProjection);

  auto* projection_parser = static_cast<WebMProjectionParser*>(parser);
  EXPECT_EQ(projection_parser->GetProjectionType(),
            VideoProjectionType::kEquirect360);
  auto transform = projection_parser->GetVideoTransformation();
  EXPECT_EQ(transform.rotation, VIDEO_ROTATION_0);
  EXPECT_FALSE(transform.mirrored);
}

TEST_F(WebMProjectionParserTest, ProjectionPrivateUnexpectedId) {
  EXPECT_MEDIA_LOG(UnexpectedProjectionId());
  static constexpr auto kData = std::to_array<uint8_t>({0});
  OnBinary(kWebMIdPrimaryBChromaticityX, kData);
}

TEST_F(WebMProjectionParserTest, ProjectionPrivateRectangular) {
  auto* parser = VideoClientOnListStart(kWebMIdProjection);
  parser->OnUInt(kWebMIdProjectionType, 0);
  parser->OnFloat(kWebMIdProjectionPoseYaw, 0.0);
  parser->OnFloat(kWebMIdProjectionPoseRoll, 0.0);
  EXPECT_TRUE(parser->OnBinary(kWebMIdProjectionPrivate, kEquirectPrivateData));
  EXPECT_MEDIA_LOG(ProjectionPrivateMustNotBePresent());
  VideoClientOnListEnd(kWebMIdProjection);
  EXPECT_EQ(static_cast<WebMProjectionParser*>(parser)->GetProjectionType(),
            VideoProjectionType::kNone);
}

TEST_F(WebMProjectionParserTest, ProjectionPrivateInvalidSize) {
  auto* parser = VideoClientOnListStart(kWebMIdProjection);
  parser->OnUInt(kWebMIdProjectionType, 1);
  parser->OnFloat(kWebMIdProjectionPoseYaw, 0.0);
  parser->OnFloat(kWebMIdProjectionPoseRoll, 0.0);
  EXPECT_MEDIA_LOG(UnexpectedProjectionPrivateSize());
  static constexpr auto kData = std::to_array<uint8_t>({
      0x00, 0x00, 0x00, 0x00,  // top
      0x00, 0x00, 0x00, 0x00,  // bottom
      0x00, 0x00               // left (incomplete)
  });
  EXPECT_TRUE(parser->OnBinary(kWebMIdProjectionPrivate, kData));
  VideoClientOnListEnd(kWebMIdProjection);
}

TEST_F(WebMProjectionParserTest, MultipleProjectionPrivate) {
  auto* parser = VideoClientOnListStart(kWebMIdProjection);
  parser->OnUInt(kWebMIdProjectionType, 1);
  EXPECT_TRUE(parser->OnBinary(kWebMIdProjectionPrivate, kEquirectPrivateData));
  EXPECT_MEDIA_LOG(UnexpectedMultipleValuesForProjectionPrivate());
  EXPECT_FALSE(
      parser->OnBinary(kWebMIdProjectionPrivate, kEquirectPrivateData));
}

TEST_F(WebMProjectionParserTest, ProjectionPrivateEquirect360) {
  auto* parser = VideoClientOnListStart(kWebMIdProjection);
  parser->OnUInt(kWebMIdProjectionType, 1);
  parser->OnFloat(kWebMIdProjectionPoseYaw, 0.0);
  parser->OnFloat(kWebMIdProjectionPoseRoll, 0.0);
  EXPECT_TRUE(parser->OnBinary(kWebMIdProjectionPrivate, kEquirectPrivateData));
  VideoClientOnListEnd(kWebMIdProjection);
  EXPECT_EQ(static_cast<WebMProjectionParser*>(parser)->GetProjectionType(),
            VideoProjectionType::kEquirect360);
}

TEST_F(WebMProjectionParserTest, ProjectionPrivateEquirect180) {
  auto* parser = VideoClientOnListStart(kWebMIdProjection);
  parser->OnUInt(kWebMIdProjectionType, 1);
  parser->OnFloat(kWebMIdProjectionPoseYaw, 0.0);
  parser->OnFloat(kWebMIdProjectionPoseRoll, 0.0);
  EXPECT_TRUE(
      parser->OnBinary(kWebMIdProjectionPrivate, kEquirect180PrivateData));
  VideoClientOnListEnd(kWebMIdProjection);
  EXPECT_EQ(static_cast<WebMProjectionParser*>(parser)->GetProjectionType(),
            VideoProjectionType::kEquirect180);
}

TEST_F(WebMProjectionParserTest, ProjectionPrivateBeforeProjectionType) {
  auto* parser = VideoClientOnListStart(kWebMIdProjection);
  EXPECT_TRUE(parser->OnBinary(kWebMIdProjectionPrivate, kEquirectPrivateData));
  parser->OnUInt(kWebMIdProjectionType, 1);
  parser->OnFloat(kWebMIdProjectionPoseYaw, 0.0);
  parser->OnFloat(kWebMIdProjectionPoseRoll, 0.0);
  VideoClientOnListEnd(kWebMIdProjection);
  EXPECT_EQ(static_cast<WebMProjectionParser*>(parser)->GetProjectionType(),
            VideoProjectionType::kEquirect360);
}

TEST_F(WebMProjectionParserTest, ProjectionTypeBeforeProjectionPrivate) {
  auto* parser = VideoClientOnListStart(kWebMIdProjection);
  parser->OnUInt(kWebMIdProjectionType, 1);
  EXPECT_TRUE(parser->OnBinary(kWebMIdProjectionPrivate, kEquirectPrivateData));
  parser->OnFloat(kWebMIdProjectionPoseYaw, 0.0);
  parser->OnFloat(kWebMIdProjectionPoseRoll, 0.0);
  VideoClientOnListEnd(kWebMIdProjection);
  EXPECT_EQ(static_cast<WebMProjectionParser*>(parser)->GetProjectionType(),
            VideoProjectionType::kEquirect360);
}

TEST_F(WebMProjectionParserTest, ProjectionPrivate180BeforeProjectionType) {
  auto* parser = VideoClientOnListStart(kWebMIdProjection);
  EXPECT_TRUE(
      parser->OnBinary(kWebMIdProjectionPrivate, kEquirect180PrivateData));
  parser->OnUInt(kWebMIdProjectionType, 1);
  parser->OnFloat(kWebMIdProjectionPoseYaw, 0.0);
  parser->OnFloat(kWebMIdProjectionPoseRoll, 0.0);
  VideoClientOnListEnd(kWebMIdProjection);
  EXPECT_EQ(static_cast<WebMProjectionParser*>(parser)->GetProjectionType(),
            VideoProjectionType::kEquirect180);
}

TEST_F(WebMProjectionParserTest, ProjectionPrivateMissingEquirect) {
  auto* parser = VideoClientOnListStart(kWebMIdProjection);
  parser->OnUInt(kWebMIdProjectionType, 1);
  parser->OnFloat(kWebMIdProjectionPoseYaw, 0.0);
  parser->OnFloat(kWebMIdProjectionPoseRoll, 0.0);
  EXPECT_MEDIA_LOG(ProjectionPrivateRequiredForEquirect());
  VideoClientOnListEnd(kWebMIdProjection);
}

TEST_F(WebMProjectionParserTest, ProjectionPrivateMissingCubemap) {
  auto* parser = VideoClientOnListStart(kWebMIdProjection);
  parser->OnUInt(kWebMIdProjectionType, 2);
  parser->OnFloat(kWebMIdProjectionPoseYaw, 0.0);
  parser->OnFloat(kWebMIdProjectionPoseRoll, 0.0);
  EXPECT_MEDIA_LOG(ProjectionPrivateRequiredForCubemapOrMesh());
  VideoClientOnListEnd(kWebMIdProjection);
}

TEST_F(WebMProjectionParserTest, ProjectionPrivateMissingMesh) {
  auto* parser = VideoClientOnListStart(kWebMIdProjection);
  parser->OnUInt(kWebMIdProjectionType, 3);
  parser->OnFloat(kWebMIdProjectionPoseYaw, 0.0);
  parser->OnFloat(kWebMIdProjectionPoseRoll, 0.0);
  EXPECT_MEDIA_LOG(ProjectionPrivateRequiredForCubemapOrMesh());
  VideoClientOnListEnd(kWebMIdProjection);
}

}  // namespace media
