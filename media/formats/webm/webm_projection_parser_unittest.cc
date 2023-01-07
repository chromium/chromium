// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/webm/webm_projection_parser.h"

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

MATCHER(UnexpectedMultipleValuesForYaw, "") {
  return CONTAINS_STRING(arg, "Multiple values for id: 0x7673");
}

MATCHER(UnexpectedMultipleValuesForPitch, "") {
  return CONTAINS_STRING(arg, "Multiple values for id: 0x7674");
}

MATCHER(UnexpectedMultipleValuesForRoll, "") {
  return CONTAINS_STRING(arg, "Multiple values for id: 0x7675");
}

MATCHER(UnexpectedProjectionYaw, "") {
  return CONTAINS_STRING(arg, "Value not within valid range. id: 0x7673 val:");
}

MATCHER(UnexpectedProjectionPitch, "") {
  return CONTAINS_STRING(arg, "Value not within valid range. id: 0x7674 val:");
}

MATCHER(UnexpectedProjectionRoll, "") {
  return CONTAINS_STRING(arg, "Value not within valid range. id: 0x7675 val:");
}

MATCHER(MissingProjectionType, "") {
  return CONTAINS_STRING(
      arg, "Projection element is incomplete; ProjectionType required.");
}

MATCHER(MissingProjectionPoseYaw, "") {
  return CONTAINS_STRING(
      arg, "Projection element is incomplete; ProjectionPoseYaw required.");
}

MATCHER(MissingProjectionPosePitch, "") {
  return CONTAINS_STRING(
      arg, "Projection element is incomplete; ProjectionPosePitch required.");
}

MATCHER(MissingProjectionPoseRoll, "") {
  return CONTAINS_STRING(
      arg, "Projection element is incomplete; ProjectionPoseRoll required.");
}

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

TEST_F(WebMProjectionParserTest, InvalidProjectionPitch) {
  EXPECT_MEDIA_LOG(UnexpectedProjectionPitch());
  OnFloat(kWebMIdProjectionPosePitch, 91);
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

TEST_F(WebMProjectionParserTest, MultipleProjectionPitch) {
  OnFloat(kWebMIdProjectionPosePitch, 90);
  EXPECT_MEDIA_LOG(UnexpectedMultipleValuesForPitch());
  OnFloat(kWebMIdProjectionPosePitch, 90);
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

TEST_F(WebMProjectionParserTest, MissingProjectionPosYaw) {
  auto* parser = VideoClientOnListStart(kWebMIdProjection);
  parser->OnUInt(kWebMIdProjectionType, 1);
  EXPECT_MEDIA_LOG(MissingProjectionPoseYaw());
  VideoClientOnListEnd(kWebMIdProjection);
}

TEST_F(WebMProjectionParserTest, MissingProjectionPosePitch) {
  auto* parser = VideoClientOnListStart(kWebMIdProjection);
  parser->OnUInt(kWebMIdProjectionType, 1);
  parser->OnFloat(kWebMIdProjectionPoseYaw, 90);
  EXPECT_MEDIA_LOG(MissingProjectionPosePitch());
  VideoClientOnListEnd(kWebMIdProjection);
}

TEST_F(WebMProjectionParserTest, MissingProjectionPoseRoll) {
  auto* parser = VideoClientOnListStart(kWebMIdProjection);
  parser->OnUInt(kWebMIdProjectionType, 1);
  parser->OnFloat(kWebMIdProjectionPoseYaw, 90);
  parser->OnFloat(kWebMIdProjectionPosePitch, 90);
  EXPECT_MEDIA_LOG(MissingProjectionPoseRoll());
  VideoClientOnListEnd(kWebMIdProjection);
}

}  // namespace media
