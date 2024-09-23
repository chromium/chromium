// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/peerconnection/h265_parameter_sets_tracker.h"

#include <string.h>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

// VPS/SPS/PPS/IDR for a 1280x720 camera capture from ffmpeg on linux.
// Contains emulation bytes but no cropping. This buffer is generated with
// following command: 1) ffmpeg -i /dev/video0 -r 30 -c:v libx265 -s 1280x720
// camera.h265
//
// The VPS/SPS/PPS are kept intact while idr1/idr2/cra1/cra2/trail1/trail2 are
// created by changing the NALU type of original IDR/TRAIL_R NALUs, and
// truncated only for testing of the tracker.
uint8_t vps[] = {0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0c, 0x01, 0xff, 0xff,
                 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0x90, 0x00, 0x00, 0x03,
                 0x00, 0x00, 0x03, 0x00, 0x5d, 0x95, 0x98, 0x09};
uint8_t sps[] = {0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0x01, 0x01, 0x60,
                 0x00, 0x00, 0x03, 0x00, 0x90, 0x00, 0x00, 0x03, 0x00,
                 0x00, 0x03, 0x00, 0x5d, 0xa0, 0x02, 0x80, 0x80, 0x2d,
                 0x16, 0x59, 0x59, 0xa4, 0x93, 0x2b, 0xc0, 0x5a, 0x70,
                 0x80, 0x00, 0x01, 0xf4, 0x80, 0x00, 0x3a, 0x98, 0x04};
uint8_t pps[] = {0x00, 0x00, 0x00, 0x01, 0x44, 0x01,
                 0xc1, 0x72, 0xb4, 0x62, 0x40};
uint8_t idr1[] = {0x00, 0x00, 0x00, 0x01, 0x28, 0x01, 0xaf,
                  0x08, 0x46, 0x0c, 0x92, 0xa3, 0xf4, 0x77};
uint8_t idr2[] = {0x00, 0x00, 0x00, 0x01, 0x28, 0x01, 0xaf,
                  0x08, 0x46, 0x0c, 0x92, 0xa3, 0xf4, 0x77};
uint8_t trail1[] = {0x00, 0x00, 0x00, 0x01, 0x02, 0x01, 0xa4, 0x04, 0x55,
                    0xa2, 0x6d, 0xce, 0xc0, 0xc3, 0xed, 0x0b, 0xac, 0xbc,
                    0x00, 0xc4, 0x44, 0x2e, 0xf7, 0x55, 0xfd, 0x05, 0x86};
uint8_t trail2[] = {0x00, 0x00, 0x00, 0x01, 0x02, 0x01, 0x23, 0xfc, 0x20,
                    0x22, 0xad, 0x13, 0x68, 0xce, 0xc3, 0x5a, 0x00, 0x01,
                    0x80, 0xe9, 0xc6, 0x38, 0x13, 0xec, 0xef, 0x0f, 0xff};
uint8_t cra[] = {0x00, 0x00, 0x00, 0x01, 0x2A, 0x01, 0xad, 0x00, 0x58, 0x81,
                 0x04, 0x11, 0xc2, 0x00, 0x44, 0x3f, 0x34, 0x46, 0x3e, 0xcc,
                 0x86, 0xd9, 0x3f, 0xf1, 0xe1, 0xda, 0x26, 0xb1, 0xc5, 0x50,
                 0xf2, 0x8b, 0x8d, 0x0c, 0xe9, 0xe1, 0xd3, 0xe0, 0xa7, 0x3e};

// Below two H264 binaries are copied from h264 bitstream parser unittests,
// to check the behavior of the tracker on stream from mismatched encoder.
uint8_t sps_pps_h264[] = {0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x80, 0x20, 0xda,
                          0x01, 0x40, 0x16, 0xe8, 0x06, 0xd0, 0xa1, 0x35, 0x00,
                          0x00, 0x00, 0x01, 0x68, 0xce, 0x06, 0xe2};
