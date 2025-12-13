// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_track_impl.h"

#include <tuple>

#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom-blink.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_long_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_track_state.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constrainlongrange_long.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_reader.h"
#include "third_party/blink/renderer/modules/mediastream/apply_constraints_processor.h"
#include "third_party/blink/renderer/modules/mediastream/media_constraints.h"
#include "third_party/blink/renderer/modules/mediastream/media_constraints_impl.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_video_content.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_sink.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/speech_recognition_media_stream_audio_sink.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/webrtc/peer_connection_remote_audio_source.h"
#include "third_party/webrtc/api/media_stream_interface.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

using testing::_;

namespace blink {

namespace {
const gfx::Size kTestScreenSize{kDefaultScreenCastWidth,
                                kDefaultScreenCastHeight};
constexpr int kReducedWidth = 640;
constexpr int kReducedHeight = 320;
constexpr float kAspectRatio = kReducedWidth / kReducedHeight;
constexpr float kMaxFrameRate = 11.0f;
constexpr float kMinFrameRate = 0.0f;

class TestObserver : public GarbageCollected<TestObserver>,
                     public MediaStreamTrack::Observer {
 public:
  void TrackChangedState() override { observation_count_++; }
  int ObservationCount() const { return observation_count_; }

 private:
  int observation_count_ = 0;
};

std::unique_ptr<MockMediaStreamVideoSource> MakeMockMediaStreamVideoSource() {
  return base::WrapUnique(new MockMediaStreamVideoSource(
      media::VideoCaptureFormat(gfx::Size(640, 480), 30.0,
                                media::PIXEL_FORMAT_I420),
      true));
}

class MockEventListener : public NativeEventListener {
 public:
  MOCK_METHOD(void, Invoke, (ExecutionContext*, Event*));
};

class MockWebMediaStreamObserver : public WebMediaStreamObserver {
 public:
  MOCK_METHOD(void, EnabledStateChangedForWebRtcAudio, (bool));
  base::WeakPtr<WebMediaStreamObserver> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockWebMediaStreamObserver> weak_ptr_factory_{this};
};

MediaStreamComponent* MakeMockVideoComponent() {
  std::unique_ptr<MockMediaStreamVideoSource> platform_source =
      MakeMockMediaStreamVideoSource();
  MockMediaStreamVideoSource* platform_source_ptr = platform_source.get();
  MediaStreamSource* source = MakeGarbageCollected<MediaStreamSource>(
      "id", MediaStreamSource::StreamType::kTypeVideo, "name",
      /*remote=*/false, std::move(platform_source));
  return MakeGarbageCollected<MediaStreamComponentImpl>(
      source, std::make_unique<MediaStreamVideoTrack>(
                  platform_source_ptr,
                  MediaStreamVideoSource::ConstraintsOnceCallback(),
                  /*enabled=*/true));
}

MediaStreamComponent* MakeMockAudioComponent() {
  MediaStreamSource* source = MakeGarbageCollected<MediaStreamSource>(
      "id", MediaStreamSource::StreamType::kTypeAudio, "name",
      /*remote=*/false,
      std::make_unique<MediaStreamAudioSource>(
          scheduler::GetSingleThreadTaskRunnerForTesting(),
          true /* is_local_source */));
  auto platform_track =
      std::make_unique<MediaStreamAudioTrack>(true /* is_local_track */);
  return MakeGarbageCollected<MediaStreamComponentImpl>(
      source, std::move(platform_track));
}

media::VideoCaptureFormat GetDefaultVideoContentCaptureFormat() {
  MediaConstraints constraints;
  constraints.Initialize();
  return blink::SelectSettingsVideoContentCapture(
             constraints, mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
             kTestScreenSize.width(), kTestScreenSize.height())
      .Format();
}

std::tuple<MediaStreamComponent*, MockMediaStreamVideoSource*>
MakeMockDisplayVideoCaptureComponent() {
  auto platform_source = std::make_unique<MockMediaStreamVideoSource>(
      GetDefaultVideoContentCaptureFormat(), false);
  platform_source->SetDevice(
      MediaStreamDevice(mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
                        "fakeSourceId", "fakeWindowCapturer"));
  MockMediaStreamVideoSource* platform_source_ptr = platform_source.get();
  MediaStreamSource* source = MakeGarbageCollected<MediaStreamSource>(
      "id", MediaStreamSource::StreamType::kTypeVideo, "name",
      false /* remote */, std::move(platform_source));
  auto platform_track = std::make_unique<MediaStreamVideoTrack>(
      platform_source_ptr,
      WebPlatformMediaStreamSource::ConstraintsOnceCallback(),
      true /* enabled */);
  MediaStreamComponent* component =
      MakeGarbageCollected<MediaStreamComponentImpl>(source,
                                                     std::move(platform_track));
  return std::make_tuple(component, platform_source_ptr);
}

MediaTrackConstraints* MakeMediaTrackConstraints(
    std::optional<int> exact_width,
    std::optional<int> exact_height,
    std::optional<float> min_frame_rate,
    std::optional<float> max_frame_rate,
    std::optional<float> aspect_ratio = std::nullopt) {
  MediaConstraints constraints;
  MediaTrackConstraintSetPlatform basic;
  if (exact_width) {
    basic.width.SetExact(*exact_width);
  }
  if (exact_height) {
    basic.height.SetExact(*exact_height);
  }
  if (min_frame_rate) {
    basic.frame_rate.SetMin(*min_frame_rate);
  }
  if (max_frame_rate) {
    basic.frame_rate.SetMax(*max_frame_rate);
  }
  if (aspect_ratio) {
    basic.aspect_ratio.SetExact(*aspect_ratio);
  }

  constraints.Initialize(basic, Vector<MediaTrackConstraintSetPlatform>());
  return media_constraints_impl::ConvertConstraints(constraints);
}

}  // namespace

class MediaStreamTrackImplTest : public testing::Test {
 public:
  ~MediaStreamTrackImplTest() override {
    WebHeap::CollectAllGarbageForTesting();
  }

  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
};

TEST_F(MediaStreamTrackImplTest, StopTrackTriggersObservers) {
  V8TestingScope v8_scope;
  std::unique_ptr<MockMediaStreamVideoSource> platform_source =
      MakeMockMediaStreamVideoSource();
  MockMediaStreamVideoSource* platform_source_ptr = platform_source.get();
  MediaStreamSource* source = MakeGarbageCollected<MediaStreamSource>(
      "id", MediaStreamSource::StreamType::kTypeVideo, "name",
      /*remote=*/false, std::move(platform_source));
  MediaStreamComponent* component =
      MakeGarbageCollected<MediaStreamComponentImpl>(
          source, std::make_unique<MediaStreamVideoTrack>(
                      platform_source_ptr,
                      MediaStreamVideoSource::ConstraintsOnceCallback(),
                      /*enabled=*/true));
  MediaStreamTrack* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), component);

  TestObserver* testObserver = MakeGarbageCollected<TestObserver>();
  track->AddObserver(testObserver);

  source->SetReadyState(MediaStreamSource::kReadyStateMuted);
  EXPECT_EQ(testObserver->ObservationCount(), 1);

  track->stopTrack(v8_scope.GetExecutionContext());
  EXPECT_EQ(testObserver->ObservationCount(), 2);
}

