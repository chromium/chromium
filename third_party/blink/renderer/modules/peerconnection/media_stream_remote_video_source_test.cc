// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/media_stream_remote_video_source.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_sink.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/web_rtc_cross_thread_copier.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/webrtc/track_observer.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/webrtc/api/rtp_packet_infos.h"
#include "third_party/webrtc/api/video/color_space.h"
#include "third_party/webrtc/api/video/i420_buffer.h"
#include "third_party/webrtc/system_wrappers/include/clock.h"
#include "ui/gfx/color_space.h"

namespace blink {

namespace {
using base::test::RunOnceClosure;
using ::testing::_;
using ::testing::Gt;
using ::testing::SaveArg;
using ::testing::Sequence;
}  // namespace

webrtc::VideoFrame::Builder CreateBlackFrameBuilder() {
  rtc::scoped_refptr<webrtc::I420Buffer> buffer =
      webrtc::I420Buffer::Create(8, 8);
  webrtc::I420Buffer::SetBlack(buffer.get());
  return webrtc::VideoFrame::Builder().set_video_frame_buffer(buffer);
}

class MediaStreamRemoteVideoSourceUnderTest
    : public blink::MediaStreamRemoteVideoSource {
 public:
  explicit MediaStreamRemoteVideoSourceUnderTest(
      std::unique_ptr<blink::TrackObserver> observer)
      : MediaStreamRemoteVideoSource(
            scheduler::GetSingleThreadTaskRunnerForTesting(),
            std::move(observer)) {}
  using MediaStreamRemoteVideoSource::EncodedSinkInterfaceForTesting;
  using MediaStreamRemoteVideoSource::SinkInterfaceForTesting;
  using MediaStreamRemoteVideoSource::StartSourceImpl;
};

class MediaStreamRemoteVideoSourceTest : public ::testing::Test {
 public:
  MediaStreamRemoteVideoSourceTest()
      : mock_factory_(
            MakeGarbageCollected<MockPeerConnectionDependencyFactory>()),
        webrtc_video_source_(blink::MockWebRtcVideoTrackSource::Create(
            /*supports_encoded_output=*/true)),
        webrtc_video_track_(
            blink::MockWebRtcVideoTrack::Create("test", webrtc_video_source_)) {
  }

  void SetUp() override {
    scoped_refptr<base::SingleThreadTaskRunner> main_thread =
        blink::scheduler::GetSingleThreadTaskRunnerForTesting();

    base::WaitableEvent waitable_event(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);

    std::unique_ptr<blink::TrackObserver> track_observer;
    mock_factory_->GetWebRtcSignalingTaskRunner()->PostTask(
        FROM_HERE,
        ConvertToBaseOnceCallback(CrossThreadBindOnce(
            [](scoped_refptr<base::SingleThreadTaskRunner> main_thread,
               webrtc::MediaStreamTrackInterface* webrtc_track,
               std::unique_ptr<blink::TrackObserver>* track_observer,
               base::WaitableEvent* waitable_event) {
              *track_observer = std::make_unique<blink::TrackObserver>(
                  main_thread, webrtc_track);
              waitable_event->Signal();
            },
            main_thread, CrossThreadUnretained(webrtc_video_track_.get()),
            CrossThreadUnretained(&track_observer),
            CrossThreadUnretained(&waitable_event))));
    waitable_event.Wait();

    auto remote_source =
        std::make_unique<MediaStreamRemoteVideoSourceUnderTest>(
            std::move(track_observer));
    remote_source_ = remote_source.get();
    source_ = MakeGarbageCollected<MediaStreamSource>(
        "dummy_source_id", MediaStreamSource::kTypeVideo, "dummy_source_name",
        true /* remote */, std::move(remote_source));
  }

  void TearDown() override {
    remote_source_->OnSourceTerminated();
    source_ = nullptr;
    blink::WebHeap::CollectAllGarbageForTesting();
  }

  MediaStreamRemoteVideoSourceUnderTest* source() { return remote_source_; }