uint8_t idr_h264[] = {
    0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x80, 0x20, 0xda, 0x01, 0x40, 0x16,
    0xe8, 0x06, 0xd0, 0xa1, 0x35, 0x00, 0x00, 0x00, 0x01, 0x68, 0xce, 0x06,
    0xe2, 0x00, 0x00, 0x00, 0x01, 0x65, 0xb8, 0x40, 0xf0, 0x8c, 0x03, 0xf2,
    0x75, 0x67, 0xad, 0x41, 0x64, 0x24, 0x0e, 0xa0, 0xb2, 0x12, 0x1e, 0xf8,
};

using ::testing::ElementsAreArray;

rtc::ArrayView<const uint8_t> Bitstream(
    const H265ParameterSetsTracker::FixedBitstream& fixed) {
  return rtc::ArrayView<const uint8_t>(fixed.bitstream->data(),
                                       fixed.bitstream->size());
}

}  // namespace

class H265ParameterSetsTrackerTest : public ::testing::Test {
 public:
  H265ParameterSetsTracker tracker_;
};

TEST_F(H265ParameterSetsTrackerTest, NoNalus) {
  uint8_t data[] = {1, 2, 3};

  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  EXPECT_THAT(fixed.action,
              H265ParameterSetsTracker::PacketAction::kPassThrough);
}

TEST_F(H265ParameterSetsTrackerTest, StreamFromMissMatchingH26xCodec) {
  std::vector<uint8_t> data;
  unsigned sps_pps_size = sizeof(sps_pps_h264) / sizeof(sps_pps_h264[0]);
  unsigned idr_size = sizeof(idr_h264) / sizeof(idr_h264[0]);
  data.insert(data.end(), sps_pps_h264, sps_pps_h264 + sps_pps_size);
  data.insert(data.end(), idr_h264, idr_h264 + idr_size);
  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  // This is not an H.265 stream. We simply pass through it.
  EXPECT_THAT(fixed.action,
              H265ParameterSetsTracker::PacketAction::kPassThrough);
}

TEST_F(H265ParameterSetsTrackerTest, AllParameterSetsInCurrentIdrSingleSlice) {
  std::vector<uint8_t> data;
  data.clear();
  unsigned vps_size = sizeof(vps) / sizeof(uint8_t);
  unsigned sps_size = sizeof(sps) / sizeof(uint8_t);
  unsigned pps_size = sizeof(pps) / sizeof(uint8_t);
  unsigned idr_size = sizeof(idr1) / sizeof(uint8_t);
  data.insert(data.end(), vps, vps + vps_size);
  data.insert(data.end(), sps, sps + sps_size);
  data.insert(data.end(), pps, pps + pps_size);
  data.insert(data.end(), idr1, idr1 + idr_size - 1);
  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  EXPECT_THAT(fixed.action,
              H265ParameterSetsTracker::PacketAction::kPassThrough);
}

TEST_F(H265ParameterSetsTrackerTest, AllParameterSetsMissingForIdr) {
  std::vector<uint8_t> data;
  unsigned idr_size = sizeof(idr1) / sizeof(idr1[0]);
  data.insert(data.end(), idr1, idr1 + idr_size);
  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  EXPECT_THAT(fixed.action,
              H265ParameterSetsTracker::PacketAction::kRequestKeyframe);
}

TEST_F(H265ParameterSetsTrackerTest, VpsMissingForIdr) {
  std::vector<uint8_t> data;
  unsigned idr_size = sizeof(idr1) / sizeof(idr1[0]);
  unsigned sps_size = sizeof(sps) / sizeof(sps[0]);
  unsigned pps_size = sizeof(pps) / sizeof(pps[0]);
  data.insert(data.end(), sps, sps + sps_size);
  data.insert(data.end(), pps, pps + pps_size);
  data.insert(data.end(), idr1, idr1 + idr_size);
  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  EXPECT_THAT(fixed.action,
              H265ParameterSetsTracker::PacketAction::kRequestKeyframe);
}

