// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_video_capturer_source.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/bind_post_task.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "media/base/video_frame.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_sink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_sink.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_mojo_media_stream_dispatcher_host.h"
#include "third_party/blink/renderer/modules/mediastream/mock_video_capturer_source.h"
#include "third_party/blink/renderer/modules/mediastream/video_track_adapter_settings.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/video_capture/video_capturer_source.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_media.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;

namespace blink {

using mojom::blink::MediaStreamRequestResult;

namespace {

MATCHER_P2(IsExpectedDOMException, name, message, "") {
  return arg->name() == name && arg->message() == message;
}

class FakeMediaStreamVideoSink : public MediaStreamVideoSink {
 public:
  FakeMediaStreamVideoSink(base::TimeTicks* capture_time,
                           media::VideoFrameMetadata* metadata,
                           base::OnceClosure got_frame_cb)
      : capture_time_(capture_time),
        metadata_(metadata),
        got_frame_cb_(std::move(got_frame_cb)) {}

  void ConnectToTrack(const WebMediaStreamTrack& track) {
    MediaStreamVideoSink::ConnectToTrack(
        track,
        ConvertToBaseRepeatingCallback(
            CrossThreadBindRepeating(&FakeMediaStreamVideoSink::OnVideoFrame,
                                     CrossThreadUnretained(this))),
        WebMediaStreamSink::IsSecure::kYes,
        WebMediaStreamSink::UsesAlpha::kDefault);
  }

  void DisconnectFromTrack() { MediaStreamVideoSink::DisconnectFromTrack(); }

  void OnVideoFrame(scoped_refptr<media::VideoFrame> frame,
                    base::TimeTicks capture_time) {
    *capture_time_ = capture_time;
    *metadata_ = frame->metadata();
    std::move(got_frame_cb_).Run();
  }

 private:
  const raw_ptr<base::TimeTicks> capture_time_;
  const raw_ptr<media::VideoFrameMetadata> metadata_;
  base::OnceClosure got_frame_cb_;
};

}  // namespace

class MediaStreamVideoCapturerSourceTest : public testing::Test {
 public:
  MediaStreamVideoCapturerSourceTest() : source_stopped_(false) {
    auto delegate = std::make_unique<MockVideoCapturerSource>();
    delegate_ = delegate.get();
    EXPECT_CALL(*delegate_, GetPreferredFormats());
    auto video_capturer_source =
        std::make_unique<MediaStreamVideoCapturerSource>(
            scheduler::GetSingleThreadTaskRunnerForTesting(),
            /*LocalFrame =*/nullptr,
            BindOnce(&MediaStreamVideoCapturerSourceTest::OnSourceStopped,
                     Unretained(this)),
            std::move(delegate));
    video_capturer_source_ = video_capturer_source.get();
    video_capturer_source_->SetMediaStreamDispatcherHostForTesting(
        mock_dispatcher_host_.CreatePendingRemoteAndBind());
    stream_source_ = MakeGarbageCollected<MediaStreamSource>(
        "dummy_source_id", MediaStreamSource::kTypeVideo, "dummy_source_name",
        false /* remote */, std::move(video_capturer_source));
    stream_source_id_ = stream_source_->Id();

    MediaStreamVideoCapturerSource::DeviceCapturerFactoryCallback callback =
        blink::BindRepeating(
            &MediaStreamVideoCapturerSourceTest::RecreateVideoCapturerSource,
            Unretained(this));
    video_capturer_source_->SetDeviceCapturerFactoryCallbackForTesting(
        std::move(callback));
  }

  void TearDown() override {
    stream_source_ = nullptr;
    WebHeap::CollectAllGarbageForTesting();
  }

  WebMediaStreamTrack StartSource(
      const VideoTrackAdapterSettings& adapter_settings,
      const std::optional<bool>& noise_reduction,
      bool is_screencast,
      double min_frame_rate) {
    bool enabled = true;
    // CreateVideoTrack will trigger StartDone.
    return MediaStreamVideoTrack::CreateVideoTrack(
        video_capturer_source_, adapter_settings, noise_reduction,
        is_screencast, min_frame_rate, nullptr, false,
        BindOnce(&MediaStreamVideoCapturerSourceTest::StartDone,
                 base::Unretained(this)),
        enabled);
  }

