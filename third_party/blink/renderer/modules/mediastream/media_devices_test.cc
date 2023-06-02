// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_devices.h"

#include <memory>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/media/capture_handle_config.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_capture_handle_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_crop_target.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_user_media_stream_constraints.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/mediastream/crop_target.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using base::HistogramTester;
using blink::mojom::blink::MediaDeviceInfoPtr;
using ::testing::_;

namespace blink {

const char kFakeAudioInputDeviceId1[] = "fake_audio_input 1";
const char kFakeAudioInputDeviceId2[] = "fake_audio_input 2";
const char kFakeAudioInputDeviceId3[] = "fake_audio_input 3";
const char kFakeVideoInputDeviceId1[] = "fake_video_input 1";
const char kFakeVideoInputDeviceId2[] = "fake_video_input 2";
const char kFakeCommonGroupId1[] = "fake_group 1";
const char kFakeCommonGroupId2[] = "fake_group 2";
const char kFakeVideoInputGroupId2[] = "fake_video_input_group 2";
const char kFakeAudioOutputDeviceId1[] = "fake_audio_output 1";
const char kFakeAudioOutputDeviceId2[] = "fake_audio_output 2";

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
  MockMediaDevicesDispatcherHost() {}

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
    WebMediaDeviceInfo device_info;
    if (request_audio_input) {
      device_info.device_id = kFakeAudioInputDeviceId1;
      device_info.label = "Fake Audio Input 1";
      device_info.group_id = kFakeCommonGroupId1;
      enumeration[static_cast<size_t>(
                      blink::mojom::blink::MediaDeviceType::MEDIA_AUDIO_INPUT)]
          .push_back(device_info);

      device_info.device_id = kFakeAudioInputDeviceId2;
      device_info.label = "Fake Audio Input 2";
      device_info.group_id = kFakeCommonGroupId2;
      enumeration[static_cast<size_t>(
                      blink::mojom::blink::MediaDeviceType::MEDIA_AUDIO_INPUT)]
          .push_back(device_info);

      device_info.device_id = kFakeAudioInputDeviceId3;
      device_info.label = "Fake Audio Input 3";
      device_info.group_id = "fake_group 3";
      enumeration[static_cast<size_t>(
                      blink::mojom::blink::MediaDeviceType::MEDIA_AUDIO_INPUT)]
          .push_back(device_info);

      // TODO(crbug.com/935960): add missing mocked capabilities and related
      // tests when media::AudioParameters is visible in this context.
    }
    if (request_video_input) {
      device_info.device_id = kFakeVideoInputDeviceId1;
      device_info.label = "Fake Video Input 1";
      device_info.group_id = kFakeCommonGroupId1;
      enumeration[static_cast<size_t>(
                      blink::mojom::blink::MediaDeviceType::MEDIA_VIDEO_INPUT)]
          .push_back(device_info);

      device_info.device_id = kFakeVideoInputDeviceId2;
      device_info.label = "Fake Video Input 2";
      device_info.group_id = kFakeVideoInputGroupId2;
      enumeration[static_cast<size_t>(
                      blink::mojom::blink::MediaDeviceType::MEDIA_VIDEO_INPUT)]
          .push_back(device_info);

      if (request_video_input_capabilities) {
        mojom::blink::VideoInputDeviceCapabilitiesPtr capabilities =
            mojom::blink::VideoInputDeviceCapabilities::New();
        capabilities->device_id = kFakeVideoInputDeviceId1;
        capabilities->group_id = kFakeCommonGroupId1;
        capabilities->facing_mode = mojom::blink::FacingMode::NONE;
        video_input_capabilities.push_back(std::move(capabilities));

        capabilities = mojom::blink::VideoInputDeviceCapabilities::New();
        capabilities->device_id = kFakeVideoInputDeviceId2;
        capabilities->group_id = kFakeVideoInputGroupId2;
        capabilities->facing_mode = mojom::blink::FacingMode::USER;
        video_input_capabilities.push_back(std::move(capabilities));
      }
    }
    if (request_audio_output) {
      device_info.device_id = kFakeAudioOutputDeviceId1;
      device_info.label = "Fake Audio Input 1";
      device_info.group_id = kFakeCommonGroupId1;
      enumeration[static_cast<size_t>(
                      blink::mojom::blink::MediaDeviceType::MEDIA_AUDIO_OUTPUT)]
          .push_back(device_info);

      device_info.device_id = kFakeAudioOutputDeviceId2;
      device_info.label = "Fake Audio Input 2";
      device_info.group_id = kFakeCommonGroupId2;
      enumeration[static_cast<size_t>(
                      blink::mojom::blink::MediaDeviceType::MEDIA_AUDIO_OUTPUT)]
          .push_back(device_info);
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
    ASSERT_TRUE(config);

    auto expected_config = std::move(expected_capture_handle_config_);
    expected_capture_handle_config_ = nullptr;
    ASSERT_TRUE(expected_config);

    // TODO(crbug.com/1208868): Define CaptureHandleConfig traits that compare
    // |permitted_origins| using SecurityOrigin::IsSameOriginWith(), thereby
    // allowing this block to be replaced by a single EXPECT_EQ. (This problem
    // only manifests in Blink.)
    EXPECT_EQ(config->expose_origin, expected_config->expose_origin);
    EXPECT_EQ(config->capture_handle, expected_config->capture_handle);
    EXPECT_EQ(config->all_origins_permitted,
              expected_config->all_origins_permitted);
    ASSERT_EQ(config->permitted_origins.size(),
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
    ASSERT_TRUE(config);
    ASSERT_FALSE(expected_capture_handle_config_) << "Unfulfilled expectation.";
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

 private:
  mojo::Remote<mojom::blink::MediaDevicesListener> listener_;
  mojo::Receiver<mojom::blink::MediaDevicesDispatcherHost> receiver_{this};
  mojom::blink::CaptureHandleConfigPtr expected_capture_handle_config_;
#if !BUILDFLAG(IS_ANDROID)
  String next_crop_id_ = "";  // Empty, not null.
#endif
};

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

  void SimulateDeviceChange() {
    DCHECK(listener());
    listener()->OnDevicesChanged(
        blink::mojom::MediaDeviceType::MEDIA_AUDIO_INPUT,
        Vector<WebMediaDeviceInfo>());
  }

  void DevicesEnumerated(const MediaDeviceInfoVector& device_infos) {
    devices_enumerated_ = true;
    for (wtf_size_t i = 0; i < device_infos.size(); i++) {
      device_infos_->push_back(MakeGarbageCollected<MediaDeviceInfo>(
          device_infos[i]->deviceId(), device_infos[i]->label(),
          device_infos[i]->groupId(), device_infos[i]->DeviceType()));
    }
  }

  void OnDispatcherHostConnectionError() {
    dispatcher_host_connection_error_ = true;
  }

  void OnDevicesChanged() { device_changed_ = true; }

  void OnListenerConnectionError() {
    listener_connection_error_ = true;
    device_changed_ = false;
  }

  mojo::Remote<mojom::blink::MediaDevicesListener>& listener() {
    return dispatcher_host_->listener();
  }

  bool listener_connection_error() const { return listener_connection_error_; }

  const MediaDeviceInfos& device_infos() const { return *device_infos_; }

  bool devices_enumerated() const { return devices_enumerated_; }

  bool dispatcher_host_connection_error() const {
    return dispatcher_host_connection_error_;
  }

  bool device_changed() const { return device_changed_; }

  ScopedTestingPlatformSupport<TestingPlatformSupport>& platform() {
    return platform_;
  }

  MockMediaDevicesDispatcherHost& dispatcher_host() {
    DCHECK(dispatcher_host_);
    return *dispatcher_host_;
  }

  base::test::ScopedFeatureList& scoped_feature_list() {
    return scoped_feature_list_;
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
  bool devices_enumerated_ = false;
  bool dispatcher_host_connection_error_ = false;
  bool device_changed_ = false;
  bool listener_connection_error_ = false;
  Persistent<MediaDevices> media_devices_;
  base::test::ScopedFeatureList scoped_feature_list_;
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
  media_devices->SetEnumerateDevicesCallbackForTesting(WTF::BindOnce(
      &MediaDevicesTest::DevicesEnumerated, WTF::Unretained(this)));
  ScriptPromise promise = media_devices->enumerateDevices(
      scope.GetScriptState(), scope.GetExceptionState());
  platform()->RunUntilIdle();
  ASSERT_FALSE(promise.IsEmpty());

  EXPECT_TRUE(devices_enumerated());
  EXPECT_EQ(7u, device_infos().size());

  ExpectEnumerateDevicesHistogramReport(EnumerateDevicesResult::kOk);

  // Audio input device with matched output ID.
  Member<MediaDeviceInfo> device = device_infos()[0];
  EXPECT_FALSE(device->deviceId().empty());
  EXPECT_EQ("audioinput", device->kind());
  EXPECT_FALSE(device->label().empty());
  EXPECT_FALSE(device->groupId().empty());

  // Audio input device with second matched output ID
  device = device_infos()[1];
  EXPECT_FALSE(device->deviceId().empty());
  EXPECT_EQ("audioinput", device->kind());
  EXPECT_FALSE(device->label().empty());
  EXPECT_FALSE(device->groupId().empty());

  // Audio input device without matched output ID.
  device = device_infos()[2];
  EXPECT_FALSE(device->deviceId().empty());
  EXPECT_EQ("audioinput", device->kind());
  EXPECT_FALSE(device->label().empty());
  EXPECT_FALSE(device->groupId().empty());

  // Video input devices.
  device = device_infos()[3];
  EXPECT_FALSE(device->deviceId().empty());
  EXPECT_EQ("videoinput", device->kind());
  EXPECT_FALSE(device->label().empty());
  EXPECT_FALSE(device->groupId().empty());

  device = device_infos()[4];
  EXPECT_FALSE(device->deviceId().empty());
  EXPECT_EQ("videoinput", device->kind());
  EXPECT_FALSE(device->label().empty());
  EXPECT_FALSE(device->groupId().empty());

  // Audio output device.
  device = device_infos()[5];
  EXPECT_FALSE(device->deviceId().empty());
  EXPECT_EQ("audiooutput", device->kind());
  EXPECT_FALSE(device->label().empty());
  EXPECT_FALSE(device->groupId().empty());

  // Second audio output device
  device = device_infos()[6];
  EXPECT_FALSE(device->deviceId().empty());
  EXPECT_EQ("audiooutput", device->kind());
  EXPECT_FALSE(device->label().empty());
  EXPECT_FALSE(device->groupId().empty());

  // Verify group IDs.
  EXPECT_EQ(device_infos()[0]->groupId(), device_infos()[3]->groupId());
  EXPECT_EQ(device_infos()[0]->groupId(), device_infos()[5]->groupId());
  EXPECT_NE(device_infos()[2]->groupId(), device_infos()[5]->groupId());
}

TEST_F(MediaDevicesTest, EnumerateDevicesAfterConnectionError) {
  V8TestingScope scope;
  auto* media_devices = GetMediaDevices(*GetDocument().domWindow());
  media_devices->SetEnumerateDevicesCallbackForTesting(WTF::BindOnce(
      &MediaDevicesTest::DevicesEnumerated, WTF::Unretained(this)));
  media_devices->SetConnectionErrorCallbackForTesting(
      WTF::BindOnce(&MediaDevicesTest::OnDispatcherHostConnectionError,
                    WTF::Unretained(this)));
  EXPECT_FALSE(dispatcher_host_connection_error());

  // Simulate a connection error by closing the binding.
  CloseBinding();
  platform()->RunUntilIdle();

  ScriptPromise promise = media_devices->enumerateDevices(
      scope.GetScriptState(), scope.GetExceptionState());
  platform()->RunUntilIdle();
  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_TRUE(dispatcher_host_connection_error());
  EXPECT_FALSE(devices_enumerated());

  ExpectEnumerateDevicesHistogramReport(
      EnumerateDevicesResult::kErrorMediaDevicesDispatcherHostDisconnected);
}

TEST_F(MediaDevicesTest, SetCaptureHandleConfigAfterConnectionError) {
  V8TestingScope scope;
  auto* media_devices = GetMediaDevices(*GetDocument().domWindow());

  media_devices->SetConnectionErrorCallbackForTesting(
      WTF::BindOnce(&MediaDevicesTest::OnDispatcherHostConnectionError,
                    WTF::Unretained(this)));
  ASSERT_FALSE(dispatcher_host_connection_error());

  // Simulate a connection error by closing the binding.
  CloseBinding();
  platform()->RunUntilIdle();

  // Note: SetCaptureHandleConfigEmpty proves the following is a valid call.
  CaptureHandleConfig input_config;
  media_devices->setCaptureHandleConfig(scope.GetScriptState(), &input_config,
                                        scope.GetExceptionState());

  platform()->RunUntilIdle();
  EXPECT_TRUE(dispatcher_host_connection_error());
}

TEST_F(MediaDevicesTest, EnumerateDevicesBeforeConnectionError) {
  V8TestingScope scope;
  auto* media_devices = GetMediaDevices(*GetDocument().domWindow());
  media_devices->SetEnumerateDevicesCallbackForTesting(WTF::BindOnce(
      &MediaDevicesTest::DevicesEnumerated, WTF::Unretained(this)));
  media_devices->SetConnectionErrorCallbackForTesting(
      WTF::BindOnce(&MediaDevicesTest::OnDispatcherHostConnectionError,
                    WTF::Unretained(this)));
  EXPECT_FALSE(dispatcher_host_connection_error());

  ScriptPromise promise = media_devices->enumerateDevices(
      scope.GetScriptState(), scope.GetExceptionState());
  platform()->RunUntilIdle();
  ASSERT_FALSE(promise.IsEmpty());

  // Simulate a connection error by closing the binding.
  CloseBinding();
  platform()->RunUntilIdle();
  EXPECT_TRUE(dispatcher_host_connection_error());
  EXPECT_TRUE(devices_enumerated());
  ExpectEnumerateDevicesHistogramReport(EnumerateDevicesResult::kOk);
}

TEST_F(MediaDevicesTest, ObserveDeviceChangeEvent) {
  V8TestingScope scope;
  auto* media_devices = GetMediaDevices(*GetDocument().domWindow());
  media_devices->SetDeviceChangeCallbackForTesting(WTF::BindOnce(
      &MediaDevicesTest::OnDevicesChanged, WTF::Unretained(this)));
  EXPECT_FALSE(listener());

  // Subscribe for device change event.
  media_devices->StartObserving();
  platform()->RunUntilIdle();
  EXPECT_TRUE(listener());
  listener().set_disconnect_handler(WTF::BindOnce(
      &MediaDevicesTest::OnListenerConnectionError, WTF::Unretained(this)));

  // Simulate a device change.
  SimulateDeviceChange();
  platform()->RunUntilIdle();
  EXPECT_TRUE(device_changed());

  // Unsubscribe.
  media_devices->StopObserving();
  platform()->RunUntilIdle();
  EXPECT_TRUE(listener_connection_error());

  // Simulate another device change.
  SimulateDeviceChange();
  platform()->RunUntilIdle();
  EXPECT_FALSE(device_changed());
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
  Element* const div = document.getElementById("test-div");
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
    Element* const element = document.getElementById(id);
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
  Element* const div = document.getElementById("test-div");
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
  Element* const div = document.getElementById("test-div");
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
  Element* const div = document.getElementById("test-div");
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
