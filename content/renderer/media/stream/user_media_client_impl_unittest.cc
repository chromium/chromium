// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/stream/user_media_client_impl.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "content/child/child_process.h"
#include "content/common/media/media_devices.h"
#include "content/renderer/media/stream/media_stream_audio_processor_options.h"
#include "content/renderer/media/stream/media_stream_audio_source.h"
#include "content/renderer/media/stream/media_stream_audio_track.h"
#include "content/renderer/media/stream/media_stream_constraints_util.h"
#include "content/renderer/media/stream/media_stream_constraints_util_video_content.h"
#include "content/renderer/media/stream/media_stream_device_observer.h"
#include "content/renderer/media/stream/media_stream_track.h"
#include "content/renderer/media/stream/mock_constraint_factory.h"
#include "content/renderer/media/stream/mock_media_stream_video_source.h"
#include "content/renderer/media/stream/mock_mojo_media_stream_dispatcher_host.h"
#include "content/renderer/media/webrtc/mock_peer_connection_dependency_factory.h"
#include "media/audio/audio_device_description.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_heap.h"

using testing::_;

namespace content {

using EchoCancellationType = AudioProcessingProperties::EchoCancellationType;

namespace {

blink::WebMediaConstraints CreateDefaultConstraints() {
  MockConstraintFactory factory;
  factory.AddAdvanced();
  return factory.CreateWebMediaConstraints();
}

blink::WebMediaConstraints CreateDeviceConstraints(
    const char* basic_exact_value,
    const char* basic_ideal_value = nullptr,
    const char* advanced_exact_value = nullptr) {
  MockConstraintFactory factory;
  if (basic_exact_value) {
    factory.basic().device_id.SetExact(
        blink::WebString::FromUTF8(basic_exact_value));
  }
  if (basic_ideal_value) {
    blink::WebString value = blink::WebString::FromUTF8(basic_ideal_value);
    factory.basic().device_id.SetIdeal(
        blink::WebVector<blink::WebString>(&value, 1));
  }

  auto& advanced = factory.AddAdvanced();
  if (advanced_exact_value) {
    blink::WebString value = blink::WebString::FromUTF8(advanced_exact_value);
    advanced.device_id.SetExact(value);
  }

  return factory.CreateWebMediaConstraints();
}

blink::WebMediaConstraints CreateFacingModeConstraints(
    const char* basic_exact_value,
    const char* basic_ideal_value = nullptr,
    const char* advanced_exact_value = nullptr) {
  MockConstraintFactory factory;
  if (basic_exact_value) {
    factory.basic().facing_mode.SetExact(
        blink::WebString::FromUTF8(basic_exact_value));
  }
  if (basic_ideal_value) {
    blink::WebString value = blink::WebString::FromUTF8(basic_ideal_value);
    factory.basic().device_id.SetIdeal(
        blink::WebVector<blink::WebString>(&value, 1));
  }

  auto& advanced = factory.AddAdvanced();
  if (advanced_exact_value) {
    blink::WebString value = blink::WebString::FromUTF8(advanced_exact_value);
    advanced.device_id.SetExact(value);
  }

  return factory.CreateWebMediaConstraints();
}

void CheckVideoSource(MediaStreamVideoSource* source,
                      int expected_source_width,
                      int expected_source_height,
                      double expected_source_frame_rate) {
  EXPECT_TRUE(source->IsRunning());
  EXPECT_TRUE(source->GetCurrentFormat().has_value());
  media::VideoCaptureFormat format = *source->GetCurrentFormat();
  EXPECT_EQ(format.frame_size.width(), expected_source_width);
  EXPECT_EQ(format.frame_size.height(), expected_source_height);
  EXPECT_EQ(format.frame_rate, expected_source_frame_rate);
}

void CheckVideoSourceAndTrack(MediaStreamVideoSource* source,
                              int expected_source_width,
                              int expected_source_height,
                              double expected_source_frame_rate,
                              const blink::WebMediaStreamTrack& web_track,
                              int expected_track_width,
                              int expected_track_height,
                              double expected_track_frame_rate) {
  CheckVideoSource(source, expected_source_width, expected_source_height,
                   expected_source_frame_rate);
  EXPECT_EQ(web_track.Source().GetReadyState(),
            blink::WebMediaStreamSource::kReadyStateLive);
  MediaStreamVideoTrack* track =
      MediaStreamVideoTrack::GetVideoTrack(web_track);
  EXPECT_EQ(track->source(), source);

  blink::WebMediaStreamTrack::Settings settings;
  track->GetSettings(settings);
  EXPECT_EQ(settings.width, expected_track_width);
  EXPECT_EQ(settings.height, expected_track_height);
  EXPECT_EQ(settings.frame_rate, expected_track_frame_rate);
}

class MockMediaStreamVideoCapturerSource : public MockMediaStreamVideoSource {
 public:
  MockMediaStreamVideoCapturerSource(const MediaStreamDevice& device,
                                     const SourceStoppedCallback& stop_callback,
                                     PeerConnectionDependencyFactory* factory)
      : MockMediaStreamVideoSource() {
    SetDevice(device);
    SetStopCallback(stop_callback);
  }
};

const char kInvalidDeviceId[] = "invalid";
const char kFakeAudioInputDeviceId1[] = "fake_audio_input 1";
const char kFakeAudioInputDeviceId2[] = "fake_audio_input 2";
const char kFakeVideoInputDeviceId1[] = "fake_video_input 1";
const char kFakeVideoInputDeviceId2[] = "fake_video_input 2";

class MockMediaDevicesDispatcherHost
    : public blink::mojom::MediaDevicesDispatcherHost {
 public:
  MockMediaDevicesDispatcherHost() {}
  void EnumerateDevices(bool request_audio_input,
                        bool request_video_input,
                        bool request_audio_output,
                        bool request_video_input_capabilities,
                        EnumerateDevicesCallback callback) override {
    NOTREACHED();
  }

  void GetVideoInputCapabilities(
      GetVideoInputCapabilitiesCallback client_callback) override {
    blink::mojom::VideoInputDeviceCapabilitiesPtr device =
        blink::mojom::VideoInputDeviceCapabilities::New();
    device->device_id = kFakeVideoInputDeviceId1;
    device->facing_mode = media::MEDIA_VIDEO_FACING_USER;
    if (!video_source_ || !video_source_->IsRunning() ||
        !video_source_->GetCurrentFormat()) {
      device->formats.push_back(media::VideoCaptureFormat(
          gfx::Size(640, 480), 30.0f, media::PIXEL_FORMAT_I420));
      device->formats.push_back(media::VideoCaptureFormat(
          gfx::Size(800, 600), 30.0f, media::PIXEL_FORMAT_I420));
      device->formats.push_back(media::VideoCaptureFormat(
          gfx::Size(1024, 768), 20.0f, media::PIXEL_FORMAT_I420));
    } else {
      device->formats.push_back(*video_source_->GetCurrentFormat());
    }
    std::vector<blink::mojom::VideoInputDeviceCapabilitiesPtr> result;
    result.push_back(std::move(device));

    device = blink::mojom::VideoInputDeviceCapabilities::New();
    device->device_id = kFakeVideoInputDeviceId2;
    device->facing_mode = media::MEDIA_VIDEO_FACING_ENVIRONMENT;
    device->formats.push_back(media::VideoCaptureFormat(
        gfx::Size(640, 480), 30.0f, media::PIXEL_FORMAT_I420));
    result.push_back(std::move(device));

    std::move(client_callback).Run(std::move(result));
  }

  void GetAudioInputCapabilities(
      GetAudioInputCapabilitiesCallback client_callback) override {
    std::vector<blink::mojom::AudioInputDeviceCapabilitiesPtr> result;
    blink::mojom::AudioInputDeviceCapabilitiesPtr device =
        blink::mojom::AudioInputDeviceCapabilities::New();
    device->device_id = media::AudioDeviceDescription::kDefaultDeviceId;
    device->parameters = audio_parameters_;
    result.push_back(std::move(device));

    device = blink::mojom::AudioInputDeviceCapabilities::New();
    device->device_id = kFakeAudioInputDeviceId1;
    device->parameters = audio_parameters_;
    result.push_back(std::move(device));

    device = blink::mojom::AudioInputDeviceCapabilities::New();
    device->device_id = kFakeAudioInputDeviceId2;
    device->parameters = audio_parameters_;
    result.push_back(std::move(device));

    std::move(client_callback).Run(std::move(result));
  }

  media::AudioParameters& AudioParameters() { return audio_parameters_; }

  void ResetAudioParameters() {
    audio_parameters_ = media::AudioParameters::UnavailableDeviceParams();
  }

  void AddMediaDevicesListener(
      bool subscribe_audio_input,
      bool subscribe_video_input,
      bool subscribe_audio_output,
      blink::mojom::MediaDevicesListenerPtr listener) override {
    NOTREACHED();
  }

  void GetAllVideoInputDeviceFormats(
      const std::string&,
      GetAllVideoInputDeviceFormatsCallback callback) override {
    media::VideoCaptureFormats formats;
    formats.push_back(media::VideoCaptureFormat(gfx::Size(640, 480), 30.0f,
                                                media::PIXEL_FORMAT_I420));
    formats.push_back(media::VideoCaptureFormat(gfx::Size(800, 600), 30.0f,
                                                media::PIXEL_FORMAT_I420));
    formats.push_back(media::VideoCaptureFormat(gfx::Size(1024, 768), 20.0f,
                                                media::PIXEL_FORMAT_I420));
    std::move(callback).Run(formats);
  }

  void GetAvailableVideoInputDeviceFormats(
      const std::string& device_id,
      GetAvailableVideoInputDeviceFormatsCallback callback) override {
    if (!video_source_ || !video_source_->IsRunning() ||
        !video_source_->GetCurrentFormat()) {
      GetAllVideoInputDeviceFormats(device_id, std::move(callback));
      return;
    }

    media::VideoCaptureFormats formats;
    formats.push_back(*video_source_->GetCurrentFormat());
    std::move(callback).Run(formats);
  }

  void SetVideoSource(MediaStreamVideoSource* video_source) {
    video_source_ = video_source;
  }

 private:
  media::AudioParameters audio_parameters_ =
      media::AudioParameters::UnavailableDeviceParams();
  MediaStreamVideoSource* video_source_ = nullptr;
};

enum RequestState {
  REQUEST_NOT_STARTED,
  REQUEST_NOT_COMPLETE,
  REQUEST_SUCCEEDED,
  REQUEST_FAILED,
};

class UserMediaProcessorUnderTest : public UserMediaProcessor {
 public:
  UserMediaProcessorUnderTest(
      PeerConnectionDependencyFactory* dependency_factory,
      std::unique_ptr<MediaStreamDeviceObserver> media_stream_device_observer,
      blink::mojom::MediaDevicesDispatcherHostPtr media_devices_dispatcher,
      RequestState* state)
      : UserMediaProcessor(
            nullptr,
            dependency_factory,
            std::move(media_stream_device_observer),
            base::BindRepeating(
                &UserMediaProcessorUnderTest::media_devices_dispatcher,
                base::Unretained(this))),
        factory_(dependency_factory),
        media_devices_dispatcher_(std::move(media_devices_dispatcher)),
        state_(state) {}

