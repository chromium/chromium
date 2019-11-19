// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_devices.h"

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using blink::mojom::blink::MediaDeviceInfoPtr;

namespace blink {

const char kFakeAudioInputDeviceId1[] = "fake_audio_input 1";
const char kFakeAudioInputDeviceId2[] = "fake_audio_input 2";
const char kFakeVideoInputDeviceId1[] = "fake_video_input 1";
const char kFakeVideoInputDeviceId2[] = "fake_video_input 2";
const char kFakeCommonGroupId1[] = "fake_group 1";
const char kFakeVideoInputGroupId2[] = "fake_video_input_group 2";
const char kFakeAudioOutputDeviceId1[] = "fake_audio_output 1";

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
    Vector<Vector<MediaDeviceInfoPtr>> enumeration(static_cast<size_t>(
        blink::mojom::blink::MediaDeviceType::NUM_MEDIA_DEVICE_TYPES));
    Vector<mojom::blink::VideoInputDeviceCapabilitiesPtr>
        video_input_capabilities;
    Vector<mojom::blink::AudioInputDeviceCapabilitiesPtr>
        audio_input_capabilities;
    MediaDeviceInfoPtr device_info;
    if (request_audio_input) {
      device_info = mojom::blink::MediaDeviceInfo::New();
      device_info->device_id = kFakeAudioInputDeviceId1;
      device_info->label = "Fake Audio Input 1";
      device_info->group_id = kFakeCommonGroupId1;
      enumeration[static_cast<size_t>(
                      blink::mojom::blink::MediaDeviceType::MEDIA_AUDIO_INPUT)]
          .push_back(std::move(device_info));

      device_info = mojom::blink::MediaDeviceInfo::New();
      device_info->device_id = kFakeAudioInputDeviceId2;
      device_info->label = "Fake Audio Input 2";
      device_info->group_id = "fake_group 2";
      enumeration[static_cast<size_t>(
                      blink::mojom::blink::MediaDeviceType::MEDIA_AUDIO_INPUT)]
          .push_back(std::move(device_info));

      // TODO(crbug.com/935960): add missing mocked capabilities and related
      // tests when media::AudioParameters is visible in this context.
    }
    if (request_video_input) {
      device_info = mojom::blink::MediaDeviceInfo::New();
      device_info->device_id = kFakeVideoInputDeviceId1;
      device_info->label = "Fake Video Input 1";
      device_info->group_id = kFakeCommonGroupId1;
      enumeration[static_cast<size_t>(
                      blink::mojom::blink::MediaDeviceType::MEDIA_VIDEO_INPUT)]
          .push_back(std::move(device_info));

      device_info = mojom::blink::MediaDeviceInfo::New();
      device_info->device_id = kFakeVideoInputDeviceId2;
      device_info->label = "Fake Video Input 2";
      device_info->group_id = kFakeVideoInputGroupId2;
      enumeration[static_cast<size_t>(
                      blink::mojom::blink::MediaDeviceType::MEDIA_VIDEO_INPUT)]
          .push_back(std::move(device_info));

      if (request_video_input_capabilities) {
        mojom::blink::VideoInputDeviceCapabilitiesPtr capabilities =
            mojom::blink::VideoInputDeviceCapabilities::New();
        capabilities->device_id = kFakeVideoInputDeviceId1;
        capabilities->group_id = kFakeCommonGroupId1;
        capabilities->facing_mode = blink::mojom::FacingMode::NONE;
        video_input_capabilities.push_back(std::move(capabilities));

        capabilities = mojom::blink::VideoInputDeviceCapabilities::New();
        capabilities->device_id = kFakeVideoInputDeviceId2;
        capabilities->group_id = kFakeVideoInputGroupId2;
        capabilities->facing_mode = blink::mojom::FacingMode::USER;
        video_input_capabilities.push_back(std::move(capabilities));
      }
    }
    if (request_audio_output) {
      device_info = mojom::blink::MediaDeviceInfo::New();
      device_info->device_id = kFakeAudioOutputDeviceId1;
      device_info->label = "Fake Audio Input 1";
      device_info->group_id = kFakeCommonGroupId1;
      enumeration[static_cast<size_t>(
                      blink::mojom::blink::MediaDeviceType::MEDIA_AUDIO_OUTPUT)]
          .push_back(std::move(device_info));
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
};

class PromiseObserver {
 public:
  PromiseObserver(ScriptState* script_state, ScriptPromise promise)
      : is_rejected_(false), is_fulfilled_(false) {
    v8::Local<v8::Function> on_fulfilled = MyScriptFunction::CreateFunction(
        script_state, &is_fulfilled_, &saved_arg_);
    v8::Local<v8::Function> on_rejected = MyScriptFunction::CreateFunction(
        script_state, &is_rejected_, &saved_arg_);
    promise.Then(on_fulfilled, on_rejected);
  }