TEST_F(H265ParameterSetsTrackerTest,
       ParameterSetsSeenBeforeButRepeatedVpsMissingForCurrentIdr) {
  std::vector<uint8_t> data;
  unsigned vps_size = sizeof(vps) / sizeof(vps[0]);
  unsigned sps_size = sizeof(sps) / sizeof(sps[0]);
  unsigned pps_size = sizeof(pps) / sizeof(pps[0]);
  unsigned idr_size = sizeof(idr1) / sizeof(idr1[0]);
  data.insert(data.end(), vps, vps + vps_size);
  data.insert(data.end(), sps, sps + sps_size);
  data.insert(data.end(), pps, pps + pps_size);
  data.insert(data.end(), idr1, idr1 + idr_size);
  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  EXPECT_THAT(fixed.action,
              H265ParameterSetsTracker::PacketAction::kPassThrough);

  // Second IDR but encoder only repeats SPS/PPS(unlikely to happen).
  std::vector<uint8_t> frame2;
  unsigned sps2_size = sizeof(sps) / sizeof(sps[0]);
  unsigned pps2_size = sizeof(pps) / sizeof(pps[0]);
  unsigned idr2_size = sizeof(idr2) / sizeof(idr2[0]);
  frame2.insert(frame2.end(), sps, sps + sps2_size);
  frame2.insert(frame2.end(), pps, pps + pps2_size);
  frame2.insert(frame2.end(), idr2, idr2 + idr2_size);
  fixed = tracker_.MaybeFixBitstream(frame2);

  // If any of the parameter set is missing, we append all of VPS/SPS/PPS and it
  // is fine to repeat any of the parameter set twice for current IDR.
  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::PacketAction::kInsert);
  std::vector<uint8_t> expected;
  expected.insert(expected.end(), vps, vps + vps_size);
  expected.insert(expected.end(), sps, sps + sps_size);
  expected.insert(expected.end(), pps, pps + pps_size);
  expected.insert(expected.end(), sps, sps + sps_size);
  expected.insert(expected.end(), pps, pps + pps_size);
  expected.insert(expected.end(), idr2, idr2 + idr2_size);
  EXPECT_THAT(Bitstream(fixed), ElementsAreArray(expected));
}

TEST_F(H265ParameterSetsTrackerTest,
       AllParameterSetsInCurrentIdrMulitpleSlices) {
  std::vector<uint8_t> data;
  unsigned vps_size = sizeof(vps) / sizeof(vps[0]);
  unsigned sps_size = sizeof(sps) / sizeof(sps[0]);
  unsigned pps_size = sizeof(pps) / sizeof(pps[0]);
  unsigned idr1_size = sizeof(idr1) / sizeof(idr1[0]);
  unsigned idr2_size = sizeof(idr2) / sizeof(idr2[0]);
  data.insert(data.end(), vps, vps + vps_size);
  data.insert(data.end(), sps, sps + sps_size);
  data.insert(data.end(), pps, pps + pps_size);
  data.insert(data.end(), idr1, idr1 + idr1_size);
  data.insert(data.end(), idr2, idr2 + idr2_size);
  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  EXPECT_THAT(fixed.action,
              H265ParameterSetsTracker::PacketAction::kPassThrough);
}

TEST_F(H265ParameterSetsTrackerTest,
       SingleDeltaSliceWithoutParameterSetsBefore) {
  std::vector<uint8_t> data;
  unsigned trail_size = sizeof(trail1) / sizeof(trail1[0]);
  data.insert(data.end(), trail1, trail1 + trail_size);
  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  EXPECT_THAT(fixed.action,
              H265ParameterSetsTracker::PacketAction::kPassThrough);
}

TEST_F(H265ParameterSetsTrackerTest,
       MultipleDeltaSlicseWithoutParameterSetsBefore) {
  std::vector<uint8_t> data;
  unsigned trail1_size = sizeof(trail1) / sizeof(trail1[0]);
  unsigned trail2_size = sizeof(trail2) / sizeof(trail2[0]);
  data.insert(data.end(), trail1, trail1 + trail1_size);
  data.insert(data.end(), trail2, trail2 + trail2_size);
  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  EXPECT_THAT(fixed.action,
              H265ParameterSetsTracker::PacketAction::kPassThrough);
}