TEST_F(MediaStreamTrackImplTest, StopTrackSynchronouslyDisablesMedia) {
  V8TestingScope v8_scope;

  MediaStreamSource* source = MakeGarbageCollected<MediaStreamSource>(
      "id", MediaStreamSource::StreamType::kTypeAudio, "name",
      /*remote=*/false, MakeMockMediaStreamVideoSource());
  auto platform_track =
      std::make_unique<MediaStreamAudioTrack>(true /* is_local_track */);
  MediaStreamAudioTrack* platform_track_ptr = platform_track.get();
  MediaStreamComponent* component =
      MakeGarbageCollected<MediaStreamComponentImpl>(source,
                                                     std::move(platform_track));
  MediaStreamTrack* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), component);

  ASSERT_TRUE(platform_track_ptr->IsEnabled());
  track->stopTrack(v8_scope.GetExecutionContext());
  EXPECT_FALSE(platform_track_ptr->IsEnabled());
}

TEST_F(MediaStreamTrackImplTest, MutedStateUpdates) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockMediaStreamVideoSource> platform_source =
      MakeMockMediaStreamVideoSource();
  MockMediaStreamVideoSource* platform_source_ptr = platform_source.get();
  MediaStreamSource* source = MakeGarbageCollected<MediaStreamSource>(
      "id", MediaStreamSource::StreamType::kTypeVideo, "name",
      /*remote=*/false, std::move(platform_source));
  MediaStreamComponent* component =
      MakeGarbageCollected<MediaStreamComponentImpl>(
          source, std::make_unique<MediaStreamVideoTrack>(
                      platform_source_ptr,
                      MediaStreamVideoSource::ConstraintsOnceCallback(),
                      /*enabled=*/true));
  MediaStreamTrack* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), component);

  EXPECT_EQ(track->muted(), false);

  source->SetReadyState(MediaStreamSource::kReadyStateMuted);
  EXPECT_EQ(track->muted(), true);

  source->SetReadyState(MediaStreamSource::kReadyStateLive);
  EXPECT_EQ(track->muted(), false);
}

class HtmlMediaElementForWebRtcAudioTest : public testing::Test {
 public:
  HtmlMediaElementForWebRtcAudioTest() { web_view_helper_.Initialize(); }

  ~HtmlMediaElementForWebRtcAudioTest() override {
    WebHeap::CollectAllGarbageForTesting();
  }

 protected:
  MediaStreamComponent* MakeMockWebRtcAudioComponent() {
    auto* source = MakeGarbageCollected<MediaStreamSource>(
        "id", MediaStreamSource::StreamType::kTypeAudio, "name",
        /*remote=*/true,
        std::make_unique<MediaStreamAudioSource>(
            scheduler::GetSingleThreadTaskRunnerForTesting(),
            false /* is_local_source */));

    scoped_refptr<webrtc::AudioTrackInterface> remote_track(
        blink::MockWebRtcAudioTrack::Create("track_id").get());
    auto webrtc_audio_track =
        std::make_unique<PeerConnectionRemoteAudioTrack>(remote_track);

    return MakeGarbageCollected<MediaStreamComponentImpl>(
        source, std::move(webrtc_audio_track));
  }