  const blink::mojom::MediaDevicesDispatcherHostPtr& media_devices_dispatcher()
      const {
    return media_devices_dispatcher_;
  }

  MockMediaStreamVideoCapturerSource* last_created_video_source() const {
    return video_source_;
  }
  void SetCreateSourceThatFails(bool should_fail) {
    create_source_that_fails_ = should_fail;
  }

  const blink::WebMediaStream& last_generated_stream() {
    return last_generated_stream_;
  }
  void ClearLastGeneratedStream() { last_generated_stream_.Reset(); }

  AudioCaptureSettings AudioSettings() const {
    return AudioCaptureSettingsForTesting();
  }
  VideoCaptureSettings VideoSettings() const {
    return VideoCaptureSettingsForTesting();
  }

  content::MediaStreamRequestResult error_reason() const { return result_; }
  blink::WebString constraint_name() const { return constraint_name_; }

  // UserMediaProcessor overrides.
  MediaStreamVideoSource* CreateVideoSource(
      const MediaStreamDevice& device,
      const MediaStreamSource::SourceStoppedCallback& stop_callback) override {
    video_source_ =
        new MockMediaStreamVideoCapturerSource(device, stop_callback, factory_);
    return video_source_;
  }

  MediaStreamAudioSource* CreateAudioSource(
      const MediaStreamDevice& device,
      const MediaStreamSource::ConstraintsCallback& source_ready) override {
    MediaStreamAudioSource* source;
    if (create_source_that_fails_) {
      class FailedAtLifeAudioSource : public MediaStreamAudioSource {
       public:
        FailedAtLifeAudioSource() : MediaStreamAudioSource(true) {}
        ~FailedAtLifeAudioSource() override {}

       protected:
        bool EnsureSourceIsStarted() override { return false; }
      };
      source = new FailedAtLifeAudioSource();
    } else {
      source = new MediaStreamAudioSource(true);
    }

    source->SetDevice(device);

    if (!create_source_that_fails_) {
      // RunUntilIdle is required for this task to complete.
      blink::scheduler::GetSingleThreadTaskRunnerForTesting()->PostTask(
          FROM_HERE,
          base::BindOnce(&UserMediaProcessorUnderTest::SignalSourceReady,
                         source_ready, source));
    }

    return source;
  }

  void GetUserMediaRequestSucceeded(
      const blink::WebMediaStream& stream,
      blink::WebUserMediaRequest request_info) override {
    last_generated_stream_ = stream;
    *state_ = REQUEST_SUCCEEDED;
  }

  void GetUserMediaRequestFailed(
      content::MediaStreamRequestResult result,
      const blink::WebString& constraint_name) override {
    last_generated_stream_.Reset();
    *state_ = REQUEST_FAILED;
    result_ = result;
    constraint_name_ = constraint_name;
  }

 private:
  static void SignalSourceReady(
      const MediaStreamSource::ConstraintsCallback& source_ready,
      MediaStreamSource* source) {
    source_ready.Run(source, MEDIA_DEVICE_OK, "");
  }

  PeerConnectionDependencyFactory* factory_;
  blink::mojom::MediaDevicesDispatcherHostPtr media_devices_dispatcher_;
  MockMediaStreamVideoCapturerSource* video_source_ = nullptr;
  bool create_source_that_fails_ = false;
  blink::WebMediaStream last_generated_stream_;
  content::MediaStreamRequestResult result_ = NUM_MEDIA_REQUEST_RESULTS;
  blink::WebString constraint_name_;
  RequestState* state_;
};

class UserMediaClientImplUnderTest : public UserMediaClientImpl {
 public:
  UserMediaClientImplUnderTest(UserMediaProcessor* user_media_processor,
                               RequestState* state)
      : UserMediaClientImpl(
            nullptr,
            base::WrapUnique(user_media_processor),
            blink::scheduler::GetSingleThreadTaskRunnerForTesting()),
        state_(state) {}

  void RequestUserMediaForTest(
      const blink::WebUserMediaRequest& user_media_request) {
    *state_ = REQUEST_NOT_COMPLETE;
    RequestUserMedia(user_media_request);
    base::RunLoop().RunUntilIdle();
  }

  void RequestUserMediaForTest() {
    blink::WebUserMediaRequest user_media_request =
        blink::WebUserMediaRequest::CreateForTesting(
            CreateDefaultConstraints(), CreateDefaultConstraints());
    RequestUserMediaForTest(user_media_request);
  }

