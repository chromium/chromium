// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/mediastream/browser_capture_media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/mock_mojo_media_stream_dispatcher_host.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_client.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

mojom::blink::MediaDevicesDispatcherHost* UnusedMediaDevicesDispatcherHost() {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void SignalSourceReady(
    WebPlatformMediaStreamSource::ConstraintsOnceCallback source_ready,
    WebPlatformMediaStreamSource* source) {
  std::move(source_ready)
      .Run(source, mojom::blink::MediaStreamRequestResult::OK, "");
}

// UserMediaProcessor with mocked CreateVideoSource and CreateAudioSource.
class MockUserMediaProcessor : public UserMediaProcessor {
 public:
  explicit MockUserMediaProcessor(LocalFrame* frame)
      : UserMediaProcessor(
            frame,
            base::BindRepeating(&UnusedMediaDevicesDispatcherHost),
            scheduler::GetSingleThreadTaskRunnerForTesting()) {}

  // UserMediaProcessor overrides.
  std::unique_ptr<MediaStreamVideoSource> CreateVideoSource(
      const MediaStreamDevice& device,
      WebPlatformMediaStreamSource::SourceStoppedCallback stop_callback)
      override {
    auto source = std::make_unique<MockMediaStreamVideoSource>();
    source->SetDevice(device);
    source->SetStopCallback(std::move(stop_callback));
    return source;
  }

  std::unique_ptr<MediaStreamAudioSource> CreateAudioSource(
      const MediaStreamDevice& device,
      WebPlatformMediaStreamSource::ConstraintsRepeatingCallback source_ready)
      override {
    auto source = std::make_unique<MediaStreamAudioSource>(
        scheduler::GetSingleThreadTaskRunnerForTesting(),
        /*is_local_source=*/true);
    source->SetDevice(device);
    // RunUntilIdle is required for this task to complete.
    scheduler::GetSingleThreadTaskRunnerForTesting()->PostTask(
        FROM_HERE, base::BindOnce(&SignalSourceReady, std::move(source_ready),
                                  source.get()));
    return source;
  }
};

class UserMediaClientUnderTest : public UserMediaClient {
 public:
  UserMediaClientUnderTest(
      LocalFrame* frame,
      UserMediaProcessor* user_media_processor,
      UserMediaProcessor* display_user_media_processor,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : UserMediaClient(frame,
                        user_media_processor,
                        display_user_media_processor,
                        task_runner) {}
};

// ScopedMockUserMediaClient creates and installs a temporary UserMediaClient in
// |window| when constructed and restores the original UserMediaClient when
// destroyed. Uses a MockMojoMediaStreamDispatcherHost and
// MockUserMediaProcessor.
class ScopedMockUserMediaClient {
 public:
  explicit ScopedMockUserMediaClient(LocalDOMWindow* window)
      : original_(Supplement<LocalDOMWindow>::From<UserMediaClient>(window)) {
    auto* user_media_processor =
        MakeGarbageCollected<MockUserMediaProcessor>(window->GetFrame());
    auto* display_user_media_processor =
        MakeGarbageCollected<MockUserMediaProcessor>(window->GetFrame());
    user_media_processor->set_media_stream_dispatcher_host_for_testing(
        mock_media_stream_dispatcher_host.CreatePendingRemoteAndBind());
    display_user_media_processor->set_media_stream_dispatcher_host_for_testing(
        display_mock_media_stream_dispatcher_host.CreatePendingRemoteAndBind());
    temp_ = MakeGarbageCollected<UserMediaClientUnderTest>(
        window->GetFrame(), user_media_processor, display_user_media_processor,
        scheduler::GetSingleThreadTaskRunnerForTesting());
    Supplement<LocalDOMWindow>::ProvideTo<UserMediaClient>(*window,
                                                           temp_.Get());
  }

  ~ScopedMockUserMediaClient() {
    auto* window = temp_->GetSupplementable();
    if (Supplement<LocalDOMWindow>::From<UserMediaClient>(window) ==
        temp_.Get()) {
      if (original_) {
        Supplement<LocalDOMWindow>::ProvideTo<UserMediaClient>(*window,
                                                               original_.Get());
      } else {
        window->Supplementable<LocalDOMWindow>::RemoveSupplement<
            UserMediaClient>();
      }
    }
  }

  MockMojoMediaStreamDispatcherHost mock_media_stream_dispatcher_host;
  MockMojoMediaStreamDispatcherHost display_mock_media_stream_dispatcher_host;

 private:
  Persistent<UserMediaClient> temp_;
  Persistent<UserMediaClient> original_;
};

MediaStreamTrack::TransferredValues TransferredValuesTabCaptureVideo() {
  // The TransferredValues here match the expectations in
  // V8ScriptValueSerializerForModulesTest.TransferMediaStreamTrack. Please keep
  // them in sync.
  return MediaStreamTrack::TransferredValues{
      .track_impl_subtype =
          BrowserCaptureMediaStreamTrack::GetStaticWrapperTypeInfo(),
      .session_id = base::UnguessableToken::Create(),
      .transfer_id = base::UnguessableToken::Create(),
      .kind = "video",
      .id = "component_id",
      .label = "test_name",
      .enabled = false,
      .muted = true,
      .content_hint = WebMediaStreamTrack::ContentHintType::kVideoMotion,
      .ready_state = MediaStreamSource::kReadyStateLive,
      .sub_capture_target_version = 0};
}

mojom::blink::StreamDevices DevicesTabCaptureVideo(
    base::UnguessableToken session_id) {
  MediaStreamDevice device(mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
                           "device_id", "device_name");
  device.set_session_id(session_id);
  device.display_media_info = media::mojom::DisplayMediaInformation::New(
      media::mojom::DisplayCaptureSurfaceType::BROWSER,
      /*logical_surface=*/true, media::mojom::CursorCaptureType::NEVER,
      /*capture_handle=*/nullptr,
      /*zoom_level=*/100);
  return {std::nullopt, device};
}

TEST(MediaStreamTrackTransferTest, TabCaptureVideoFromTransferredStateBasic) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform;
  ScopedMockUserMediaClient scoped_user_media_client(&scope.GetWindow());

  auto data = TransferredValuesTabCaptureVideo();
#if BUILDFLAG(IS_ANDROID)
  data.track_impl_subtype = MediaStreamTrack::GetStaticWrapperTypeInfo();
  data.sub_capture_target_version = std::nullopt;
#endif
  scoped_user_media_client.display_mock_media_stream_dispatcher_host
      .SetStreamDevices(DevicesTabCaptureVideo(data.session_id));

  auto* new_track =
      MediaStreamTrack::FromTransferredState(scope.GetScriptState(), data);

#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(new_track->GetWrapperTypeInfo(),
            MediaStreamTrack::GetStaticWrapperTypeInfo());
#else
  EXPECT_EQ(new_track->GetWrapperTypeInfo(),
            BrowserCaptureMediaStreamTrack::GetStaticWrapperTypeInfo());
#endif
  EXPECT_EQ(new_track->Component()->GetSourceName(), "device_name");
  // TODO(crbug.com/1288839): the ID needs to be set correctly
  // EXPECT_EQ(new_track->id(), "component_id");
  // TODO(crbug.com/1288839): Should this match the device info or the
  // transferred data?
  EXPECT_EQ(new_track->label(), "device_name");
  EXPECT_EQ(new_track->kind(), "video");
  // TODO(crbug.com/1288839): enabled needs to be set correctly
  // EXPECT_EQ(new_track->enabled(), false);
  // TODO(crbug.com/1288839): muted needs to be set correctly
  // EXPECT_EQ(new_track->muted(), true);
  // TODO(crbug.com/1288839): the content hint needs to be set correctly
  // EXPECT_EQ(new_track->ContentHint(), "motion");
  EXPECT_EQ(new_track->readyState(), "live");

  platform->RunUntilIdle();
  ThreadState::Current()->CollectAllGarbageForTesting();
}