  MediaStreamComponent* MakeMockAudioComponent() {
    MediaStreamSource* source = MakeGarbageCollected<MediaStreamSource>(
        "id", MediaStreamSource::StreamType::kTypeAudio, "name",
        /*remote=*/false,
        std::make_unique<MediaStreamAudioSource>(
            scheduler::GetSingleThreadTaskRunnerForTesting(),
            true /* is_local_source */));
    auto platform_track =
        std::make_unique<MediaStreamAudioTrack>(true /* is_local_track */);
    return MakeGarbageCollected<MediaStreamComponentImpl>(
        source, std::move(platform_track));
  }

  Document* GetDocument() {
    return web_view_helper_.LocalMainFrame()->GetFrame()->GetDocument();
  }

  void SetupHtmlVideoElement() {
    video_ = MakeGarbageCollected<HTMLVideoElement>(*GetDocument());
    GetDocument()->body()->AppendChild(video_);
  }
  HTMLMediaElement* Video() const { return video_.Get(); }

  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
  frame_test_helpers::WebViewHelper web_view_helper_;
  WeakPersistent<HTMLMediaElement> video_;
};

TEST_F(HtmlMediaElementForWebRtcAudioTest,
       MuteWebRtcAudioTrackPropagatesToMediaStream) {
  V8TestingScope v8_scope;

  // pc.ontrack = function(event) {
  //    let track = event.track;
  // }
  MediaStreamComponent* component = MakeMockWebRtcAudioComponent();
  MediaStreamTrack* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), component);

  MockWebMediaStreamObserver observer;
  if (base::FeatureList::IsEnabled(kPropagateEnabledEventForWebRtcAudioTrack)) {
    EXPECT_CALL(observer, EnabledStateChangedForWebRtcAudio(false)).Times(1);
  } else {
    EXPECT_CALL(observer, EnabledStateChangedForWebRtcAudio(_)).Times(0);
  }

  // let media_stream = new MediaStream();
  // media_stream.addTrack(track);
  MediaStreamTrackVector audio_tracks = {track};
  auto* media_stream =
      MediaStream::Create(v8_scope.GetExecutionContext(), audio_tracks);
  auto* descriptor = media_stream->Descriptor();
  descriptor->SetActive(true);
  descriptor->AddObserver(observer.AsWeakPtr());

  // let video = document.createElement('video');
  // video.srcObject = media_stream;
  SetupHtmlVideoElement();
  Video()->SetSrcObjectVariant(descriptor);
  test::RunPendingTasks();

  // track.enabled = false;
  track->setEnabled(false);
}

TEST_F(HtmlMediaElementForWebRtcAudioTest,
       MuteWebLocalAudioTrackDoNotPropagatesToMediaStream) {
  V8TestingScope v8_scope;

  // Create local audio track.
  MediaStreamComponent* component = MakeMockAudioComponent();
  MediaStreamTrack* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), component);

  MockWebMediaStreamObserver observer;
  if (base::FeatureList::IsEnabled(kPropagateEnabledEventForWebRtcAudioTrack)) {
    EXPECT_CALL(observer, EnabledStateChangedForWebRtcAudio(_)).Times(0);
  } else {
    EXPECT_CALL(observer, EnabledStateChangedForWebRtcAudio(_)).Times(0);
  }

  // let media_stream = new MediaStream();
  // media_stream.addTrack(track);
  MediaStreamTrackVector audio_tracks = {track};
  auto* media_stream =
      MediaStream::Create(v8_scope.GetExecutionContext(), audio_tracks);
  auto* descriptor = media_stream->Descriptor();
  descriptor->SetActive(true);
  descriptor->AddObserver(observer.AsWeakPtr());

  // let video = document.createElement('video');
  // video.srcObject = media_stream;
  SetupHtmlVideoElement();
  Video()->SetSrcObjectVariant(descriptor);
  test::RunPendingTasks();

  // track.enabled = false;
  track->setEnabled(false);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(MediaStreamTrackImplTest,
       ZoomStateUpdatesAndTriggersConfigurationChangeEvent) {
  V8TestingScope v8_scope;
  MediaStreamComponent* component;
  MockMediaStreamVideoSource* platform_source_ptr;
  std::tie(component, platform_source_ptr) =
      MakeMockDisplayVideoCaptureComponent();

  MediaStreamTrackImpl* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), component);
  testing::StrictMock<MockEventListener>* event_listener =
      MakeGarbageCollected<testing::StrictMock<MockEventListener>>();
  track->addEventListener(event_type_names::kConfigurationchange,
                          event_listener);
  EXPECT_CALL(*event_listener, Invoke(_, _)).Times(1);

  // Start the source.
  platform_source_ptr->StartMockedSource();
  MediaStreamSource* source = component->Source();

  EXPECT_EQ(track->GetZoomLevelForTesting(), std::nullopt);
  ASSERT_TRUE(track->device());
  source->OnZoomLevelChange(*track->device(), 125);
  EXPECT_EQ(track->GetZoomLevelForTesting(), 125);

  // Stop the track.
  track->stopTrack(v8_scope.GetExecutionContext());

  // After the track stops, zoom_level of the device should not change.
  source->OnZoomLevelChange(*track->device(), 150);
  EXPECT_EQ(track->GetZoomLevelForTesting(), 125);
}
#endif

