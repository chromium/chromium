// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/media_stream_remote_video_source.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/public/platform/modules/webrtc/track_observer.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_sink.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/webrtc/api/video/color_space.h"
#include "third_party/webrtc/api/video/i420_buffer.h"
#include "ui/gfx/color_space.h"

namespace blink {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

class MediaStreamRemoteVideoSourceUnderTest
    : public blink::MediaStreamRemoteVideoSource {
 public:
  explicit MediaStreamRemoteVideoSourceUnderTest(
      std::unique_ptr<blink::TrackObserver> observer)
      : MediaStreamRemoteVideoSource(std::move(observer)) {}
  using MediaStreamRemoteVideoSource::SinkInterfaceForTesting;
  using MediaStreamRemoteVideoSource::StartSourceImpl;
};

class MediaStreamRemoteVideoSourceTest : public ::testing::Test {
 public:
  MediaStreamRemoteVideoSourceTest()
      : mock_factory_(new blink::MockPeerConnectionDependencyFactory()),
        webrtc_video_track_(blink::MockWebRtcVideoTrack::Create("test")),
        remote_source_(nullptr),
        number_of_successful_track_starts_(0),
        number_of_failed_track_starts_(0) {}

  void SetUp() override {
    scoped_refptr<base::SingleThreadTaskRunner> main_thread =
        blink::scheduler::GetSingleThreadTaskRunnerForTesting();

    base::WaitableEvent waitable_event(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);

    std::unique_ptr<blink::TrackObserver> track_observer;
    mock_factory_->GetWebRtcSignalingTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](scoped_refptr<base::SingleThreadTaskRunner> main_thread,
               webrtc::MediaStreamTrackInterface* webrtc_track,
               std::unique_ptr<blink::TrackObserver>* track_observer,
               base::WaitableEvent* waitable_event) {
              track_observer->reset(
                  new blink::TrackObserver(main_thread, webrtc_track));
              waitable_event->Signal();
            },
            main_thread, base::Unretained(webrtc_video_track_.get()),
            base::Unretained(&track_observer),
            base::Unretained(&waitable_event)));
    waitable_event.Wait();

    remote_source_ =
        new MediaStreamRemoteVideoSourceUnderTest(std::move(track_observer));
    web_source_.Initialize(blink::WebString::FromASCII("dummy_source_id"),
                           blink::WebMediaStreamSource::kTypeVideo,
                           blink::WebString::FromASCII("dummy_source_name"),
                           true /* remote */);
    web_source_.SetPlatformSource(base::WrapUnique(remote_source_));
  }

  void TearDown() override {
    remote_source_->OnSourceTerminated();
    web_source_.Reset();
    blink::WebHeap::CollectAllGarbageForTesting();
  }

  MediaStreamRemoteVideoSourceUnderTest* source() { return remote_source_; }

  blink::MediaStreamVideoTrack* CreateTrack() {
    bool enabled = true;
    return new blink::MediaStreamVideoTrack(
        source(),
        base::Bind(&MediaStreamRemoteVideoSourceTest::OnTrackStarted,
                   base::Unretained(this)),
        enabled);
  }

  int NumberOfSuccessConstraintsCallbacks() const {
    return number_of_successful_track_starts_;
  }

  int NumberOfFailedConstraintsCallbacks() const {
    return number_of_failed_track_starts_;
  }

  void StopWebRtcTrack() {
    base::WaitableEvent waitable_event(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    mock_factory_->GetWebRtcSignalingTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](blink::MockWebRtcVideoTrack* video_track,
               base::WaitableEvent* waitable_event) {
              video_track->SetEnded();
              waitable_event->Signal();
            },
            base::Unretained(static_cast<blink::MockWebRtcVideoTrack*>(
                webrtc_video_track_.get())),
            base::Unretained(&waitable_event)));
    waitable_event.Wait();
  }

  const blink::WebMediaStreamSource& webkit_source() const {
    return web_source_;
  }

 private:
  void OnTrackStarted(blink::WebPlatformMediaStreamSource* source,
                      blink::mojom::MediaStreamRequestResult result,
                      const blink::WebString& result_name) {
    ASSERT_EQ(source, remote_source_);
    if (result == blink::mojom::MediaStreamRequestResult::OK)
      ++number_of_successful_track_starts_;
    else
      ++number_of_failed_track_starts_;
  }

  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
  std::unique_ptr<blink::MockPeerConnectionDependencyFactory> mock_factory_;
  scoped_refptr<webrtc::VideoTrackInterface> webrtc_video_track_;
  // |remote_source_| is owned by |web_source_|.
  MediaStreamRemoteVideoSourceUnderTest* remote_source_;
  blink::WebMediaStreamSource web_source_;
  int number_of_successful_track_starts_;
  int number_of_failed_track_starts_;
};