  blink::MediaStreamVideoTrack* CreateTrack() {
    bool enabled = true;
    return new blink::MediaStreamVideoTrack(
        source(),
        ConvertToBaseOnceCallback(CrossThreadBindOnce(
            &MediaStreamRemoteVideoSourceTest::OnTrackStarted,
            CrossThreadUnretained(this))),
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
        ConvertToBaseOnceCallback(CrossThreadBindOnce(
            [](blink::MockWebRtcVideoTrack* video_track,
               base::WaitableEvent* waitable_event) {
              video_track->SetEnded();
              waitable_event->Signal();
            },
            CrossThreadUnretained(static_cast<blink::MockWebRtcVideoTrack*>(
                webrtc_video_track_.get())),
            CrossThreadUnretained(&waitable_event))));
    waitable_event.Wait();
  }

  MediaStreamSource* Source() const { return source_.Get(); }

  const base::TimeDelta& time_diff() const { return time_diff_; }

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

  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
  Persistent<blink::MockPeerConnectionDependencyFactory> mock_factory_;
  scoped_refptr<webrtc::VideoTrackSourceInterface> webrtc_video_source_;
  scoped_refptr<webrtc::VideoTrackInterface> webrtc_video_track_;
  // |remote_source_| is owned by |source_|.
  raw_ptr<MediaStreamRemoteVideoSourceUnderTest, DanglingUntriaged>
      remote_source_ = nullptr;
  Persistent<MediaStreamSource> source_;
  int number_of_successful_track_starts_ = 0;
  int number_of_failed_track_starts_ = 0;
  // WebRTC Chromium timestamp diff
  const base::TimeDelta time_diff_;
};

TEST_F(MediaStreamRemoteVideoSourceTest, StartTrack) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());
  EXPECT_EQ(1, NumberOfSuccessConstraintsCallbacks());

  blink::MockMediaStreamVideoSink sink;
  track->AddSink(&sink, sink.GetDeliverFrameCB(),
                 MediaStreamVideoSink::IsSecure::kNo,
                 MediaStreamVideoSink::UsesAlpha::kDefault);
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(sink, OnVideoFrame)
      .WillOnce(RunOnceClosure(std::move(quit_closure)));
  rtc::scoped_refptr<webrtc::I420Buffer> buffer(
      new rtc::RefCountedObject<webrtc::I420Buffer>(320, 240));

  webrtc::I420Buffer::SetBlack(buffer.get());

  source()->SinkInterfaceForTesting()->OnFrame(
      webrtc::VideoFrame::Builder()
          .set_video_frame_buffer(buffer)
          .set_timestamp_us(1000)
          .build());
  run_loop.Run();

  EXPECT_EQ(1, sink.number_of_frames());
  track->RemoveSink(&sink);
}

TEST_F(MediaStreamRemoteVideoSourceTest,
       SourceTerminationWithEncodedSinkAdded) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());
  blink::MockMediaStreamVideoSink sink;
  track->AddEncodedSink(&sink, sink.GetDeliverEncodedVideoFrameCB());
  source()->OnSourceTerminated();
  track->RemoveEncodedSink(&sink);
}

TEST_F(MediaStreamRemoteVideoSourceTest,
       SourceTerminationBeforeEncodedSinkAdded) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());
  blink::MockMediaStreamVideoSink sink;
  source()->OnSourceTerminated();
  track->AddEncodedSink(&sink, sink.GetDeliverEncodedVideoFrameCB());
  track->RemoveEncodedSink(&sink);
}

TEST_F(MediaStreamRemoteVideoSourceTest,
       SourceTerminationBeforeRequestRefreshFrame) {
  source()->OnSourceTerminated();
  source()->RequestRefreshFrame();
}