TEST_F(H265ParameterSetsTrackerTest,
       ParameterSetsInPreviousIdrNotInCurrentIdr) {
  std::vector<uint8_t> data;
  unsigned vps_size = sizeof(vps) / sizeof(vps[0]);
  unsigned sps_size = sizeof(sps) / sizeof(sps[0]);
  unsigned pps_size = sizeof(pps) / sizeof(pps[0]);
  unsigned idr_size = sizeof(idr1) / sizeof(idr1[0]);
  data.insert(data.end(), vps, vps + vps_size);
  data.insert(data.end(), sps, sps + sps_size);
  data.insert(data.end(), pps, pps + pps_size);
  data.insert(data.end(), idr1, idr1 + idr_size);
  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  EXPECT_THAT(fixed.action,
              H265ParameterSetsTracker::PacketAction::kPassThrough);

  std::vector<uint8_t> frame2;
  unsigned idr2_size = sizeof(idr2) / sizeof(idr2[0]);
  frame2.insert(frame2.end(), idr2, idr2 + idr2_size);
  fixed = tracker_.MaybeFixBitstream(frame2);

  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::PacketAction::kInsert);

  std::vector<uint8_t> expected;
  expected.insert(expected.end(), vps, vps + vps_size);
  expected.insert(expected.end(), sps, sps + sps_size);
  expected.insert(expected.end(), pps, pps + pps_size);
  expected.insert(expected.end(), idr2, idr2 + idr2_size);
  EXPECT_THAT(Bitstream(fixed), ElementsAreArray(expected));
}

TEST_F(H265ParameterSetsTrackerTest,
       ParameterSetsInPreviousIdrNotInCurrentCra) {
  std::vector<uint8_t> data;
  unsigned vps_size = sizeof(vps) / sizeof(vps[0]);
  unsigned sps_size = sizeof(sps) / sizeof(sps[0]);
  unsigned pps_size = sizeof(pps) / sizeof(pps[0]);
  unsigned idr_size = sizeof(idr1) / sizeof(idr1[0]);
  data.insert(data.end(), vps, vps + vps_size);
  data.insert(data.end(), sps, sps + sps_size);
  data.insert(data.end(), pps, pps + pps_size);
  data.insert(data.end(), idr1, idr1 + idr_size);
  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  EXPECT_THAT(fixed.action,
              H265ParameterSetsTracker::PacketAction::kPassThrough);

  std::vector<uint8_t> frame2;
  unsigned cra_size = sizeof(cra) / sizeof(cra[0]);
  frame2.insert(frame2.end(), cra, cra + cra_size);
  fixed = tracker_.MaybeFixBitstream(frame2);

  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::PacketAction::kInsert);
  std::vector<uint8_t> expected;
  expected.insert(expected.end(), vps, vps + vps_size);
  expected.insert(expected.end(), sps, sps + sps_size);
  expected.insert(expected.end(), pps, pps + pps_size);
  expected.insert(expected.end(), cra, cra + cra_size);
  EXPECT_THAT(Bitstream(fixed), ElementsAreArray(expected));
}

