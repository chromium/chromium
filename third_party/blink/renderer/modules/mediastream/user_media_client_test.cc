// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/user_media_client.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "media/audio/audio_device_description.h"
#include "media/capture/mojom/video_capture_types.mojom-blink.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/mediastream/media_device_id.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "third_party/blink/public/mojom/media/capture_handle_config.mojom-blink.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom-blink.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_source.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_track.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/modules/mediastream/web_media_stream_device_observer.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_video_content.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track_impl.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_constraint_factory.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/mock_mojo_media_stream_dispatcher_host.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_request.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_processor_options.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_track_platform.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/display/screen_info.h"

using media::mojom::SubCaptureTargetType;
using ::testing::_;
using ::testing::Mock;

namespace blink {

using EchoCancellationType =
    blink::AudioProcessingProperties::EchoCancellationType;

namespace {

MediaConstraints CreateDefaultConstraints() {
  blink::MockConstraintFactory factory;
  factory.AddAdvanced();
  return factory.CreateMediaConstraints();
}

MediaConstraints CreateDeviceConstraints(
    const String& basic_exact_value,
    const String& basic_ideal_value = g_empty_string,
    const String& advanced_exact_value = g_empty_string) {
  blink::MockConstraintFactory factory;
  if (!basic_exact_value.empty()) {
    factory.basic().device_id.SetExact(basic_exact_value);
  }
  if (!basic_ideal_value.empty()) {
    factory.basic().device_id.SetIdeal(Vector({basic_ideal_value}));
  }

  auto& advanced = factory.AddAdvanced();
  if (!advanced_exact_value.empty()) {
    advanced.device_id.SetExact(advanced_exact_value);
  }

  return factory.CreateMediaConstraints();
}

MediaConstraints CreateFacingModeConstraints(
    const char* basic_exact_value,
    const char* basic_ideal_value = nullptr,
    const char* advanced_exact_value = nullptr) {
  blink::MockConstraintFactory factory;
  if (basic_exact_value) {
    factory.basic().facing_mode.SetExact(String::FromUTF8(basic_exact_value));
  }
  if (basic_ideal_value) {
    factory.basic().device_id.SetIdeal(Vector<String>({basic_ideal_value}));
  }

  auto& advanced = factory.AddAdvanced();
  if (advanced_exact_value) {
    String value = String::FromUTF8(advanced_exact_value);
    advanced.device_id.SetExact(value);
  }

  return factory.CreateMediaConstraints();
}

void CheckVideoSource(blink::MediaStreamVideoSource* source,
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

void CheckVideoSourceAndTrack(blink::MediaStreamVideoSource* source,
                              int expected_source_width,
                              int expected_source_height,
                              double expected_source_frame_rate,
                              MediaStreamComponent* component,
                              int expected_track_width,
                              int expected_track_height,
                              double expected_track_frame_rate) {
  CheckVideoSource(source, expected_source_width, expected_source_height,
                   expected_source_frame_rate);
  EXPECT_EQ(component->GetReadyState(), MediaStreamSource::kReadyStateLive);
  MediaStreamVideoTrack* track = MediaStreamVideoTrack::From(component);
  EXPECT_EQ(track->source(), source);

  MediaStreamTrackPlatform::Settings settings;
  track->GetSettings(settings);
  EXPECT_EQ(settings.width, expected_track_width);
  EXPECT_EQ(settings.height, expected_track_height);
  EXPECT_EQ(settings.frame_rate, expected_track_frame_rate);
}

class MockLocalMediaStreamAudioSource : public blink::MediaStreamAudioSource {
 public:
  MockLocalMediaStreamAudioSource()
      : blink::MediaStreamAudioSource(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
            true /* is_local_source */) {}

  MOCK_METHOD0(EnsureSourceIsStopped, void());

  void ChangeSourceImpl(const blink::MediaStreamDevice& new_device) override {
    EnsureSourceIsStopped();
  }
};

class MockMediaStreamVideoCapturerSource
    : public blink::MockMediaStreamVideoSource {
 public:
  MockMediaStreamVideoCapturerSource(const blink::MediaStreamDevice& device,
                                     SourceStoppedCallback stop_callback)
      : blink::MockMediaStreamVideoSource() {
    SetDevice(device);
    SetStopCallback(std::move(stop_callback));
  }

  MOCK_METHOD1(ChangeSourceImpl,
               void(const blink::MediaStreamDevice& new_device));
};

String MakeValidDeviceId(std::string_view id) {
  std::string padding =
      base::ToLowerASCII(base::HexEncode(base::RandBytesAsVector(32)));
  std::string padded_id = base::StrCat({id, padding}).substr(0, 64);
  CHECK(blink::IsValidMediaDeviceId(padded_id));
  return String(padded_id);
}

class FakeDeviceIds {
 public:
  static FakeDeviceIds* GetInstance() {
    return base::Singleton<FakeDeviceIds>::get();
  }

  const String invalid_device = MakeValidDeviceId("invalid");
  const String audio_input_1 = MakeValidDeviceId("fakeaudioinput1");
  const String audio_input_2 = MakeValidDeviceId("fakeaudioinput2");
  const String video_input_1 = MakeValidDeviceId("fakevideoinput1");
  const String video_input_2 = MakeValidDeviceId("fakevideoinput2");
  const String video_input_3 = MakeValidDeviceId("fakevideoinput3");
};

class MediaDevicesDispatcherHostMock
    : public mojom::blink::MediaDevicesDispatcherHost {
 public:
  explicit MediaDevicesDispatcherHostMock() {}
  void EnumerateDevices(bool request_audio_input,
                        bool request_video_input,
                        bool request_audio_output,
                        bool request_video_input_capabilities,
                        bool request_audio_input_capabilities,
                        EnumerateDevicesCallback callback) override {
    NOTREACHED_IN_MIGRATION();
  }

  void GetVideoInputCapabilities(
      GetVideoInputCapabilitiesCallback client_callback) override {
    NOTREACHED_IN_MIGRATION();
  }

  void GetAudioInputCapabilities(
      GetAudioInputCapabilitiesCallback client_callback) override {
    NOTREACHED_IN_MIGRATION();
  }

  void AddMediaDevicesListener(
      bool subscribe_audio_input,
      bool subscribe_video_input,
      bool subscribe_audio_output,
      mojo::PendingRemote<blink::mojom::blink::MediaDevicesListener> listener)
      override {
    NOTREACHED_IN_MIGRATION();
  }

  void SetCaptureHandleConfig(mojom::blink::CaptureHandleConfigPtr) override {
    NOTREACHED_IN_MIGRATION();
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  void CloseFocusWindowOfOpportunity(const String& label) override {
    NOTREACHED_IN_MIGRATION();
  }

  void ProduceSubCaptureTargetId(
      SubCaptureTargetType type,
      ProduceSubCaptureTargetIdCallback callback) override {
    NOTREACHED_IN_MIGRATION();
  }
#endif

  void GetAllVideoInputDeviceFormats(
      const String& device_id,
      GetAllVideoInputDeviceFormatsCallback callback) override {
    devices_count_++;
  }

  void GetAvailableVideoInputDeviceFormats(
      const String& device_id,
      GetAvailableVideoInputDeviceFormatsCallback callback) override {
    devices_count_++;
  }

  size_t devices_count() const { return devices_count_; }

 private:
  size_t devices_count_ = 0;
};

class MockMediaDevicesDispatcherHost
    : public mojom::blink::MediaDevicesDispatcherHost {
 public:
  MockMediaDevicesDispatcherHost() {}
  void EnumerateDevices(bool request_audio_input,
                        bool request_video_input,
                        bool request_audio_output,
                        bool request_video_input_capabilities,
                        bool request_audio_input_capabilities,
                        EnumerateDevicesCallback callback) override {
    NOTREACHED_IN_MIGRATION();
  }

  void SetVideoInputCapabilities(
      Vector<blink::mojom::blink::VideoInputDeviceCapabilitiesPtr>
          capabilities) {
    video_input_capabilities_ = std::move(capabilities);
  }

  void GetVideoInputCapabilities(
      GetVideoInputCapabilitiesCallback client_callback) override {
    if (!video_input_capabilities_.empty()) {
      // blink::mojom::blink::VideoInputDeviceCapabilitiesPtr disallows copy so
      // we move our capabilities.
      std::move(client_callback).Run(std::move(video_input_capabilities_));
      // Clear moved `video_input_capabilities_`.
      video_input_capabilities_ =
          Vector<blink::mojom::blink::VideoInputDeviceCapabilitiesPtr>();
      return;
    }
    blink::mojom::blink::VideoInputDeviceCapabilitiesPtr device =
        blink::mojom::blink::VideoInputDeviceCapabilities::New();
    device->device_id = FakeDeviceIds::GetInstance()->video_input_1;
    device->group_id = String("dummy");
    device->facing_mode = mojom::blink::FacingMode::kUser;
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
    Vector<blink::mojom::blink::VideoInputDeviceCapabilitiesPtr> result;
    result.push_back(std::move(device));

    device = blink::mojom::blink::VideoInputDeviceCapabilities::New();
    device->device_id = FakeDeviceIds::GetInstance()->video_input_2;
    device->group_id = String("dummy");
    device->facing_mode = mojom::blink::FacingMode::kEnvironment;
    device->formats.push_back(media::VideoCaptureFormat(
        gfx::Size(640, 480), 30.0f, media::PIXEL_FORMAT_I420));
    result.push_back(std::move(device));

    std::move(client_callback).Run(std::move(result));
  }

  void GetAudioInputCapabilities(
      GetAudioInputCapabilitiesCallback client_callback) override {
    Vector<blink::mojom::blink::AudioInputDeviceCapabilitiesPtr> result;
    blink::mojom::blink::AudioInputDeviceCapabilitiesPtr device =
        blink::mojom::blink::AudioInputDeviceCapabilities::New();
    device->device_id = media::AudioDeviceDescription::kDefaultDeviceId;
    device->group_id = String("dummy");
    device->parameters = audio_parameters_;
    result.push_back(std::move(device));

    device = blink::mojom::blink::AudioInputDeviceCapabilities::New();
    device->device_id = FakeDeviceIds::GetInstance()->audio_input_1;
    device->group_id = String("dummy");
    device->parameters = audio_parameters_;
    result.push_back(std::move(device));

    device = blink::mojom::blink::AudioInputDeviceCapabilities::New();
    device->device_id = FakeDeviceIds::GetInstance()->audio_input_2;
    device->group_id = String("dummy");
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
      mojo::PendingRemote<blink::mojom::blink::MediaDevicesListener> listener)
      override {
    NOTREACHED_IN_MIGRATION();
  }

  void SetCaptureHandleConfig(mojom::blink::CaptureHandleConfigPtr) override {
    NOTREACHED_IN_MIGRATION();
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  void CloseFocusWindowOfOpportunity(const String& label) override {
    NOTREACHED_IN_MIGRATION();
  }

  void ProduceSubCaptureTargetId(
      SubCaptureTargetType type,
      ProduceSubCaptureTargetIdCallback callback) override {
    std::move(callback).Run("");
  }
#endif

  void GetAllVideoInputDeviceFormats(
      const String&,
      GetAllVideoInputDeviceFormatsCallback callback) override {
    Vector<media::VideoCaptureFormat> formats;
    formats.push_back(media::VideoCaptureFormat(gfx::Size(640, 480), 30.0f,
                                                media::PIXEL_FORMAT_I420));
    formats.push_back(media::VideoCaptureFormat(gfx::Size(800, 600), 30.0f,
                                                media::PIXEL_FORMAT_I420));
    formats.push_back(media::VideoCaptureFormat(gfx::Size(1024, 768), 20.0f,
                                                media::PIXEL_FORMAT_I420));
    std::move(callback).Run(formats);
  }

  void GetAvailableVideoInputDeviceFormats(
      const String& device_id,
      GetAvailableVideoInputDeviceFormatsCallback callback) override {
    if (!video_source_ || !video_source_->IsRunning() ||
        !video_source_->GetCurrentFormat()) {
      GetAllVideoInputDeviceFormats(device_id, std::move(callback));
      return;
    }

    Vector<media::VideoCaptureFormat> formats;
    formats.push_back(*video_source_->GetCurrentFormat());
    std::move(callback).Run(formats);
  }

  void SetVideoSource(blink::MediaStreamVideoSource* video_source) {
    video_source_ = video_source;
  }

 private:
  media::AudioParameters audio_parameters_ =
      media::AudioParameters::UnavailableDeviceParams();
  raw_ptr<blink::MediaStreamVideoSource, DanglingUntriaged> video_source_ =
      nullptr;
  // If set, overrides the default ones otherwise returned by
  // GetVideoInputCapabilities()
  Vector<blink::mojom::blink::VideoInputDeviceCapabilitiesPtr>
      video_input_capabilities_;
};

enum RequestState {
  kRequestNotStarted,
  kRequestNotComplete,
  kRequestSucceeded,
  kRequestFailed,
};

class UserMediaProcessorUnderTest : public UserMediaProcessor {
 public:
  UserMediaProcessorUnderTest(
      LocalFrame* frame,
      std::unique_ptr<blink::WebMediaStreamDeviceObserver>
          media_stream_device_observer,
      mojo::PendingRemote<blink::mojom::blink::MediaDevicesDispatcherHost>
          media_devices_dispatcher,
      RequestState* state)
      : UserMediaProcessor(
            frame,
            WTF::BindRepeating(
                // Note: this uses a lambda because binding a non-static method
                // with a weak receiver triggers special cancellation handling,
                // which cannot handle non-void return types.
                [](UserMediaProcessorUnderTest* processor)
                    -> blink::mojom::blink::MediaDevicesDispatcherHost* {
                  // In a test, `processor` should always be kept alive.
                  CHECK(processor);
                  return processor->media_devices_dispatcher_.get();
                },
                WrapWeakPersistent(this)),
            blink::scheduler::GetSingleThreadTaskRunnerForTesting()),
        media_stream_device_observer_(std::move(media_stream_device_observer)),
        media_devices_dispatcher_(frame->DomWindow()),
        state_(state) {
    media_devices_dispatcher_.Bind(
        std::move(media_devices_dispatcher),
        blink::scheduler::GetSingleThreadTaskRunnerForTesting());
    SetMediaStreamDeviceObserverForTesting(media_stream_device_observer_.get());
  }

  MockMediaStreamVideoCapturerSource* last_created_video_source() const {
    return video_source_;
  }
  MockLocalMediaStreamAudioSource* last_created_local_audio_source() const {
    return local_audio_source_;
  }

  void SetCreateSourceThatFails(bool should_fail) {
    create_source_that_fails_ = should_fail;
  }

  MediaStreamDescriptor* last_generated_descriptor() {
    return last_generated_descriptor_.Get();
  }
  void ClearLastGeneratedStream() { last_generated_descriptor_ = nullptr; }

  blink::AudioCaptureSettings AudioSettings() const {
    return AudioCaptureSettingsForTesting();
  }
  const Vector<blink::AudioCaptureSettings>& EligibleAudioSettings() const {
    return EligibleAudioCaptureSettingsForTesting();
  }
  blink::VideoCaptureSettings VideoSettings() const {
    return VideoCaptureSettingsForTesting();
  }
  const Vector<blink::VideoCaptureSettings> EligibleVideoSettings() const {
    return EligibleVideoCaptureSettingsForTesting();
  }

  blink::mojom::blink::MediaStreamRequestResult error_reason() const {
    return result_;
  }
  String constraint_name() const { return constraint_name_; }

  // UserMediaProcessor overrides.
  std::unique_ptr<blink::MediaStreamVideoSource> CreateVideoSource(
      const blink::MediaStreamDevice& device,
      blink::WebPlatformMediaStreamSource::SourceStoppedCallback stop_callback)
      override {
    video_source_ = new MockMediaStreamVideoCapturerSource(
        device, std::move(stop_callback));
    return base::WrapUnique(video_source_.get());
  }

  std::unique_ptr<blink::MediaStreamAudioSource> CreateAudioSource(
      const blink::MediaStreamDevice& device,
      blink::WebPlatformMediaStreamSource::ConstraintsRepeatingCallback
          source_ready) override {
    std::unique_ptr<blink::MediaStreamAudioSource> source;
    if (create_source_that_fails_) {
      class FailedAtLifeAudioSource : public blink::MediaStreamAudioSource {
       public:
        FailedAtLifeAudioSource()
            : blink::MediaStreamAudioSource(
                  blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
                  true) {}
        ~FailedAtLifeAudioSource() override {}

       protected:
        bool EnsureSourceIsStarted() override { return false; }
      };
      source = std::make_unique<FailedAtLifeAudioSource>();
    } else if (blink::IsDesktopCaptureMediaType(device.type)) {
      local_audio_source_ = new MockLocalMediaStreamAudioSource();
      source = base::WrapUnique(local_audio_source_.get());
    } else {
      source = std::make_unique<blink::MediaStreamAudioSource>(
          blink::scheduler::GetSingleThreadTaskRunnerForTesting(), true);
    }

    source->SetDevice(device);

    if (!create_source_that_fails_) {
      // RunUntilIdle is required for this task to complete.
      blink::scheduler::GetSingleThreadTaskRunnerForTesting()->PostTask(
          FROM_HERE,
          base::BindOnce(&UserMediaProcessorUnderTest::SignalSourceReady,
                         std::move(source_ready), source.get()));
    }

    return source;
  }

  void GetUserMediaRequestSucceeded(MediaStreamDescriptorVector* descriptors,
                                    UserMediaRequest* request_info) override {
    // TODO(crbug.com/1300883): Generalize to multiple streams.
    DCHECK_EQ(descriptors->size(), 1u);
    last_generated_descriptor_ = (*descriptors)[0];
    *state_ = kRequestSucceeded;
  }

  void GetUserMediaRequestFailed(
      blink::mojom::blink::MediaStreamRequestResult result,
      const String& constraint_name) override {
    last_generated_descriptor_ = nullptr;
    *state_ = kRequestFailed;
    result_ = result;
    constraint_name_ = constraint_name;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(media_devices_dispatcher_);
    visitor->Trace(last_generated_descriptor_);
    UserMediaProcessor::Trace(visitor);
  }

 private:
  static void SignalSourceReady(
      blink::WebPlatformMediaStreamSource::ConstraintsOnceCallback source_ready,
      blink::WebPlatformMediaStreamSource* source) {
    std::move(source_ready)
        .Run(source, blink::mojom::blink::MediaStreamRequestResult::OK, "");
  }

  std::unique_ptr<WebMediaStreamDeviceObserver> media_stream_device_observer_;
  HeapMojoRemote<blink::mojom::blink::MediaDevicesDispatcherHost>
      media_devices_dispatcher_;
  raw_ptr<MockMediaStreamVideoCapturerSource, DanglingUntriaged> video_source_ =
      nullptr;
  raw_ptr<MockLocalMediaStreamAudioSource, DanglingUntriaged>
      local_audio_source_ = nullptr;
  bool create_source_that_fails_ = false;
  Member<MediaStreamDescriptor> last_generated_descriptor_;
  blink::mojom::blink::MediaStreamRequestResult result_ =
      blink::mojom::blink::MediaStreamRequestResult::NUM_MEDIA_REQUEST_RESULTS;
  String constraint_name_;
  raw_ptr<RequestState> state_;
};

class UserMediaClientUnderTest : public UserMediaClient {
 public:
  UserMediaClientUnderTest(LocalFrame* frame,
                           UserMediaProcessor* user_media_processor,
                           UserMediaProcessor* display_user_media_processor,
                           RequestState* state)
      : UserMediaClient(
            frame,
            user_media_processor,
            display_user_media_processor,
            blink::scheduler::GetSingleThreadTaskRunnerForTesting()),
        state_(state) {}

  void RequestUserMediaForTest(UserMediaRequest* user_media_request) {
    *state_ = kRequestNotComplete;
    RequestUserMedia(user_media_request);
    base::RunLoop().RunUntilIdle();
  }

  void RequestUserMediaForTest() {
    UserMediaRequest* user_media_request = UserMediaRequest::CreateForTesting(
        CreateDefaultConstraints(), CreateDefaultConstraints());
    RequestUserMediaForTest(user_media_request);
  }

 private:
  raw_ptr<RequestState> state_;
};

class UserMediaChromeClient : public EmptyChromeClient {
 public:
  UserMediaChromeClient() {
    screen_info_.rect = gfx::Rect(blink::kDefaultScreenCastWidth,
                                  blink::kDefaultScreenCastHeight);
  }
  const display::ScreenInfo& GetScreenInfo(LocalFrame&) const override {
    return screen_info_;
  }

 private:
  display::ScreenInfo screen_info_;
};

}  // namespace

class UserMediaClientTest : public ::testing::Test {
 public:
  UserMediaClientTest()
      : user_media_processor_receiver_(&media_devices_dispatcher_),
        display_user_media_processor_receiver_(&media_devices_dispatcher_),
        user_media_client_receiver_(&media_devices_dispatcher_) {}

  void SetUp() override {
    // Create our test object.
    auto* msd_observer = new blink::WebMediaStreamDeviceObserver(nullptr);

    ChromeClient* chrome_client = MakeGarbageCollected<UserMediaChromeClient>();
    dummy_page_holder_ =
        std::make_unique<DummyPageHolder>(gfx::Size(1, 1), chrome_client);

    user_media_processor_ = MakeGarbageCollected<UserMediaProcessorUnderTest>(
        &(dummy_page_holder_->GetFrame()), base::WrapUnique(msd_observer),
        user_media_processor_receiver_.BindNewPipeAndPassRemote(), &state_);
    user_media_processor_->set_media_stream_dispatcher_host_for_testing(
        mock_dispatcher_host_.CreatePendingRemoteAndBind());

    auto* display_msd_observer =
        new blink::WebMediaStreamDeviceObserver(nullptr);
    display_user_media_processor_ =
        MakeGarbageCollected<UserMediaProcessorUnderTest>(
            &(dummy_page_holder_->GetFrame()),
            base::WrapUnique(display_msd_observer),
            display_user_media_processor_receiver_.BindNewPipeAndPassRemote(),
            &state_);
    display_user_media_processor_->set_media_stream_dispatcher_host_for_testing(
        display_mock_dispatcher_host_.CreatePendingRemoteAndBind());

    user_media_client_impl_ = MakeGarbageCollected<UserMediaClientUnderTest>(
        &(dummy_page_holder_->GetFrame()), user_media_processor_,
        display_user_media_processor_, &state_);

    user_media_client_impl_->SetMediaDevicesDispatcherForTesting(
        user_media_client_receiver_.BindNewPipeAndPassRemote());
  }

  void TearDown() override {
    user_media_client_impl_->ContextDestroyed();
    user_media_client_impl_ = nullptr;

    blink::WebHeap::CollectAllGarbageForTesting();
  }

  void LoadNewDocumentInFrame() {
    user_media_client_impl_->ContextDestroyed();
    base::RunLoop().RunUntilIdle();
  }

  MediaStreamDescriptor* RequestLocalMediaStream() {
    user_media_client_impl_->RequestUserMediaForTest();
    StartMockedVideoSource(user_media_processor_);

    EXPECT_EQ(kRequestSucceeded, request_state());

    MediaStreamDescriptor* desc =
        user_media_processor_->last_generated_descriptor();
    auto audio_components = desc->AudioComponents();
    auto video_components = desc->VideoComponents();

    EXPECT_EQ(1u, audio_components.size());
    EXPECT_EQ(1u, video_components.size());
    EXPECT_NE(audio_components[0]->Id(), video_components[0]->Id());
    return desc;
  }

  MediaStreamTrack* RequestLocalVideoTrack() {
    UserMediaRequest* user_media_request = UserMediaRequest::CreateForTesting(
        MediaConstraints(), CreateDefaultConstraints());
    user_media_client_impl_->RequestUserMediaForTest(user_media_request);
    StartMockedVideoSource(user_media_processor_);
    EXPECT_EQ(kRequestSucceeded, request_state());

    MediaStreamDescriptor* descriptor =
        user_media_processor_->last_generated_descriptor();
    auto audio_components = descriptor->AudioComponents();
    auto video_components = descriptor->VideoComponents();

    EXPECT_EQ(audio_components.size(), 0U);
    EXPECT_EQ(video_components.size(), 1U);

    return MakeGarbageCollected<MediaStreamTrackImpl>(
        /*execution_context=*/nullptr, video_components[0]);
  }

  MediaStreamComponent* RequestLocalAudioTrackWithAssociatedSink(
      bool render_to_associated_sink) {
    blink::MockConstraintFactory constraint_factory;
    constraint_factory.basic().render_to_associated_sink.SetExact(
        render_to_associated_sink);
    UserMediaRequest* user_media_request = UserMediaRequest::CreateForTesting(
        constraint_factory.CreateMediaConstraints(), MediaConstraints());
    user_media_client_impl_->RequestUserMediaForTest(user_media_request);

    EXPECT_EQ(kRequestSucceeded, request_state());

    MediaStreamDescriptor* desc =
        user_media_processor_->last_generated_descriptor();
    auto audio_components = desc->AudioComponents();
    auto video_components = desc->VideoComponents();

    EXPECT_EQ(audio_components.size(), 1u);
    EXPECT_TRUE(video_components.empty());

    return audio_components[0].Get();
  }

  void StartMockedVideoSource(
      UserMediaProcessorUnderTest* user_media_processor) {
    MockMediaStreamVideoCapturerSource* video_source =
        user_media_processor->last_created_video_source();
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
      const MediaConstraints& audio_constraints,
      const MediaConstraints& video_constraints,
      const String& expected_audio_device_id,
      const String& expected_video_device_id) {
    DCHECK(!audio_constraints.IsNull());
    DCHECK(!video_constraints.IsNull());
    UserMediaRequest* request = UserMediaRequest::CreateForTesting(
        audio_constraints, video_constraints);
    user_media_client_impl_->RequestUserMediaForTest(request);
    StartMockedVideoSource(user_media_processor_);

    EXPECT_EQ(kRequestSucceeded, request_state());
    EXPECT_NE(std::nullopt, mock_dispatcher_host_.devices().audio_device);
    EXPECT_NE(std::nullopt, mock_dispatcher_host_.devices().video_device);
    EXPECT_EQ(expected_audio_device_id.Ascii(),
              mock_dispatcher_host_.devices().audio_device.value().id);
    EXPECT_EQ(expected_video_device_id.Ascii(),
              mock_dispatcher_host_.devices().video_device.value().id);
  }

  void ApplyConstraintsVideoMode(
      MediaStreamTrack* track,
      int width,
      int height,
      const std::optional<double>& frame_rate = std::optional<double>()) {
    blink::MockConstraintFactory factory;
    factory.basic().width.SetExact(width);
    factory.basic().height.SetExact(height);
    if (frame_rate)
      factory.basic().frame_rate.SetExact(*frame_rate);

    auto* apply_constraints_request =
        MakeGarbageCollected<ApplyConstraintsRequest>(
            track, factory.CreateMediaConstraints(), nullptr);
    user_media_client_impl_->ApplyConstraints(apply_constraints_request);
    base::RunLoop().RunUntilIdle();
  }

  RequestState request_state() const { return state_; }

  UserMediaProcessorUnderTest* UserMediaProcessorForDisplayCapture() {
    return display_user_media_processor_;
  }

  const MockMojoMediaStreamDispatcherHost&
  MediaStreamDispatcherHostForDisplayCapture() {
    return display_mock_dispatcher_host_;
  }

 protected:
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport>
      testing_platform_;
  test::TaskEnvironment task_environment_;
  MockMojoMediaStreamDispatcherHost mock_dispatcher_host_;
  MockMojoMediaStreamDispatcherHost display_mock_dispatcher_host_;
  MockMediaDevicesDispatcherHost media_devices_dispatcher_;
  mojo::Receiver<blink::mojom::blink::MediaDevicesDispatcherHost>
      user_media_processor_receiver_;
  mojo::Receiver<blink::mojom::blink::MediaDevicesDispatcherHost>
      display_user_media_processor_receiver_;
  mojo::Receiver<blink::mojom::blink::MediaDevicesDispatcherHost>
      user_media_client_receiver_;

  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  WeakPersistent<UserMediaProcessorUnderTest> user_media_processor_;
  WeakPersistent<UserMediaProcessorUnderTest> display_user_media_processor_;
  Persistent<UserMediaClientUnderTest> user_media_client_impl_;
  RequestState state_ = kRequestNotStarted;
  raw_ptr<FakeDeviceIds> fake_ids_ = FakeDeviceIds::GetInstance();
};

TEST_F(UserMediaClientTest, GenerateMediaStream) {
  // Generate a stream with both audio and video.
  MediaStreamDescriptor* mixed_desc = RequestLocalMediaStream();
  EXPECT_TRUE(mixed_desc);
}

// Test that the same source object is used if two MediaStreams are generated
// using the same source.
TEST_F(UserMediaClientTest, GenerateTwoMediaStreamsWithSameSource) {
  mock_dispatcher_host_.SetAppendSessionIdToDeviceIds(true);
  MediaStreamDescriptor* desc1 = RequestLocalMediaStream();
  MediaStreamDescriptor* desc2 = RequestLocalMediaStream();

  auto desc1_video_components = desc1->VideoComponents();
  auto desc2_video_components = desc2->VideoComponents();
  EXPECT_EQ(desc1_video_components[0]->Source()->GetName(),
            desc2_video_components[0]->Source()->GetName());

  EXPECT_EQ(desc1_video_components[0]->Source()->GetPlatformSource(),
            desc2_video_components[0]->Source()->GetPlatformSource());

  auto desc1_audio_components = desc1->AudioComponents();
  auto desc2_audio_components = desc2->AudioComponents();
  EXPECT_EQ(desc1_audio_components[0]->Source()->GetName(),
            desc2_audio_components[0]->Source()->GetName());

  EXPECT_EQ(MediaStreamAudioSource::From(desc1_audio_components[0]->Source()),
            MediaStreamAudioSource::From(desc2_audio_components[0]->Source()));
}

// Test that the same source object is not used if two MediaStreams are
// generated using different sources.
TEST_F(UserMediaClientTest, GenerateTwoMediaStreamsWithDifferentSources) {
  mock_dispatcher_host_.SetAppendSessionIdToDeviceIds(true);
  MediaStreamDescriptor* desc1 = RequestLocalMediaStream();
  // Make sure another device is selected (another |session_id|) in  the next
  // gUM request.
  mock_dispatcher_host_.ResetSessionId();
  MediaStreamDescriptor* desc2 = RequestLocalMediaStream();

  auto desc1_video_components = desc1->VideoComponents();
  auto desc2_video_components = desc2->VideoComponents();
  EXPECT_NE(desc1_video_components[0]->Source()->GetName(),
            desc2_video_components[0]->Source()->GetName());

  EXPECT_NE(desc1_video_components[0]->Source()->GetPlatformSource(),
            desc2_video_components[0]->Source()->GetPlatformSource());

  auto desc1_audio_components = desc1->AudioComponents();
  auto desc2_audio_components = desc2->AudioComponents();
  EXPECT_NE(desc1_audio_components[0]->Source()->GetName(),
            desc2_audio_components[0]->Source()->GetName());

  EXPECT_NE(MediaStreamAudioSource::From(desc1_audio_components[0]->Source()),
            MediaStreamAudioSource::From(desc2_audio_components[0]->Source()));
}

TEST_F(UserMediaClientTest, StopLocalTracks) {
  // Generate a stream with both audio and video.
  MediaStreamDescriptor* mixed_desc = RequestLocalMediaStream();

  auto audio_components = mixed_desc->AudioComponents();
  MediaStreamTrackPlatform* audio_track = MediaStreamTrackPlatform::GetTrack(
      WebMediaStreamTrack(audio_components[0]));
  audio_track->Stop();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_dispatcher_host_.stop_audio_device_counter());

  auto video_components = mixed_desc->VideoComponents();
  MediaStreamTrackPlatform* video_track = MediaStreamTrackPlatform::GetTrack(
      WebMediaStreamTrack(video_components[0]));
  video_track->Stop();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_dispatcher_host_.stop_video_device_counter());
}

// This test that a source is not stopped even if the tracks in a
// MediaStream is stopped if there are two MediaStreams with tracks using the
// same device. The source is stopped
// if there are no more MediaStream tracks using the device.
TEST_F(UserMediaClientTest, StopLocalTracksWhenTwoStreamUseSameDevices) {
  // Generate a stream with both audio and video.
  MediaStreamDescriptor* desc1 = RequestLocalMediaStream();
  MediaStreamDescriptor* desc2 = RequestLocalMediaStream();

  auto audio_components1 = desc1->AudioComponents();
  MediaStreamTrackPlatform* audio_track1 = MediaStreamTrackPlatform::GetTrack(
      WebMediaStreamTrack(audio_components1[0]));
  audio_track1->Stop();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, mock_dispatcher_host_.stop_audio_device_counter());