TEST_F(MediaStreamRemoteVideoSourceTest, SurvivesSourceTermination) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());

  blink::MockMediaStreamVideoSink sink;
  track->AddSink(&sink, sink.GetDeliverFrameCB(),
                 MediaStreamVideoSink::IsSecure::kNo,
                 MediaStreamVideoSink::UsesAlpha::kDefault);
  EXPECT_EQ(blink::WebMediaStreamSource::kReadyStateLive, sink.state());
  EXPECT_EQ(MediaStreamSource::kReadyStateLive, Source()->GetReadyState());
  StopWebRtcTrack();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(MediaStreamSource::kReadyStateEnded, Source()->GetReadyState());
  EXPECT_EQ(blink::WebMediaStreamSource::kReadyStateEnded, sink.state());

  track->RemoveSink(&sink);
}

TEST_F(MediaStreamRemoteVideoSourceTest, PreservesColorSpace) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());
  blink::MockMediaStreamVideoSink sink;
  track->AddSink(&sink, sink.GetDeliverFrameCB(),
                 MediaStreamVideoSink::IsSecure::kNo,
                 MediaStreamVideoSink::UsesAlpha::kDefault);

  base::RunLoop run_loop;
  EXPECT_CALL(sink, OnVideoFrame)
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
  rtc::scoped_refptr<webrtc::I420Buffer> buffer(
      new rtc::RefCountedObject<webrtc::I420Buffer>(320, 240));
  webrtc::ColorSpace kColorSpace(webrtc::ColorSpace::PrimaryID::kSMPTE240M,
                                 webrtc::ColorSpace::TransferID::kSMPTE240M,
                                 webrtc::ColorSpace::MatrixID::kSMPTE240M,
                                 webrtc::ColorSpace::RangeID::kLimited);
  const webrtc::VideoFrame& input_frame = webrtc::VideoFrame::Builder()
                                              .set_video_frame_buffer(buffer)
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

TEST_F(MediaStreamRemoteVideoSourceTest,
       UnspecifiedColorSpaceIsTreatedAsBt709) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());
  blink::MockMediaStreamVideoSink sink;
  track->AddSink(&sink, sink.GetDeliverFrameCB(),
                 MediaStreamVideoSink::IsSecure::kNo,
                 MediaStreamVideoSink::UsesAlpha::kDefault);

  base::RunLoop run_loop;
  EXPECT_CALL(sink, OnVideoFrame)
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
  rtc::scoped_refptr<webrtc::I420Buffer> buffer(
      new rtc::RefCountedObject<webrtc::I420Buffer>(320, 240));
  webrtc::ColorSpace kColorSpace(webrtc::ColorSpace::PrimaryID::kUnspecified,
                                 webrtc::ColorSpace::TransferID::kUnspecified,
                                 webrtc::ColorSpace::MatrixID::kUnspecified,
                                 webrtc::ColorSpace::RangeID::kLimited);
  const webrtc::VideoFrame& input_frame = webrtc::VideoFrame::Builder()
                                              .set_video_frame_buffer(buffer)
                                              .set_color_space(kColorSpace)
                                              .build();
  source()->SinkInterfaceForTesting()->OnFrame(input_frame);
  run_loop.Run();

  EXPECT_EQ(1, sink.number_of_frames());
  scoped_refptr<media::VideoFrame> output_frame = sink.last_frame();
  EXPECT_TRUE(output_frame);
  EXPECT_TRUE(output_frame->ColorSpace() ==
              gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT709,
                              gfx::ColorSpace::TransferID::BT709,
                              gfx::ColorSpace::MatrixID::BT709,
                              gfx::ColorSpace::RangeID::LIMITED));
  track->RemoveSink(&sink);
}