TEST_F(MediaStreamTrackImplTest, MutedDoesntUpdateAfterEnding) {
  V8TestingScope v8_scope;
  std::unique_ptr<MockMediaStreamVideoSource> platform_source =
      MakeMockMediaStreamVideoSource();
  MockMediaStreamVideoSource* platform_source_ptr = platform_source.get();
  MediaStreamSource* source = MakeGarbageCollected<MediaStreamSource>(
      "id", MediaStreamSource::StreamType::kTypeVideo, "name",
      false /* remote */, std::move(platform_source));
  MediaStreamComponent* component =
      MakeGarbageCollected<MediaStreamComponentImpl>(
          source, std::make_unique<MediaStreamVideoTrack>(
                      platform_source_ptr,
                      MediaStreamVideoSource::ConstraintsOnceCallback(),
                      /*enabled=*/true));
  MediaStreamTrack* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), component);

  ASSERT_EQ(track->muted(), false);

  track->stopTrack(v8_scope.GetExecutionContext());

  source->SetReadyState(MediaStreamSource::kReadyStateMuted);

  EXPECT_EQ(track->muted(), false);
}

TEST_F(MediaStreamTrackImplTest, CloneVideoTrack) {
  V8TestingScope v8_scope;
  MediaStreamTrack* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), MakeMockVideoComponent());

  MediaStreamTrack* clone = track->clone(v8_scope.GetExecutionContext());

  // The clone should have a component initialized with a MediaStreamVideoTrack
  // instance as its platform track.
  EXPECT_TRUE(clone->Component()->GetPlatformTrack());
  EXPECT_TRUE(MediaStreamVideoTrack::From(clone->Component()));

  // Clones should share the same source object.
  EXPECT_EQ(clone->Component()->Source(), track->Component()->Source());
}

TEST_F(MediaStreamTrackImplTest, CloneAudioTrack) {
  V8TestingScope v8_scope;

  MediaStreamComponent* component = MakeMockAudioComponent();
  MediaStreamTrack* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), component);

  MediaStreamTrack* clone = track->clone(v8_scope.GetExecutionContext());

  // The clone should have a component initialized with a MediaStreamAudioTrack
  // instance as its platform track.
  EXPECT_TRUE(clone->Component()->GetPlatformTrack());
  EXPECT_TRUE(MediaStreamAudioTrack::From(clone->Component()));

  // Clones should share the same source object.
  EXPECT_EQ(clone->Component()->Source(), component->Source());
}

TEST_F(MediaStreamTrackImplTest, CloningPreservesConstraints) {
  V8TestingScope v8_scope;

  auto platform_source = std::make_unique<MockMediaStreamVideoSource>(
      media::VideoCaptureFormat(gfx::Size(1280, 720), 1000.0,
                                media::PIXEL_FORMAT_I420),
      false);
  MockMediaStreamVideoSource* platform_source_ptr = platform_source.get();
  MediaStreamSource* source = MakeGarbageCollected<MediaStreamSource>(
      "id", MediaStreamSource::StreamType::kTypeVideo, "name",
      false /* remote */, std::move(platform_source));
  auto platform_track = std::make_unique<MediaStreamVideoTrack>(
      platform_source_ptr,
      WebPlatformMediaStreamSource::ConstraintsOnceCallback(),
      true /* enabled */);
  MediaStreamComponent* component =
      MakeGarbageCollected<MediaStreamComponentImpl>(source,
                                                     std::move(platform_track));
  MediaStreamTrack* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), component);

  MediaConstraints constraints;
  MediaTrackConstraintSetPlatform basic;
  basic.width.SetMax(240);
  constraints.Initialize(basic, Vector<MediaTrackConstraintSetPlatform>());
  track->SetInitialConstraints(constraints);

  MediaStreamTrack* clone = track->clone(v8_scope.GetExecutionContext());
  MediaTrackConstraints* clone_constraints = clone->getConstraints();
  EXPECT_TRUE(clone_constraints->hasWidth());
  EXPECT_EQ(clone_constraints->width()->GetAsConstrainLongRange()->max(), 240);
}

// These tests rely on the ability to restart content capture. This is
// currently not possible on Android.
// TODO(crbug.com/436623747): We may be able to re-enable these once we have
// an API to reconfigure the capture instead of restarting it.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ApplyConstraintsUpdatesSourceFormat \
  DISABLED_ApplyConstraintsUpdatesSourceFormat