  MockVideoCapturerSource& mock_delegate() { return *delegate_; }

  void OnSourceStopped(const WebMediaStreamSource& source) {
    source_stopped_ = true;
    if (source.IsNull())
      return;
    EXPECT_EQ(String(source.Id()), stream_source_id_);
  }
  void OnStarted(bool result) {
    VideoCaptureRunState run_state = result ? VideoCaptureRunState::kRunning
                                            : VideoCaptureRunState::kStopped;
    video_capturer_source_->OnRunStateChanged(delegate_->capture_params(),
                                              run_state);
  }

  void SetStopCaptureFlag() { stop_capture_flag_ = true; }

  MOCK_METHOD0(MockNotification, void());

  std::unique_ptr<VideoCapturerSource> RecreateVideoCapturerSource(
      const base::UnguessableToken& session_id) {
    auto delegate = std::make_unique<MockVideoCapturerSource>();
    delegate_ = delegate.get();
    EXPECT_CALL(*delegate_, MockStartCapture(_, _, _))
        .WillOnce(Return(VideoCaptureRunState::kRunning));
    return delegate;
  }

 protected:
  WebMediaStreamTrack StartTabCaptureSource() {
    MediaStreamDevice device(mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE,
                             "dummy_device_id", "dummy_device_name");
    device.set_session_id(base::UnguessableToken::Create());
    video_capturer_source_->SetDevice(device);
    EXPECT_CALL(mock_delegate(), MockStartCapture(_, _, _))
        .WillOnce(Return(VideoCaptureRunState::kRunning));
    return StartSource(VideoTrackAdapterSettings(), std::nullopt, false, 0.0);
  }

  WebMediaStreamTrack CloneTrack(const WebMediaStreamTrack& original_track,
                                 const String& clone_id = "cloned_track") {
    auto* original_native_track = MediaStreamVideoTrack::From(original_track);
    std::unique_ptr<MediaStreamTrackPlatform> cloned_platform =
        original_native_track->CreateFromComponent(original_track, clone_id);
    return WebMediaStreamTrack(MakeGarbageCollected<MediaStreamComponentImpl>(
        clone_id, original_track.Source(), std::move(cloned_platform)));
  }

  std::optional<media::CaptureVersion> GetNextCaptureVersion(
      media::mojom::SubCaptureTargetType type) {
    return video_capturer_source_->GetNextCaptureVersion(type);
  }

  media::mojom::ApplySubCaptureTargetResult ApplySubCaptureTarget(
      MediaStreamVideoTrack* track,
      media::mojom::SubCaptureTargetType type,
      const base::Token& track_target,
      uint32_t version) {
    base::test::TestFuture<media::mojom::ApplySubCaptureTargetResult> result;
    video_capturer_source_->ApplySubCaptureTarget(
        track, type, track_target, version, result.GetCallback());
    return result.Get();
  }

  void ApplySubCaptureTargetAndExpectSuccess(
      MediaStreamVideoTrack* track,
      media::mojom::SubCaptureTargetType type,
      const base::Token& track_target,
      const base::Token& expected_gpu_target,
      uint32_t expected_version) {
    EXPECT_CALL(mock_dispatcher_host_,
                ApplySubCaptureTarget(_, type, expected_gpu_target,
                                      expected_version, _))
        .WillOnce([](const base::UnguessableToken&,
                     media::mojom::SubCaptureTargetType, const base::Token&,
                     uint32_t,
                     mojom::blink::MediaStreamDispatcherHost::
                         ApplySubCaptureTargetCallback callback) {
          std::move(callback).Run(
              media::mojom::ApplySubCaptureTargetResult::kSuccess);
        });

    auto next_version = GetNextCaptureVersion(type);
    ASSERT_TRUE(next_version.has_value());
    ASSERT_EQ(next_version->sub_capture, expected_version);

    ASSERT_EQ(ApplySubCaptureTarget(track, type, track_target,
                                    next_version->sub_capture),
              media::mojom::ApplySubCaptureTargetResult::kSuccess);
  }