 private:
  RequestState* state_;
};

}  // namespace

class UserMediaClientImplTest : public ::testing::Test {
 public:
  UserMediaClientImplTest()
      : binding_user_media_processor_(&media_devices_dispatcher_),
        binding_user_media_client_(&media_devices_dispatcher_) {}

  void SetUp() override {
    // Create our test object.
    dependency_factory_.reset(new MockPeerConnectionDependencyFactory());

    msd_observer_ = new MediaStreamDeviceObserver(nullptr);

    blink::mojom::MediaDevicesDispatcherHostPtr user_media_processor_host_proxy;
    binding_user_media_processor_.Bind(
        mojo::MakeRequest(&user_media_processor_host_proxy));
    user_media_processor_ = new UserMediaProcessorUnderTest(
        dependency_factory_.get(), base::WrapUnique(msd_observer_),
        std::move(user_media_processor_host_proxy), &state_);
    mojom::MediaStreamDispatcherHostPtr dispatcher_host =
        mock_dispatcher_host_.CreateInterfacePtrAndBind();
    user_media_processor_->set_media_stream_dispatcher_host_for_testing(
        std::move(dispatcher_host));

    user_media_client_impl_ = std::make_unique<UserMediaClientImplUnderTest>(
        user_media_processor_, &state_);
    blink::mojom::MediaDevicesDispatcherHostPtr user_media_client_host_proxy;
    binding_user_media_client_.Bind(
        mojo::MakeRequest(&user_media_client_host_proxy));
    user_media_client_impl_->SetMediaDevicesDispatcherForTesting(
        std::move(user_media_client_host_proxy));
  }

  void TearDown() override {
    user_media_client_impl_.reset();
    blink::WebHeap::CollectAllGarbageForTesting();
  }

  void LoadNewDocumentInFrame() {
    user_media_client_impl_->WillCommitProvisionalLoad();
    base::RunLoop().RunUntilIdle();
  }

  blink::WebMediaStream RequestLocalMediaStream() {
    user_media_client_impl_->RequestUserMediaForTest();
    StartMockedVideoSource();

    EXPECT_EQ(REQUEST_SUCCEEDED, request_state());

    blink::WebMediaStream desc = user_media_processor_->last_generated_stream();
    blink::WebVector<blink::WebMediaStreamTrack> audio_tracks =
        desc.AudioTracks();
    blink::WebVector<blink::WebMediaStreamTrack> video_tracks =
        desc.VideoTracks();

    EXPECT_EQ(1u, audio_tracks.size());
    EXPECT_EQ(1u, video_tracks.size());
    EXPECT_NE(audio_tracks[0].Id(), video_tracks[0].Id());
    return desc;
  }

  blink::WebMediaStreamTrack RequestLocalVideoTrack() {
    blink::WebUserMediaRequest user_media_request =
        blink::WebUserMediaRequest::CreateForTesting(
            blink::WebMediaConstraints(), CreateDefaultConstraints());
    user_media_client_impl_->RequestUserMediaForTest(user_media_request);
    StartMockedVideoSource();
    EXPECT_EQ(REQUEST_SUCCEEDED, request_state());

    blink::WebMediaStream web_stream =
        user_media_processor_->last_generated_stream();
    blink::WebVector<blink::WebMediaStreamTrack> audio_tracks =
        web_stream.AudioTracks();
    blink::WebVector<blink::WebMediaStreamTrack> video_tracks =
        web_stream.VideoTracks();

    EXPECT_EQ(audio_tracks.size(), 0U);
    EXPECT_EQ(video_tracks.size(), 1U);

    return video_tracks[0];
  }

  blink::WebMediaStreamTrack RequestLocalAudioTrackWithAssociatedSink(
      bool render_to_associated_sink) {
    MockConstraintFactory constraint_factory;
    constraint_factory.basic().render_to_associated_sink.SetExact(
        render_to_associated_sink);
    blink::WebUserMediaRequest user_media_request =
        blink::WebUserMediaRequest::CreateForTesting(
            constraint_factory.CreateWebMediaConstraints(),
            blink::WebMediaConstraints());
    user_media_client_impl_->RequestUserMediaForTest(user_media_request);

    EXPECT_EQ(REQUEST_SUCCEEDED, request_state());

    blink::WebMediaStream desc = user_media_processor_->last_generated_stream();
    blink::WebVector<blink::WebMediaStreamTrack> audio_tracks =
        desc.AudioTracks();
    blink::WebVector<blink::WebMediaStreamTrack> video_tracks =
        desc.VideoTracks();

    EXPECT_EQ(audio_tracks.size(), 1u);
    EXPECT_TRUE(video_tracks.empty());

    return audio_tracks[0];
  }

  void StartMockedVideoSource() {
    MockMediaStreamVideoCapturerSource* video_source =
        user_media_processor_->last_created_video_source();
    if (video_source->SourceHasAttemptedToStart())
      video_source->StartMockedSource();
  }

  void FailToStartMockedVideoSource() {
    MockMediaStreamVideoCapturerSource* video_source =
        user_media_processor_->last_created_video_source();
    if (video_source->SourceHasAttemptedToStart())
      video_source->FailToStartMockedSource();
    blink::WebHeap::CollectGarbageForTesting();
  }

  void TestValidRequestWithConstraints(
      const blink::WebMediaConstraints& audio_constraints,
      const blink::WebMediaConstraints& video_constraints,
      const std::string& expected_audio_device_id,
      const std::string& expected_video_device_id) {
    DCHECK(!audio_constraints.IsNull());
    DCHECK(!video_constraints.IsNull());
    blink::WebUserMediaRequest request =
        blink::WebUserMediaRequest::CreateForTesting(audio_constraints,
                                                     video_constraints);
    user_media_client_impl_->RequestUserMediaForTest(request);
    StartMockedVideoSource();

    EXPECT_EQ(REQUEST_SUCCEEDED, request_state());
    EXPECT_EQ(1U, mock_dispatcher_host_.audio_devices().size());
    EXPECT_EQ(1U, mock_dispatcher_host_.video_devices().size());
    // MockMojoMediaStreamDispatcherHost appends the session ID to its internal
    // device IDs.
    EXPECT_EQ(std::string(expected_audio_device_id) + "0",
              mock_dispatcher_host_.audio_devices()[0].id);
    EXPECT_EQ(std::string(expected_video_device_id) + "0",
              mock_dispatcher_host_.video_devices()[0].id);
  }

  void ApplyConstraintsVideoMode(
      const blink::WebMediaStreamTrack& web_track,
      int width,
      int height,
      const base::Optional<double>& frame_rate = base::Optional<double>()) {
    MockConstraintFactory factory;
    factory.basic().width.SetExact(width);
    factory.basic().height.SetExact(height);
    if (frame_rate)
      factory.basic().frame_rate.SetExact(*frame_rate);

    blink::WebApplyConstraintsRequest apply_constraints_request =
        blink::WebApplyConstraintsRequest::CreateForTesting(
            web_track, factory.CreateWebMediaConstraints());
    user_media_client_impl_->ApplyConstraints(apply_constraints_request);
    base::RunLoop().RunUntilIdle();
  }

  RequestState request_state() const { return state_; }

 protected:
  // The ScopedTaskEnvironment prevents the ChildProcess from leaking a
  // TaskScheduler.
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  ChildProcess child_process_;
  MediaStreamDeviceObserver* msd_observer_ =
      nullptr;  // Owned by |used_media_processor_|.
  MockMojoMediaStreamDispatcherHost mock_dispatcher_host_;
  MockMediaDevicesDispatcherHost media_devices_dispatcher_;
  mojo::Binding<blink::mojom::MediaDevicesDispatcherHost>
      binding_user_media_processor_;
  mojo::Binding<blink::mojom::MediaDevicesDispatcherHost>
      binding_user_media_client_;