  bool isDecided() { return is_rejected_ || is_fulfilled_; }

  bool isFulfilled() { return is_fulfilled_; }
  bool isRejected() { return is_rejected_; }
  ScriptValue argument() { return saved_arg_; }
  void Trace(blink::Visitor* visitor) { visitor->Trace(saved_arg_); }

 private:
  class MyScriptFunction : public ScriptFunction {
   public:
    static v8::Local<v8::Function> CreateFunction(ScriptState* script_state,
                                                  bool* flag_to_set,
                                                  ScriptValue* arg_to_set) {
      MyScriptFunction* self = MakeGarbageCollected<MyScriptFunction>(
          script_state, flag_to_set, arg_to_set);
      return self->BindToV8Function();
    }

    MyScriptFunction(ScriptState* script_state,
                     bool* flag_to_set,
                     ScriptValue* arg_to_set)
        : ScriptFunction(script_state),
          flag_to_set_(flag_to_set),
          arg_to_set_(arg_to_set) {}
    ScriptValue Call(ScriptValue arg) override {
      *flag_to_set_ = true;
      *arg_to_set_ = arg;
      return arg;
    }

   private:
    bool* flag_to_set_;
    ScriptValue* arg_to_set_;
  };

  bool is_rejected_;
  bool is_fulfilled_;
  ScriptValue saved_arg_;
};

class MediaDevicesTest : public testing::Test {
 public:
  using MediaDeviceInfos = HeapVector<Member<MediaDeviceInfo>>;

  MediaDevicesTest() : device_infos_(MakeGarbageCollected<MediaDeviceInfos>()) {
    dispatcher_host_ = std::make_unique<MockMediaDevicesDispatcherHost>();
  }

  MediaDevices* GetMediaDevices(ExecutionContext* context) {
    if (!media_devices_) {
      media_devices_ = MakeGarbageCollected<MediaDevices>(context);
      media_devices_->SetDispatcherHostForTesting(
          dispatcher_host_->CreatePendingRemoteAndBind());
    }
    return media_devices_;
  }

  void CloseBinding() { dispatcher_host_->CloseBinding(); }