#else
#define MAYBE_ApplyConstraintsUpdatesSourceFormat \
  ApplyConstraintsUpdatesSourceFormat
#endif
TEST_F(MediaStreamTrackImplTest, MAYBE_ApplyConstraintsUpdatesSourceFormat) {
  V8TestingScope v8_scope;
  MediaStreamComponent* component;
  MockMediaStreamVideoSource* platform_source_ptr;
  std::tie(component, platform_source_ptr) =
      MakeMockDisplayVideoCaptureComponent();
  MediaStreamTrack* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), component);
  MediaStreamVideoTrack* video_track = MediaStreamVideoTrack::From(component);

  // Start the source.
  platform_source_ptr->StartMockedSource();
  // Verify that initial settings are not the same as the constraints.
  EXPECT_NE(platform_source_ptr->max_requested_width(), kReducedWidth);
  EXPECT_NE(platform_source_ptr->max_requested_height(), kReducedHeight);
  EXPECT_NE(platform_source_ptr->max_requested_frame_rate(), kMaxFrameRate);
  EXPECT_FALSE(video_track->min_frame_rate());
  // Apply new frame rate constraints.
  MediaTrackConstraints* track_constraints = MakeMediaTrackConstraints(
      kReducedWidth, kReducedHeight, kMinFrameRate, kMaxFrameRate);
  auto apply_constraints_promise =
      track->applyConstraints(v8_scope.GetScriptState(), track_constraints);

  ScriptPromiseTester tester(v8_scope.GetScriptState(),
                             apply_constraints_promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  // Verify updated settings and that the source was restarted.
  EXPECT_EQ(platform_source_ptr->restart_count(), 1);
  EXPECT_EQ(platform_source_ptr->max_requested_width(), kReducedWidth);
  EXPECT_EQ(platform_source_ptr->max_requested_height(), kReducedHeight);
  EXPECT_EQ(platform_source_ptr->max_requested_frame_rate(), kMaxFrameRate);
  // Verify that min frame rate is updated.
  EXPECT_EQ(video_track->min_frame_rate(), kMinFrameRate);
}

// TODO(crbug.com/436623747): Re-enable.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ApplyConstraintsFramerateDoesNotAffectResolution \
  DISABLED_ApplyConstraintsFramerateDoesNotAffectResolution
#else
#define MAYBE_ApplyConstraintsFramerateDoesNotAffectResolution \
  ApplyConstraintsFramerateDoesNotAffectResolution
#endif
TEST_F(MediaStreamTrackImplTest,
       MAYBE_ApplyConstraintsFramerateDoesNotAffectResolution) {
  V8TestingScope v8_scope;
  MediaStreamComponent* component;
  MockMediaStreamVideoSource* platform_source_ptr;
  std::tie(component, platform_source_ptr) =
      MakeMockDisplayVideoCaptureComponent();
  MediaStreamTrack* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), component);

  // Start the source.
  platform_source_ptr->StartMockedSource();
  // Get initial settings and verify that initial frame rate is not same as the
  // new constraint.
  int initialWidth = platform_source_ptr->max_requested_width();
  int initialHeight = platform_source_ptr->max_requested_height();
  float initialFrameRate = platform_source_ptr->max_requested_frame_rate();
  EXPECT_NE(initialFrameRate, kMaxFrameRate);
  // Apply new frame rate constraints.
  MediaTrackConstraints* track_constraints = MakeMediaTrackConstraints(
      std::nullopt, std::nullopt, kMinFrameRate, kMaxFrameRate);
  auto apply_constraints_promise =
      track->applyConstraints(v8_scope.GetScriptState(), track_constraints);

  ScriptPromiseTester tester(v8_scope.GetScriptState(),
                             apply_constraints_promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  // Verify updated settings and that the source was restarted.
  EXPECT_EQ(platform_source_ptr->restart_count(), 1);
  EXPECT_EQ(platform_source_ptr->max_requested_width(), initialWidth);
  EXPECT_EQ(platform_source_ptr->max_requested_height(), initialHeight);
  EXPECT_EQ(platform_source_ptr->max_requested_frame_rate(), kMaxFrameRate);
}

// TODO(crbug.com/436623747): Re-enable.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ApplyConstraintsResolutionDoesNotAffectFramerate \
  DISABLED_ApplyConstraintsResolutionDoesNotAffectFramerate
#else
#define MAYBE_ApplyConstraintsResolutionDoesNotAffectFramerate \
  ApplyConstraintsResolutionDoesNotAffectFramerate