  auto audio_components2 = desc2->AudioComponents();
  MediaStreamTrackPlatform* audio_track2 = MediaStreamTrackPlatform::GetTrack(
      WebMediaStreamTrack(audio_components2[0]));
  audio_track2->Stop();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_dispatcher_host_.stop_audio_device_counter());

  auto video_components1 = desc1->VideoComponents();
  MediaStreamTrackPlatform* video_track1 = MediaStreamTrackPlatform::GetTrack(
      WebMediaStreamTrack(video_components1[0]));
  video_track1->Stop();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, mock_dispatcher_host_.stop_video_device_counter());

  auto video_components2 = desc2->VideoComponents();
  MediaStreamTrackPlatform* video_track2 = MediaStreamTrackPlatform::GetTrack(
      WebMediaStreamTrack(video_components2[0]));
  video_track2->Stop();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_dispatcher_host_.stop_video_device_counter());
}

TEST_F(UserMediaClientTest, StopSourceWhenMediaStreamGoesOutOfScope) {
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
TEST_F(UserMediaClientTest, LoadNewDocumentInFrame) {
  // Test a stream with both audio and video.
  MediaStreamDescriptor* mixed_desc = RequestLocalMediaStream();
  EXPECT_TRUE(mixed_desc);
  MediaStreamDescriptor* desc2 = RequestLocalMediaStream();
  EXPECT_TRUE(desc2);
  LoadNewDocumentInFrame();
  WebHeap::CollectAllGarbageForTesting();
  EXPECT_EQ(1, mock_dispatcher_host_.stop_audio_device_counter());
  EXPECT_EQ(1, mock_dispatcher_host_.stop_video_device_counter());
}

// This test what happens if a video source to a MediaSteam fails to start.
TEST_F(UserMediaClientTest, MediaVideoSourceFailToStart) {
  user_media_client_impl_->RequestUserMediaForTest();
  FailToStartMockedVideoSource();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kRequestFailed, request_state());
  EXPECT_EQ(
      blink::mojom::blink::MediaStreamRequestResult::TRACK_START_FAILURE_VIDEO,
      user_media_processor_->error_reason());
  blink::WebHeap::CollectAllGarbageForTesting();
  EXPECT_EQ(1, mock_dispatcher_host_.request_stream_counter());
  EXPECT_EQ(1, mock_dispatcher_host_.stop_audio_device_counter());
  EXPECT_EQ(1, mock_dispatcher_host_.stop_video_device_counter());
}