  UserMediaProcessorUnderTest* user_media_processor_ =
      nullptr;  // Owned by |user_media_client_impl_|
  std::unique_ptr<UserMediaClientImplUnderTest> user_media_client_impl_;
  std::unique_ptr<MockPeerConnectionDependencyFactory> dependency_factory_;
  RequestState state_ = REQUEST_NOT_STARTED;
};

TEST_F(UserMediaClientImplTest, GenerateMediaStream) {
  // Generate a stream with both audio and video.
  blink::WebMediaStream mixed_desc = RequestLocalMediaStream();
}

// Test that the same source object is used if two MediaStreams are generated
// using the same source.
TEST_F(UserMediaClientImplTest, GenerateTwoMediaStreamsWithSameSource) {
  blink::WebMediaStream desc1 = RequestLocalMediaStream();
  blink::WebMediaStream desc2 = RequestLocalMediaStream();

  blink::WebVector<blink::WebMediaStreamTrack> desc1_video_tracks =
      desc1.VideoTracks();
  blink::WebVector<blink::WebMediaStreamTrack> desc2_video_tracks =
      desc2.VideoTracks();
  EXPECT_EQ(desc1_video_tracks[0].Source().Id(),
            desc2_video_tracks[0].Source().Id());

  EXPECT_EQ(desc1_video_tracks[0].Source().GetExtraData(),
            desc2_video_tracks[0].Source().GetExtraData());

  blink::WebVector<blink::WebMediaStreamTrack> desc1_audio_tracks =
      desc1.AudioTracks();
  blink::WebVector<blink::WebMediaStreamTrack> desc2_audio_tracks =
      desc2.AudioTracks();
  EXPECT_EQ(desc1_audio_tracks[0].Source().Id(),
            desc2_audio_tracks[0].Source().Id());

  EXPECT_EQ(MediaStreamAudioSource::From(desc1_audio_tracks[0].Source()),
            MediaStreamAudioSource::From(desc2_audio_tracks[0].Source()));
}

// Test that the same source object is not used if two MediaStreams are
// generated using different sources.
TEST_F(UserMediaClientImplTest, GenerateTwoMediaStreamsWithDifferentSources) {
  blink::WebMediaStream desc1 = RequestLocalMediaStream();
  // Make sure another device is selected (another |session_id|) in  the next
  // gUM request.
  mock_dispatcher_host_.IncrementSessionId();
  blink::WebMediaStream desc2 = RequestLocalMediaStream();

  blink::WebVector<blink::WebMediaStreamTrack> desc1_video_tracks =
      desc1.VideoTracks();
  blink::WebVector<blink::WebMediaStreamTrack> desc2_video_tracks =
      desc2.VideoTracks();
  EXPECT_NE(desc1_video_tracks[0].Source().Id(),
            desc2_video_tracks[0].Source().Id());

  EXPECT_NE(desc1_video_tracks[0].Source().GetExtraData(),
            desc2_video_tracks[0].Source().GetExtraData());

  blink::WebVector<blink::WebMediaStreamTrack> desc1_audio_tracks =
      desc1.AudioTracks();
  blink::WebVector<blink::WebMediaStreamTrack> desc2_audio_tracks =
      desc2.AudioTracks();
  EXPECT_NE(desc1_audio_tracks[0].Source().Id(),
            desc2_audio_tracks[0].Source().Id());

  EXPECT_NE(MediaStreamAudioSource::From(desc1_audio_tracks[0].Source()),
            MediaStreamAudioSource::From(desc2_audio_tracks[0].Source()));
}

TEST_F(UserMediaClientImplTest, StopLocalTracks) {
  // Generate a stream with both audio and video.
  blink::WebMediaStream mixed_desc = RequestLocalMediaStream();

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks =
      mixed_desc.AudioTracks();
  MediaStreamTrack* audio_track = MediaStreamTrack::GetTrack(audio_tracks[0]);
  audio_track->Stop();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_dispatcher_host_.stop_audio_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks =
      mixed_desc.VideoTracks();
  MediaStreamTrack* video_track = MediaStreamTrack::GetTrack(video_tracks[0]);
  video_track->Stop();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_dispatcher_host_.stop_video_device_counter());
}

// This test that a source is not stopped even if the tracks in a
// MediaStream is stopped if there are two MediaStreams with tracks using the
// same device. The source is stopped
// if there are no more MediaStream tracks using the device.
TEST_F(UserMediaClientImplTest, StopLocalTracksWhenTwoStreamUseSameDevices) {
  // Generate a stream with both audio and video.
  blink::WebMediaStream desc1 = RequestLocalMediaStream();
  blink::WebMediaStream desc2 = RequestLocalMediaStream();

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks1 =
      desc1.AudioTracks();
  MediaStreamTrack* audio_track1 = MediaStreamTrack::GetTrack(audio_tracks1[0]);
  audio_track1->Stop();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, mock_dispatcher_host_.stop_audio_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks2 =
      desc2.AudioTracks();
  MediaStreamTrack* audio_track2 = MediaStreamTrack::GetTrack(audio_tracks2[0]);
  audio_track2->Stop();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_dispatcher_host_.stop_audio_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks1 =
      desc1.VideoTracks();
  MediaStreamTrack* video_track1 = MediaStreamTrack::GetTrack(video_tracks1[0]);
  video_track1->Stop();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, mock_dispatcher_host_.stop_video_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks2 =
      desc2.VideoTracks();
  MediaStreamTrack* video_track2 = MediaStreamTrack::GetTrack(video_tracks2[0]);
  video_track2->Stop();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_dispatcher_host_.stop_video_device_counter());
}

TEST_F(UserMediaClientImplTest, StopSourceWhenMediaStreamGoesOutOfScope) {
  // Generate a stream with both audio and video.
  RequestLocalMediaStream();
  // Makes sure the test itself don't hold a reference to the created
  // MediaStream.
  user_media_processor_->ClearLastGeneratedStream();
  blink::WebHeap::CollectAllGarbageForTesting();
  base::RunLoop().RunUntilIdle();

  // Expect the sources to be stopped when the MediaStream goes out of scope.
  EXPECT_EQ(1, mock_dispatcher_host_.stop_audio_device_counter());
  EXPECT_EQ(1, mock_dispatcher_host_.stop_video_device_counter());
}

// Test that the MediaStreams are deleted if a new document is loaded in the
// frame.
TEST_F(UserMediaClientImplTest, LoadNewDocumentInFrame) {
  // Test a stream with both audio and video.
  blink::WebMediaStream mixed_desc = RequestLocalMediaStream();
  blink::WebMediaStream desc2 = RequestLocalMediaStream();
  LoadNewDocumentInFrame();
  blink::WebHeap::CollectAllGarbageForTesting();
  EXPECT_EQ(1, mock_dispatcher_host_.stop_audio_device_counter());
  EXPECT_EQ(1, mock_dispatcher_host_.stop_video_device_counter());
}

// This test what happens if a video source to a MediaSteam fails to start.
TEST_F(UserMediaClientImplTest, MediaVideoSourceFailToStart) {
  user_media_client_impl_->RequestUserMediaForTest();
  FailToStartMockedVideoSource();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(REQUEST_FAILED, request_state());
  EXPECT_EQ(MEDIA_DEVICE_TRACK_START_FAILURE_VIDEO,
            user_media_processor_->error_reason());
  blink::WebHeap::CollectAllGarbageForTesting();
  EXPECT_EQ(1, mock_dispatcher_host_.request_stream_counter());
  EXPECT_EQ(1, mock_dispatcher_host_.stop_audio_device_counter());
  EXPECT_EQ(1, mock_dispatcher_host_.stop_video_device_counter());
}

