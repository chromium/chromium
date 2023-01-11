// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "media/base/video_codecs.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "media/mojo/services/webrtc_video_perf_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace media {

namespace {

// Aliases for readability.
const bool kDecode = true;
const bool kEncode = false;
const VideoCodecProfile kProfileA = H264PROFILE_MIN;
const VideoCodecProfile kProfileB = VP9PROFILE_MIN;
const VideoCodecProfile kProfileC = VP8PROFILE_MIN;
const VideoCodecProfile kProfileUnknown = VIDEO_CODEC_PROFILE_UNKNOWN;
const int kPixelSizeA = 1280 * 720;
const int kPixelSizeB = 1920 * 1080;
const bool kHw = true;
const bool kSw = false;

MATCHER_P(MojoEq, value, "") {
  return arg.Equals(value);
}

}  // namespace

using Features = media::mojom::WebrtcPredictionFeatures;
using VideoStats = media::mojom::WebrtcVideoStats;

class WebrtcVideoPerfRecorderTest : public ::testing::Test {
 public:
  void MakeRecorder() {
    recorder_ = std::make_unique<WebrtcVideoPerfRecorder>(
        base::BindRepeating(&WebrtcVideoPerfRecorderTest::SavePerfCallback,
                            base::Unretained(this)));
  }
  ~WebrtcVideoPerfRecorderTest() override = default;

  MOCK_METHOD3(SavePerfCallback,
               void(media::mojom::WebrtcPredictionFeatures features,
                    media::mojom::WebrtcVideoStats targets,
                    base::OnceClosure save_done_cb));

 protected:
  std::unique_ptr<WebrtcVideoPerfRecorder> recorder_;
};

TEST_F(WebrtcVideoPerfRecorderTest, SaveOnNewConfig) {
  MakeRecorder();
  // Update decode entry.
  recorder_->UpdateRecord(Features::New(kDecode, kProfileA, kPixelSizeA, kSw),
                          VideoStats::New(11, 3, 12.0f));
  // New data for the same decode entry should not result in a callback.
  recorder_->UpdateRecord(Features::New(kDecode, kProfileA, kPixelSizeA, kSw),
                          VideoStats::New(111, 8, 11.7f));

  // Update encode entry. This should not result in a callback either since
  // encode and decode states are tracked individually.
  recorder_->UpdateRecord(Features::New(kEncode, kProfileC, kPixelSizeA, kSw),
                          VideoStats::New(13, 7, 17.0f));

  // Expect save with the previous state upon changing to a HW decoder.
  EXPECT_CALL(*this,
              SavePerfCallback((Features(kDecode, kProfileA, kPixelSizeA, kSw)),
                               (VideoStats(111, 8, 11.7f)), _))
      .Times(1);
  recorder_->UpdateRecord(Features::New(kDecode, kProfileA, kPixelSizeA, kHw),
                          VideoStats::New(15, 4, 9.0f));

  // Expect save with the previous state upon changing decode pixel size.
  EXPECT_CALL(*this,
              SavePerfCallback((Features(kDecode, kProfileA, kPixelSizeA, kHw)),
                               (VideoStats(15, 4, 9.0f)), _))
      .Times(1);
  recorder_->UpdateRecord(Features::New(kDecode, kProfileA, kPixelSizeB, kHw),
                          VideoStats::New(17, 6, 14.0f));

  // Expect save with the previous state upon changing decode codec profile.
  EXPECT_CALL(*this,
              SavePerfCallback((Features(kDecode, kProfileA, kPixelSizeB, kHw)),
                               (VideoStats(17, 6, 14.0f)), _))
      .Times(1);
  recorder_->UpdateRecord(Features::New(kDecode, kProfileB, kPixelSizeB, kHw),
                          VideoStats::New(21, 5, 13.0f));

  // An empty record saves the current state.
  EXPECT_CALL(*this,
              SavePerfCallback((Features(kDecode, kProfileB, kPixelSizeB, kHw)),
                               (VideoStats(21, 5, 13.0f)), _))
      .Times(1);
  recorder_->UpdateRecord(Features::New(kDecode, kProfileUnknown, 0, kHw),
                          VideoStats::New(0, 0, 0.0f));

  EXPECT_CALL(*this,
              SavePerfCallback((Features(kEncode, kProfileC, kPixelSizeA, kSw)),
                               (VideoStats(13, 7, 17.0f)), _))
      .Times(1);
  recorder_->UpdateRecord(Features::New(kEncode, kProfileUnknown, 0, kHw),
                          VideoStats::New(0, 0, 0.0f));
}

TEST_F(WebrtcVideoPerfRecorderTest, SaveOnDestruction) {
  MakeRecorder();
  // Update decode entry.
  recorder_->UpdateRecord(Features::New(kDecode, kProfileA, kPixelSizeA, kSw),
                          VideoStats::New(11, 3, 12.0f));
  // Update encode entry. This should not result in a callback either since
  // encode and decode states are tracked individually.
  recorder_->UpdateRecord(Features::New(kEncode, kProfileB, kPixelSizeB, kHw),
                          VideoStats::New(13, 7, 17.0f));

  // Expect save for both encode and decode upon destruction.
  EXPECT_CALL(*this, SavePerfCallback(
                         MojoEq(Features(kDecode, kProfileA, kPixelSizeA, kSw)),
                         MojoEq(VideoStats(11, 3, 12.0f)), _))
      .Times(1);
  EXPECT_CALL(*this, SavePerfCallback(
                         MojoEq(Features(kEncode, kProfileB, kPixelSizeB, kHw)),
                         MojoEq(VideoStats(13, 7, 17.0f)), _))
      .Times(1);

  recorder_.reset();
}

}  // namespace media