TEST_F(MediaStreamRemoteVideoSourceTest, UnspecifiedColorSpaceIsIgnored) {
  base::test::ScopedFeatureList scoped_feauture_list;
  scoped_feauture_list.InitAndEnableFeature(
      blink::features::kWebRtcIgnoreUnspecifiedColorSpace);
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());
  blink::MockMediaStreamVideoSink sink;
  track->AddSink(&sink, sink.GetDeliverFrameCB(),
                 MediaStreamVideoSink::IsSecure::kNo,
                 MediaStreamVideoSink::UsesAlpha::kDefault);

  base::RunLoop run_loop;
  EXPECT_CALL(sink, OnVideoFrame)
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
  rtc::scoped_refptr<webrtc::I420Buffer> buffer(
      new rtc::RefCountedObject<webrtc::I420Buffer>(320, 240));
  webrtc::ColorSpace kColorSpace(webrtc::ColorSpace::PrimaryID::kUnspecified,
                                 webrtc::ColorSpace::TransferID::kUnspecified,
                                 webrtc::ColorSpace::MatrixID::kUnspecified,
                                 webrtc::ColorSpace::RangeID::kLimited);
  const webrtc::VideoFrame& input_frame = webrtc::VideoFrame::Builder()
                                              .set_video_frame_buffer(buffer)
                                              .set_color_space(kColorSpace)
                                              .build();
  source()->SinkInterfaceForTesting()->OnFrame(input_frame);
  run_loop.Run();

  EXPECT_EQ(1, sink.number_of_frames());
  scoped_refptr<media::VideoFrame> output_frame = sink.last_frame();
  EXPECT_TRUE(output_frame);
  EXPECT_TRUE(output_frame->ColorSpace() ==
              gfx::ColorSpace(gfx::ColorSpace::PrimaryID::INVALID,
                              gfx::ColorSpace::TransferID::INVALID,
                              gfx::ColorSpace::MatrixID::INVALID,
                              gfx::ColorSpace::RangeID::INVALID));
  track->RemoveSink(&sink);
}

TEST_F(MediaStreamRemoteVideoSourceTest,
       PopulateRequestAnimationFrameMetadata) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());
  blink::MockMediaStreamVideoSink sink;
  track->AddSink(&sink, sink.GetDeliverFrameCB(),
                 MediaStreamVideoSink::IsSecure::kNo,
                 MediaStreamVideoSink::UsesAlpha::kDefault);

  base::RunLoop run_loop;
  EXPECT_CALL(sink, OnVideoFrame)
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
  rtc::scoped_refptr<webrtc::I420Buffer> buffer(
      new rtc::RefCountedObject<webrtc::I420Buffer>(320, 240));

  uint32_t kSsrc = 0;
  const std::vector<uint32_t> kCsrcs;
  uint32_t kRtpTimestamp = 123456;
  float kProcessingTime = 0.014;

  const webrtc::Timestamp kProcessingFinish =
      webrtc::Timestamp::Millis(rtc::TimeMillis());
  const webrtc::Timestamp kProcessingStart =
      kProcessingFinish - webrtc::TimeDelta::Millis(1.0e3 * kProcessingTime);
  const webrtc::Timestamp kCaptureTime =
      kProcessingStart - webrtc::TimeDelta::Millis(20.0);
  webrtc::Clock* clock = webrtc::Clock::GetRealTimeClock();
  const int64_t ntp_offset =
      clock->CurrentNtpInMilliseconds() - clock->TimeInMilliseconds();
  const webrtc::Timestamp kCaptureTimeNtp =
      kCaptureTime + webrtc::TimeDelta::Millis(ntp_offset);
  // Expected capture time.
  base::TimeTicks kExpectedCaptureTime =
      base::TimeTicks() + base::Milliseconds(kCaptureTime.ms());

  webrtc::RtpPacketInfos::vector_type packet_infos;
  for (int i = 0; i < 4; ++i) {
    webrtc::Timestamp receive_time =
        kProcessingStart - webrtc::TimeDelta::Micros(10000 - i * 30);
    packet_infos.emplace_back(kSsrc, kCsrcs, kRtpTimestamp, receive_time);
  }
  // Expected receive time should be the same as the last arrival time.
  base::TimeTicks kExpectedReceiveTime =
      base::TimeTicks() +
      base::Microseconds(kProcessingStart.us() - (10000 - 3 * 30));

  webrtc::VideoFrame input_frame =
      webrtc::VideoFrame::Builder()
          .set_video_frame_buffer(buffer)
          .set_rtp_timestamp(kRtpTimestamp)
          .set_ntp_time_ms(kCaptureTimeNtp.ms())
          .set_packet_infos(webrtc::RtpPacketInfos(packet_infos))
          .build();

  input_frame.set_processing_time({kProcessingStart, kProcessingFinish});
  source()->SinkInterfaceForTesting()->OnFrame(input_frame);
  run_loop.Run();

  EXPECT_EQ(1, sink.number_of_frames());
  scoped_refptr<media::VideoFrame> output_frame = sink.last_frame();
  EXPECT_TRUE(output_frame);

  EXPECT_FLOAT_EQ(output_frame->metadata().processing_time->InSecondsF(),
                  kProcessingTime);

  // The NTP offset is estimated both here and in the code that is tested.
  // Therefore, we cannot exactly determine what capture_begin_time will be set
  // to.
  // TODO(kron): Find a lower tolerance without causing the test to be flaky or
  // make the clock injectable so that a fake clock can be used in the test.
  constexpr float kNtpOffsetToleranceMs = 40.0;
  EXPECT_NEAR(
      (*output_frame->metadata().capture_begin_time - kExpectedCaptureTime)
          .InMillisecondsF(),
      0.0f, kNtpOffsetToleranceMs);

  EXPECT_FLOAT_EQ(
      (*output_frame->metadata().receive_time - kExpectedReceiveTime)
          .InMillisecondsF(),
      0.0f);

  EXPECT_EQ(static_cast<uint32_t>(*output_frame->metadata().rtp_timestamp),
            kRtpTimestamp);

  track->RemoveSink(&sink);
}