// This test what happens if an audio source fail to initialize.
TEST_F(UserMediaClientImplTest, MediaAudioSourceFailToInitialize) {
  user_media_processor_->SetCreateSourceThatFails(true);
  user_media_client_impl_->RequestUserMediaForTest();
  StartMockedVideoSource();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(REQUEST_FAILED, request_state());
  EXPECT_EQ(MEDIA_DEVICE_TRACK_START_FAILURE_AUDIO,
            user_media_processor_->error_reason());
  blink::WebHeap::CollectAllGarbageForTesting();
  EXPECT_EQ(1, mock_dispatcher_host_.request_stream_counter());
  EXPECT_EQ(1, mock_dispatcher_host_.stop_audio_device_counter());
  EXPECT_EQ(1, mock_dispatcher_host_.stop_video_device_counter());
}

// This test what happens if UserMediaClientImpl is deleted before a source has
// started.
TEST_F(UserMediaClientImplTest, MediaStreamImplShutDown) {
  user_media_client_impl_->RequestUserMediaForTest();
  EXPECT_EQ(1, mock_dispatcher_host_.request_stream_counter());
  EXPECT_EQ(REQUEST_NOT_COMPLETE, request_state());
  user_media_client_impl_.reset();
}

// This test what happens if a new document is loaded in the frame while the
// MediaStream is being generated by the MediaStreamDeviceObserver.
TEST_F(UserMediaClientImplTest, ReloadFrameWhileGeneratingStream) {
  mock_dispatcher_host_.DoNotRunCallback();

  user_media_client_impl_->RequestUserMediaForTest();
  LoadNewDocumentInFrame();
  EXPECT_EQ(1, mock_dispatcher_host_.request_stream_counter());
  EXPECT_EQ(0, mock_dispatcher_host_.stop_audio_device_counter());
  EXPECT_EQ(0, mock_dispatcher_host_.stop_video_device_counter());
  EXPECT_EQ(REQUEST_NOT_COMPLETE, request_state());
}

// This test what happens if a newdocument is loaded in the frame while the
// sources are being started.
TEST_F(UserMediaClientImplTest, ReloadFrameWhileGeneratingSources) {
  user_media_client_impl_->RequestUserMediaForTest();
  EXPECT_EQ(1, mock_dispatcher_host_.request_stream_counter());
  LoadNewDocumentInFrame();
  EXPECT_EQ(1, mock_dispatcher_host_.stop_audio_device_counter());
  EXPECT_EQ(1, mock_dispatcher_host_.stop_video_device_counter());
  EXPECT_EQ(REQUEST_NOT_COMPLETE, request_state());
}

// This test what happens if stop is called on a track after the frame has
// been reloaded.
TEST_F(UserMediaClientImplTest, StopTrackAfterReload) {
  blink::WebMediaStream mixed_desc = RequestLocalMediaStream();
  EXPECT_EQ(1, mock_dispatcher_host_.request_stream_counter());
  LoadNewDocumentInFrame();
  blink::WebHeap::CollectAllGarbageForTesting();
  EXPECT_EQ(1, mock_dispatcher_host_.stop_audio_device_counter());
  EXPECT_EQ(1, mock_dispatcher_host_.stop_video_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks =
      mixed_desc.AudioTracks();
  MediaStreamTrack* audio_track = MediaStreamTrack::GetTrack(audio_tracks[0]);
  audio_track->Stop();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_dispatcher_host_.stop_audio_device_counter());

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks =
      mixed_desc.VideoTracks();
  MediaStreamTrack* video_track = MediaStreamTrack::GetTrack(video_tracks[0]);
  video_track->Stop();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_dispatcher_host_.stop_video_device_counter());
}

TEST_F(UserMediaClientImplTest, DefaultConstraintsPropagate) {
  blink::WebUserMediaRequest request =
      blink::WebUserMediaRequest::CreateForTesting(CreateDefaultConstraints(),
                                                   CreateDefaultConstraints());
  user_media_client_impl_->RequestUserMediaForTest(request);
  AudioCaptureSettings audio_capture_settings =
      user_media_processor_->AudioSettings();
  VideoCaptureSettings video_capture_settings =
      user_media_processor_->VideoSettings();
  user_media_client_impl_->CancelUserMediaRequest(request);

  // Check default values selected by the constraints algorithm.
  EXPECT_TRUE(audio_capture_settings.HasValue());
  EXPECT_EQ(media::AudioDeviceDescription::kDefaultDeviceId,
            audio_capture_settings.device_id());
  EXPECT_FALSE(audio_capture_settings.hotword_enabled());
  EXPECT_TRUE(audio_capture_settings.disable_local_echo());
  EXPECT_FALSE(audio_capture_settings.render_to_associated_sink());

  const AudioProcessingProperties& properties =
      audio_capture_settings.audio_processing_properties();
  EXPECT_EQ(EchoCancellationType::kEchoCancellationAec2,
            properties.echo_cancellation_type);
  EXPECT_FALSE(properties.goog_audio_mirroring);
  EXPECT_TRUE(properties.goog_auto_gain_control);
  // The default value for goog_experimental_echo_cancellation is platform
  // dependent.
  EXPECT_EQ(AudioProcessingProperties().goog_experimental_echo_cancellation,
            properties.goog_experimental_echo_cancellation);
  EXPECT_TRUE(properties.goog_typing_noise_detection);
  EXPECT_TRUE(properties.goog_noise_suppression);
  EXPECT_TRUE(properties.goog_experimental_noise_suppression);
  EXPECT_TRUE(properties.goog_highpass_filter);
  EXPECT_TRUE(properties.goog_experimental_auto_gain_control);

  EXPECT_TRUE(video_capture_settings.HasValue());
  EXPECT_EQ(video_capture_settings.Width(),
            MediaStreamVideoSource::kDefaultWidth);
  EXPECT_EQ(video_capture_settings.Height(),
            MediaStreamVideoSource::kDefaultHeight);
  EXPECT_EQ(video_capture_settings.FrameRate(),
            MediaStreamVideoSource::kDefaultFrameRate);
  EXPECT_EQ(video_capture_settings.ResolutionChangePolicy(),
            media::ResolutionChangePolicy::FIXED_RESOLUTION);
  EXPECT_FALSE(video_capture_settings.noise_reduction());
  EXPECT_FALSE(video_capture_settings.min_frame_rate().has_value());

  const VideoTrackAdapterSettings& track_settings =
      video_capture_settings.track_adapter_settings();
  EXPECT_EQ(track_settings.target_width(),
            MediaStreamVideoSource::kDefaultWidth);
  EXPECT_EQ(track_settings.target_height(),
            MediaStreamVideoSource::kDefaultHeight);
  EXPECT_EQ(track_settings.min_aspect_ratio(),
            1.0 / MediaStreamVideoSource::kDefaultHeight);
  EXPECT_EQ(track_settings.max_aspect_ratio(),
            MediaStreamVideoSource::kDefaultWidth);
  // 0.0 is the default max_frame_rate and it indicates no frame-rate adjustment
  EXPECT_EQ(track_settings.max_frame_rate(), 0.0);
}