// This test what happens if an audio source fail to initialize.
TEST_F(UserMediaClientTest, MediaAudioSourceFailToInitialize) {
  user_media_processor_->SetCreateSourceThatFails(true);
  user_media_client_impl_->RequestUserMediaForTest();
  StartMockedVideoSource(user_media_processor_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kRequestFailed, request_state());
  EXPECT_EQ(
      blink::mojom::blink::MediaStreamRequestResult::TRACK_START_FAILURE_AUDIO,
      user_media_processor_->error_reason());
  blink::WebHeap::CollectAllGarbageForTesting();
  EXPECT_EQ(1, mock_dispatcher_host_.request_stream_counter());
  EXPECT_EQ(1, mock_dispatcher_host_.stop_audio_device_counter());
  EXPECT_EQ(1, mock_dispatcher_host_.stop_video_device_counter());
}

// This test what happens if UserMediaClient is deleted before a source has
// started.
TEST_F(UserMediaClientTest, MediaStreamImplShutDown) {
  user_media_client_impl_->RequestUserMediaForTest();
  EXPECT_EQ(1, mock_dispatcher_host_.request_stream_counter());
  EXPECT_EQ(kRequestNotComplete, request_state());
  // TearDown() nulls out |user_media_client_impl_| and forces GC to garbage
  // collect it.
}