TEST_F(MediaStreamRemoteVideoSourceTest, ReferenceTimeEqualsTimestampUs) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());
  blink::MockMediaStreamVideoSink sink;
  track->AddSink(&sink, sink.GetDeliverFrameCB(),
                 MediaStreamVideoSink::IsSecure::kNo,
                 MediaStreamVideoSink::UsesAlpha::kDefault);

  base::RunLoop run_loop;
  EXPECT_CALL(sink, OnVideoFrame)
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
  rtc::scoped_refptr<webrtc::I420Buffer> buffer(
      new rtc::RefCountedObject<webrtc::I420Buffer>(320, 240));

  int64_t kTimestampUs = rtc::TimeMicros();
  webrtc::VideoFrame input_frame = webrtc::VideoFrame::Builder()
                                       .set_video_frame_buffer(buffer)
                                       .set_timestamp_us(kTimestampUs)
                                       .build();

  source()->SinkInterfaceForTesting()->OnFrame(input_frame);
  run_loop.Run();

  EXPECT_EQ(1, sink.number_of_frames());
  scoped_refptr<media::VideoFrame> output_frame = sink.last_frame();
  EXPECT_TRUE(output_frame);

  EXPECT_FLOAT_EQ((*output_frame->metadata().reference_time -
                   (base::TimeTicks() + base::Microseconds(kTimestampUs)))
                      .InMillisecondsF(),
                  0.0f);
  track->RemoveSink(&sink);
}

TEST_F(MediaStreamRemoteVideoSourceTest, BaseTimeTicksAndRtcMicrosAreTheSame) {
  base::TimeTicks first_chromium_timestamp = base::TimeTicks::Now();
  base::TimeTicks webrtc_timestamp =
      base::TimeTicks() + base::Microseconds(rtc::TimeMicros());
  base::TimeTicks second_chromium_timestamp = base::TimeTicks::Now();

  // Test that the timestamps are correctly ordered, which they can only be if
  // the clocks are the same (assuming at least one of the clocks is functioning
  // correctly).
  EXPECT_GE((webrtc_timestamp - first_chromium_timestamp).InMillisecondsF(),
            0.0f);
  EXPECT_GE((second_chromium_timestamp - webrtc_timestamp).InMillisecondsF(),
            0.0f);
}