TEST_F(UserMediaClientImplTest, DefaultTabCapturePropagate) {
  MockConstraintFactory factory;
  factory.basic().media_stream_source.SetExact(
      blink::WebString::FromASCII(kMediaStreamSourceTab));
  blink::WebMediaConstraints audio_constraints =
      factory.CreateWebMediaConstraints();
  blink::WebMediaConstraints video_constraints =
      factory.CreateWebMediaConstraints();
  blink::WebUserMediaRequest request =
      blink::WebUserMediaRequest::CreateForTesting(audio_constraints,
                                                   video_constraints);
  user_media_client_impl_->RequestUserMediaForTest(request);
  AudioCaptureSettings audio_capture_settings =
      user_media_processor_->AudioSettings();
  VideoCaptureSettings video_capture_settings =
      user_media_processor_->VideoSettings();
  user_media_client_impl_->CancelUserMediaRequest(request);

  // Check default values selected by the constraints algorithm.
  EXPECT_TRUE(audio_capture_settings.HasValue());
  EXPECT_EQ(std::string(), audio_capture_settings.device_id());
  EXPECT_FALSE(audio_capture_settings.hotword_enabled());
  EXPECT_TRUE(audio_capture_settings.disable_local_echo());
  EXPECT_FALSE(audio_capture_settings.render_to_associated_sink());

  const AudioProcessingProperties& properties =
      audio_capture_settings.audio_processing_properties();
  EXPECT_EQ(EchoCancellationType::kEchoCancellationDisabled,
            properties.echo_cancellation_type);
  EXPECT_FALSE(properties.goog_audio_mirroring);
  EXPECT_FALSE(properties.goog_auto_gain_control);
  EXPECT_FALSE(properties.goog_experimental_echo_cancellation);
  EXPECT_FALSE(properties.goog_typing_noise_detection);
  EXPECT_FALSE(properties.goog_noise_suppression);
  EXPECT_FALSE(properties.goog_experimental_noise_suppression);
  EXPECT_FALSE(properties.goog_highpass_filter);
  EXPECT_FALSE(properties.goog_experimental_auto_gain_control);

  EXPECT_TRUE(video_capture_settings.HasValue());
  EXPECT_EQ(video_capture_settings.Width(), kDefaultScreenCastWidth);
  EXPECT_EQ(video_capture_settings.Height(), kDefaultScreenCastHeight);
  EXPECT_EQ(video_capture_settings.FrameRate(), kDefaultScreenCastFrameRate);
  EXPECT_EQ(video_capture_settings.ResolutionChangePolicy(),
            media::ResolutionChangePolicy::FIXED_RESOLUTION);
  EXPECT_FALSE(video_capture_settings.noise_reduction());
  EXPECT_FALSE(video_capture_settings.min_frame_rate().has_value());
  EXPECT_FALSE(video_capture_settings.max_frame_rate().has_value());

  const VideoTrackAdapterSettings& track_settings =
      video_capture_settings.track_adapter_settings();
  EXPECT_EQ(track_settings.target_width(), kDefaultScreenCastWidth);
  EXPECT_EQ(track_settings.target_height(), kDefaultScreenCastHeight);
  EXPECT_EQ(track_settings.min_aspect_ratio(), 1.0 / kMaxScreenCastDimension);
  EXPECT_EQ(track_settings.max_aspect_ratio(), kMaxScreenCastDimension);
  // 0.0 is the default max_frame_rate and it indicates no frame-rate adjustment
  EXPECT_EQ(track_settings.max_frame_rate(), 0.0);
}

TEST_F(UserMediaClientImplTest, DefaultDesktopCapturePropagate) {
  MockConstraintFactory factory;
  factory.basic().media_stream_source.SetExact(
      blink::WebString::FromASCII(kMediaStreamSourceDesktop));
  blink::WebMediaConstraints audio_constraints =
      factory.CreateWebMediaConstraints();
  blink::WebMediaConstraints video_constraints =
      factory.CreateWebMediaConstraints();
  blink::WebUserMediaRequest request =
      blink::WebUserMediaRequest::CreateForTesting(audio_constraints,
                                                   video_constraints);
  user_media_client_impl_->RequestUserMediaForTest(request);
  AudioCaptureSettings audio_capture_settings =
      user_media_processor_->AudioSettings();
  VideoCaptureSettings video_capture_settings =
      user_media_processor_->VideoSettings();
  user_media_client_impl_->CancelUserMediaRequest(request);
  base::RunLoop().RunUntilIdle();

  // Check default values selected by the constraints algorithm.
  EXPECT_TRUE(audio_capture_settings.HasValue());
  EXPECT_EQ(std::string(), audio_capture_settings.device_id());
  EXPECT_FALSE(audio_capture_settings.hotword_enabled());
  EXPECT_FALSE(audio_capture_settings.disable_local_echo());
  EXPECT_FALSE(audio_capture_settings.render_to_associated_sink());

  const AudioProcessingProperties& properties =
      audio_capture_settings.audio_processing_properties();
  EXPECT_EQ(EchoCancellationType::kEchoCancellationDisabled,
            properties.echo_cancellation_type);
  EXPECT_FALSE(properties.goog_audio_mirroring);
  EXPECT_FALSE(properties.goog_auto_gain_control);
  EXPECT_FALSE(properties.goog_experimental_echo_cancellation);
  EXPECT_FALSE(properties.goog_typing_noise_detection);
  EXPECT_FALSE(properties.goog_noise_suppression);
  EXPECT_FALSE(properties.goog_experimental_noise_suppression);
  EXPECT_FALSE(properties.goog_highpass_filter);
  EXPECT_FALSE(properties.goog_experimental_auto_gain_control);

  EXPECT_TRUE(video_capture_settings.HasValue());
  EXPECT_EQ(video_capture_settings.Width(), kDefaultScreenCastWidth);
  EXPECT_EQ(video_capture_settings.Height(), kDefaultScreenCastHeight);
  EXPECT_EQ(video_capture_settings.FrameRate(), kDefaultScreenCastFrameRate);
  EXPECT_EQ(video_capture_settings.ResolutionChangePolicy(),
            media::ResolutionChangePolicy::ANY_WITHIN_LIMIT);
  EXPECT_FALSE(video_capture_settings.noise_reduction());
  EXPECT_FALSE(video_capture_settings.min_frame_rate().has_value());
  EXPECT_FALSE(video_capture_settings.max_frame_rate().has_value());

  const VideoTrackAdapterSettings& track_settings =
      video_capture_settings.track_adapter_settings();
  EXPECT_EQ(track_settings.target_width(), kDefaultScreenCastWidth);
  EXPECT_EQ(track_settings.target_height(), kDefaultScreenCastHeight);
  EXPECT_EQ(track_settings.min_aspect_ratio(), 1.0 / kMaxScreenCastDimension);
  EXPECT_EQ(track_settings.max_aspect_ratio(), kMaxScreenCastDimension);
  // 0.0 is the default max_frame_rate and it indicates no frame-rate adjustment
  EXPECT_EQ(track_settings.max_frame_rate(), 0.0);
}

TEST_F(UserMediaClientImplTest, NonDefaultAudioConstraintsPropagate) {
  mock_dispatcher_host_.DoNotRunCallback();

  MockConstraintFactory factory;
  factory.basic().device_id.SetExact(
      blink::WebString::FromASCII(kFakeAudioInputDeviceId1));
  factory.basic().hotword_enabled.SetExact(true);
  factory.basic().disable_local_echo.SetExact(true);
  factory.basic().render_to_associated_sink.SetExact(true);
  factory.basic().echo_cancellation.SetExact(false);
  factory.basic().goog_audio_mirroring.SetExact(true);
  factory.basic().goog_typing_noise_detection.SetExact(true);
  blink::WebMediaConstraints audio_constraints =
      factory.CreateWebMediaConstraints();
  // Request contains only audio
  blink::WebUserMediaRequest request =
      blink::WebUserMediaRequest::CreateForTesting(
          audio_constraints, blink::WebMediaConstraints());
  user_media_client_impl_->RequestUserMediaForTest(request);
  AudioCaptureSettings audio_capture_settings =
      user_media_processor_->AudioSettings();
  VideoCaptureSettings video_capture_settings =
      user_media_processor_->VideoSettings();
  user_media_client_impl_->CancelUserMediaRequest(request);

  EXPECT_FALSE(video_capture_settings.HasValue());

  EXPECT_TRUE(audio_capture_settings.HasValue());
  EXPECT_EQ(kFakeAudioInputDeviceId1, audio_capture_settings.device_id());
  EXPECT_TRUE(audio_capture_settings.hotword_enabled());
  EXPECT_TRUE(audio_capture_settings.disable_local_echo());
  EXPECT_TRUE(audio_capture_settings.render_to_associated_sink());

  const AudioProcessingProperties& properties =
      audio_capture_settings.audio_processing_properties();
  EXPECT_EQ(EchoCancellationType::kEchoCancellationDisabled,
            properties.echo_cancellation_type);
  EXPECT_TRUE(properties.goog_audio_mirroring);
  EXPECT_FALSE(properties.goog_auto_gain_control);
  EXPECT_FALSE(properties.goog_experimental_echo_cancellation);
  EXPECT_TRUE(properties.goog_typing_noise_detection);
  EXPECT_FALSE(properties.goog_noise_suppression);
  EXPECT_FALSE(properties.goog_experimental_noise_suppression);
  EXPECT_FALSE(properties.goog_highpass_filter);
  EXPECT_FALSE(properties.goog_experimental_auto_gain_control);
}