// This test what happens if a new document is loaded in the frame while the
// MediaStream is being generated by the blink::WebMediaStreamDeviceObserver.
TEST_F(UserMediaClientTest, ReloadFrameWhileGeneratingStream) {
  mock_dispatcher_host_.DoNotRunCallback();

  user_media_client_impl_->RequestUserMediaForTest();
  LoadNewDocumentInFrame();
  EXPECT_EQ(1, mock_dispatcher_host_.request_stream_counter());
  EXPECT_EQ(0, mock_dispatcher_host_.stop_audio_device_counter());
  EXPECT_EQ(0, mock_dispatcher_host_.stop_video_device_counter());
  EXPECT_EQ(kRequestNotComplete, request_state());
}

// This test what happens if a newdocument is loaded in the frame while the
// sources are being started.
TEST_F(UserMediaClientTest, ReloadFrameWhileGeneratingSources) {
  user_media_client_impl_->RequestUserMediaForTest();
  EXPECT_EQ(1, mock_dispatcher_host_.request_stream_counter());
  LoadNewDocumentInFrame();
  EXPECT_EQ(1, mock_dispatcher_host_.stop_audio_device_counter());
  EXPECT_EQ(1, mock_dispatcher_host_.stop_video_device_counter());
  EXPECT_EQ(kRequestNotComplete, request_state());
}

// This test what happens if stop is called on a track after the frame has
// been reloaded.
TEST_F(UserMediaClientTest, StopTrackAfterReload) {
  MediaStreamDescriptor* mixed_desc = RequestLocalMediaStream();
  EXPECT_EQ(1, mock_dispatcher_host_.request_stream_counter());
  LoadNewDocumentInFrame();
  WebHeap::CollectAllGarbageForTesting();
  EXPECT_EQ(1, mock_dispatcher_host_.stop_audio_device_counter());
  EXPECT_EQ(1, mock_dispatcher_host_.stop_video_device_counter());

  auto audio_components = mixed_desc->AudioComponents();
  MediaStreamTrackPlatform* audio_track = MediaStreamTrackPlatform::GetTrack(
      WebMediaStreamTrack(audio_components[0]));
  audio_track->Stop();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_dispatcher_host_.stop_audio_device_counter());

  auto video_components = mixed_desc->VideoComponents();
  MediaStreamTrackPlatform* video_track = MediaStreamTrackPlatform::GetTrack(
      WebMediaStreamTrack(video_components[0]));
  video_track->Stop();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_dispatcher_host_.stop_video_device_counter());
}

TEST_F(UserMediaClientTest, DefaultConstraintsPropagate) {
  UserMediaRequest* request = UserMediaRequest::CreateForTesting(
      CreateDefaultConstraints(), CreateDefaultConstraints());
  user_media_client_impl_->RequestUserMediaForTest(request);
  blink::AudioCaptureSettings audio_capture_settings =
      user_media_processor_->AudioSettings();
  blink::VideoCaptureSettings video_capture_settings =
      user_media_processor_->VideoSettings();
  user_media_client_impl_->CancelUserMediaRequest(request);

  // Check default values selected by the constraints algorithm.
  EXPECT_TRUE(audio_capture_settings.HasValue());
  EXPECT_EQ(media::AudioDeviceDescription::kDefaultDeviceId,
            audio_capture_settings.device_id());
  EXPECT_TRUE(audio_capture_settings.disable_local_echo());
  EXPECT_FALSE(audio_capture_settings.render_to_associated_sink());

  const blink::AudioProcessingProperties& properties =
      audio_capture_settings.audio_processing_properties();
  EXPECT_EQ(EchoCancellationType::kEchoCancellationAec3,
            properties.echo_cancellation_type);
  EXPECT_FALSE(properties.goog_audio_mirroring);
  EXPECT_TRUE(properties.goog_auto_gain_control);
  // The default value for goog_experimental_echo_cancellation is platform
  // dependent.
  EXPECT_EQ(
      blink::AudioProcessingProperties().goog_experimental_echo_cancellation,
      properties.goog_experimental_echo_cancellation);
  EXPECT_TRUE(properties.goog_noise_suppression);
  EXPECT_TRUE(properties.goog_experimental_noise_suppression);
  EXPECT_TRUE(properties.goog_highpass_filter);

  EXPECT_TRUE(video_capture_settings.HasValue());
  EXPECT_EQ(video_capture_settings.Width(),
            blink::MediaStreamVideoSource::kDefaultWidth);
  EXPECT_EQ(video_capture_settings.Height(),
            blink::MediaStreamVideoSource::kDefaultHeight);
  EXPECT_EQ(
      video_capture_settings.FrameRate(),
      static_cast<float>(blink::MediaStreamVideoSource::kDefaultFrameRate));
  EXPECT_EQ(video_capture_settings.ResolutionChangePolicy(),
            media::ResolutionChangePolicy::FIXED_RESOLUTION);
  EXPECT_FALSE(video_capture_settings.noise_reduction());
  EXPECT_FALSE(video_capture_settings.min_frame_rate().has_value());

  const blink::VideoTrackAdapterSettings& track_settings =
      video_capture_settings.track_adapter_settings();
  EXPECT_FALSE(track_settings.target_size().has_value());
  EXPECT_EQ(
      track_settings.min_aspect_ratio(),
      1.0 / static_cast<double>(blink::MediaStreamVideoSource::kDefaultHeight));
  EXPECT_EQ(track_settings.max_aspect_ratio(),
            static_cast<double>(blink::MediaStreamVideoSource::kDefaultWidth));
  EXPECT_EQ(track_settings.max_frame_rate(), std::nullopt);
}