  void SimulateDeviceChange() {
    DCHECK(listener());
    listener()->OnDevicesChanged(
        blink::mojom::blink::MediaDeviceType::MEDIA_AUDIO_INPUT,
        Vector<MediaDeviceInfoPtr>());
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

 private:
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
  std::unique_ptr<MockMediaDevicesDispatcherHost> dispatcher_host_;
  Persistent<MediaDeviceInfos> device_infos_;
  bool devices_enumerated_ = false;
  bool dispatcher_host_connection_error_ = false;
  bool device_changed_ = false;
  bool listener_connection_error_ = false;
  Persistent<MediaDevices> media_devices_;
};

TEST_F(MediaDevicesTest, GetUserMediaCanBeCalled) {
  V8TestingScope scope;
  MediaStreamConstraints* constraints = MediaStreamConstraints::Create();
  ScriptPromise promise =
      GetMediaDevices(scope.GetExecutionContext())
          ->getUserMedia(scope.GetScriptState(), constraints,
                         scope.GetExceptionState());
  ASSERT_FALSE(promise.IsEmpty());
  PromiseObserver promise_observer(scope.GetScriptState(), promise);
  EXPECT_FALSE(promise_observer.isDecided());
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_TRUE(promise_observer.isDecided());
  // In the default test environment, we expect a DOM rejection because
  // the script state's execution context's document's frame doesn't
  // have an UserMediaController.
  EXPECT_TRUE(promise_observer.isRejected());
  // TODO(hta): Check that the correct error ("not supported") is returned.
  EXPECT_FALSE(promise_observer.argument().IsNull());
  // This log statement is included as a demonstration of how to get the string
  // value of the argument.
  VLOG(1) << "Argument is"
          << ToCoreString(promise_observer.argument()
                              .V8Value()
                              ->ToString(scope.GetContext())
                              .ToLocalChecked());
}

TEST_F(MediaDevicesTest, EnumerateDevices) {
  V8TestingScope scope;
  auto* media_devices = GetMediaDevices(scope.GetExecutionContext());
  media_devices->SetEnumerateDevicesCallbackForTesting(
      WTF::Bind(&MediaDevicesTest::DevicesEnumerated, WTF::Unretained(this)));
  ScriptPromise promise =
      media_devices->enumerateDevices(scope.GetScriptState());
  platform()->RunUntilIdle();
  ASSERT_FALSE(promise.IsEmpty());

  EXPECT_TRUE(devices_enumerated());
  EXPECT_EQ(5u, device_infos().size());

  // Audio input device with matched output ID.
  Member<MediaDeviceInfo> device = device_infos()[0];
  EXPECT_FALSE(device->deviceId().IsEmpty());
  EXPECT_EQ("audioinput", device->kind());
  EXPECT_FALSE(device->label().IsEmpty());
  EXPECT_FALSE(device->groupId().IsEmpty());

  // Audio input device without matched output ID.
  device = device_infos()[1];
  EXPECT_FALSE(device->deviceId().IsEmpty());
  EXPECT_EQ("audioinput", device->kind());
  EXPECT_FALSE(device->label().IsEmpty());
  EXPECT_FALSE(device->groupId().IsEmpty());

  // Video input devices.
  device = device_infos()[2];
  EXPECT_FALSE(device->deviceId().IsEmpty());
  EXPECT_EQ("videoinput", device->kind());
  EXPECT_FALSE(device->label().IsEmpty());
  EXPECT_FALSE(device->groupId().IsEmpty());

  device = device_infos()[3];
  EXPECT_FALSE(device->deviceId().IsEmpty());
  EXPECT_EQ("videoinput", device->kind());
  EXPECT_FALSE(device->label().IsEmpty());
  EXPECT_FALSE(device->groupId().IsEmpty());

  // Audio output device.
  device = device_infos()[4];
  EXPECT_FALSE(device->deviceId().IsEmpty());
  EXPECT_EQ("audiooutput", device->kind());
  EXPECT_FALSE(device->label().IsEmpty());
  EXPECT_FALSE(device->groupId().IsEmpty());

  // Verify group IDs.
  EXPECT_EQ(device_infos()[0]->groupId(), device_infos()[2]->groupId());
  EXPECT_EQ(device_infos()[0]->groupId(), device_infos()[4]->groupId());
  EXPECT_NE(device_infos()[1]->groupId(), device_infos()[4]->groupId());
}

TEST_F(MediaDevicesTest, EnumerateDevicesAfterConnectionError) {
  V8TestingScope scope;
  auto* media_devices = GetMediaDevices(scope.GetExecutionContext());
  media_devices->SetEnumerateDevicesCallbackForTesting(
      WTF::Bind(&MediaDevicesTest::DevicesEnumerated, WTF::Unretained(this)));
  media_devices->SetConnectionErrorCallbackForTesting(
      WTF::Bind(&MediaDevicesTest::OnDispatcherHostConnectionError,
                WTF::Unretained(this)));
  EXPECT_FALSE(dispatcher_host_connection_error());

  // Simulate a connection error by closing the binding.
  CloseBinding();
  platform()->RunUntilIdle();

  ScriptPromise promise =
      media_devices->enumerateDevices(scope.GetScriptState());
  platform()->RunUntilIdle();
  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_TRUE(dispatcher_host_connection_error());
  EXPECT_FALSE(devices_enumerated());
}

TEST_F(MediaDevicesTest, EnumerateDevicesBeforeConnectionError) {
  V8TestingScope scope;
  auto* media_devices = GetMediaDevices(scope.GetExecutionContext());
  media_devices->SetEnumerateDevicesCallbackForTesting(
      WTF::Bind(&MediaDevicesTest::DevicesEnumerated, WTF::Unretained(this)));
  media_devices->SetConnectionErrorCallbackForTesting(
      WTF::Bind(&MediaDevicesTest::OnDispatcherHostConnectionError,
                WTF::Unretained(this)));
  EXPECT_FALSE(dispatcher_host_connection_error());

  ScriptPromise promise =
      media_devices->enumerateDevices(scope.GetScriptState());
  platform()->RunUntilIdle();
  ASSERT_FALSE(promise.IsEmpty());

  // Simulate a connection error by closing the binding.
  CloseBinding();
  platform()->RunUntilIdle();
  EXPECT_TRUE(dispatcher_host_connection_error());
  EXPECT_TRUE(devices_enumerated());
}

TEST_F(MediaDevicesTest, ObserveDeviceChangeEvent) {
  V8TestingScope scope;
  auto* media_devices = GetMediaDevices(scope.GetExecutionContext());
  media_devices->SetDeviceChangeCallbackForTesting(
      WTF::Bind(&MediaDevicesTest::OnDevicesChanged, WTF::Unretained(this)));
  EXPECT_FALSE(listener());

  // Subscribe for device change event.
  media_devices->StartObserving();
  platform()->RunUntilIdle();
  EXPECT_TRUE(listener());
  listener().set_disconnect_handler(WTF::Bind(
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

}  // namespace blink
