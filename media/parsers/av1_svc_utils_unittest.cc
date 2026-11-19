// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/parsers/av1_svc_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

libgav1::ObuSequenceHeader SequenceHeader(int operating_points,
                                          int operating_point_idc) {
  libgav1::ObuSequenceHeader sequence_header = {};
  sequence_header.operating_points = operating_points;
  sequence_header.operating_point_idc[0] = operating_point_idc;
  return sequence_header;
}

libgav1::ObuHeader ObuHeader(int8_t spatial_id) {
  libgav1::ObuHeader header = {};
  header.spatial_id = spatial_id;
  return header;
}

}  // namespace

TEST(AV1SvcUtilsTest, WildcardOperatingPointIsRtpWildcard) {
  EXPECT_TRUE(IsAV1RtpWildcardOperatingPoint(SequenceHeader(1, 0xfff)));
}

TEST(AV1SvcUtilsTest, WildcardIdcWithSeveralOperatingPointsIsNotRtpWildcard) {
  // The wildcard must be the only operating point.
  EXPECT_FALSE(IsAV1RtpWildcardOperatingPoint(SequenceHeader(2, 0xfff)));
}

TEST(AV1SvcUtilsTest, NoScalabilityIsNotRtpWildcard) {
  EXPECT_FALSE(IsAV1RtpWildcardOperatingPoint(SequenceHeader(1, 0)));
}

TEST(AV1SvcUtilsTest, ExplicitSpatialLayersAreNotRtpWildcard) {
  EXPECT_FALSE(IsAV1RtpWildcardOperatingPoint(SequenceHeader(1, 0x303)));
}

TEST(AV1SvcUtilsTest, NoScalabilityIsNotSpatialLayerSequence) {
  EXPECT_FALSE(IsAV1SpatialLayerSequence(SequenceHeader(1, 0)));
}

TEST(AV1SvcUtilsTest, L1T3IsNotSpatialLayerSequence) {
  // Temporal layers only.
  EXPECT_FALSE(IsAV1SpatialLayerSequence(SequenceHeader(3, 0x107)));
}

TEST(AV1SvcUtilsTest, L2T2IsSpatialLayerSequence) {
  EXPECT_TRUE(IsAV1SpatialLayerSequence(SequenceHeader(4, 0x303)));
}

TEST(AV1SvcUtilsTest, L3T3IsSpatialLayerSequence) {
  EXPECT_TRUE(IsAV1SpatialLayerSequence(SequenceHeader(9, 0x707)));
}

TEST(AV1SvcUtilsTest, RtpWildcardIsNotSpatialLayerSequence) {
  // The RTP wildcard does not explicitly specify spatial layers.
  EXPECT_FALSE(IsAV1SpatialLayerSequence(SequenceHeader(1, 0xfff)));
}

TEST(AV1SvcUtilsTest, NoObusHaveNoSpatialLayerObu) {
  libgav1::Vector<libgav1::ObuHeader> obu_headers;
  EXPECT_FALSE(HasAV1SpatialLayerObu(obu_headers));
}

TEST(AV1SvcUtilsTest, BaseLayerObuIsNotSpatialLayerObu) {
  libgav1::Vector<libgav1::ObuHeader> obu_headers;
  ASSERT_TRUE(obu_headers.push_back(ObuHeader(/*spatial_id=*/0)));
  EXPECT_FALSE(HasAV1SpatialLayerObu(obu_headers));
}

TEST(AV1SvcUtilsTest, UpperLayerObuIsSpatialLayerObu) {
  libgav1::Vector<libgav1::ObuHeader> obu_headers;
  ASSERT_TRUE(obu_headers.push_back(ObuHeader(/*spatial_id=*/0)));
  ASSERT_TRUE(obu_headers.push_back(ObuHeader(/*spatial_id=*/1)));
  EXPECT_TRUE(HasAV1SpatialLayerObu(obu_headers));
}

}  // namespace media