TEST_F(UserMediaClientTest, DefaultTabCapturePropagate) {
  blink::MockConstraintFactory factory;
  factory.basic().media_stream_source.SetExact(kMediaStreamSourceTab);
  MediaConstraints audio_constraints = factory.CreateMediaConstraints();
  MediaConstraints video_constraints = factory.CreateMediaConstraints();
  UserMediaRequest* request =
      UserMediaRequest::CreateForTesting(audio_constraints, video_constraints);
  user_media_client_impl_->RequestUserMediaForTest(request);
  blink::AudioCaptureSettings audio_capture_settings =
      UserMediaProcessorForDisplayCapture()->AudioSettings();
  blink::VideoCaptureSettings video_capture_settings =
      UserMediaProcessorForDisplayCapture()->VideoSettings();
  user_media_client_impl_->CancelUserMediaRequest(request);

  // Check default values selected by the constraints algorithm.
  EXPECT_TRUE(audio_capture_settings.HasValue());
  EXPECT_EQ(std::string(), audio_capture_settings.device_id());
  EXPECT_TRUE(audio_capture_settings.disable_local_echo());
  EXPECT_FALSE(audio_capture_settings.render_to_associated_sink());

  const blink::AudioProcessingProperties& properties =
      audio_capture_settings.audio_processing_properties();
  EXPECT_EQ(EchoCancellationType::kEchoCancellationDisabled,
            properties.echo_cancellation_type);
  EXPECT_FALSE(properties.goog_audio_mirroring);
  EXPECT_FALSE(properties.goog_auto_gain_control);
  EXPECT_FALSE(properties.goog_experimental_echo_cancellation);
  EXPECT_FALSE(properties.goog_noise_suppression);
  EXPECT_FALSE(properties.goog_experimental_noise_suppression);
  EXPECT_FALSE(properties.goog_highpass_filter);

  EXPECT_TRUE(video_capture_settings.HasValue());
  EXPECT_EQ(video_capture_settings.Width(), blink::kDefaultScreenCastWidth);
  EXPECT_EQ(video_capture_settings.Height(), blink::kDefaultScreenCastHeight);
  EXPECT_EQ(video_capture_settings.FrameRate(),
            blink::kDefaultScreenCastFrameRate);
  EXPECT_EQ(video_capture_settings.ResolutionChangePolicy(),
            media::ResolutionChangePolicy::FIXED_RESOLUTION);
  EXPECT_FALSE(video_capture_settings.noise_reduction());
  EXPECT_FALSE(video_capture_settings.min_frame_rate().has_value());
  EXPECT_FALSE(video_capture_settings.max_frame_rate().has_value());

  const blink::VideoTrackAdapterSettings& track_settings =
      video_capture_settings.track_adapter_settings();
  EXPECT_EQ(track_settings.target_width(), blink::kDefaultScreenCastWidth);
  EXPECT_EQ(track_settings.target_height(), blink::kDefaultScreenCastHeight);
  EXPECT_EQ(track_settings.min_aspect_ratio(),
            1.0 / blink::kMaxScreenCastDimension);
  EXPECT_EQ(track_settings.max_aspect_ratio(), blink::kMaxScreenCastDimension);
  EXPECT_EQ(track_settings.max_frame_rate(), std::nullopt);
}

TEST_F(UserMediaClientTest, DefaultDesktopCapturePropagate) {
  blink::MockConstraintFactory factory;
  factory.basic().media_stream_source.SetExact(kMediaStreamSourceDesktop);
  MediaConstraints audio_constraints = factory.CreateMediaConstraints();
  MediaConstraints video_constraints = factory.CreateMediaConstraints();
  UserMediaRequest* request =
      UserMediaRequest::CreateForTesting(audio_constraints, video_constraints);
  user_media_client_impl_->RequestUserMediaForTest(request);
  blink::AudioCaptureSettings audio_capture_settings =
      UserMediaProcessorForDisplayCapture()->AudioSettings();
  blink::VideoCaptureSettings video_capture_settings =
      UserMediaProcessorForDisplayCapture()->VideoSettings();
  user_media_client_impl_->CancelUserMediaRequest(request);
  base::RunLoop().RunUntilIdle();

  // Check default values selected by the constraints algorithm.
  EXPECT_TRUE(audio_capture_settings.HasValue());
  EXPECT_EQ(std::string(), audio_capture_settings.device_id());
  EXPECT_FALSE(audio_capture_settings.disable_local_echo());
  EXPECT_FALSE(audio_capture_settings.render_to_associated_sink());

  const blink::AudioProcessingProperties& properties =
      audio_capture_settings.audio_processing_properties();
  EXPECT_EQ(EchoCancellationType::kEchoCancellationDisabled,
            properties.echo_cancellation_type);
  EXPECT_FALSE(properties.goog_audio_mirroring);
  EXPECT_FALSE(properties.goog_auto_gain_control);
  EXPECT_FALSE(properties.goog_experimental_echo_cancellation);
  EXPECT_FALSE(properties.goog_noise_suppression);
  EXPECT_FALSE(properties.goog_experimental_noise_suppression);
  EXPECT_FALSE(properties.goog_highpass_filter);

  EXPECT_TRUE(video_capture_settings.HasValue());
  EXPECT_EQ(video_capture_settings.Width(), blink::kDefaultScreenCastWidth);
  EXPECT_EQ(video_capture_settings.Height(), blink::kDefaultScreenCastHeight);
  EXPECT_EQ(video_capture_settings.FrameRate(),
            blink::kDefaultScreenCastFrameRate);
  EXPECT_EQ(video_capture_settings.ResolutionChangePolicy(),
            media::ResolutionChangePolicy::ANY_WITHIN_LIMIT);
  EXPECT_FALSE(video_capture_settings.noise_reduction());
  EXPECT_FALSE(video_capture_settings.min_frame_rate().has_value());
  EXPECT_FALSE(video_capture_settings.max_frame_rate().has_value());

  const blink::VideoTrackAdapterSettings& track_settings =
      video_capture_settings.track_adapter_settings();
  EXPECT_EQ(track_settings.target_width(), blink::kDefaultScreenCastWidth);
  EXPECT_EQ(track_settings.target_height(), blink::kDefaultScreenCastHeight);
  EXPECT_EQ(track_settings.min_aspect_ratio(),
            1.0 / blink::kMaxScreenCastDimension);
  EXPECT_EQ(track_settings.max_aspect_ratio(), blink::kMaxScreenCastDimension);
  EXPECT_EQ(track_settings.max_frame_rate(), std::nullopt);
}

TEST_F(UserMediaClientTest, NonDefaultAudioConstraintsPropagate) {
  mock_dispatcher_host_.DoNotRunCallback();

  blink::MockConstraintFactory factory;
  factory.basic().device_id.SetExact(fake_ids_->audio_input_1);
  factory.basic().disable_local_echo.SetExact(true);
  factory.basic().render_to_associated_sink.SetExact(true);
  factory.basic().echo_cancellation.SetExact(false);
  factory.basic().goog_audio_mirroring.SetExact(true);
  MediaConstraints audio_constraints = factory.CreateMediaConstraints();
  // Request contains only audio
  UserMediaRequest* request =
      UserMediaRequest::CreateForTesting(audio_constraints, MediaConstraints());
  user_media_client_impl_->RequestUserMediaForTest(request);
  blink::AudioCaptureSettings audio_capture_settings =
      user_media_processor_->AudioSettings();
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_FUCHSIA)
  if (base::FeatureList::IsEnabled(
          features::kGetUserMediaDeferredDeviceSettingsSelection)) {
    audio_capture_settings = user_media_processor_->EligibleAudioSettings()[0];
  }
#endif

  blink::VideoCaptureSettings video_capture_settings =
      user_media_processor_->VideoSettings();
  user_media_client_impl_->CancelUserMediaRequest(request);

  EXPECT_FALSE(video_capture_settings.HasValue());

  EXPECT_TRUE(audio_capture_settings.HasValue());
  EXPECT_EQ(fake_ids_->audio_input_1.Ascii(),
            audio_capture_settings.device_id());
  EXPECT_TRUE(audio_capture_settings.disable_local_echo());
  EXPECT_TRUE(audio_capture_settings.render_to_associated_sink());

  const blink::AudioProcessingProperties& properties =
      audio_capture_settings.audio_processing_properties();
  EXPECT_EQ(EchoCancellationType::kEchoCancellationDisabled,
            properties.echo_cancellation_type);
  EXPECT_TRUE(properties.goog_audio_mirroring);
  EXPECT_FALSE(properties.goog_auto_gain_control);
  EXPECT_FALSE(properties.goog_experimental_echo_cancellation);
  EXPECT_FALSE(properties.goog_noise_suppression);
  EXPECT_FALSE(properties.goog_experimental_noise_suppression);
  EXPECT_FALSE(properties.goog_highpass_filter);
}