// This is a special case that is used to signal "render immediately".
TEST_F(MediaStreamRemoteVideoSourceTest, NoTimestampUsMeansNoReferenceTime) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());
  blink::MockMediaStreamVideoSink sink;
  track->AddSink(&sink, sink.GetDeliverFrameCB(),
                 MediaStreamVideoSink::IsSecure::kNo,
                 MediaStreamVideoSink::UsesAlpha::kDefault);

  base::RunLoop run_loop;
  EXPECT_CALL(sink, OnVideoFrame)
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
  rtc::scoped_refptr<webrtc::I420Buffer> buffer(
      new rtc::RefCountedObject<webrtc::I420Buffer>(320, 240));

  webrtc::VideoFrame input_frame =
      webrtc::VideoFrame::Builder().set_video_frame_buffer(buffer).build();
  input_frame.set_render_parameters({.use_low_latency_rendering = true});

  source()->SinkInterfaceForTesting()->OnFrame(input_frame);
  run_loop.Run();

  EXPECT_EQ(1, sink.number_of_frames());
  scoped_refptr<media::VideoFrame> output_frame = sink.last_frame();
  EXPECT_TRUE(output_frame);

  EXPECT_FALSE(output_frame->metadata().reference_time.has_value());

  track->RemoveSink(&sink);
}

class TestEncodedVideoFrame : public webrtc::RecordableEncodedFrame {
 public:
  explicit TestEncodedVideoFrame(webrtc::Timestamp timestamp)
      : timestamp_(timestamp) {}

  rtc::scoped_refptr<const webrtc::EncodedImageBufferInterface> encoded_buffer()
      const override {
    return nullptr;
  }
  std::optional<webrtc::ColorSpace> color_space() const override {
    return std::nullopt;
  }
  webrtc::VideoCodecType codec() const override {
    return webrtc::kVideoCodecVP8;
  }
  bool is_key_frame() const override { return true; }
  EncodedResolution resolution() const override {
    return EncodedResolution{0, 0};
  }
  webrtc::Timestamp render_time() const override { return timestamp_; }

 private:
  webrtc::Timestamp timestamp_;
};

TEST_F(MediaStreamRemoteVideoSourceTest, ForwardsEncodedVideoFrames) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());
  blink::MockMediaStreamVideoSink sink;
  track->AddEncodedSink(&sink, sink.GetDeliverEncodedVideoFrameCB());
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(sink, OnEncodedVideoFrame)
      .WillOnce(RunOnceClosure(std::move(quit_closure)));
  source()->EncodedSinkInterfaceForTesting()->OnFrame(
      TestEncodedVideoFrame(webrtc::Timestamp::Millis(0)));
  run_loop.Run();
  track->RemoveEncodedSink(&sink);
}

TEST_F(MediaStreamRemoteVideoSourceTest,
       ForwardsFramesWithIncreasingTimestampsWithNullSourceTimestamp) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());
  blink::MockMediaStreamVideoSink sink;
  track->AddSink(&sink, sink.GetDeliverFrameCB(),
                 MediaStreamVideoSink::IsSecure::kNo,
                 MediaStreamVideoSink::UsesAlpha::kDefault);
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();

  base::TimeTicks frame_timestamp1;
  Sequence s;
  EXPECT_CALL(sink, OnVideoFrame)
      .InSequence(s)
      .WillOnce(SaveArg<0>(&frame_timestamp1));
  EXPECT_CALL(sink, OnVideoFrame(Gt(frame_timestamp1)))
      .InSequence(s)
      .WillOnce(RunOnceClosure(std::move(quit_closure)));
  source()->SinkInterfaceForTesting()->OnFrame(
      CreateBlackFrameBuilder().set_timestamp_ms(0).build());
  // Spin until the time counter changes.
  base::TimeTicks now = base::TimeTicks::Now();
  while (base::TimeTicks::Now() == now) {
  }
  source()->SinkInterfaceForTesting()->OnFrame(
      CreateBlackFrameBuilder().set_timestamp_ms(0).build());
  run_loop.Run();
  track->RemoveSink(&sink);
}