TEST_F(H265ParameterSetsTrackerTest, ParameterSetsInBothPreviousAndCurrentIdr) {
  std::vector<uint8_t> data;
  unsigned vps_size = sizeof(vps) / sizeof(vps[0]);
  unsigned sps_size = sizeof(sps) / sizeof(sps[0]);
  unsigned pps_size = sizeof(pps) / sizeof(pps[0]);
  unsigned idr_size = sizeof(idr1) / sizeof(idr1[0]);
  data.insert(data.end(), vps, vps + vps_size);
  data.insert(data.end(), sps, sps + sps_size);
  data.insert(data.end(), pps, pps + pps_size);
  data.insert(data.end(), idr1, idr1 + idr_size);
  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  EXPECT_THAT(fixed.action,
              H265ParameterSetsTracker::PacketAction::kPassThrough);

  std::vector<uint8_t> frame2;
  unsigned idr2_size = sizeof(idr2) / sizeof(idr2[0]);
  frame2.insert(frame2.end(), vps, vps + vps_size);
  frame2.insert(frame2.end(), sps, sps + sps_size);
  frame2.insert(frame2.end(), pps, pps + pps_size);
  frame2.insert(frame2.end(), idr2, idr2 + idr2_size);
  fixed = tracker_.MaybeFixBitstream(frame2);

  EXPECT_THAT(fixed.action,
              H265ParameterSetsTracker::PacketAction::kPassThrough);
}

TEST_F(H265ParameterSetsTrackerTest, TwoGopsWithIdrTrailAndCra) {
  std::vector<uint8_t> data;
  unsigned vps_size = sizeof(vps) / sizeof(vps[0]);
  unsigned sps_size = sizeof(sps) / sizeof(sps[0]);
  unsigned pps_size = sizeof(pps) / sizeof(pps[0]);
  unsigned idr_size = sizeof(idr1) / sizeof(idr1[0]);
  data.insert(data.end(), vps, vps + vps_size);
  data.insert(data.end(), sps, sps + sps_size);
  data.insert(data.end(), pps, pps + pps_size);
  data.insert(data.end(), idr1, idr1 + idr_size);
  H265ParameterSetsTracker::FixedBitstream fixed =
      tracker_.MaybeFixBitstream(data);

  EXPECT_THAT(fixed.action,
              H265ParameterSetsTracker::PacketAction::kPassThrough);

  // Second frame, a TRAIL_R picture.
  std::vector<uint8_t> frame2;
  unsigned trail_size = sizeof(trail1) / sizeof(trail1[0]);
  frame2.insert(frame2.end(), trail1, trail1 + trail_size);
  fixed = tracker_.MaybeFixBitstream(frame2);

  EXPECT_THAT(fixed.action,
              H265ParameterSetsTracker::PacketAction::kPassThrough);

  // Third frame, a TRAIL_R picture.
  std::vector<uint8_t> frame3;
  unsigned trail2_size = sizeof(trail2) / sizeof(trail2[0]);
  frame3.insert(frame3.end(), trail2, trail2 + trail2_size);
  fixed = tracker_.MaybeFixBitstream(frame3);

  EXPECT_THAT(fixed.action,
              H265ParameterSetsTracker::PacketAction::kPassThrough);

  // Fourth frame, a CRA picture.
  std::vector<uint8_t> frame4;
  unsigned cra_size = sizeof(cra) / sizeof(cra[0]);
  frame4.insert(frame4.end(), cra, cra + cra_size);
  fixed = tracker_.MaybeFixBitstream(frame4);

  EXPECT_THAT(fixed.action, H265ParameterSetsTracker::PacketAction::kInsert);

  std::vector<uint8_t> expected;
  expected.insert(expected.end(), vps, vps + vps_size);
  expected.insert(expected.end(), sps, sps + sps_size);
  expected.insert(expected.end(), pps, pps + pps_size);
  expected.insert(expected.end(), cra, cra + cra_size);
  EXPECT_THAT(Bitstream(fixed), ElementsAreArray(expected));

  // Last frame, a TRAIL_R picture with 2 slices.
  std::vector<uint8_t> frame5;
  unsigned trail3_size = sizeof(trail1) / sizeof(trail1[0]);
  unsigned trail4_size = sizeof(trail2) / sizeof(trail2[0]);
  frame5.insert(frame5.end(), trail1, trail1 + trail3_size);
  frame5.insert(frame5.end(), trail2, trail2 + trail4_size);
  fixed = tracker_.MaybeFixBitstream(frame5);

  EXPECT_THAT(fixed.action,
              H265ParameterSetsTracker::PacketAction::kPassThrough);
}

}  // namespace blink