#endif
TEST_F(MediaStreamTrackImplTest,
       MAYBE_ApplyConstraintsResolutionDoesNotAffectFramerate) {
  V8TestingScope v8_scope;
  MediaStreamComponent* component;
  MockMediaStreamVideoSource* platform_source_ptr;
  std::tie(component, platform_source_ptr) =
      MakeMockDisplayVideoCaptureComponent();
  MediaStreamTrack* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), component);

  // Start the source.
  platform_source_ptr->StartMockedSource();
  // Get initial settings and verify that the initial resolution is not the same
  // as the new constraint.
  int initialWidth = platform_source_ptr->max_requested_width();
  int initialHeight = platform_source_ptr->max_requested_height();
  float initialFrameRate = platform_source_ptr->max_requested_frame_rate();
  EXPECT_NE(initialWidth, kReducedWidth);
  EXPECT_NE(initialHeight, kReducedHeight);
  // Apply new frame rate constraints.
  MediaTrackConstraints* track_constraints = MakeMediaTrackConstraints(
      kReducedWidth, kReducedHeight, std::nullopt, std::nullopt);
  auto apply_constraints_promise =
      track->applyConstraints(v8_scope.GetScriptState(), track_constraints);

  ScriptPromiseTester tester(v8_scope.GetScriptState(),
                             apply_constraints_promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  // Verify updated settings and that the source was restarted.
  EXPECT_EQ(platform_source_ptr->restart_count(), 1);
  EXPECT_EQ(platform_source_ptr->max_requested_width(), kReducedWidth);
  EXPECT_EQ(platform_source_ptr->max_requested_height(), kReducedHeight);
  EXPECT_EQ(platform_source_ptr->max_requested_frame_rate(), initialFrameRate);
}

// TODO(crbug.com/436623747): Re-enable.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ApplyConstraintsWidthDoesNotAffectAspectRatio \
  DISABLED_ApplyConstraintsWidthDoesNotAffectAspectRatio
#else
#define MAYBE_ApplyConstraintsWidthDoesNotAffectAspectRatio \
  ApplyConstraintsWidthDoesNotAffectAspectRatio
#endif
TEST_F(MediaStreamTrackImplTest,
       MAYBE_ApplyConstraintsWidthDoesNotAffectAspectRatio) {
  V8TestingScope v8_scope;
  MediaStreamComponent* component;
  MockMediaStreamVideoSource* platform_source_ptr;
  std::tie(component, platform_source_ptr) =
      MakeMockDisplayVideoCaptureComponent();
  MediaStreamTrack* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), component);

  // Start the source.
  platform_source_ptr->StartMockedSource();
  // Get initial settings and verify that the initial resolution is not the same
  // as the new constraint.
  int initialWidth = platform_source_ptr->max_requested_width();
  int initialHeight = platform_source_ptr->max_requested_height();
  float initialFrameRate = platform_source_ptr->max_requested_frame_rate();
  EXPECT_NE(initialWidth, kReducedWidth);
  EXPECT_NE(initialHeight, kReducedHeight);
  // Apply new frame rate constraints.
  MediaTrackConstraints* track_constraints = MakeMediaTrackConstraints(
      kReducedWidth, std::nullopt, std::nullopt, std::nullopt);
  auto apply_constraints_promise =
      track->applyConstraints(v8_scope.GetScriptState(), track_constraints);

  ScriptPromiseTester tester(v8_scope.GetScriptState(),
                             apply_constraints_promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  // Verify updated settings and that the source was restarted.
  EXPECT_EQ(platform_source_ptr->restart_count(), 1);
  float aspect_ratio =
      static_cast<float>(initialWidth) / static_cast<float>(initialHeight);
  EXPECT_EQ(platform_source_ptr->max_requested_width(), kReducedWidth);
  EXPECT_EQ(platform_source_ptr->max_requested_height(),
            kReducedWidth / aspect_ratio);
  EXPECT_EQ(platform_source_ptr->max_requested_frame_rate(), initialFrameRate);
}

// TODO(crbug.com/436623747): Re-enable.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ApplyConstraintsWidthAndAspectRatio \
  DISABLED_ApplyConstraintsWidthAndAspectRatio
#else
#define MAYBE_ApplyConstraintsWidthAndAspectRatio \
  ApplyConstraintsWidthAndAspectRatio
#endif
TEST_F(MediaStreamTrackImplTest, MAYBE_ApplyConstraintsWidthAndAspectRatio) {
  V8TestingScope v8_scope;
  MediaStreamComponent* component;
  MockMediaStreamVideoSource* platform_source_ptr;
  std::tie(component, platform_source_ptr) =
      MakeMockDisplayVideoCaptureComponent();
  MediaStreamTrack* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), component);

  // Start the source.
  platform_source_ptr->StartMockedSource();
  // Get initial settings and verify that the initial resolution is not the same
  // as the new constraint.
  int initialWidth = platform_source_ptr->max_requested_width();
  int initialHeight = platform_source_ptr->max_requested_height();
  float initialFrameRate = platform_source_ptr->max_requested_frame_rate();
  EXPECT_NE(initialWidth, kReducedWidth);
  EXPECT_NE(initialHeight, kReducedHeight);
  // Apply new frame rate constraints.
  MediaTrackConstraints* track_constraints = MakeMediaTrackConstraints(
      kReducedWidth, std::nullopt, std::nullopt, std::nullopt, kAspectRatio);
  auto apply_constraints_promise =
      track->applyConstraints(v8_scope.GetScriptState(), track_constraints);

  ScriptPromiseTester tester(v8_scope.GetScriptState(),
                             apply_constraints_promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  // Verify updated settings and that the source was restarted.
  EXPECT_EQ(platform_source_ptr->restart_count(), 1);
  EXPECT_EQ(platform_source_ptr->max_requested_width(), kReducedWidth);
  EXPECT_EQ(platform_source_ptr->max_requested_height(),
            kReducedWidth / kAspectRatio);
  EXPECT_EQ(platform_source_ptr->max_requested_frame_rate(), initialFrameRate);
}