TEST_F(UserMediaClientImplTest, CreateWithMandatoryInvalidAudioDeviceId) {
  blink::WebMediaConstraints audio_constraints =
      CreateDeviceConstraints(kInvalidDeviceId);
  blink::WebUserMediaRequest request =
      blink::WebUserMediaRequest::CreateForTesting(
          audio_constraints, blink::WebMediaConstraints());
  user_media_client_impl_->RequestUserMediaForTest(request);
  EXPECT_EQ(REQUEST_FAILED, request_state());
}

TEST_F(UserMediaClientImplTest, CreateWithMandatoryInvalidVideoDeviceId) {
  blink::WebMediaConstraints video_constraints =
      CreateDeviceConstraints(kInvalidDeviceId);
  blink::WebUserMediaRequest request =
      blink::WebUserMediaRequest::CreateForTesting(blink::WebMediaConstraints(),
                                                   video_constraints);
  user_media_client_impl_->RequestUserMediaForTest(request);
  EXPECT_EQ(REQUEST_FAILED, request_state());
}

TEST_F(UserMediaClientImplTest, CreateWithMandatoryValidDeviceIds) {
  blink::WebMediaConstraints audio_constraints =
      CreateDeviceConstraints(kFakeAudioInputDeviceId1);
  blink::WebMediaConstraints video_constraints =
      CreateDeviceConstraints(kFakeVideoInputDeviceId1);
  TestValidRequestWithConstraints(audio_constraints, video_constraints,
                                  kFakeAudioInputDeviceId1,
                                  kFakeVideoInputDeviceId1);
}

TEST_F(UserMediaClientImplTest, CreateWithBasicIdealValidDeviceId) {
  blink::WebMediaConstraints audio_constraints =
      CreateDeviceConstraints(nullptr, kFakeAudioInputDeviceId1);
  blink::WebMediaConstraints video_constraints =
      CreateDeviceConstraints(nullptr, kFakeVideoInputDeviceId1);
  TestValidRequestWithConstraints(audio_constraints, video_constraints,
                                  kFakeAudioInputDeviceId1,
                                  kFakeVideoInputDeviceId1);
}

TEST_F(UserMediaClientImplTest, CreateWithAdvancedExactValidDeviceId) {
  blink::WebMediaConstraints audio_constraints =
      CreateDeviceConstraints(nullptr, nullptr, kFakeAudioInputDeviceId1);
  blink::WebMediaConstraints video_constraints =
      CreateDeviceConstraints(nullptr, nullptr, kFakeVideoInputDeviceId1);
  TestValidRequestWithConstraints(audio_constraints, video_constraints,
                                  kFakeAudioInputDeviceId1,
                                  kFakeVideoInputDeviceId1);
}

TEST_F(UserMediaClientImplTest, CreateWithAllOptionalInvalidDeviceId) {
  blink::WebMediaConstraints audio_constraints =
      CreateDeviceConstraints(nullptr, kInvalidDeviceId, kInvalidDeviceId);
  blink::WebMediaConstraints video_constraints =
      CreateDeviceConstraints(nullptr, kInvalidDeviceId, kInvalidDeviceId);
  // MockMojoMediaStreamDispatcherHost uses empty string as default audio device
  // ID. MockMediaDevicesDispatcher uses the first device in the enumeration as
  // default audio or video device ID.
  std::string expected_audio_device_id =
      media::AudioDeviceDescription::kDefaultDeviceId;
  TestValidRequestWithConstraints(audio_constraints, video_constraints,
                                  expected_audio_device_id,
                                  kFakeVideoInputDeviceId1);
}

TEST_F(UserMediaClientImplTest, CreateWithFacingModeUser) {
  blink::WebMediaConstraints audio_constraints =
      CreateDeviceConstraints(kFakeAudioInputDeviceId1);
  blink::WebMediaConstraints video_constraints =
      CreateFacingModeConstraints("user");
  // kFakeVideoInputDeviceId1 has user facing mode.
  TestValidRequestWithConstraints(audio_constraints, video_constraints,
                                  kFakeAudioInputDeviceId1,
                                  kFakeVideoInputDeviceId1);
}

TEST_F(UserMediaClientImplTest, CreateWithFacingModeEnvironment) {
  blink::WebMediaConstraints audio_constraints =
      CreateDeviceConstraints(kFakeAudioInputDeviceId1);
  blink::WebMediaConstraints video_constraints =
      CreateFacingModeConstraints("environment");
  // kFakeVideoInputDeviceId2 has environment facing mode.
  TestValidRequestWithConstraints(audio_constraints, video_constraints,
                                  kFakeAudioInputDeviceId1,
                                  kFakeVideoInputDeviceId2);
}

TEST_F(UserMediaClientImplTest, ApplyConstraintsVideoDeviceSingleTrack) {
  EXPECT_CALL(mock_dispatcher_host_, OnStreamStarted(_));
  blink::WebMediaStreamTrack web_track = RequestLocalVideoTrack();
  MediaStreamVideoTrack* track =
      MediaStreamVideoTrack::GetVideoTrack(web_track);
  MediaStreamVideoSource* source = track->source();
  CheckVideoSource(source, 0, 0, 0.0);

  media_devices_dispatcher_.SetVideoSource(source);

  // The following applyConstraint() request should force a source restart and
  // produce a video mode with 1024x768.
  ApplyConstraintsVideoMode(web_track, 1024, 768);
  CheckVideoSourceAndTrack(source, 1024, 768, 20.0, web_track, 1024, 768, 20.0);

  // The following applyConstraints() requests should not result in a source
  // restart since the only format supported by the mock MDDH that supports
  // 801x600 is the existing 1024x768 mode with downscaling.
  ApplyConstraintsVideoMode(web_track, 801, 600);
  CheckVideoSourceAndTrack(source, 1024, 768, 20.0, web_track, 801, 600, 20.0);

  // The following applyConstraints() requests should result in a source restart
  // since there is a native mode of 800x600 supported by the mock MDDH.
  ApplyConstraintsVideoMode(web_track, 800, 600);
  CheckVideoSourceAndTrack(source, 800, 600, 30.0, web_track, 800, 600, 30.0);

  // The following applyConstraints() requests should fail since the mock MDDH
  // does not have any mode that can produce 2000x2000.
  ApplyConstraintsVideoMode(web_track, 2000, 2000);
  CheckVideoSourceAndTrack(source, 800, 600, 30.0, web_track, 800, 600, 30.0);
}