TEST_F(UserMediaClientTest, CreateWithMandatoryInvalidAudioDeviceId) {
  MediaConstraints audio_constraints =
      CreateDeviceConstraints(fake_ids_->invalid_device);
  UserMediaRequest* request =
      UserMediaRequest::CreateForTesting(audio_constraints, MediaConstraints());
  user_media_client_impl_->RequestUserMediaForTest(request);
  EXPECT_EQ(kRequestFailed, request_state());
}

TEST_F(UserMediaClientTest, CreateWithMandatoryInvalidVideoDeviceId) {
  MediaConstraints video_constraints =
      CreateDeviceConstraints(fake_ids_->invalid_device);
  UserMediaRequest* request =
      UserMediaRequest::CreateForTesting(MediaConstraints(), video_constraints);
  user_media_client_impl_->RequestUserMediaForTest(request);
  EXPECT_EQ(kRequestFailed, request_state());
}

TEST_F(UserMediaClientTest, CreateWithMandatoryValidDeviceIds) {
  MediaConstraints audio_constraints =
      CreateDeviceConstraints(fake_ids_->audio_input_1);
  MediaConstraints video_constraints =
      CreateDeviceConstraints(fake_ids_->video_input_1);
  TestValidRequestWithConstraints(audio_constraints, video_constraints,
                                  fake_ids_->audio_input_1,
                                  fake_ids_->video_input_1);
}

TEST_F(UserMediaClientTest, CreateWithBasicIdealValidDeviceId) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_FUCHSIA)
  // Ideal device ids are overridden by user preference under this flag.
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndDisableFeature(
      features::kGetUserMediaDeferredDeviceSettingsSelection);
#endif
  MediaConstraints audio_constraints =
      CreateDeviceConstraints(g_empty_string, fake_ids_->audio_input_1);
  MediaConstraints video_constraints =
      CreateDeviceConstraints(g_empty_string, fake_ids_->video_input_1);
  TestValidRequestWithConstraints(audio_constraints, video_constraints,
                                  fake_ids_->audio_input_1,
                                  fake_ids_->video_input_1);
}

TEST_F(UserMediaClientTest, CreateWithAdvancedExactValidDeviceId) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_FUCHSIA)
  // Advanced exact device id constraints are overridden by user preference
  // under this flag.
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndDisableFeature(
      features::kGetUserMediaDeferredDeviceSettingsSelection);
#endif
  MediaConstraints audio_constraints = CreateDeviceConstraints(
      g_empty_string, g_empty_string, fake_ids_->audio_input_1);
  MediaConstraints video_constraints = CreateDeviceConstraints(
      g_empty_string, g_empty_string, fake_ids_->video_input_1);
  TestValidRequestWithConstraints(audio_constraints, video_constraints,
                                  fake_ids_->audio_input_1,
                                  fake_ids_->video_input_1);
}

TEST_F(UserMediaClientTest, CreateWithAllOptionalInvalidDeviceId) {
  MediaConstraints audio_constraints = CreateDeviceConstraints(
      g_empty_string, fake_ids_->invalid_device, fake_ids_->invalid_device);
  MediaConstraints video_constraints = CreateDeviceConstraints(
      g_empty_string, fake_ids_->invalid_device, fake_ids_->invalid_device);
  // MockMojoMediaStreamDispatcherHost uses empty string as default audio device
  // ID. MockMediaDevicesDispatcher uses the first device in the enumeration as
  // default audio or video device ID.
  String expected_audio_device_id =
      String(media::AudioDeviceDescription::kDefaultDeviceId);
  TestValidRequestWithConstraints(audio_constraints, video_constraints,
                                  expected_audio_device_id,
                                  fake_ids_->video_input_1);
}

TEST_F(UserMediaClientTest, CreateWithFacingModeUser) {
  MediaConstraints audio_constraints =
      CreateDeviceConstraints(fake_ids_->audio_input_1);
  MediaConstraints video_constraints = CreateFacingModeConstraints("user");
  // fake_ids_->video_input_1 has user facing mode.
  TestValidRequestWithConstraints(audio_constraints, video_constraints,
                                  fake_ids_->audio_input_1,
                                  fake_ids_->video_input_1);
}

TEST_F(UserMediaClientTest, CreateWithFacingModeEnvironment) {
  MediaConstraints audio_constraints =
      CreateDeviceConstraints(fake_ids_->audio_input_1);
  MediaConstraints video_constraints =
      CreateFacingModeConstraints("environment");
  // fake_ids_->video_input_2 has kEnvironment facing mode.
  TestValidRequestWithConstraints(audio_constraints, video_constraints,
                                  fake_ids_->audio_input_1,
                                  fake_ids_->video_input_2);
}

TEST_F(UserMediaClientTest, ApplyConstraintsVideoDeviceSingleTrack) {
  MediaStreamTrack* track = RequestLocalVideoTrack();
  MediaStreamComponent* component = track->Component();
  MediaStreamVideoTrack* platform_track =
      MediaStreamVideoTrack::From(component);
  blink::MediaStreamVideoSource* source = platform_track->source();
  CheckVideoSource(source, 0, 0, 0.0);

  media_devices_dispatcher_.SetVideoSource(source);

  // The following applyConstraint() request should force a source restart and
  // produce a video mode with 1024x768.
  ApplyConstraintsVideoMode(track, 1024, 768);
  CheckVideoSourceAndTrack(source, 1024, 768, 20.0, component, 1024, 768, 20.0);

  // The following applyConstraints() requests should not result in a source
  // restart since the only format supported by the mock MDDH that supports
  // 801x600 is the existing 1024x768 mode with downscaling.
  ApplyConstraintsVideoMode(track, 801, 600);
  CheckVideoSourceAndTrack(source, 1024, 768, 20.0, component, 801, 600, 20.0);

  // The following applyConstraints() requests should result in a source restart
  // since there is a native mode of 800x600 supported by the mock MDDH.
  ApplyConstraintsVideoMode(track, 800, 600);
  CheckVideoSourceAndTrack(source, 800, 600, 30.0, component, 800, 600, 30.0);

  // The following applyConstraints() requests should fail since the mock MDDH
  // does not have any mode that can produce 2000x2000.
  ApplyConstraintsVideoMode(track, 2000, 2000);
  CheckVideoSourceAndTrack(source, 800, 600, 30.0, component, 800, 600, 30.0);
}

TEST_F(UserMediaClientTest, CameraCaptureCapabilityHistograms) {
  // With two HD/FullHD capable devices.
  {
    base::HistogramTester histogram_tester;

    Vector<blink::mojom::blink::VideoInputDeviceCapabilitiesPtr> capabilities;
    // The first device supports 360p and 720p.
    blink::mojom::blink::VideoInputDeviceCapabilitiesPtr device1 =
        blink::mojom::blink::VideoInputDeviceCapabilities::New();
    device1->device_id = FakeDeviceIds::GetInstance()->video_input_1;
    device1->group_id = String("dummy1");
    device1->facing_mode = mojom::blink::FacingMode::kUser;
    device1->formats.push_back(media::VideoCaptureFormat(
        gfx::Size(640, 360), 30.0f, media::PIXEL_FORMAT_NV12));
    device1->formats.push_back(media::VideoCaptureFormat(
        gfx::Size(1280, 720), 30.0f, media::PIXEL_FORMAT_NV12));
    capabilities.push_back(std::move(device1));
    // The second device supports 480p and 1080p.
    blink::mojom::blink::VideoInputDeviceCapabilitiesPtr device2 =
        blink::mojom::blink::VideoInputDeviceCapabilities::New();
    device2->device_id = FakeDeviceIds::GetInstance()->video_input_2;
    device2->group_id = String("dummy2");
    device2->facing_mode = mojom::blink::FacingMode::kUser;
    device2->formats.push_back(media::VideoCaptureFormat(
        gfx::Size(640, 480), 30.0f, media::PIXEL_FORMAT_NV12));
    device2->formats.push_back(media::VideoCaptureFormat(
        gfx::Size(1920, 1080), 30.0f, media::PIXEL_FORMAT_NV12));
    capabilities.push_back(std::move(device2));
    media_devices_dispatcher_.SetVideoInputCapabilities(
        std::move(capabilities));

    // Perform getUserMedia() and verify one camera capability value per device.
    RequestLocalVideoTrack();
    histogram_tester.ExpectBucketCount(
        "Media.MediaDevices.GetUserMedia.CameraCaptureCapability",
        CameraCaptureCapability::kHdOrFullHd, 0);
    histogram_tester.ExpectBucketCount(
        "Media.MediaDevices.GetUserMedia.CameraCaptureCapability",
        CameraCaptureCapability::kHdOrFullHd_360p, 1);
    histogram_tester.ExpectBucketCount(
        "Media.MediaDevices.GetUserMedia.CameraCaptureCapability",
        CameraCaptureCapability::kHdOrFullHd_480p, 1);
    histogram_tester.ExpectBucketCount(
        "Media.MediaDevices.GetUserMedia.CameraCaptureCapability",
        CameraCaptureCapability::kHdOrFullHd_360p_480p, 0);
    histogram_tester.ExpectBucketCount(
        "Media.MediaDevices.GetUserMedia.CameraCaptureCapability",
        CameraCaptureCapability::kHdAndFullHdMissing, 0);
  }

  // With three devices: No HD, HD with both 360p and 480p, and HD with neither.
  {
    base::HistogramTester histogram_tester;

    Vector<blink::mojom::blink::VideoInputDeviceCapabilitiesPtr> capabilities;
    // The first device supports 360p and 480p.
    blink::mojom::blink::VideoInputDeviceCapabilitiesPtr device1 =
        blink::mojom::blink::VideoInputDeviceCapabilities::New();
    device1->device_id = FakeDeviceIds::GetInstance()->video_input_1;
    device1->group_id = String("dummy1");
    device1->facing_mode = mojom::blink::FacingMode::kUser;
    device1->formats.push_back(media::VideoCaptureFormat(
        gfx::Size(640, 360), 30.0f, media::PIXEL_FORMAT_NV12));
    device1->formats.push_back(media::VideoCaptureFormat(
        gfx::Size(640, 480), 30.0f, media::PIXEL_FORMAT_NV12));
    capabilities.push_back(std::move(device1));
    // The second device supports 360p, 480p and 720p.
    blink::mojom::blink::VideoInputDeviceCapabilitiesPtr device2 =
        blink::mojom::blink::VideoInputDeviceCapabilities::New();
    device2->device_id = FakeDeviceIds::GetInstance()->video_input_2;
    device2->group_id = String("dummy2");
    device2->facing_mode = mojom::blink::FacingMode::kUser;
    device2->formats.push_back(media::VideoCaptureFormat(
        gfx::Size(640, 360), 30.0f, media::PIXEL_FORMAT_NV12));
    device2->formats.push_back(media::VideoCaptureFormat(
        gfx::Size(640, 480), 30.0f, media::PIXEL_FORMAT_NV12));
    device2->formats.push_back(media::VideoCaptureFormat(
        gfx::Size(1280, 720), 30.0f, media::PIXEL_FORMAT_NV12));
    capabilities.push_back(std::move(device2));
    // The third device supports 720p and 1080p.
    blink::mojom::blink::VideoInputDeviceCapabilitiesPtr device3 =
        blink::mojom::blink::VideoInputDeviceCapabilities::New();
    device3->device_id = FakeDeviceIds::GetInstance()->video_input_3;
    device3->group_id = String("dummy3");
    device3->facing_mode = mojom::blink::FacingMode::kUser;
    device3->formats.push_back(media::VideoCaptureFormat(
        gfx::Size(1280, 720), 30.0f, media::PIXEL_FORMAT_NV12));
    device3->formats.push_back(media::VideoCaptureFormat(
        gfx::Size(1920, 1080), 30.0f, media::PIXEL_FORMAT_NV12));
    capabilities.push_back(std::move(device3));
    media_devices_dispatcher_.SetVideoInputCapabilities(
        std::move(capabilities));

    // Perform getUserMedia() and verify one camera capability value per device.
    RequestLocalVideoTrack();
    histogram_tester.ExpectBucketCount(
        "Media.MediaDevices.GetUserMedia.CameraCaptureCapability",
        CameraCaptureCapability::kHdOrFullHd, 1);
    histogram_tester.ExpectBucketCount(
        "Media.MediaDevices.GetUserMedia.CameraCaptureCapability",
        CameraCaptureCapability::kHdOrFullHd_360p, 0);
    histogram_tester.ExpectBucketCount(
        "Media.MediaDevices.GetUserMedia.CameraCaptureCapability",
        CameraCaptureCapability::kHdOrFullHd_480p, 0);
    histogram_tester.ExpectBucketCount(
        "Media.MediaDevices.GetUserMedia.CameraCaptureCapability",
        CameraCaptureCapability::kHdOrFullHd_360p_480p, 1);
    histogram_tester.ExpectBucketCount(
        "Media.MediaDevices.GetUserMedia.CameraCaptureCapability",
        CameraCaptureCapability::kHdAndFullHdMissing, 1);
  }
}