// cropTo() is not supported on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ApplyConstraintsDoesNotUpdateFormatForCroppedSources \
  DISABLED_ApplyConstraintsDoesNotUpdateFormatForCroppedSources
#else
#define MAYBE_ApplyConstraintsDoesNotUpdateFormatForCroppedSources \
  ApplyConstraintsDoesNotUpdateFormatForCroppedSources
#endif

TEST_F(MediaStreamTrackImplTest,
       MAYBE_ApplyConstraintsDoesNotUpdateFormatForCroppedSources) {
  V8TestingScope v8_scope;
  MediaStreamComponent* component;
  MockMediaStreamVideoSource* platform_source_ptr;
  std::tie(component, platform_source_ptr) =
      MakeMockDisplayVideoCaptureComponent();
  MediaStreamTrack* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), component);

  // Start the source.
  platform_source_ptr->StartMockedSource();
  // Get initial settings and verify that resolution and frame rate are
  // different than the new constraints.
  int initialWidth = platform_source_ptr->max_requested_width();
  int initialHeight = platform_source_ptr->max_requested_height();
  float initialFrameRate = platform_source_ptr->max_requested_frame_rate();
  EXPECT_NE(initialWidth, kReducedWidth);
  EXPECT_NE(initialHeight, kReducedHeight);
  EXPECT_NE(initialFrameRate, kMaxFrameRate);
  // Apply new constraints.
  MediaTrackConstraints* track_constraints = MakeMediaTrackConstraints(
      kReducedWidth, kReducedHeight, kMinFrameRate, kMaxFrameRate);
  EXPECT_CALL(*platform_source_ptr, GetCaptureVersion)
      .WillRepeatedly(testing::Return(
          media::CaptureVersion(/*source=*/0, /*sub_capture=*/1)));
  auto apply_constraints_promise =
      track->applyConstraints(v8_scope.GetScriptState(), track_constraints);

  ScriptPromiseTester tester(v8_scope.GetScriptState(),
                             apply_constraints_promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  // Verify that the settings are not updated and that the source was not
  // restarted.
  EXPECT_EQ(platform_source_ptr->restart_count(), 0);
  EXPECT_EQ(platform_source_ptr->max_requested_width(), initialWidth);
  EXPECT_EQ(platform_source_ptr->max_requested_height(), initialHeight);
  EXPECT_EQ(platform_source_ptr->max_requested_frame_rate(), initialFrameRate);
}

TEST_F(MediaStreamTrackImplTest, ApplyConstraintsWithUnchangedConstraints) {
  V8TestingScope v8_scope;
  MediaStreamComponent* component;
  MockMediaStreamVideoSource* platform_source_ptr;
  std::tie(component, platform_source_ptr) =
      MakeMockDisplayVideoCaptureComponent();
  MediaStreamTrack* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), component);

  // Start the source.
  platform_source_ptr->StartMockedSource();
  // Get initial settings
  int initialWidth = platform_source_ptr->max_requested_width();
  int initialHeight = platform_source_ptr->max_requested_height();
  float initialFrameRate = platform_source_ptr->max_requested_frame_rate();
  // Apply new constraints that are fulfilled by the current settings.
  MediaTrackConstraints* track_constraints = MakeMediaTrackConstraints(
      initialWidth, initialHeight, initialFrameRate, initialFrameRate);
  auto apply_constraints_promise =
      track->applyConstraints(v8_scope.GetScriptState(), track_constraints);

  ScriptPromiseTester tester(v8_scope.GetScriptState(),
                             apply_constraints_promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  // Verify that the settings are the same and that the source did not restart.
  EXPECT_EQ(platform_source_ptr->restart_count(), 0);
  EXPECT_EQ(platform_source_ptr->max_requested_width(), initialWidth);
  EXPECT_EQ(platform_source_ptr->max_requested_height(), initialHeight);
  EXPECT_EQ(platform_source_ptr->max_requested_frame_rate(), initialFrameRate);
}