TEST_F(MediaStreamRemoteVideoSourceTest,
       ForwardsFramesWithIncreasingTimestampsWithSourceTimestamp) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());
  blink::MockMediaStreamVideoSink sink;
  track->AddSink(&sink, sink.GetDeliverFrameCB(),
                 MediaStreamVideoSink::IsSecure::kNo,
                 MediaStreamVideoSink::UsesAlpha::kDefault);
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();

  base::TimeTicks frame_timestamp1;
  Sequence s;
  EXPECT_CALL(sink, OnVideoFrame)
      .InSequence(s)
      .WillOnce(SaveArg<0>(&frame_timestamp1));
  EXPECT_CALL(sink, OnVideoFrame(Gt(frame_timestamp1)))
      .InSequence(s)
      .WillOnce(RunOnceClosure(std::move(quit_closure)));
  source()->SinkInterfaceForTesting()->OnFrame(
      CreateBlackFrameBuilder().set_timestamp_ms(4711).build());
  source()->SinkInterfaceForTesting()->OnFrame(
      CreateBlackFrameBuilder().set_timestamp_ms(4712).build());
  run_loop.Run();
  track->RemoveSink(&sink);
}

TEST_F(MediaStreamRemoteVideoSourceTest,
       ForwardsEncodedFramesWithIncreasingTimestampsWithNullSourceTimestamp) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());
  blink::MockMediaStreamVideoSink sink;
  track->AddEncodedSink(&sink, sink.GetDeliverEncodedVideoFrameCB());
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();

  base::TimeTicks frame_timestamp1;
  Sequence s;
  EXPECT_CALL(sink, OnEncodedVideoFrame)
      .InSequence(s)
      .WillOnce(SaveArg<0>(&frame_timestamp1));
  EXPECT_CALL(sink, OnEncodedVideoFrame(Gt(frame_timestamp1)))
      .InSequence(s)
      .WillOnce(RunOnceClosure(std::move(quit_closure)));
  source()->EncodedSinkInterfaceForTesting()->OnFrame(
      TestEncodedVideoFrame(webrtc::Timestamp::Millis(0)));
  // Spin until the time counter changes.
  base::TimeTicks now = base::TimeTicks::Now();
  while (base::TimeTicks::Now() == now) {
  }
  source()->EncodedSinkInterfaceForTesting()->OnFrame(
      TestEncodedVideoFrame(webrtc::Timestamp::Millis(0)));
  run_loop.Run();
  track->RemoveEncodedSink(&sink);
}

TEST_F(MediaStreamRemoteVideoSourceTest,
       ForwardsEncodedFramesWithIncreasingTimestampsWithSourceTimestamp) {
  std::unique_ptr<blink::MediaStreamVideoTrack> track(CreateTrack());
  blink::MockMediaStreamVideoSink sink;
  track->AddEncodedSink(&sink, sink.GetDeliverEncodedVideoFrameCB());
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();

  base::TimeTicks frame_timestamp1;
  Sequence s;
  EXPECT_CALL(sink, OnEncodedVideoFrame)
      .InSequence(s)
      .WillOnce(SaveArg<0>(&frame_timestamp1));
  EXPECT_CALL(sink, OnEncodedVideoFrame(Gt(frame_timestamp1)))
      .InSequence(s)
      .WillOnce(RunOnceClosure(std::move(quit_closure)));
  source()->EncodedSinkInterfaceForTesting()->OnFrame(
      TestEncodedVideoFrame(webrtc::Timestamp::Millis(42)));
  source()->EncodedSinkInterfaceForTesting()->OnFrame(
      TestEncodedVideoFrame(webrtc::Timestamp::Millis(43)));
  run_loop.Run();
  track->RemoveEncodedSink(&sink);
}

}  // namespace blink