TEST_F(UserMediaClientTest, ApplyConstraintsVideoDeviceTwoTracks) {
  MediaStreamTrack* track = RequestLocalVideoTrack();
  MediaStreamComponent* component = track->Component();
  MockMediaStreamVideoCapturerSource* source =
      user_media_processor_->last_created_video_source();
  CheckVideoSource(source, 0, 0, 0.0);
  media_devices_dispatcher_.SetVideoSource(source);

  // Switch the source and track to 1024x768@20Hz.
  ApplyConstraintsVideoMode(track, 1024, 768);
  CheckVideoSourceAndTrack(source, 1024, 768, 20.0, component, 1024, 768, 20.0);

  // Create a new track and verify that it uses the same source and that the
  // source's format did not change. The new track uses the same format as the
  // source by default.
  MediaStreamTrack* track2 = RequestLocalVideoTrack();
  MediaStreamComponent* component2 = track2->Component();
  CheckVideoSourceAndTrack(source, 1024, 768, 20.0, component2, 1024, 768,
                           20.0);

  // Use applyConstraints() to change the first track to 800x600 and verify
  // that the source is not reconfigured. Downscaling is used instead because
  // there is more than one track using the source. The second track is left
  // unmodified.
  ApplyConstraintsVideoMode(track, 800, 600);
  CheckVideoSourceAndTrack(source, 1024, 768, 20.0, component, 800, 600, 20.0);
  CheckVideoSourceAndTrack(source, 1024, 768, 20.0, component2, 1024, 768,
                           20.0);

  // Try to use applyConstraints() to change the first track to 800x600@30Hz.
  // It fails, because the source is open in native 20Hz mode and it does not
  // support reconfiguration when more than one track is connected.
  // TODO(guidou): Allow reconfiguring sources with more than one track.
  // https://crbug.com/768205.
  ApplyConstraintsVideoMode(track, 800, 600, 30.0);
  CheckVideoSourceAndTrack(source, 1024, 768, 20.0, component, 800, 600, 20.0);
  CheckVideoSourceAndTrack(source, 1024, 768, 20.0, component2, 1024, 768,
                           20.0);

  // Try to use applyConstraints() to change the first track to 800x600@30Hz.
  // after stopping the second track. In this case, the source is left with a
  // single track and it supports reconfiguration to the requested mode.
  blink::MediaStreamTrackPlatform::GetTrack(WebMediaStreamTrack(component2))
      ->Stop();
  ApplyConstraintsVideoMode(track, 800, 600, 30.0);
  CheckVideoSourceAndTrack(source, 800, 600, 30.0, component, 800, 600, 30.0);
}

TEST_F(UserMediaClientTest, ApplyConstraintsVideoDeviceFailsToStopForRestart) {
  MediaStreamTrack* track = RequestLocalVideoTrack();
  MediaStreamComponent* component = track->Component();
  MockMediaStreamVideoCapturerSource* source =
      user_media_processor_->last_created_video_source();
  CheckVideoSource(source, 0, 0, 0.0);
  media_devices_dispatcher_.SetVideoSource(source);

  // Switch the source and track to 1024x768@20Hz.
  ApplyConstraintsVideoMode(track, 1024, 768);
  CheckVideoSourceAndTrack(source, 1024, 768, 20.0, component, 1024, 768, 20.0);

  // Try to switch the source and track to 640x480. Since the source cannot
  // stop for restart, downscaling is used for the track.
  source->DisableStopForRestart();
  ApplyConstraintsVideoMode(track, 640, 480);
  CheckVideoSourceAndTrack(source, 1024, 768, 20.0, component, 640, 480, 20.0);
}

TEST_F(UserMediaClientTest,
       ApplyConstraintsVideoDeviceFailsToRestartAfterStop) {
  MediaStreamTrack* track = RequestLocalVideoTrack();
  MediaStreamComponent* component = track->Component();
  MockMediaStreamVideoCapturerSource* source =
      user_media_processor_->last_created_video_source();
  CheckVideoSource(source, 0, 0, 0.0);
  media_devices_dispatcher_.SetVideoSource(source);

  // Switch the source and track to 1024x768.
  ApplyConstraintsVideoMode(track, 1024, 768);
  CheckVideoSourceAndTrack(source, 1024, 768, 20.0, component, 1024, 768, 20.0);

  // Try to switch the source and track to 640x480. Since the source cannot
  // restart, source and track are stopped.
  source->DisableRestart();
  ApplyConstraintsVideoMode(track, 640, 480);

  EXPECT_EQ(component->GetReadyState(), MediaStreamSource::kReadyStateEnded);
  EXPECT_FALSE(source->IsRunning());
}

TEST_F(UserMediaClientTest, ApplyConstraintsVideoDeviceStopped) {
  MediaStreamTrack* track = RequestLocalVideoTrack();
  MediaStreamComponent* component = track->Component();
  MockMediaStreamVideoCapturerSource* source =
      user_media_processor_->last_created_video_source();
  CheckVideoSource(source, 0, 0, 0.0);
  media_devices_dispatcher_.SetVideoSource(source);

  // Switch the source and track to 1024x768.
  ApplyConstraintsVideoMode(track, 1024, 768);
  CheckVideoSourceAndTrack(source, 1024, 768, 20.0, component, 1024, 768, 20.0);

  // Try to switch the source and track to 640x480 after stopping the track.
  MediaStreamTrackPlatform* platform_track =
      MediaStreamTrackPlatform::GetTrack(WebMediaStreamTrack(component));
  platform_track->Stop();
  EXPECT_EQ(component->GetReadyState(), MediaStreamSource::kReadyStateEnded);
  EXPECT_FALSE(source->IsRunning());
  {
    MediaStreamTrackPlatform::Settings settings;
    platform_track->GetSettings(settings);
    EXPECT_EQ(settings.width, -1);
    EXPECT_EQ(settings.height, -1);
    EXPECT_EQ(settings.frame_rate, -1.0);
  }

  ApplyConstraintsVideoMode(track, 640, 480);
  EXPECT_EQ(component->GetReadyState(), MediaStreamSource::kReadyStateEnded);
  EXPECT_FALSE(source->IsRunning());
  {
    MediaStreamTrackPlatform::Settings settings;
    platform_track->GetSettings(settings);
    EXPECT_EQ(settings.width, -1);
    EXPECT_EQ(settings.height, -1);
    EXPECT_EQ(settings.frame_rate, -1.0);
  }
}

// These tests check that the associated output device id is
// set according to the renderToAssociatedSink constrainable property.
TEST_F(UserMediaClientTest,
       RenderToAssociatedSinkTrueAssociatedOutputDeviceId) {
  MediaStreamComponent* component =
      RequestLocalAudioTrackWithAssociatedSink(true);
  MediaStreamAudioSource* source =
      MediaStreamAudioSource::From(component->Source());
  EXPECT_TRUE(source->device().matched_output_device_id);
}

TEST_F(UserMediaClientTest,
       RenderToAssociatedSinkFalseAssociatedOutputDeviceId) {
  MediaStreamComponent* component =
      RequestLocalAudioTrackWithAssociatedSink(false);
  MediaStreamAudioSource* source =
      MediaStreamAudioSource::From(component->Source());
  EXPECT_FALSE(source->device().matched_output_device_id);
}