// TODO(crbug.com/1288839): implement and test transferred sub-capture-target
// version

TEST(MediaStreamTrackTransferTest, TabCaptureAudioFromTransferredState) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  // The TransferredValues here match the expectations in
  // V8ScriptValueSerializerForModulesTest.TransferAudioMediaStreamTrack. Please
  // keep them in sync.
  MediaStreamTrack::TransferredValues data{
      .track_impl_subtype = MediaStreamTrack::GetStaticWrapperTypeInfo(),
      .session_id = base::UnguessableToken::Create(),
      .transfer_id = base::UnguessableToken::Create(),
      .kind = "audio",
      .id = "component_id",
      .label = "test_name",
      .enabled = true,
      .muted = true,
      .content_hint = WebMediaStreamTrack::ContentHintType::kAudioSpeech,
      .ready_state = MediaStreamSource::kReadyStateLive};

  ScopedMockUserMediaClient scoped_user_media_client(&scope.GetWindow());

  MediaStreamDevice device(mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE,
                           "device_id", "device_name");
  device.set_session_id(data.session_id);
  device.display_media_info = media::mojom::DisplayMediaInformation::New(
      media::mojom::DisplayCaptureSurfaceType::BROWSER,
      /*logical_surface=*/true, media::mojom::CursorCaptureType::NEVER,
      /*capture_handle=*/nullptr,
      /*zoom_level=*/100);
  scoped_user_media_client.display_mock_media_stream_dispatcher_host
      .SetStreamDevices({std::nullopt, device});

  auto* new_track =
      MediaStreamTrack::FromTransferredState(scope.GetScriptState(), data);

  EXPECT_EQ(new_track->GetWrapperTypeInfo(),
            MediaStreamTrack::GetStaticWrapperTypeInfo());
  EXPECT_EQ(new_track->Component()->GetSourceName(), "device_name");
  // TODO(crbug.com/1288839): the ID needs to be set correctly
  // EXPECT_EQ(new_track->id(), "component_id");
  // TODO(crbug.com/1288839): Should this match the device info or the
  // transferred data?
  EXPECT_EQ(new_track->label(), "device_name");
  EXPECT_EQ(new_track->kind(), "audio");
  EXPECT_EQ(new_track->enabled(), true);
  // TODO(crbug.com/1288839): muted needs to be set correctly
  // EXPECT_EQ(new_track->muted(), true);
  // TODO(crbug.com/1288839): the content hint needs to be set correctly
  // EXPECT_EQ(new_track->ContentHint(), "speech");
  EXPECT_EQ(new_track->readyState(), "live");

  base::RunLoop().RunUntilIdle();
  ThreadState::Current()->CollectAllGarbageForTesting();
}

}  // namespace
}  // namespace blink
