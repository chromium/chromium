// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/media_stream_video_webrtc_sink.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_registry.h"
#include "third_party/blink/renderer/modules/mediastream/video_track_adapter_settings.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

using ::testing::AllOf;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Optional;

class MockWebRtcVideoSink : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
 public:
  MOCK_METHOD(void, OnFrame, (const webrtc::VideoFrame&), (override));
  MOCK_METHOD(void, OnDiscardedFrame, (), (override));
};

class MockPeerConnectionDependencyFactory2
    : public MockPeerConnectionDependencyFactory {
 public:
  MOCK_METHOD(scoped_refptr<webrtc::VideoTrackSourceInterface>,
              CreateVideoTrackSourceProxy,
              (webrtc::VideoTrackSourceInterface * source),
              (override));
};

class MockVideoTrackSourceProxy : public MockWebRtcVideoTrackSource {
 public:
  MockVideoTrackSourceProxy()
      : MockWebRtcVideoTrackSource(/*supports_encoded_output=*/false) {}
  MOCK_METHOD(void,
              ProcessConstraints,
              (const webrtc::VideoTrackSourceConstraints& constraints),
              (override));
};

class MediaStreamVideoWebRtcSinkTest : public ::testing::Test {
 public:
  ~MediaStreamVideoWebRtcSinkTest() override {
    registry_.reset();
    component_ = nullptr;
    dependency_factory_ = nullptr;
    ThreadState::Current()->CollectAllGarbageForTesting();
  }

  MockMediaStreamVideoSource* SetVideoTrack() {
    registry_.Init();
    MockMediaStreamVideoSource* source =
        registry_.AddVideoTrack("test video track");
    CompleteSetVideoTrack();
    return source;
  }

  void SetVideoTrack(const std::optional<bool>& noise_reduction) {
    registry_.Init();
    registry_.AddVideoTrack("test video track",
                            blink::VideoTrackAdapterSettings(), noise_reduction,
                            false, 0.0);
    CompleteSetVideoTrack();
  }

  MockMediaStreamVideoSource* SetVideoTrackWithMaxFramerate(
      int max_frame_rate) {
    registry_.Init();
    MockMediaStreamVideoSource* source = registry_.AddVideoTrack(
        "test video track",
        blink::VideoTrackAdapterSettings(gfx::Size(100, 100), max_frame_rate),
        std::nullopt, false, 0.0);
    CompleteSetVideoTrack();
    return source;
  }

 protected:
  test::TaskEnvironment task_environment_;
  Persistent<MediaStreamComponent> component_;
  Persistent<MockPeerConnectionDependencyFactory> dependency_factory_ =
      MakeGarbageCollected<MockPeerConnectionDependencyFactory>();
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;

 private:
  void CompleteSetVideoTrack() {
    auto video_components = registry_.test_stream()->VideoComponents();
    component_ = video_components[0];
    // TODO(hta): Verify that component_ is valid. When constraints produce
    // no valid format, using the track will cause a crash.
  }

  blink::MockMediaStreamRegistry registry_;
};

TEST_F(MediaStreamVideoWebRtcSinkTest, NoiseReductionDefaultsToNotSet) {
  SetVideoTrack();
  blink::MediaStreamVideoWebRtcSink my_sink(
      component_, dependency_factory_.Get(),
      blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  EXPECT_TRUE(my_sink.webrtc_video_track());
  EXPECT_FALSE(my_sink.SourceNeedsDenoisingForTesting());
}

TEST_F(MediaStreamVideoWebRtcSinkTest, NotifiesFrameDropped) {
  MockMediaStreamVideoSource* mock_source = SetVideoTrackWithMaxFramerate(10);
  mock_source->StartMockedSource();
  blink::MediaStreamVideoWebRtcSink my_sink(
      component_, dependency_factory_.Get(),
      blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  webrtc::VideoTrackInterface* webrtc_track = my_sink.webrtc_video_track();
  MockWebRtcVideoSink mock_sink;
  webrtc_track->GetSource()->AddOrUpdateSink(&mock_sink, rtc::VideoSinkWants());

  // Drive two frames too closely spaced through. Expect one frame drop.
  base::RunLoop run_loop;
  base::OnceClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(mock_sink, OnDiscardedFrame).WillOnce([&] {
    std::move(quit_closure).Run();
  });
  scoped_refptr<media::VideoFrame> frame1 =
      media::VideoFrame::CreateBlackFrame(gfx::Size(100, 100));
  frame1->set_timestamp(base::Milliseconds(1));
  mock_source->DeliverVideoFrame(frame1);
  scoped_refptr<media::VideoFrame> frame2 =
      media::VideoFrame::CreateBlackFrame(gfx::Size(100, 100));
  frame2->set_timestamp(base::Milliseconds(2));
  mock_source->DeliverVideoFrame(frame2);
  platform_->RunUntilIdle();
  run_loop.Run();
}

TEST_F(MediaStreamVideoWebRtcSinkTest,
       ForwardsConstraintsChangeToWebRtcVideoTrackSourceProxy) {
  Persistent<MockPeerConnectionDependencyFactory2> dependency_factory2 =
      MakeGarbageCollected<MockPeerConnectionDependencyFactory2>();
  dependency_factory_ = dependency_factory2;
  MockVideoTrackSourceProxy* source_proxy = nullptr;
  EXPECT_CALL(*dependency_factory2, CreateVideoTrackSourceProxy)
      .WillOnce(Invoke([&source_proxy](webrtc::VideoTrackSourceInterface*) {
        source_proxy = new MockVideoTrackSourceProxy();
        return source_proxy;
      }));
  SetVideoTrack();
  blink::MediaStreamVideoWebRtcSink sink(
      component_, dependency_factory_.Get(),
      blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  ASSERT_TRUE(source_proxy != nullptr);
  Mock::VerifyAndClearExpectations(dependency_factory_);

  EXPECT_CALL(
      *source_proxy,
      ProcessConstraints(AllOf(
          Field(&webrtc::VideoTrackSourceConstraints::min_fps, Optional(12.0)),
          Field(&webrtc::VideoTrackSourceConstraints::max_fps,
                Optional(34.0)))));
  sink.OnVideoConstraintsChanged(12, 34);
}

TEST_F(MediaStreamVideoWebRtcSinkTest, RequestsRefreshFrameFromSource) {
  MockMediaStreamVideoSource* source = SetVideoTrack();
  MediaStreamVideoWebRtcSink sink(
      component_, dependency_factory_.Get(),
      blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  EXPECT_CALL(*source, OnRequestRefreshFrame);
  sink.webrtc_video_track()->GetSource()->RequestRefreshFrame();
  platform_->RunUntilIdle();
  Mock::VerifyAndClearExpectations(source);
}

}  // namespace blink
