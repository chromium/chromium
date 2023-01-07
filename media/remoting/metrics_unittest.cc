// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/metrics.h"

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using testing::ElementsAre;

namespace media {
namespace remoting {

namespace {

class MediaRemotingMetricsTest : public testing::Test {
 protected:
  media::remoting::SessionMetricsRecorder recorder_;
};

}  // namespace

TEST_F(MediaRemotingMetricsTest, RecordVideoPixelRateSupport) {
  constexpr char kPixelRateSupportHistogramName[] =
      "Media.Remoting.VideoPixelRateSupport";
  base::HistogramTester tester;
  tester.ExpectTotalCount(kPixelRateSupportHistogramName, 0);

  recorder_.RecordVideoPixelRateSupport(PixelRateSupport::k4kNotSupported);
  recorder_.RecordVideoPixelRateSupport(PixelRateSupport::k2kSupported);
  recorder_.RecordVideoPixelRateSupport(PixelRateSupport::k4kNotSupported);

  // We record only for the first RecordVideoPixelRateSupport() call for the
  // given SessionMetricsRecorder instance.
  EXPECT_THAT(tester.GetAllSamples(kPixelRateSupportHistogramName),
              ElementsAre(Bucket(
                  static_cast<int>(PixelRateSupport::k4kNotSupported), 1)));
}

TEST_F(MediaRemotingMetricsTest, RecordCompatibility) {
  constexpr char kCompatibilityHistogramName[] = "Media.Remoting.Compatibility";
  base::HistogramTester tester;
  tester.ExpectTotalCount(kCompatibilityHistogramName, 0);

  recorder_.RecordCompatibility(RemotingCompatibility::kIncompatibleVideoCodec);
  recorder_.RecordCompatibility(RemotingCompatibility::kCompatible);
  recorder_.RecordCompatibility(RemotingCompatibility::kIncompatibleVideoCodec);

  // We record only for the first RecordCompatibility() call for the
  // given SessionMetricsRecorder instance.
  EXPECT_THAT(
      tester.GetAllSamples(kCompatibilityHistogramName),
      ElementsAre(Bucket(
          static_cast<int>(RemotingCompatibility::kIncompatibleVideoCodec),
          1)));
}

}  // namespace remoting
}  // namespace media
