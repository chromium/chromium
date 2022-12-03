// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// gtest.h has to be included first.
// See http://code.google.com/p/googletest/issues/detail?id=371
#include "testing/gtest/include/gtest/gtest.h"

#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "media/base/video_codecs.h"
#include "media/gpu/v4l2/v4l2_device.h"

namespace media {

namespace {

#define TOSTR(enumCase) \
  case enumCase:        \
    return #enumCase

const char* VideoCodecProfileToString(VideoCodecProfile profile) {
  switch (profile) {
    TOSTR(H264PROFILE_BASELINE);
    TOSTR(H264PROFILE_MAIN);
    TOSTR(H264PROFILE_EXTENDED);
    TOSTR(H264PROFILE_HIGH);
    TOSTR(VP8PROFILE_ANY);
    TOSTR(VP9PROFILE_PROFILE0);
    TOSTR(AV1PROFILE_PROFILE_MAIN);
    default:
      return "profile_not_enumerated";
  }
}

}  // namespace

class V4L2MinigbmTest
    : public testing::TestWithParam<std::tuple<VideoCodecProfile, gfx::Size>> {
 public:
  V4L2MinigbmTest() = default;
  ~V4L2MinigbmTest() = default;

  struct PrintToStringParamName {
    template <class ParamType>
    std::string operator()(
        const testing::TestParamInfo<ParamType>& info) const {
      return base::StringPrintf(
          "%s__%s", VideoCodecProfileToString(std::get<0>(info.param)),
          std::get<1>(info.param).ToString().c_str());
    }
  };
};

// This test sets up a v4l2 device for the given video codec profiles,
// and resolution  (as per the test parameters). It then verifies that
// said metadata (e.g. width, height, number of planes, pitch) are the
// same as those we would allocate via minigbm.
TEST_P(V4L2MinigbmTest, AllocateAndCompareWithMinigbm) {
  GTEST_SKIP() << "Test not implemented yet";
}

constexpr VideoCodecProfile kVideoCodecProfiles[] = {H264PROFILE_BASELINE};
constexpr gfx::Size kResolutions[] = {gfx::Size(127, 128), gfx::Size(128, 128),
                                      gfx::Size(323, 243), gfx::Size(640, 360),
                                      gfx::Size(1280, 720)};

INSTANTIATE_TEST_SUITE_P(
    ,
    V4L2MinigbmTest,
    ::testing::Combine(::testing::ValuesIn(kVideoCodecProfiles),
                       ::testing::ValuesIn(kResolutions)),
    V4L2MinigbmTest::PrintToStringParamName());

}  // namespace media

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);
  {}

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}