TEST_F(MediaStreamTrackImplTest, ApplyConstraintsCannotRestartSource) {
  V8TestingScope v8_scope;
  MediaStreamComponent* component;
  MockMediaStreamVideoSource* platform_source_ptr;
  std::tie(component, platform_source_ptr) =
      MakeMockDisplayVideoCaptureComponent();
  MediaStreamTrack* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), component);

  // Start the source.
  platform_source_ptr->DisableStopForRestart();
  platform_source_ptr->StartMockedSource();
  // Get initial settings and verify that resolution and frame rate are
  // different than the new constraints.
  int initialWidth = platform_source_ptr->max_requested_width();
  int initialHeight = platform_source_ptr->max_requested_height();
  float initialFrameRate = platform_source_ptr->max_requested_frame_rate();
  EXPECT_NE(initialWidth, kReducedWidth);
  EXPECT_NE(initialHeight, kReducedHeight);
  EXPECT_NE(initialFrameRate, kMaxFrameRate);
  // Apply new constraints.
  MediaTrackConstraints* track_constraints = MakeMediaTrackConstraints(
      kReducedWidth, kReducedHeight, kMinFrameRate, kMaxFrameRate);
  auto apply_constraints_promise =
      track->applyConstraints(v8_scope.GetScriptState(), track_constraints);

  ScriptPromiseTester tester(v8_scope.GetScriptState(),
                             apply_constraints_promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  // Verify that the settings are not updated and that the source was not
  // restarted.
  EXPECT_EQ(platform_source_ptr->restart_count(), 0);
  EXPECT_EQ(platform_source_ptr->max_requested_width(), initialWidth);
  EXPECT_EQ(platform_source_ptr->max_requested_height(), initialHeight);
  EXPECT_EQ(platform_source_ptr->max_requested_frame_rate(), initialFrameRate);
}

TEST_F(MediaStreamTrackImplTest, ApplyConstraintsUpdatesMinFps) {
  V8TestingScope v8_scope;
  MediaStreamComponent* component;
  MockMediaStreamVideoSource* platform_source_ptr;
  std::tie(component, platform_source_ptr) =
      MakeMockDisplayVideoCaptureComponent();
  MediaStreamTrack* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), component);
  MediaStreamVideoTrack* video_track = MediaStreamVideoTrack::From(component);

  // Start the source.
  platform_source_ptr->StartMockedSource();
  // Get initial settings and verify that resolution and frame rate are
  // different than the new constraints.
  int initialWidth = platform_source_ptr->max_requested_width();
  int initialHeight = platform_source_ptr->max_requested_height();
  float initialFrameRate = platform_source_ptr->max_requested_frame_rate();
  // Min frame rate not set.
  EXPECT_FALSE(video_track->min_frame_rate());

  // Apply new constraints.
  MediaTrackConstraints* track_constraints = MakeMediaTrackConstraints(
      std::nullopt, std::nullopt, kMinFrameRate, initialFrameRate);
  auto apply_constraints_promise =
      track->applyConstraints(v8_scope.GetScriptState(), track_constraints);
  ScriptPromiseTester tester(v8_scope.GetScriptState(),
                             apply_constraints_promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  // Verify that min frame rate is updated even though max frame rate was not
  // changed. The source does not need to restart.
  EXPECT_EQ(platform_source_ptr->restart_count(), 0);
  EXPECT_EQ(platform_source_ptr->max_requested_width(), initialWidth);
  EXPECT_EQ(platform_source_ptr->max_requested_height(), initialHeight);
  EXPECT_EQ(platform_source_ptr->max_requested_frame_rate(), initialFrameRate);
  EXPECT_EQ(video_track->min_frame_rate(), kMinFrameRate);
}

TEST_F(MediaStreamTrackImplTest, StopAudioTrackAfterSinkDestroyed) {
  V8TestingScope v8_scope;

  // 1. Create the underlying platform track and its component.
  // Keep this component alive with a Persistent handle to control its
  // lifetime, ensuring it outlives the temporary track and sink created below.
  Persistent<MediaStreamComponent> component = MakeMockAudioComponent();
  MediaStreamAudioSource* source =
      MediaStreamAudioSource::From(component->Source());

  MediaStreamAudioTrack* platform_track =
      MediaStreamAudioTrack::From(component.Get());
  ASSERT_TRUE(source);
  ASSERT_TRUE(platform_track);

  // 2. Start the platform track by connecting it to the source. After this, the
  // track is "live" and can accept sinks.
  source->ConnectToInitializedTrack(component.Get());

  // 3. Create a temporary MediaStreamTrackImpl wrapper and a sink in a
  // separate scope. This wrapper will "own" the sink via a strong GC ref.
  {
    MediaStreamTrack* track1 = MakeGarbageCollected<MediaStreamTrackImpl>(
        v8_scope.GetExecutionContext(), component.Get());
    auto* sink = MakeGarbageCollected<SpeechRecognitionMediaStreamAudioSink>(
        v8_scope.GetExecutionContext(), base::DoNothing());

    // 4. Register the sink with the wrapper and add its raw pointer to the
    // platform track's sink list.
    track1->RegisterSink(sink);
    platform_track->AddSink(sink);
  }

  // 5. Force garbage collection. This destroys `track_with_sink` and `sink`.
  // If the fix is present, `track_with_sink->Dispose()` is called, removing
  // the sink from `platform_track`.
  WebHeap::CollectAllGarbageForTesting();

  // 5. Now, destroy the component that owns the platform track by clearing the
  // persistent handle and running GC again. The component's pre-finalizer,
  // Dispose(), will call `platform_track->StopAndNotify()`.
  component.Clear();
  WebHeap::CollectAllGarbageForTesting();

  // The test passes if it doesn't crash.
}

}  // namespace blink