TEST_F(UserMediaClientImplTest, ApplyConstraintsVideoDeviceTwoTracks) {
  EXPECT_CALL(mock_dispatcher_host_, OnStreamStarted(_));
  blink::WebMediaStreamTrack web_track = RequestLocalVideoTrack();
  MockMediaStreamVideoCapturerSource* source =
      user_media_processor_->last_created_video_source();
  CheckVideoSource(source, 0, 0, 0.0);
  media_devices_dispatcher_.SetVideoSource(source);

  // Switch the source and track to 1024x768@20Hz.
  ApplyConstraintsVideoMode(web_track, 1024, 768);
  CheckVideoSourceAndTrack(source, 1024, 768, 20.0, web_track, 1024, 768, 20.0);

  // Create a new track and verify that it uses the same source and that the
  // source's format did not change. The new track uses the same format as the
  // source by default.
  EXPECT_CALL(mock_dispatcher_host_, OnStreamStarted(_));
  blink::WebMediaStreamTrack web_track2 = RequestLocalVideoTrack();
  CheckVideoSourceAndTrack(source, 1024, 768, 20.0, web_track2, 1024, 768,
                           20.0);

  // Use applyConstraints() to change the first track to 800x600 and verify
  // that the source is not reconfigured. Downscaling is used instead because
  // there is more than one track using the source. The second track is left
  // unmodified.
  ApplyConstraintsVideoMode(web_track, 800, 600);
  CheckVideoSourceAndTrack(source, 1024, 768, 20.0, web_track, 800, 600, 20.0);
  CheckVideoSourceAndTrack(source, 1024, 768, 20.0, web_track2, 1024, 768,
                           20.0);

  // Try to use applyConstraints() to change the first track to 800x600@30Hz.
  // It fails, because the source is open in native 20Hz mode and it does not
  // support reconfiguration when more than one track is connected.
  // TODO(guidou): Allow reconfiguring sources with more than one track.
  // http://crbug.com/768205.
  ApplyConstraintsVideoMode(web_track, 800, 600, 30.0);
  CheckVideoSourceAndTrack(source, 1024, 768, 20.0, web_track, 800, 600, 20.0);
  CheckVideoSourceAndTrack(source, 1024, 768, 20.0, web_track2, 1024, 768,
                           20.0);

  // Try to use applyConstraints() to change the first track to 800x600@30Hz.
  // after stopping the second track. In this case, the source is left with a
  // single track and it supports reconfiguration to the requested mode.
  MediaStreamTrack::GetTrack(web_track2)->Stop();
  ApplyConstraintsVideoMode(web_track, 800, 600, 30.0);
  CheckVideoSourceAndTrack(source, 800, 600, 30.0, web_track, 800, 600, 30.0);
}

TEST_F(UserMediaClientImplTest,
       ApplyConstraintsVideoDeviceFailsToStopForRestart) {
  EXPECT_CALL(mock_dispatcher_host_, OnStreamStarted(_));
  blink::WebMediaStreamTrack web_track = RequestLocalVideoTrack();
  MockMediaStreamVideoCapturerSource* source =
      user_media_processor_->last_created_video_source();
  CheckVideoSource(source, 0, 0, 0.0);
  media_devices_dispatcher_.SetVideoSource(source);

  // Switch the source and track to 1024x768@20Hz.
  ApplyConstraintsVideoMode(web_track, 1024, 768);
  CheckVideoSourceAndTrack(source, 1024, 768, 20.0, web_track, 1024, 768, 20.0);

  // Try to switch the source and track to 640x480. Since the source cannot
  // stop for restart, downscaling is used for the track.
  source->DisableStopForRestart();
  ApplyConstraintsVideoMode(web_track, 640, 480);
  CheckVideoSourceAndTrack(source, 1024, 768, 20.0, web_track, 640, 480, 20.0);
}

TEST_F(UserMediaClientImplTest,
       ApplyConstraintsVideoDeviceFailsToRestartAfterStop) {
  EXPECT_CALL(mock_dispatcher_host_, OnStreamStarted(_));
  blink::WebMediaStreamTrack web_track = RequestLocalVideoTrack();
  MockMediaStreamVideoCapturerSource* source =
      user_media_processor_->last_created_video_source();
  CheckVideoSource(source, 0, 0, 0.0);
  media_devices_dispatcher_.SetVideoSource(source);

  // Switch the source and track to 1024x768.
  ApplyConstraintsVideoMode(web_track, 1024, 768);
  CheckVideoSourceAndTrack(source, 1024, 768, 20.0, web_track, 1024, 768, 20.0);

  // Try to switch the source and track to 640x480. Since the source cannot
  // restart, source and track are stopped.
  source->DisableRestart();
  ApplyConstraintsVideoMode(web_track, 640, 480);

  EXPECT_EQ(web_track.Source().GetReadyState(),
            blink::WebMediaStreamSource::kReadyStateEnded);
  EXPECT_FALSE(source->IsRunning());
}

TEST_F(UserMediaClientImplTest, ApplyConstraintsVideoDeviceStopped) {
  EXPECT_CALL(mock_dispatcher_host_, OnStreamStarted(_));
  blink::WebMediaStreamTrack web_track = RequestLocalVideoTrack();
  MockMediaStreamVideoCapturerSource* source =
      user_media_processor_->last_created_video_source();
  CheckVideoSource(source, 0, 0, 0.0);
  media_devices_dispatcher_.SetVideoSource(source);

  // Switch the source and track to 1024x768.
  ApplyConstraintsVideoMode(web_track, 1024, 768);
  CheckVideoSourceAndTrack(source, 1024, 768, 20.0, web_track, 1024, 768, 20.0);

  // Try to switch the source and track to 640x480 after stopping the track.
  MediaStreamTrack* track = MediaStreamTrack::GetTrack(web_track);
  track->Stop();
  EXPECT_EQ(web_track.Source().GetReadyState(),
            blink::WebMediaStreamSource::kReadyStateEnded);
  EXPECT_FALSE(source->IsRunning());
  {
    blink::WebMediaStreamTrack::Settings settings;
    track->GetSettings(settings);
    EXPECT_EQ(settings.width, -1);
    EXPECT_EQ(settings.height, -1);
    EXPECT_EQ(settings.frame_rate, -1.0);
  }

  ApplyConstraintsVideoMode(web_track, 640, 480);
  EXPECT_EQ(web_track.Source().GetReadyState(),
            blink::WebMediaStreamSource::kReadyStateEnded);
  EXPECT_FALSE(source->IsRunning());
  {
    blink::WebMediaStreamTrack::Settings settings;
    track->GetSettings(settings);
    EXPECT_EQ(settings.width, -1);
    EXPECT_EQ(settings.height, -1);
    EXPECT_EQ(settings.frame_rate, -1.0);
  }
}

// These tests check that the associated output device id is
// set according to the renderToAssociatedSink constrainable property.
TEST_F(UserMediaClientImplTest,
       RenderToAssociatedSinkTrueAssociatedOutputDeviceId) {
  EXPECT_CALL(mock_dispatcher_host_, OnStreamStarted(_));
  blink::WebMediaStreamTrack web_track =
      RequestLocalAudioTrackWithAssociatedSink(true);
  MediaStreamAudioSource* source =
      MediaStreamAudioSource::From(web_track.Source());
  EXPECT_TRUE(source->device().matched_output_device_id);
}

TEST_F(UserMediaClientImplTest,
       RenderToAssociatedSinkFalseAssociatedOutputDeviceId) {
  EXPECT_CALL(mock_dispatcher_host_, OnStreamStarted(_));
  blink::WebMediaStreamTrack web_track =
      RequestLocalAudioTrackWithAssociatedSink(false);
  MediaStreamAudioSource* source =
      MediaStreamAudioSource::From(web_track.Source());
  EXPECT_FALSE(source->device().matched_output_device_id);
}

TEST_F(UserMediaClientImplTest, IsCapturing) {
  EXPECT_FALSE(user_media_client_impl_->IsCapturing());
  EXPECT_CALL(mock_dispatcher_host_, OnStreamStarted(_));
  blink::WebMediaStream stream = RequestLocalMediaStream();
  EXPECT_TRUE(user_media_client_impl_->IsCapturing());

  user_media_client_impl_->StopTrack(stream.AudioTracks()[0]);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(user_media_client_impl_->IsCapturing());

  user_media_client_impl_->StopTrack(stream.VideoTracks()[0]);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(user_media_client_impl_->IsCapturing());
}

}  // namespace content