  base::Token GetTrackTarget(const MediaStreamVideoTrack* track) const {
    auto it = video_capturer_source_->track_targets_.find(track);
    return it != video_capturer_source_->track_targets_.end() ? it->value.target
                                                              : base::Token();
  }

  void StartDone(WebPlatformMediaStreamSource* source,
                 MediaStreamRequestResult result,
                 const WebString& result_name) {
    start_result_ = result;
  }

  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;

  Persistent<MediaStreamSource> stream_source_;
  MockMojoMediaStreamDispatcherHost mock_dispatcher_host_;
  raw_ptr<MediaStreamVideoCapturerSource, DanglingUntriaged>
      video_capturer_source_;  // owned by |stream_source_|.
  raw_ptr<MockVideoCapturerSource, DanglingUntriaged>
      delegate_;  // owned by |source_|.
  String stream_source_id_;
  bool source_stopped_;
  bool stop_capture_flag_ = false;
  MediaStreamRequestResult start_result_;
};

TEST_F(MediaStreamVideoCapturerSourceTest, StartAndStop) {
  InSequence s;
  EXPECT_CALL(mock_delegate(), MockStartCapture(_, _, _));
  WebMediaStreamTrack track =
      StartSource(VideoTrackAdapterSettings(), std::nullopt, false, 0.0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(MediaStreamSource::kReadyStateLive,
            stream_source_->GetReadyState());
  EXPECT_FALSE(source_stopped_);

  // A bogus notification of running from the delegate when the source has
  // already started should not change the state.
  delegate_->SetRunning(VideoCaptureRunState::kRunning);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(MediaStreamSource::kReadyStateLive,
            stream_source_->GetReadyState());
  EXPECT_FALSE(source_stopped_);
  EXPECT_TRUE(video_capturer_source_->GetCurrentFormat().has_value());

  // If the delegate stops, the source should stop.
  EXPECT_CALL(mock_delegate(), MockStopCapture());
  delegate_->SetRunning(VideoCaptureRunState::kStopped);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(MediaStreamSource::kReadyStateEnded,
            stream_source_->GetReadyState());
  // Verify that WebPlatformMediaStreamSource::SourceStoppedCallback has
  // been triggered.
  EXPECT_TRUE(source_stopped_);
}

TEST_F(MediaStreamVideoCapturerSourceTest, CaptureTimeAndMetadataPlumbing) {
  VideoCaptureDeliverFrameCB deliver_frame_cb;
  VideoCapturerSource::VideoCaptureRunningCallbackCB running_cb;

  InSequence s;
  EXPECT_CALL(mock_delegate(), MockStartCapture(_, _, _))
      .WillOnce(testing::DoAll(testing::SaveArg<1>(&deliver_frame_cb),
                               testing::SaveArg<2>(&running_cb),
                               Return(VideoCaptureRunState::kRunning)));
  EXPECT_CALL(mock_delegate(), RequestRefreshFrame());
  EXPECT_CALL(mock_delegate(), MockStopCapture());
  WebMediaStreamTrack track =
      StartSource(VideoTrackAdapterSettings(), std::nullopt, false, 0.0);
  running_cb.Run(VideoCaptureRunState::kRunning);

  base::RunLoop run_loop;
  base::TimeTicks reference_capture_time =
      base::TimeTicks::FromInternalValue(60013);
  base::TimeTicks capture_time;
  media::VideoFrameMetadata metadata;
  FakeMediaStreamVideoSink fake_sink(
      &capture_time, &metadata,
      base::BindPostTaskToCurrentDefault(run_loop.QuitClosure()));
  fake_sink.ConnectToTrack(track);
  const scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(2, 2));
  frame->metadata().frame_rate = 30.0;
  PostCrossThreadTask(
      *Platform::Current()->GetIOTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(deliver_frame_cb, frame, reference_capture_time));
  run_loop.Run();
  fake_sink.DisconnectFromTrack();
  EXPECT_EQ(reference_capture_time, capture_time);
  EXPECT_EQ(30.0, *metadata.frame_rate);
}

TEST_F(MediaStreamVideoCapturerSourceTest, Restart) {
  InSequence s;
  EXPECT_CALL(mock_delegate(), MockStartCapture(_, _, _))
      .WillOnce(Return(VideoCaptureRunState::kRunning));
  WebMediaStreamTrack track =
      StartSource(VideoTrackAdapterSettings(), std::nullopt, false, 0.0);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(MediaStreamSource::kReadyStateLive,
            stream_source_->GetReadyState());
  EXPECT_FALSE(source_stopped_);

  EXPECT_CALL(mock_delegate(), MockStopCapture());
  EXPECT_TRUE(video_capturer_source_->IsRunning());
  video_capturer_source_->StopForRestart(
      BindOnce([](MediaStreamVideoSource::RestartResult result) {
        EXPECT_EQ(result, MediaStreamVideoSource::RestartResult::IS_STOPPED);
      }));
  base::RunLoop().RunUntilIdle();
  // When the source has stopped for restart, the source is not considered
  // stopped, even if the underlying delegate is not running anymore.
  // WebPlatformMediaStreamSource::SourceStoppedCallback should not be
  // triggered.
  EXPECT_EQ(stream_source_->GetReadyState(),
            MediaStreamSource::kReadyStateLive);
  EXPECT_FALSE(source_stopped_);
  EXPECT_FALSE(video_capturer_source_->IsRunning());

  // A second StopForRestart() should fail with invalid state, since it only
  // makes sense when the source is running. Existing ready state should remain
  // the same.
  EXPECT_FALSE(video_capturer_source_->IsRunning());
  video_capturer_source_->StopForRestart(
      BindOnce([](MediaStreamVideoSource::RestartResult result) {
        EXPECT_EQ(result, MediaStreamVideoSource::RestartResult::INVALID_STATE);
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(stream_source_->GetReadyState(),
            MediaStreamSource::kReadyStateLive);
  EXPECT_FALSE(source_stopped_);
  EXPECT_FALSE(video_capturer_source_->IsRunning());

  // Restart the source. With the mock delegate, any video format will do.
  EXPECT_CALL(mock_delegate(), MockStartCapture(_, _, _))
      .WillOnce(Return(VideoCaptureRunState::kRunning));
  EXPECT_FALSE(video_capturer_source_->IsRunning());
  video_capturer_source_->Restart(
      media::VideoCaptureFormat(),
      BindOnce([](MediaStreamVideoSource::RestartResult result) {
        EXPECT_EQ(result, MediaStreamVideoSource::RestartResult::IS_RUNNING);
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(stream_source_->GetReadyState(),
            MediaStreamSource::kReadyStateLive);
  EXPECT_TRUE(video_capturer_source_->IsRunning());

  // A second Restart() should fail with invalid state since Restart() is
  // defined only when the source is stopped for restart. Existing ready state
  // should remain the same.
  EXPECT_TRUE(video_capturer_source_->IsRunning());
  video_capturer_source_->Restart(
      media::VideoCaptureFormat(),
      BindOnce([](MediaStreamVideoSource::RestartResult result) {
        EXPECT_EQ(result, MediaStreamVideoSource::RestartResult::INVALID_STATE);
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(stream_source_->GetReadyState(),
            MediaStreamSource::kReadyStateLive);
  EXPECT_TRUE(video_capturer_source_->IsRunning());

  // An delegate stop should stop the source and change the track state to
  // "ended".
  EXPECT_CALL(mock_delegate(), MockStopCapture());
  delegate_->SetRunning(VideoCaptureRunState::kStopped);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(MediaStreamSource::kReadyStateEnded,
            stream_source_->GetReadyState());
  // Verify that WebPlatformMediaStreamSource::SourceStoppedCallback has
  // been triggered.
  EXPECT_TRUE(source_stopped_);
  EXPECT_FALSE(video_capturer_source_->IsRunning());
}

TEST_F(MediaStreamVideoCapturerSourceTest, StartStopAndNotify) {
  InSequence s;
  EXPECT_CALL(mock_delegate(), MockStartCapture(_, _, _))
      .WillOnce(Return(VideoCaptureRunState::kRunning));
  WebMediaStreamTrack web_track =
      StartSource(VideoTrackAdapterSettings(), std::nullopt, false, 0.0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(MediaStreamSource::kReadyStateLive,
            stream_source_->GetReadyState());
  EXPECT_FALSE(source_stopped_);
  EXPECT_EQ(start_result_, MediaStreamRequestResult::OK);

  stop_capture_flag_ = false;
  EXPECT_CALL(mock_delegate(), MockStopCapture())
      .WillOnce(InvokeWithoutArgs(
          this, &MediaStreamVideoCapturerSourceTest::SetStopCaptureFlag));
  EXPECT_CALL(*this, MockNotification());
  MediaStreamTrackPlatform* track =
      MediaStreamTrackPlatform::GetTrack(web_track);
  track->StopAndNotify(
      BindOnce(&MediaStreamVideoCapturerSourceTest::MockNotification,
               base::Unretained(this)));
  EXPECT_EQ(MediaStreamSource::kReadyStateEnded,
            stream_source_->GetReadyState());
  EXPECT_TRUE(source_stopped_);
  // It is a requirement that StopCapture() gets called in the same task as
  // StopAndNotify(), as CORS security checks for element capture rely on this.
  EXPECT_TRUE(stop_capture_flag_);
  // The readyState is updated in the current task, but the notification is
  // received on a separate task.
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaStreamVideoCapturerSourceTest, ChangeSource) {
  InSequence s;
  EXPECT_CALL(mock_delegate(), MockStartCapture(_, _, _))
      .WillOnce(Return(VideoCaptureRunState::kRunning));
  WebMediaStreamTrack track =
      StartSource(VideoTrackAdapterSettings(), std::nullopt, false, 0.0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(MediaStreamSource::kReadyStateLive,
            stream_source_->GetReadyState());
  EXPECT_FALSE(source_stopped_);
  EXPECT_EQ(start_result_, MediaStreamRequestResult::OK);

  // A bogus notification of running from the delegate when the source has
  // already started should not change the state.
  delegate_->SetRunning(VideoCaptureRunState::kRunning);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(MediaStreamSource::kReadyStateLive,
            stream_source_->GetReadyState());
  EXPECT_FALSE(source_stopped_);
  EXPECT_EQ(video_capturer_source_->GetCaptureVersion(),
            media::CaptureVersion());

  // |ChangeSourceImpl()| will recreate the |delegate_|, so check the
  // |MockStartCapture()| invoking in the |RecreateVideoCapturerSource()|.
  EXPECT_CALL(mock_delegate(), MockStopCapture());
  MediaStreamDevice fake_video_device(
      mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE, "Fake_Video_Device",
      "Fake Video Device");
  video_capturer_source_->ChangeSourceImpl(fake_video_device);
  EXPECT_EQ(MediaStreamSource::kReadyStateLive,
            stream_source_->GetReadyState());
  EXPECT_FALSE(source_stopped_);
  EXPECT_EQ(video_capturer_source_->GetCaptureVersion(),
            media::CaptureVersion(/*source=*/1, /*sub_capture=*/0));

  // If the delegate stops, the source should stop.
  EXPECT_CALL(mock_delegate(), MockStopCapture());
  delegate_->SetRunning(VideoCaptureRunState::kStopped);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(MediaStreamSource::kReadyStateEnded,
            stream_source_->GetReadyState());
  // Verify that WebPlatformMediaStreamSource::SourceStoppedCallback has
  // been triggered.
  EXPECT_TRUE(source_stopped_);
}

TEST_F(MediaStreamVideoCapturerSourceTest, FailStartSystemPermission) {
  InSequence s;
  EXPECT_CALL(mock_delegate(), MockStartCapture(_, _, _))
      .WillOnce(Return(VideoCaptureRunState::kSystemPermissionsError));
  WebMediaStreamTrack track =
      StartSource(VideoTrackAdapterSettings(), std::nullopt, false, 0.0);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(source_stopped_);
  EXPECT_EQ(start_result_,
            MediaStreamRequestResult::PERMISSION_DENIED_BY_SYSTEM);
}

TEST_F(MediaStreamVideoCapturerSourceTest, FailStartCamInUse) {
  InSequence s;
  EXPECT_CALL(mock_delegate(), MockStartCapture(_, _, _))
      .WillOnce(Return(VideoCaptureRunState::kCameraBusyError));
  WebMediaStreamTrack track =
      StartSource(VideoTrackAdapterSettings(), std::nullopt, false, 0.0);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(source_stopped_);
  EXPECT_EQ(start_result_, MediaStreamRequestResult::DEVICE_IN_USE);
}

TEST_F(MediaStreamVideoCapturerSourceTest, ApplySubCaptureTargetSingleTrack) {
  InSequence s;
  WebMediaStreamTrack track = StartTabCaptureSource();
  auto* native_track = MediaStreamVideoTrack::From(track);

  const base::Token token(123, 456);
  ASSERT_NO_FATAL_FAILURE(ApplySubCaptureTargetAndExpectSuccess(
      native_track, media::mojom::SubCaptureTargetType::kCropTarget, token,
      /*expected_gpu_target=*/token, /*expected_version=*/1u));
}

TEST_F(MediaStreamVideoCapturerSourceTest,
       ApplySubCaptureTargetFailsWhenHostMissingDoesNotMutateState) {
  WebMediaStreamTrack track = StartTabCaptureSource();
  auto* native_track = MediaStreamVideoTrack::From(track);
  ASSERT_TRUE(native_track);

  // Clear the dispatcher host remote so GetMediaStreamDispatcherHost() returns
  // nullptr.
  video_capturer_source_->SetMediaStreamDispatcherHostForTesting(
      mojo::NullRemote());

  const base::Token token(123, 456);
  EXPECT_EQ(ApplySubCaptureTarget(
                native_track, media::mojom::SubCaptureTargetType::kCropTarget,
                token, 1u),
            media::mojom::ApplySubCaptureTargetResult::kErrorGeneric);

  // Verify local state was NOT mutated.
  EXPECT_TRUE(GetTrackTarget(native_track).is_zero());
}

TEST_F(MediaStreamVideoCapturerSourceTest, ApplySubCaptureTargetClones) {
  InSequence s;
  WebMediaStreamTrack track1 = StartTabCaptureSource();
  WebMediaStreamTrack track2 = CloneTrack(track1, "track2");

  auto* native_track1 = MediaStreamVideoTrack::From(track1);
  auto* native_track2 = MediaStreamVideoTrack::From(track2);

  const base::Token token1(111, 222);
  const base::Token token2(333, 444);

  // 1. First track cropped, second track uncropped -> Multi-Track (Full Frame
  // mode in GPU).
  ASSERT_NO_FATAL_FAILURE(ApplySubCaptureTargetAndExpectSuccess(
      native_track1, media::mojom::SubCaptureTargetType::kCropTarget, token1,
      /*expected_gpu_target=*/base::Token(), /*expected_version=*/1u));

  // 2. Both tracks cropped to the SAME target -> Optimized Single Target mode
  // in GPU.
  ASSERT_NO_FATAL_FAILURE(ApplySubCaptureTargetAndExpectSuccess(
      native_track2, media::mojom::SubCaptureTargetType::kCropTarget, token1,
      /*expected_gpu_target=*/token1, /*expected_version=*/2u));

  // 3. Second track changed to DIFFERENT target token2 -> GPU switched back to
  // Full Frame mode.
  ASSERT_NO_FATAL_FAILURE(ApplySubCaptureTargetAndExpectSuccess(
      native_track2, media::mojom::SubCaptureTargetType::kCropTarget, token2,
      /*expected_gpu_target=*/base::Token(), /*expected_version=*/3u));
}

TEST_F(MediaStreamVideoCapturerSourceTest,
       ApplySubCaptureTargetClonedTrackInheritsTarget) {
  InSequence s;
  WebMediaStreamTrack track1 = StartTabCaptureSource();
  auto* native_track1 = MediaStreamVideoTrack::From(track1);

  const base::Token token1(111, 222);
  const base::Token token2(333, 444);

  // 1. Crop track1 to token1.
  ASSERT_NO_FATAL_FAILURE(ApplySubCaptureTargetAndExpectSuccess(
      native_track1, media::mojom::SubCaptureTargetType::kCropTarget, token1,
      /*expected_gpu_target=*/token1, /*expected_version=*/1u));

  // 2. Clone track1 -> track2.
  WebMediaStreamTrack track2 = CloneTrack(track1, "track2");
  auto* native_track2 = MediaStreamVideoTrack::From(track2);
  ASSERT_TRUE(native_track2);
  EXPECT_EQ(GetTrackTarget(native_track2), token1);

  // 3. Crop track1 to token2. Track2 retains token1, GPU switches to full
  // frame.
  ASSERT_NO_FATAL_FAILURE(ApplySubCaptureTargetAndExpectSuccess(
      native_track1, media::mojom::SubCaptureTargetType::kCropTarget, token2,
      /*expected_gpu_target=*/base::Token(), /*expected_version=*/2u));

  EXPECT_EQ(GetTrackTarget(native_track1), token2);
  EXPECT_EQ(GetTrackTarget(native_track2), token1);

  // 4. Clone track2 (which has token1) -> track3. Track3 inherits token1.
  WebMediaStreamTrack track3 = CloneTrack(track2, "track3");
  auto* native_track3 = MediaStreamVideoTrack::From(track3);
  ASSERT_TRUE(native_track3);
  EXPECT_EQ(GetTrackTarget(native_track3), token1);

  // 5. Clone track1 (which has token2) -> track4. Track4 inherits token2.
  WebMediaStreamTrack track4 = CloneTrack(track1, "track4");
  auto* native_track4 = MediaStreamVideoTrack::From(track4);
  ASSERT_TRUE(native_track4);
  EXPECT_EQ(GetTrackTarget(native_track4), token2);
}

TEST_F(MediaStreamVideoCapturerSourceTest,
       ApplySubCaptureTargetRestrictionTargetFailsIfClonesExist) {
  InSequence s;
  WebMediaStreamTrack track1 = StartTabCaptureSource();
  auto* native_track1 = MediaStreamVideoTrack::From(track1);
  ASSERT_TRUE(native_track1);

  const base::Token token1(111, 222);

  // Single track: ApplySubCaptureTarget for kRestrictionTarget succeeds.
  ASSERT_NO_FATAL_FAILURE(ApplySubCaptureTargetAndExpectSuccess(
      native_track1, media::mojom::SubCaptureTargetType::kRestrictionTarget,
      token1, /*expected_gpu_target=*/token1, /*expected_version=*/1u));

  // Clone track1 -> track2.
  WebMediaStreamTrack track2 = CloneTrack(track1, "track2");
  ASSERT_TRUE(MediaStreamVideoTrack::From(track2));

  // Multiple tracks: GetNextCaptureVersion for kRestrictionTarget must fail
  // (std::nullopt).
  auto version2 = GetNextCaptureVersion(
      media::mojom::SubCaptureTargetType::kRestrictionTarget);
  EXPECT_FALSE(version2.has_value());

  // Applying restriction target when clones exist must reject with
  // kInvalidTarget.
  const base::Token token2(333, 444);
  EXPECT_EQ(
      ApplySubCaptureTarget(
          native_track1, media::mojom::SubCaptureTargetType::kRestrictionTarget,
          token2, 99),
      media::mojom::ApplySubCaptureTargetResult::kInvalidTarget);
}

TEST_F(MediaStreamVideoCapturerSourceTest,
       ApplySubCaptureTargetClearsGpuStateOnLastTrackRemoval) {
  WebMediaStreamTrack track1 = StartTabCaptureSource();
  auto* native_track1 = MediaStreamVideoTrack::From(track1);
  ASSERT_TRUE(native_track1);

  const base::Token token1(111, 222);
  ASSERT_NO_FATAL_FAILURE(ApplySubCaptureTargetAndExpectSuccess(
      native_track1, media::mojom::SubCaptureTargetType::kCropTarget, token1,
      /*expected_gpu_target=*/token1, /*expected_version=*/1u));

  // Removing the last track must send a zero token IPC to clear the GPU state
  // and stop capture.
  base::test::TestFuture<void> teardown_future;
  EXPECT_CALL(
      mock_dispatcher_host_,
      ApplySubCaptureTarget(_, media::mojom::SubCaptureTargetType::kCropTarget,
                            base::Token(), 2u, _))
      .WillOnce([&teardown_future](const base::UnguessableToken&,
                                   media::mojom::SubCaptureTargetType,
                                   const base::Token&, uint32_t,
                                   mojom::blink::MediaStreamDispatcherHost::
                                       ApplySubCaptureTargetCallback callback) {
        std::move(callback).Run(
            media::mojom::ApplySubCaptureTargetResult::kSuccess);
        teardown_future.SetValue();
      });
  EXPECT_CALL(mock_delegate(), MockStopCapture());

  native_track1->Stop();
  EXPECT_TRUE(teardown_future.Wait());
}

TEST_F(MediaStreamVideoCapturerSourceTest,
       ApplySubCaptureTargetClonedAfterRestrictionInheritsTarget) {
  WebMediaStreamTrack track1 = StartTabCaptureSource();
  auto* native_track1 = MediaStreamVideoTrack::From(track1);
  ASSERT_TRUE(native_track1);

  const base::Token token1(111, 222);
  ASSERT_NO_FATAL_FAILURE(ApplySubCaptureTargetAndExpectSuccess(
      native_track1, media::mojom::SubCaptureTargetType::kRestrictionTarget,
      token1, /*expected_gpu_target=*/token1, /*expected_version=*/1u));

  // Clone track1 -> track2.
  WebMediaStreamTrack track2 = CloneTrack(track1, "track2");
  auto* native_track2 = MediaStreamVideoTrack::From(track2);
  ASSERT_TRUE(native_track2);

  // Attempting to apply a crop target on track2 must fail with kInvalidTarget
  // because an active restriction target is present on another track.
  const base::Token token2(333, 444);
  EXPECT_EQ(ApplySubCaptureTarget(
                native_track2, media::mojom::SubCaptureTargetType::kCropTarget,
                token2, 99),
            media::mojom::ApplySubCaptureTargetResult::kInvalidTarget);
}

TEST_F(MediaStreamVideoCapturerSourceTest,
       RemovingOneOfIdenticallyCroppedTracksRetainsGpuCrop) {
  WebMediaStreamTrack track1 = StartTabCaptureSource();
  auto* native_track1 = MediaStreamVideoTrack::From(track1);
  ASSERT_TRUE(native_track1);

  const base::Token token1(111, 222);
  ASSERT_NO_FATAL_FAILURE(ApplySubCaptureTargetAndExpectSuccess(
      native_track1, media::mojom::SubCaptureTargetType::kCropTarget, token1,
      /*expected_gpu_target=*/token1, /*expected_version=*/1u));

  // Clone track1 -> track2 (inherits token1).
  WebMediaStreamTrack track2 = CloneTrack(track1, "track2");
  auto* native_track2 = MediaStreamVideoTrack::From(track2);
  ASSERT_TRUE(native_track2);

  // Stopping track1 must NOT send any uncrop IPC to the GPU since track2 is
  // still active and cropped to token1.
  EXPECT_CALL(mock_dispatcher_host_, ApplySubCaptureTarget(_, _, _, _, _))
      .Times(0);
  native_track1->Stop();
  testing::Mock::VerifyAndClearExpectations(&mock_dispatcher_host_);

  // Stopping track2 (the last track) clears the GPU state with a zero token.
  base::test::TestFuture<void> teardown_future;
  EXPECT_CALL(
      mock_dispatcher_host_,
      ApplySubCaptureTarget(_, media::mojom::SubCaptureTargetType::kCropTarget,
                            base::Token(), 2u, _))
      .WillOnce([&teardown_future](const base::UnguessableToken&,
                                   media::mojom::SubCaptureTargetType,
                                   const base::Token&, uint32_t,
                                   mojom::blink::MediaStreamDispatcherHost::
                                       ApplySubCaptureTargetCallback callback) {
        std::move(callback).Run(
            media::mojom::ApplySubCaptureTargetResult::kSuccess);
        teardown_future.SetValue();
      });
  EXPECT_CALL(mock_delegate(), MockStopCapture());

  native_track2->Stop();
  EXPECT_TRUE(teardown_future.Wait());
}

}  // namespace blink