TEST_F(UserMediaClientTest, IsCapturing) {
  EXPECT_FALSE(user_media_client_impl_->IsCapturing());
  MediaStreamDescriptor* descriptor = RequestLocalMediaStream();
  EXPECT_TRUE(user_media_client_impl_->IsCapturing());

  user_media_client_impl_->StopTrack(descriptor->AudioComponents()[0]);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(user_media_client_impl_->IsCapturing());

  user_media_client_impl_->StopTrack(descriptor->VideoComponents()[0]);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(user_media_client_impl_->IsCapturing());
}

TEST_F(UserMediaClientTest, DesktopCaptureChangeSource) {
  blink::MockConstraintFactory factory;
  factory.basic().media_stream_source.SetExact(
      blink::WebString::FromASCII(blink::kMediaStreamSourceDesktop));
  MediaConstraints audio_constraints = factory.CreateMediaConstraints();
  MediaConstraints video_constraints = factory.CreateMediaConstraints();
  UserMediaRequest* request =
      UserMediaRequest::CreateForTesting(audio_constraints, video_constraints);
  user_media_client_impl_->RequestUserMediaForTest(request);

  // Test changing video source.
  MockMediaStreamVideoCapturerSource* video_source =
      UserMediaProcessorForDisplayCapture()->last_created_video_source();
  blink::MediaStreamDevice fake_video_device(
      blink::mojom::blink::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
      fake_ids_->video_input_1.Ascii(), "Fake Video Device");
  EXPECT_CALL(*video_source, ChangeSourceImpl(_));
  UserMediaProcessorForDisplayCapture()->OnDeviceChanged(video_source->device(),
                                                         fake_video_device);

  // Test changing audio source.
  MockLocalMediaStreamAudioSource* audio_source =
      UserMediaProcessorForDisplayCapture()->last_created_local_audio_source();
  EXPECT_NE(audio_source, nullptr);
  blink::MediaStreamDevice fake_audio_device(
      blink::mojom::blink::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE,
      fake_ids_->video_input_1.Ascii(), "Fake Audio Device");
  EXPECT_CALL(*audio_source, EnsureSourceIsStopped()).Times(2);
  UserMediaProcessorForDisplayCapture()->OnDeviceChanged(audio_source->device(),
                                                         fake_audio_device);

  user_media_client_impl_->CancelUserMediaRequest(request);
  base::RunLoop().RunUntilIdle();
}

TEST_F(UserMediaClientTest, DesktopCaptureChangeSourceWithoutAudio) {
  blink::MockConstraintFactory factory;
  factory.basic().media_stream_source.SetExact(kMediaStreamSourceDesktop);
  MediaConstraints audio_constraints = factory.CreateMediaConstraints();
  MediaConstraints video_constraints = factory.CreateMediaConstraints();
  UserMediaRequest* request =
      UserMediaRequest::CreateForTesting(audio_constraints, video_constraints);
  user_media_client_impl_->RequestUserMediaForTest(request);
  EXPECT_NE(
      std::nullopt,
      MediaStreamDispatcherHostForDisplayCapture().devices().audio_device);
  EXPECT_NE(
      std::nullopt,
      MediaStreamDispatcherHostForDisplayCapture().devices().video_device);

  // If the new desktop capture source doesn't have audio, the previous audio
  // device should be stopped. Here |EnsureSourceIsStopped()| should be called
  // only once by |OnDeviceChanged()|.
  MockLocalMediaStreamAudioSource* audio_source =
      UserMediaProcessorForDisplayCapture()->last_created_local_audio_source();
  EXPECT_NE(audio_source, nullptr);
  EXPECT_CALL(*audio_source, EnsureSourceIsStopped()).Times(1);
  blink::MediaStreamDevice fake_audio_device(
      blink::mojom::blink::MediaStreamType::NO_SERVICE, "", "");
  UserMediaProcessorForDisplayCapture()->OnDeviceChanged(audio_source->device(),
                                                         fake_audio_device);
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(audio_source);
  EXPECT_CALL(*audio_source, EnsureSourceIsStopped()).Times(0);
  user_media_client_impl_->CancelUserMediaRequest(request);
  base::RunLoop().RunUntilIdle();
}

TEST_F(UserMediaClientTest, PanConstraintRequestPanTiltZoomPermission) {
  EXPECT_FALSE(UserMediaProcessor::IsPanTiltZoomPermissionRequested(
      CreateDefaultConstraints()));

  blink::MockConstraintFactory basic_factory;
  basic_factory.basic().pan.SetIsPresent(true);
  EXPECT_TRUE(UserMediaProcessor::IsPanTiltZoomPermissionRequested(
      basic_factory.CreateMediaConstraints()));

  blink::MockConstraintFactory advanced_factory;
  auto& exact_advanced = advanced_factory.AddAdvanced();
  exact_advanced.pan.SetIsPresent(true);
  EXPECT_TRUE(UserMediaProcessor::IsPanTiltZoomPermissionRequested(
      advanced_factory.CreateMediaConstraints()));
}

TEST_F(UserMediaClientTest, TiltConstraintRequestPanTiltZoomPermission) {
  EXPECT_FALSE(UserMediaProcessor::IsPanTiltZoomPermissionRequested(
      CreateDefaultConstraints()));

  blink::MockConstraintFactory basic_factory;
  basic_factory.basic().tilt.SetIsPresent(true);
  EXPECT_TRUE(UserMediaProcessor::IsPanTiltZoomPermissionRequested(
      basic_factory.CreateMediaConstraints()));

  blink::MockConstraintFactory advanced_factory;
  auto& exact_advanced = advanced_factory.AddAdvanced();
  exact_advanced.tilt.SetIsPresent(true);
  EXPECT_TRUE(UserMediaProcessor::IsPanTiltZoomPermissionRequested(
      advanced_factory.CreateMediaConstraints()));
}

TEST_F(UserMediaClientTest, ZoomConstraintRequestPanTiltZoomPermission) {
  EXPECT_FALSE(UserMediaProcessor::IsPanTiltZoomPermissionRequested(
      CreateDefaultConstraints()));

  blink::MockConstraintFactory basic_factory;
  basic_factory.basic().zoom.SetIsPresent(true);
  EXPECT_TRUE(UserMediaProcessor::IsPanTiltZoomPermissionRequested(
      basic_factory.CreateMediaConstraints()));

  blink::MockConstraintFactory advanced_factory;
  auto& exact_advanced = advanced_factory.AddAdvanced();
  exact_advanced.zoom.SetIsPresent(true);
  EXPECT_TRUE(UserMediaProcessor::IsPanTiltZoomPermissionRequested(
      advanced_factory.CreateMediaConstraints()));
}

TEST_F(UserMediaClientTest, MultiDeviceOnStreamsGenerated) {
  const size_t devices_count = 5u;
  const int32_t request_id = 0;
  std::unique_ptr<blink::MediaDevicesDispatcherHostMock>
      media_devices_dispatcher_host_mock =
          std::make_unique<blink::MediaDevicesDispatcherHostMock>();
  blink::Member<blink::UserMediaRequest> user_media_request =
      blink::UserMediaRequest::CreateForTesting(CreateDefaultConstraints(),
                                                CreateDefaultConstraints());
  user_media_request->set_request_id(request_id);
  user_media_processor_->ProcessRequest(user_media_request, base::DoNothing());
  user_media_processor_->media_devices_dispatcher_cb_ =
      base::BindLambdaForTesting(
          [&media_devices_dispatcher_host_mock]()
              -> blink::mojom::blink::MediaDevicesDispatcherHost* {
            return media_devices_dispatcher_host_mock.get();
          });

  blink::mojom::blink::StreamDevicesSetPtr stream_devices_set =
      blink::mojom::blink::StreamDevicesSet::New();
  for (size_t stream_index = 0; stream_index < devices_count; ++stream_index) {
    stream_devices_set->stream_devices.emplace_back(
        blink::mojom::blink::StreamDevices::New(std::nullopt,
                                                blink::MediaStreamDevice()));
  }
  user_media_processor_->OnStreamsGenerated(
      request_id, blink::mojom::MediaStreamRequestResult::OK, "",
      std::move(stream_devices_set), /*pan_tilt_zoom_allowed=*/false);
  base::RunLoop run_loop;
  DCHECK_EQ(devices_count, media_devices_dispatcher_host_mock->devices_count());
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_FUCHSIA)

class UserMediaClientDeferredDeviceSelectionTest : public UserMediaClientTest {
  void SetUp() override {
    feature_list.InitWithFeatures(
        /*enabled_features=*/
        {
            features::kCameraMicPreview,
            features::kGetUserMediaDeferredDeviceSettingsSelection,
        },
        /*disabled_features=*/{});
    UserMediaClientTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

TEST_F(UserMediaClientDeferredDeviceSelectionTest, GenerateMediaStream) {
  // Generate a stream with both audio and video.
  MediaStreamDescriptor* mixed_desc = RequestLocalMediaStream();
  EXPECT_TRUE(mixed_desc);
}

TEST_F(UserMediaClientDeferredDeviceSelectionTest,
       CreateWithMandatoryInvalidAudioDeviceId) {
  MediaConstraints audio_constraints =
      CreateDeviceConstraints(fake_ids_->invalid_device);
  UserMediaRequest* request =
      UserMediaRequest::CreateForTesting(audio_constraints, MediaConstraints());
  user_media_client_impl_->RequestUserMediaForTest(request);
  EXPECT_EQ(kRequestFailed, request_state());
}

TEST_F(UserMediaClientDeferredDeviceSelectionTest,
       CreateWithMandatoryInvalidVideoDeviceId) {
  MediaConstraints video_constraints =
      CreateDeviceConstraints(fake_ids_->invalid_device);
  UserMediaRequest* request =
      UserMediaRequest::CreateForTesting(MediaConstraints(), video_constraints);
  user_media_client_impl_->RequestUserMediaForTest(request);
  EXPECT_EQ(kRequestFailed, request_state());
}

TEST_F(UserMediaClientDeferredDeviceSelectionTest,
       CreateWithMandatoryValidDeviceIds) {
  MediaConstraints audio_constraints =
      CreateDeviceConstraints(fake_ids_->audio_input_1);
  MediaConstraints video_constraints =
      CreateDeviceConstraints(fake_ids_->video_input_1);
  TestValidRequestWithConstraints(audio_constraints, video_constraints,
                                  fake_ids_->audio_input_1,
                                  fake_ids_->video_input_1);
}

#endif
}  // namespace blink
