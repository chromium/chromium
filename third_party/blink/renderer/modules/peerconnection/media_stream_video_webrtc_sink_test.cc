// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/media_stream_video_webrtc_sink.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_registry.h"
#include "third_party/blink/renderer/modules/mediastream/video_track_adapter_settings.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"

namespace blink {

class MediaStreamVideoWebRtcSinkTest : public ::testing::Test {
 public:
  void SetVideoTrack() {
    registry_.Init();
    registry_.AddVideoTrack("test video track");
    auto video_components = registry_.test_stream()->VideoComponents();
    component_ = video_components[0];
    // TODO(hta): Verify that component_ is valid. When constraints produce
    // no valid format, using the track will cause a crash.
  }

  void SetVideoTrack(const base::Optional<bool>& noise_reduction) {
    registry_.Init();
    registry_.AddVideoTrack("test video track",
                            blink::VideoTrackAdapterSettings(), noise_reduction,
                            false, 0.0);
    auto video_components = registry_.test_stream()->VideoComponents();
    component_ = video_components[0];
    // TODO(hta): Verify that component_ is valid. When constraints produce
    // no valid format, using the track will cause a crash.
  }

 protected:
  Persistent<MediaStreamComponent> component_;
  blink::MockPeerConnectionDependencyFactory dependency_factory_;

 private:
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;

  blink::MockMediaStreamRegistry registry_;
};

TEST_F(MediaStreamVideoWebRtcSinkTest, NoiseReductionDefaultsToNotSet) {
  SetVideoTrack();
  blink::MediaStreamVideoWebRtcSink my_sink(
      component_, &dependency_factory_,
      blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  EXPECT_TRUE(my_sink.webrtc_video_track());
  EXPECT_FALSE(my_sink.SourceNeedsDenoisingForTesting());
}

}  // namespace blink
