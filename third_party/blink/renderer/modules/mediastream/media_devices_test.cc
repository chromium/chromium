// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_devices.h"

#include <memory>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "third_party/blink/public/mojom/media/capture_handle_config.mojom-blink.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom-blink.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_capture_handle_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_crop_target.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_device_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_user_media_stream_constraints.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/mediastream/crop_target.h"
#include "third_party/blink/renderer/modules/mediastream/media_device_info.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

using ::base::HistogramTester;
using ::blink::mojom::blink::MediaDeviceInfoPtr;
using ::testing::_;
using ::testing::StrictMock;
using MediaDeviceType = ::blink::mojom::MediaDeviceType;

namespace {

String MaxLengthCaptureHandle() {
  String maxHandle = "0123456789abcdef";  // 16 characters.
  while (maxHandle.length() < 1024) {
    maxHandle = maxHandle + maxHandle;
  }
  CHECK_EQ(maxHandle.length(), 1024u) << "Malformed test.";
  return maxHandle;
}

class MockMediaDevicesDispatcherHost final
    : public mojom::blink::MediaDevicesDispatcherHost {
 public:
  MockMediaDevicesDispatcherHost()
      : enumeration_({
            // clang-format off
            {
              {"fake_audio_input_1", "Fake Audio Input 1", "common_group_1"},
              {"fake_audio_input_2", "Fake Audio Input 2", "common_group_2"},
              {"fake_audio_input_3", "Fake Audio Input 3", "audio_input_group"}
            }, {
              {"fake_video_input_1", "Fake Video Input 1", "common_group_1"},
              {"fake_video_input_2", "Fake Video Input 2", "video_input_group"}
            }, {
              {"fake_audio_output_1", "Fake Audio Output 1", "common_group_1"},
              {"fake_audio_putput_2", "Fake Audio Output 2", "common_group_2"},
            }
            // clang-format on
        }) {
    // TODO(crbug.com/935960): add missing mocked capabilities and related
    // tests when media::AudioParameters is visible in this context.

    mojom::blink::VideoInputDeviceCapabilitiesPtr capabilities =
        mojom::blink::VideoInputDeviceCapabilities::New();
    capabilities->device_id = String(enumeration_[1][0].device_id);
    capabilities->group_id = String(enumeration_[1][0].group_id);
    capabilities->facing_mode = mojom::blink::FacingMode::NONE;
    video_input_capabilities_.push_back(std::move(capabilities));

    capabilities = mojom::blink::VideoInputDeviceCapabilities::New();
    capabilities->device_id = String(enumeration_[1][1].device_id);
    capabilities->group_id = String(enumeration_[1][1].group_id);
    capabilities->facing_mode = mojom::blink::FacingMode::USER;
    video_input_capabilities_.push_back(std::move(capabilities));
  }

  ~MockMediaDevicesDispatcherHost() override {
    EXPECT_FALSE(expected_capture_handle_config_);
  }

  void EnumerateDevices(bool request_audio_input,
                        bool request_video_input,
                        bool request_audio_output,
                        bool request_video_input_capabilities,
                        bool request_audio_input_capabilities,
                        EnumerateDevicesCallback callback) override {
    Vector<Vector<WebMediaDeviceInfo>> enumeration(static_cast<size_t>(
        blink::mojom::blink::MediaDeviceType::NUM_MEDIA_DEVICE_TYPES));
    Vector<mojom::blink::VideoInputDeviceCapabilitiesPtr>
        video_input_capabilities;
    Vector<mojom::blink::AudioInputDeviceCapabilitiesPtr>
        audio_input_capabilities;
    if (request_audio_input) {
      wtf_size_t index = static_cast<wtf_size_t>(
          blink::mojom::blink::MediaDeviceType::MEDIA_AUDIO_INPUT);
      enumeration[index] = enumeration_[index];

      if (request_audio_input_capabilities) {
        for (const auto& c : audio_input_capabilities_) {
          mojom::blink::AudioInputDeviceCapabilitiesPtr capabilities =
              mojom::blink::AudioInputDeviceCapabilities::New();
          *capabilities = *c;
          audio_input_capabilities.push_back(std::move(capabilities));
        }
      }
    }
    if (request_video_input) {
      wtf_size_t index = static_cast<wtf_size_t>(
          blink::mojom::blink::MediaDeviceType::MEDIA_VIDEO_INPUT);
      enumeration[index] = enumeration_[index];

      if (request_video_input_capabilities) {
        for (const auto& c : video_input_capabilities_) {
          mojom::blink::VideoInputDeviceCapabilitiesPtr capabilities =
              mojom::blink::VideoInputDeviceCapabilities::New();
          *capabilities = *c;
          video_input_capabilities.push_back(std::move(capabilities));
        }
      }
    }
    if (request_audio_output) {
      wtf_size_t index = static_cast<wtf_size_t>(
          blink::mojom::blink::MediaDeviceType::MEDIA_AUDIO_OUTPUT);
      enumeration[index] = enumeration_[index];
    }
    std::move(callback).Run(std::move(enumeration),
                            std::move(video_input_capabilities),
                            std::move(audio_input_capabilities));
  }

  void GetVideoInputCapabilities(GetVideoInputCapabilitiesCallback) override {
    NOTREACHED();
  }

  void GetAllVideoInputDeviceFormats(
      const String&,
      GetAllVideoInputDeviceFormatsCallback) override {
    NOTREACHED();
  }

  void GetAvailableVideoInputDeviceFormats(
      const String&,
      GetAvailableVideoInputDeviceFormatsCallback) override {
    NOTREACHED();
  }

  void GetAudioInputCapabilities(GetAudioInputCapabilitiesCallback) override {
    NOTREACHED();
  }

  void AddMediaDevicesListener(
      bool subscribe_audio_input,
      bool subscribe_video_input,
      bool subscribe_audio_output,
      mojo::PendingRemote<mojom::blink::MediaDevicesListener> listener)
      override {
    listener_.Bind(std::move(listener));
  }

  void SetCaptureHandleConfig(
      mojom::blink::CaptureHandleConfigPtr config) override {
    CHECK(config);

    auto expected_config = std::move(expected_capture_handle_config_);
    expected_capture_handle_config_ = nullptr;
    CHECK(expected_config);

    // TODO(crbug.com/1208868): Define CaptureHandleConfig traits that compare
    // |permitted_origins| using SecurityOrigin::IsSameOriginWith(), thereby
    // allowing this block to be replaced by a single EXPECT_EQ. (This problem
    // only manifests in Blink.)
    EXPECT_EQ(config->expose_origin, expected_config->expose_origin);
    EXPECT_EQ(config->capture_handle, expected_config->capture_handle);
    EXPECT_EQ(config->all_origins_permitted,
              expected_config->all_origins_permitted);
    CHECK_EQ(config->permitted_origins.size(),
             expected_config->permitted_origins.size());
    for (wtf_size_t i = 0; i < config->permitted_origins.size(); ++i) {
      EXPECT_TRUE(config->permitted_origins[i]->IsSameOriginWith(
          expected_config->permitted_origins[i].get()));
    }
  }

#if !BUILDFLAG(IS_ANDROID)
  void CloseFocusWindowOfOpportunity(const String& label) override {}

  void ProduceCropId(ProduceCropIdCallback callback) override {
    String next_crop_id = "";  // Empty, not null.
    std::swap(next_crop_id_, next_crop_id);
    std::move(callback).Run(std::move(next_crop_id));
  }

  void SetNextCropId(String next_crop_id) {
    next_crop_id_ = std::move(next_crop_id);
  }
#endif

  void ExpectSetCaptureHandleConfig(
      mojom::blink::CaptureHandleConfigPtr config) {
    CHECK(config);
    CHECK(!expected_capture_handle_config_) << "Unfulfilled expectation.";
    expected_capture_handle_config_ = std::move(config);
  }

  mojom::blink::CaptureHandleConfigPtr expected_capture_handle_config() {
    return std::move(expected_capture_handle_config_);
  }

  mojo::PendingRemote<mojom::blink::MediaDevicesDispatcherHost>
  CreatePendingRemoteAndBind() {
    mojo::PendingRemote<mojom::blink::MediaDevicesDispatcherHost> remote;
    receiver_.Bind(remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  void CloseBinding() { receiver_.reset(); }

  mojo::Remote<mojom::blink::MediaDevicesListener>& listener() {
    return listener_;
  }

  const Vector<Vector<WebMediaDeviceInfo>>& enumeration() const {
    return enumeration_;
  }

  void NotifyDeviceChanges() {
    listener()->OnDevicesChanged(MediaDeviceType::MEDIA_AUDIO_INPUT,
                                 enumeration_[static_cast<wtf_size_t>(
                                     MediaDeviceType::MEDIA_AUDIO_INPUT)]);
    listener()->OnDevicesChanged(MediaDeviceType::MEDIA_VIDEO_INPUT,
                                 enumeration_[static_cast<wtf_size_t>(
                                     MediaDeviceType::MEDIA_VIDEO_INPUT)]);
    listener()->OnDevicesChanged(MediaDeviceType::MEDIA_AUDIO_OUTPUT,
                                 enumeration_[static_cast<wtf_size_t>(
                                     MediaDeviceType::MEDIA_AUDIO_OUTPUT)]);
  }

  Vector<WebMediaDeviceInfo>& AudioInputDevices() {
    return enumeration_[static_cast<wtf_size_t>(
        MediaDeviceType::MEDIA_AUDIO_INPUT)];
  }
  Vector<WebMediaDeviceInfo>& VideoInputDevices() {
    return enumeration_[static_cast<wtf_size_t>(
        MediaDeviceType::MEDIA_VIDEO_INPUT)];
  }
  Vector<WebMediaDeviceInfo>& AudioOutputDevices() {
    return enumeration_[static_cast<wtf_size_t>(
        MediaDeviceType::MEDIA_AUDIO_OUTPUT)];
  }

 private:
  mojo::Remote<mojom::blink::MediaDevicesListener> listener_;
  mojo::Receiver<mojom::blink::MediaDevicesDispatcherHost> receiver_{this};
  mojom::blink::CaptureHandleConfigPtr expected_capture_handle_config_;
#if !BUILDFLAG(IS_ANDROID)
  String next_crop_id_ = "";  // Empty, not null.
#endif

  Vector<Vector<WebMediaDeviceInfo>> enumeration_{static_cast<size_t>(
      blink::mojom::blink::MediaDeviceType::NUM_MEDIA_DEVICE_TYPES)};
  Vector<mojom::blink::VideoInputDeviceCapabilitiesPtr>
      video_input_capabilities_;
  Vector<mojom::blink::AudioInputDeviceCapabilitiesPtr>
      audio_input_capabilities_;
};

class MockDeviceChangeEventListener : public NativeEventListener {
 public:
  MOCK_METHOD(void, Invoke, (ExecutionContext*, Event*));
};

String ToString(MediaDeviceType type) {
  switch (type) {
    case MediaDeviceType::MEDIA_AUDIO_INPUT:
      return "audioinput";
    case blink::MediaDeviceType::MEDIA_VIDEO_INPUT:
      return "videoinput";
    case blink::MediaDeviceType::MEDIA_AUDIO_OUTPUT:
      return "audiooutput";
    default:
      return String();
  }
}

void VerifyDeviceInfo(const MediaDeviceInfo* device,
                      const WebMediaDeviceInfo& expected,
                      MediaDeviceType type) {
  EXPECT_EQ(device->deviceId(), String(expected.device_id));
  EXPECT_EQ(device->groupId(), String(expected.group_id));
  EXPECT_EQ(device->label(), String(expected.label));
  EXPECT_EQ(device->kind(), ToString(type));
}

}  // namespace

class MediaDevicesTest : public PageTestBase {
 public:
  using MediaDeviceInfos = HeapVector<Member<MediaDeviceInfo>>;

  MediaDevicesTest()
      : dispatcher_host_(std::make_unique<MockMediaDevicesDispatcherHost>()),
        device_infos_(MakeGarbageCollected<MediaDeviceInfos>()) {}

  MediaDevices* GetMediaDevices(LocalDOMWindow& window) {
    if (!media_devices_) {
      media_devices_ = MakeGarbageCollected<MediaDevices>(*window.navigator());
      media_devices_->SetDispatcherHostForTesting(
          dispatcher_host_->CreatePendingRemoteAndBind());
    }
    return media_devices_;
  }

  void CloseBinding() { dispatcher_host_->CloseBinding(); }

  void OnListenerConnectionError() { listener_connection_error_ = true; }
  bool listener_connection_error() const { return listener_connection_error_; }

  ScopedTestingPlatformSupport<TestingPlatformSupport>& platform() {
    return platform_;
  }

  MockMediaDevicesDispatcherHost& dispatcher_host() {
    DCHECK(dispatcher_host_);
    return *dispatcher_host_;
  }

  void AddDeviceChangeListener(EventListener* event_listener) {
    GetMediaDevices(*GetDocument().domWindow())
        ->addEventListener(event_type_names::kDevicechange, event_listener);
    platform()->RunUntilIdle();
  }

  void RemoveDeviceChangeListener(EventListener* event_listener) {
    GetMediaDevices(*GetDocument().domWindow())
        ->removeEventListener(event_type_names::kDevicechange, event_listener,
                              /*use_capture=*/false);
    platform()->RunUntilIdle();
  }

  void NotifyDeviceChanges() {
    dispatcher_host().NotifyDeviceChanges();
    platform()->RunUntilIdle();
  }

  void ExpectEnumerateDevicesHistogramReport(
      EnumerateDevicesResult expected_result) {
    histogram_tester_.ExpectTotalCount(
        "Media.MediaDevices.EnumerateDevices.Result", 1);
    histogram_tester_.ExpectUniqueSample(
        "Media.MediaDevices.EnumerateDevices.Result", expected_result, 1);
    histogram_tester_.ExpectTotalCount(
        "Media.MediaDevices.EnumerateDevices.Latency", 1);
  }

 private:
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
  std::unique_ptr<MockMediaDevicesDispatcherHost> dispatcher_host_;
  Persistent<MediaDeviceInfos> device_infos_;
  bool listener_connection_error_ = false;
  Persistent<MediaDevices> media_devices_;
  HistogramTester histogram_tester_;
};

TEST_F(MediaDevicesTest, GetUserMediaCanBeCalled) {
  V8TestingScope scope;
  UserMediaStreamConstraints* constraints =
      UserMediaStreamConstraints::Create();
  ScriptPromise promise =
      GetMediaDevices(scope.GetWindow())
          ->getUserMedia(scope.GetScriptState(), constraints,
                         scope.GetExceptionState());
  ASSERT_TRUE(promise.IsEmpty());
  // We expect a type error because the given constraints are empty.
  EXPECT_EQ(scope.GetExceptionState().Code(),
            ToExceptionCode(ESErrorType::kTypeError));
  VLOG(1) << "Exception message is" << scope.GetExceptionState().Message();
}

TEST_F(MediaDevicesTest, EnumerateDevices) {
  V8TestingScope scope;
  auto* media_devices = GetMediaDevices(*GetDocument().domWindow());
  ScriptPromiseTester tester(
      scope.GetScriptState(),
      media_devices->enumerateDevices(scope.GetScriptState(),
                                      scope.GetExceptionState()));
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());

  auto device_infos = NativeValueTraits<IDLArray<MediaDeviceInfo>>::NativeValue(
      scope.GetIsolate(), tester.Value().V8Value(), scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());

  ExpectEnumerateDevicesHistogramReport(EnumerateDevicesResult::kOk);

  for (wtf_size_t i = 0, result_index = 0;
       i < static_cast<wtf_size_t>(MediaDeviceType::NUM_MEDIA_DEVICE_TYPES);
       ++i) {
    for (const auto& device_info : dispatcher_host().enumeration()[i]) {
      testing::Message message;
      message << "Verifying result index " << result_index;
      SCOPED_TRACE(message);
      VerifyDeviceInfo(device_infos[result_index++], device_info,
                       static_cast<MediaDeviceType>(i));
    }
  }
}

TEST_F(MediaDevicesTest, EnumerateDevicesAfterConnectionError) {
  V8TestingScope scope;
  auto* media_devices = GetMediaDevices(*GetDocument().domWindow());

  // Simulate a connection error by closing the binding.
  CloseBinding();
  platform()->RunUntilIdle();

  ScriptPromiseTester tester(
      scope.GetScriptState(),
      media_devices->enumerateDevices(scope.GetScriptState(),
                                      scope.GetExceptionState()));
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
  ExpectEnumerateDevicesHistogramReport(
      EnumerateDevicesResult::kErrorMediaDevicesDispatcherHostDisconnected);
}

TEST_F(MediaDevicesTest, SetCaptureHandleConfigAfterConnectionError) {
  V8TestingScope scope;
  auto* media_devices = GetMediaDevices(*GetDocument().domWindow());

  // Simulate a connection error by closing the binding.
  CloseBinding();
  platform()->RunUntilIdle();

  // Note: SetCaptureHandleConfigEmpty proves the following is a valid call.
  CaptureHandleConfig input_config;
  media_devices->setCaptureHandleConfig(scope.GetScriptState(), &input_config,
                                        scope.GetExceptionState());
  platform()->RunUntilIdle();
}

TEST_F(MediaDevicesTest, ObserveDeviceChangeEvent) {
  EXPECT_FALSE(dispatcher_host().listener());

  // Subscribe to the devicechange event.
  StrictMock<MockDeviceChangeEventListener>* event_listener =
      MakeGarbageCollected<StrictMock<MockDeviceChangeEventListener>>();
  AddDeviceChangeListener(event_listener);
  EXPECT_TRUE(dispatcher_host().listener());
  dispatcher_host().listener().set_disconnect_handler(WTF::BindOnce(
      &MediaDevicesTest::OnListenerConnectionError, WTF::Unretained(this)));

  // Send a device change notification from the dispatcher host. The event is
  // not fired because devices did not actually change.
  NotifyDeviceChanges();

  // Adding a new device fires the event.
  EXPECT_CALL(*event_listener, Invoke(_, _));
  dispatcher_host().AudioInputDevices().push_back(WebMediaDeviceInfo(
      "new_fake_audio_input_device", "new_fake_label", "new_fake_group"));
  NotifyDeviceChanges();

  // Renaming a group ID does not fire the event.
  dispatcher_host().AudioOutputDevices().begin()->group_id = "new_group_id";
  NotifyDeviceChanges();

  // Unsubscribe.
  RemoveDeviceChangeListener(event_listener);
  EXPECT_TRUE(listener_connection_error());

  // Sending a device change notification after unsubscribe does not fire the
  // event.
  dispatcher_host().AudioInputDevices().push_back(WebMediaDeviceInfo(
      "yet_another_input_device", "yet_another_label", "yet_another_group"));
  NotifyDeviceChanges();
}

TEST_F(MediaDevicesTest, RemoveDeviceFiresDeviceChange) {
  StrictMock<MockDeviceChangeEventListener>* event_listener =
      MakeGarbageCollected<StrictMock<MockDeviceChangeEventListener>>();
  AddDeviceChangeListener(event_listener);

  EXPECT_CALL(*event_listener, Invoke(_, _));
  dispatcher_host().VideoInputDevices().EraseAt(0);
  NotifyDeviceChanges();
}

TEST_F(MediaDevicesTest, RenameDeviceIDFiresDeviceChange) {
  StrictMock<MockDeviceChangeEventListener>* event_listener =
      MakeGarbageCollected<StrictMock<MockDeviceChangeEventListener>>();
  AddDeviceChangeListener(event_listener);

  EXPECT_CALL(*event_listener, Invoke(_, _));
  dispatcher_host().AudioOutputDevices().begin()->device_id = "new_device_id";
  NotifyDeviceChanges();
}

TEST_F(MediaDevicesTest, RenameLabelFiresDeviceChange) {
  StrictMock<MockDeviceChangeEventListener>* event_listener =
      MakeGarbageCollected<StrictMock<MockDeviceChangeEventListener>>();
  AddDeviceChangeListener(event_listener);

  EXPECT_CALL(*event_listener, Invoke(_, _));
  dispatcher_host().AudioOutputDevices().begin()->label = "new_label";
  NotifyDeviceChanges();
}

TEST_F(MediaDevicesTest, SetCaptureHandleConfigEmpty) {
  V8TestingScope scope;
  auto* media_devices = GetMediaDevices(*GetDocument().domWindow());

  CaptureHandleConfig input_config;

  // Expected output.
  auto expected_config = mojom::blink::CaptureHandleConfig::New();
  expected_config->expose_origin = false;
  expected_config->capture_handle = "";
  expected_config->all_origins_permitted = false;
  expected_config->permitted_origins = {};
  dispatcher_host().ExpectSetCaptureHandleConfig(std::move(expected_config));

  media_devices->setCaptureHandleConfig(scope.GetScriptState(), &input_config,
                                        scope.GetExceptionState());

  platform()->RunUntilIdle();

  EXPECT_FALSE(scope.GetExceptionState().HadException());
}

TEST_F(MediaDevicesTest, SetCaptureHandleConfigWithExposeOrigin) {
  V8TestingScope scope;
  auto* media_devices = GetMediaDevices(*GetDocument().domWindow());

  CaptureHandleConfig input_config;
  input_config.setExposeOrigin(true);

  // Expected output.
  auto expected_config = mojom::blink::CaptureHandleConfig::New();
  expected_config->expose_origin = true;
  expected_config->capture_handle = "";
  expected_config->all_origins_permitted = false;
  expected_config->permitted_origins = {};
  dispatcher_host().ExpectSetCaptureHandleConfig(std::move(expected_config));

  media_devices->setCaptureHandleConfig(scope.GetScriptState(), &input_config,
                                        scope.GetExceptionState());

  platform()->RunUntilIdle();

  EXPECT_FALSE(scope.GetExceptionState().HadException());
}

TEST_F(MediaDevicesTest, SetCaptureHandleConfigCaptureWithHandle) {
  V8TestingScope scope;
  auto* media_devices = GetMediaDevices(*GetDocument().domWindow());

  CaptureHandleConfig input_config;
  input_config.setHandle("0xabcdef0123456789");

  // Expected output.
  auto expected_config = mojom::blink::CaptureHandleConfig::New();
  expected_config->expose_origin = false;
  expected_config->capture_handle = "0xabcdef0123456789";
  expected_config->all_origins_permitted = false;
  expected_config->permitted_origins = {};
  dispatcher_host().ExpectSetCaptureHandleConfig(std::move(expected_config));

  media_devices->setCaptureHandleConfig(scope.GetScriptState(), &input_config,
                                        scope.GetExceptionState());

  platform()->RunUntilIdle();

  EXPECT_FALSE(scope.GetExceptionState().HadException());
}

TEST_F(MediaDevicesTest, SetCaptureHandleConfigCaptureWithMaxHandle) {
  V8TestingScope scope;
  auto* media_devices = GetMediaDevices(*GetDocument().domWindow());

  const String maxHandle = MaxLengthCaptureHandle();

  CaptureHandleConfig input_config;
  input_config.setHandle(maxHandle);

  // Expected output.
  auto expected_config = mojom::blink::CaptureHandleConfig::New();
  expected_config->expose_origin = false;
  expected_config->capture_handle = maxHandle;
  expected_config->all_origins_permitted = false;
  expected_config->permitted_origins = {};
  dispatcher_host().ExpectSetCaptureHandleConfig(std::move(expected_config));

  media_devices->setCaptureHandleConfig(scope.GetScriptState(), &input_config,
                                        scope.GetExceptionState());

  platform()->RunUntilIdle();

  EXPECT_FALSE(scope.GetExceptionState().HadException());
}

TEST_F(MediaDevicesTest,
       SetCaptureHandleConfigCaptureWithOverMaxHandleRejected) {
  V8TestingScope scope;
  auto* media_devices = GetMediaDevices(*GetDocument().domWindow());

  CaptureHandleConfig input_config;
  input_config.setHandle(MaxLengthCaptureHandle() + "a");  // Over max length.

  // Note: dispatcher_host().ExpectSetCaptureHandleConfig() not called.

  media_devices->setCaptureHandleConfig(scope.GetScriptState(), &input_config,
                                        scope.GetExceptionState());

  platform()->RunUntilIdle();

  ASSERT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(scope.GetExceptionState().Code(),
            ToExceptionCode(ESErrorType::kTypeError));
}

TEST_F(MediaDevicesTest,
       SetCaptureHandleConfigCaptureWithPermittedOriginsWildcard) {
  V8TestingScope scope;
  auto* media_devices = GetMediaDevices(*GetDocument().domWindow());

  CaptureHandleConfig input_config;
  input_config.setPermittedOrigins({"*"});

  // Expected output.
  auto expected_config = mojom::blink::CaptureHandleConfig::New();
  expected_config->expose_origin = false;
  expected_config->capture_handle = "";
  expected_config->all_origins_permitted = true;
  expected_config->permitted_origins = {};
  dispatcher_host().ExpectSetCaptureHandleConfig(std::move(expected_config));

  media_devices->setCaptureHandleConfig(scope.GetScriptState(), &input_config,
                                        scope.GetExceptionState());

  platform()->RunUntilIdle();

  EXPECT_FALSE(scope.GetExceptionState().HadException());
}

TEST_F(MediaDevicesTest, SetCaptureHandleConfigCaptureWithPermittedOrigins) {
  V8TestingScope scope;
  auto* media_devices = GetMediaDevices(*GetDocument().domWindow());

  CaptureHandleConfig input_config;
  input_config.setPermittedOrigins(
      {"https://chromium.org", "ftp://chromium.org:1234"});

  // Expected output.
  auto expected_config = mojom::blink::CaptureHandleConfig::New();
  expected_config->expose_origin = false;
  expected_config->capture_handle = "";
  expected_config->all_origins_permitted = false;
  expected_config->permitted_origins = {
      SecurityOrigin::CreateFromString("https://chromium.org"),
      SecurityOrigin::CreateFromString("ftp://chromium.org:1234")};
  dispatcher_host().ExpectSetCaptureHandleConfig(std::move(expected_config));

  media_devices->setCaptureHandleConfig(scope.GetScriptState(), &input_config,
                                        scope.GetExceptionState());

  platform()->RunUntilIdle();

  EXPECT_FALSE(scope.GetExceptionState().HadException());
}

TEST_F(MediaDevicesTest,
       SetCaptureHandleConfigCaptureWithWildcardAndSomethingElseRejected) {
  V8TestingScope scope;
  auto* media_devices = GetMediaDevices(*GetDocument().domWindow());

  CaptureHandleConfig input_config;
  input_config.setPermittedOrigins({"*", "https://chromium.org"});

  // Note: dispatcher_host().ExpectSetCaptureHandleConfig() not called.

  media_devices->setCaptureHandleConfig(scope.GetScriptState(), &input_config,
                                        scope.GetExceptionState());

  platform()->RunUntilIdle();

  ASSERT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(scope.GetExceptionState().Code(),
            ToExceptionCode(DOMExceptionCode::kNotSupportedError));
}

TEST_F(MediaDevicesTest,
       SetCaptureHandleConfigCaptureWithMalformedOriginRejected) {
  V8TestingScope scope;
  auto* media_devices = GetMediaDevices(*GetDocument().domWindow());

  CaptureHandleConfig input_config;
  input_config.setPermittedOrigins({"https://chromium.org:99999"});  // Invalid.

  // Note: dispatcher_host().ExpectSetCaptureHandleConfig() not called.

  media_devices->setCaptureHandleConfig(scope.GetScriptState(), &input_config,
                                        scope.GetExceptionState());

  platform()->RunUntilIdle();

  ASSERT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(scope.GetExceptionState().Code(),
            ToExceptionCode(DOMExceptionCode::kNotSupportedError));
}

// Note: This test runs on non-Android too in order to prove that the test
// itself is sane. (Rather than, for example, an exception always being thrown.)
TEST_F(MediaDevicesTest, ProduceCropIdUnsupportedOnAndroid) {
  V8TestingScope scope;
  auto* media_devices = GetMediaDevices(*GetDocument().domWindow());
  ASSERT_TRUE(media_devices);

  SetBodyContent(R"HTML(
    <div id='test-div'></div>
    <iframe id='test-iframe' src="about:blank" />
  )HTML");

  Document& document = GetDocument();
  Element* const div = document.getElementById(AtomicString("test-div"));
  const ScriptPromise div_promise = media_devices->ProduceCropTarget(
      scope.GetScriptState(), div, scope.GetExceptionState());
  platform()->RunUntilIdle();
#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(scope.GetExceptionState().HadException());
#else  // Non-Android shown to work, proving the test is sane.
  EXPECT_FALSE(div_promise.IsEmpty());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
#endif
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(MediaDevicesTest, ProduceCropIdWithValidElement) {
  V8TestingScope scope;
  auto* media_devices = GetMediaDevices(*GetDocument().domWindow());
  ASSERT_TRUE(media_devices);

  SetBodyContent(R"HTML(
    <div id='test-div'></div>
    <iframe id='test-iframe' src="about:blank"></iframe>
    <p id='test-p'>
      <var id='test-var'>e</var> equals mc<sup id='test-sup'>2</sup>, or is
      <wbr id='test-wbr'>it mc<sub id='test-sub'>2</sub>?
      <u id='test-u'>probz</u>.
    </p>
    <select id='test-select'></select>

    <svg id='test-svg' width="400" height="110">
      <rect id='test-rect' width="300" height="100"/>
    </svg>

    <math id='test-math' xmlns='http://www.w3.org/1998/Math/MathML'>
    </math>
  )HTML");

  Document& document = GetDocument();
  static const std::vector<const char*> kElementIds{
      "test-div",    "test-iframe", "test-p",    "test-var",
      "test-sup",    "test-wbr",    "test-sub",  "test-u",
      "test-select", "test-svg",    "test-rect", "test-math"};

  for (const char* id : kElementIds) {
    Element* const element = document.getElementById(AtomicString(id));
    dispatcher_host().SetNextCropId(
        String(base::Uuid::GenerateRandomV4().AsLowercaseString()));
    const ScriptPromise promise = media_devices->ProduceCropTarget(
        scope.GetScriptState(), element, scope.GetExceptionState());

    ScriptPromiseTester script_promise_tester(scope.GetScriptState(), promise);
    script_promise_tester.WaitUntilSettled();
    EXPECT_TRUE(script_promise_tester.IsFulfilled())
        << "Failed promise for element id=" << id;
    EXPECT_FALSE(scope.GetExceptionState().HadException());
  }
}

TEST_F(MediaDevicesTest, ProduceCropIdRejectedIfDifferentWindow) {
  V8TestingScope scope;
  // Intentionally sets up a MediaDevices object in a different window.
  auto* media_devices = GetMediaDevices(scope.GetWindow());
  ASSERT_TRUE(media_devices);

  SetBodyContent(R"HTML(
    <div id='test-div'></div>
    <iframe id='test-iframe' src="about:blank" />
  )HTML");

  Document& document = GetDocument();
  Element* const div = document.getElementById(AtomicString("test-div"));
  const ScriptPromise element_promise = media_devices->ProduceCropTarget(
      scope.GetScriptState(), div, scope.GetExceptionState());
  platform()->RunUntilIdle();
  EXPECT_TRUE(element_promise.IsEmpty());
  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kNotSupportedError);
  EXPECT_EQ(
      scope.GetExceptionState().Message(),
      String("The Element and the MediaDevices object must be same-window."));
}

TEST_F(MediaDevicesTest, ProduceCropIdDuplicate) {
  V8TestingScope scope;
  auto* media_devices = GetMediaDevices(*GetDocument().domWindow());
  ASSERT_TRUE(media_devices);
  dispatcher_host().SetNextCropId(
      String(base::Uuid::GenerateRandomV4().AsLowercaseString()));

  SetBodyContent(R"HTML(
    <div id='test-div'></div>
  )HTML");

  Document& document = GetDocument();
  Element* const div = document.getElementById(AtomicString("test-div"));
  const ScriptPromise first_promise = media_devices->ProduceCropTarget(
      scope.GetScriptState(), div, scope.GetExceptionState());
  ScriptPromiseTester first_tester(scope.GetScriptState(), first_promise);
  first_tester.WaitUntilSettled();
  EXPECT_TRUE(first_tester.IsFulfilled());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  // The second call to |produceCropId| should return the same ID.
  const ScriptPromise second_promise = media_devices->ProduceCropTarget(
      scope.GetScriptState(), div, scope.GetExceptionState());
  ScriptPromiseTester second_tester(scope.GetScriptState(), second_promise);
  second_tester.WaitUntilSettled();
  EXPECT_TRUE(second_tester.IsFulfilled());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  WTF::String first_result, second_result;
  first_tester.Value().ToString(first_result);
  second_tester.Value().ToString(second_result);
  EXPECT_EQ(first_result, second_result);
}

TEST_F(MediaDevicesTest, ProduceCropIdStringFormat) {
  V8TestingScope scope;
  auto* media_devices = GetMediaDevices(*GetDocument().domWindow());
  ASSERT_TRUE(media_devices);

  SetBodyContent(R"HTML(
    <div id='test-div'></div>
  )HTML");

  Document& document = GetDocument();
  Element* const div = document.getElementById(AtomicString("test-div"));
  dispatcher_host().SetNextCropId(
      String(base::Uuid::GenerateRandomV4().AsLowercaseString()));
  const ScriptPromise promise = media_devices->ProduceCropTarget(
      scope.GetScriptState(), div, scope.GetExceptionState());
  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  const CropTarget* const crop_target =
      V8CropTarget::ToWrappable(scope.GetIsolate(), tester.Value().V8Value());
  const WTF::String& crop_id = crop_target->GetCropId();
  EXPECT_TRUE(crop_id.ContainsOnlyASCIIOrEmpty());
  EXPECT_TRUE(base::Uuid::ParseLowercase(crop_id.Ascii()).is_valid());
}
#endif

}  // namespace blink