TEST_F(MediaStreamRemoteVideoSourceTest, StartTrack) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());
  EXPECT_EQ(1, NumberOfSuccessConstraintsCallbacks());

  blink::MockMediaStreamVideoSink sink;
  track->AddSink(&sink, sink.GetDeliverFrameCB(), false);
  base::RunLoop run_loop;
  base::Closure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(sink, OnVideoFrame())
      .WillOnce(RunClosure(std::move(quit_closure)));
  rtc::scoped_refptr<webrtc::I420Buffer> buffer(
      new rtc::RefCountedObject<webrtc::I420Buffer>(320, 240));

  webrtc::I420Buffer::SetBlack(buffer);

  source()->SinkInterfaceForTesting()->OnFrame(
      webrtc::VideoFrame::Builder()
          .set_video_frame_buffer(buffer)
          .set_rotation(webrtc::kVideoRotation_0)
          .set_timestamp_us(1000)
          .build());
  run_loop.Run();

  EXPECT_EQ(1, sink.number_of_frames());
  track->RemoveSink(&sink);
}

TEST_F(MediaStreamRemoteVideoSourceTest, RemoteTrackStop) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());

  blink::MockMediaStreamVideoSink sink;
  track->AddSink(&sink, sink.GetDeliverFrameCB(), false);
  EXPECT_EQ(blink::WebMediaStreamSource::kReadyStateLive, sink.state());
  EXPECT_EQ(blink::WebMediaStreamSource::kReadyStateLive,
            webkit_source().GetReadyState());
  StopWebRtcTrack();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(blink::WebMediaStreamSource::kReadyStateEnded,
            webkit_source().GetReadyState());
  EXPECT_EQ(blink::WebMediaStreamSource::kReadyStateEnded, sink.state());

  track->RemoveSink(&sink);
}

TEST_F(MediaStreamRemoteVideoSourceTest, PreservesColorSpace) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());
  blink::MockMediaStreamVideoSink sink;
  track->AddSink(&sink, sink.GetDeliverFrameCB(), false);

  base::RunLoop run_loop;
  EXPECT_CALL(sink, OnVideoFrame())
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  rtc::scoped_refptr<webrtc::I420Buffer> buffer(
      new rtc::RefCountedObject<webrtc::I420Buffer>(320, 240));
  webrtc::ColorSpace kColorSpace(webrtc::ColorSpace::PrimaryID::kSMPTE240M,
                                 webrtc::ColorSpace::TransferID::kSMPTE240M,
                                 webrtc::ColorSpace::MatrixID::kSMPTE240M,
                                 webrtc::ColorSpace::RangeID::kLimited);
  const webrtc::VideoFrame& input_frame =
      webrtc::VideoFrame::Builder()
          .set_video_frame_buffer(buffer)
          .set_timestamp_ms(0)
          .set_rotation(webrtc::kVideoRotation_0)
          .set_color_space(kColorSpace)
          .build();
  source()->SinkInterfaceForTesting()->OnFrame(input_frame);
  run_loop.Run();

  EXPECT_EQ(1, sink.number_of_frames());
  scoped_refptr<media::VideoFrame> output_frame = sink.last_frame();
  EXPECT_TRUE(output_frame);
  EXPECT_TRUE(output_frame->ColorSpace() ==
              gfx::ColorSpace(gfx::ColorSpace::PrimaryID::SMPTE240M,
                              gfx::ColorSpace::TransferID::SMPTE240M,
                              gfx::ColorSpace::MatrixID::SMPTE240M,
                              gfx::ColorSpace::RangeID::LIMITED));
  track->RemoveSink(&sink);
}

}  // namespace blink
