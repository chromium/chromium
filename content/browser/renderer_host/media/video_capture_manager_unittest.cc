// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit test for VideoCaptureManager.

#include "content/browser/renderer_host/media/video_capture_manager.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/media/fake_video_capture_provider.h"
#include "content/browser/renderer_host/media/in_process_video_capture_provider.h"
#include "content/browser/renderer_host/media/media_stream_provider.h"
#include "content/browser/renderer_host/media/video_capture_controller_event_handler.h"
#include "content/browser/renderer_host/media/video_capture_provider_switcher.h"
#include "content/browser/screenlock_monitor/screenlock_monitor.h"
#include "content/browser/screenlock_monitor/screenlock_monitor_source.h"
#include "content/common/buildflags.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "media/base/media_switches.h"
#include "media/capture/video/fake_video_capture_device_factory.h"
#include "media/capture/video/video_capture_system_impl.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom-forward.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

namespace content {

namespace {

// Wraps FakeVideoCaptureDeviceFactory to allow mocking of the
// VideoCaptureDevice MaybeSuspend() and Resume() methods. This is used to check
// that devices are asked to suspend or resume at the correct times.
class WrappedDeviceFactory final : public media::FakeVideoCaptureDeviceFactory {
 public:
  class WrappedDevice final : public media::VideoCaptureDevice {
   public:
    WrappedDevice(std::unique_ptr<media::VideoCaptureDevice> device,
                  WrappedDeviceFactory* factory)
        : device_(std::move(device)), factory_(factory) {
      factory_->OnDeviceCreated(this);
    }

    WrappedDevice(const WrappedDevice&) = delete;
    WrappedDevice& operator=(const WrappedDevice&) = delete;

    ~WrappedDevice() override { factory_->OnDeviceDestroyed(this); }

    void AllocateAndStart(const media::VideoCaptureParams& params,
                          std::unique_ptr<Client> client) override {
      device_->AllocateAndStart(params, std::move(client));
    }

    void RequestRefreshFrame() override { device_->RequestRefreshFrame(); }

    void MaybeSuspend() override {
      factory_->WillSuspendDevice();
      device_->MaybeSuspend();
    }

    void Resume() override {
      factory_->WillResumeDevice();
      device_->Resume();
    }

    void StopAndDeAllocate() override { device_->StopAndDeAllocate(); }

    void GetPhotoState(GetPhotoStateCallback callback) override {
      device_->GetPhotoState(std::move(callback));
    }

    void TakePhoto(TakePhotoCallback callback) override {
      device_->TakePhoto(std::move(callback));
    }

   private:
    const std::unique_ptr<media::VideoCaptureDevice> device_;
    const raw_ptr<WrappedDeviceFactory> factory_;
  };

  static const media::VideoFacingMode DEFAULT_FACING =
      media::VideoFacingMode::MEDIA_VIDEO_FACING_USER;

  WrappedDeviceFactory() = default;

  WrappedDeviceFactory(const WrappedDeviceFactory&) = delete;
  WrappedDeviceFactory& operator=(const WrappedDeviceFactory&) = delete;

  ~WrappedDeviceFactory() override = default;

  media::VideoCaptureErrorOrDevice CreateDevice(
      const media::VideoCaptureDeviceDescriptor& device_descriptor) override {
    auto device = std::make_unique<WrappedDevice>(
        FakeVideoCaptureDeviceFactory::CreateDevice(device_descriptor)
            .ReleaseDevice(),
        this);
    return media::VideoCaptureErrorOrDevice(std::move(device));
  }

  void GetDevicesInfo(GetDevicesInfoCallback callback) override {
    media::FakeVideoCaptureDeviceFactory::GetDevicesInfo(
        base::BindLambdaForTesting(
            [callback =
                 std::move(callback)](std::vector<media::VideoCaptureDeviceInfo>
                                          devices_info) mutable {
              for (auto& device : devices_info) {
                if (device.descriptor.facing ==
                    media::VideoFacingMode::MEDIA_VIDEO_FACING_NONE) {
                  device.descriptor.facing = DEFAULT_FACING;
                }
              }
              std::move(callback).Run(std::move(devices_info));
            }));
  }

  bool has_active_devices() const { return !devices_.empty(); }

  MOCK_METHOD0(WillSuspendDevice, void());
  MOCK_METHOD0(WillResumeDevice, void());

 private:
  void OnDeviceCreated(WrappedDevice* device) { devices_.push_back(device); }

  void OnDeviceDestroyed(WrappedDevice* device) {
    const auto it = base::ranges::find(devices_, device);
    CHECK(it != devices_.end());
    devices_.erase(it);
  }

  std::vector<raw_ptr<WrappedDevice, VectorExperimental>> devices_;
};

// Listener class used to track progress of VideoCaptureManager test.
class MockMediaStreamProviderListener : public MediaStreamProviderListener {
 public:
  MockMediaStreamProviderListener() {}
  ~MockMediaStreamProviderListener() override {}

  MOCK_METHOD2(Opened,
               void(blink::mojom::MediaStreamType,
                    const base::UnguessableToken&));
  MOCK_METHOD2(Closed,
               void(blink::mojom::MediaStreamType,
                    const base::UnguessableToken&));
  MOCK_METHOD2(Aborted,
               void(blink::mojom::MediaStreamType,
                    const base::UnguessableToken&));
};  // class MockMediaStreamProviderListener

// Needed as an input argument to ConnectClient().
class MockFrameObserver : public VideoCaptureControllerEventHandler {
 public:
  MOCK_METHOD2(OnError,
               void(const VideoCaptureControllerID& id,
                    media::VideoCaptureError error));
  MOCK_METHOD1(OnStarted, void(const VideoCaptureControllerID& id));
  MOCK_METHOD1(OnStartedUsingGpuDecode,
               void(const VideoCaptureControllerID& id));

  void OnCaptureConfigurationChanged(
      const VideoCaptureControllerID& id) override {}
  void OnNewBuffer(const VideoCaptureControllerID& id,
                   media::mojom::VideoBufferHandlePtr buffer_handle,
                   int buffer_id) override {}
  void OnBufferDestroyed(const VideoCaptureControllerID& id,
                         int buffer_id) override {}
  void OnBufferReady(const VideoCaptureControllerID& id,
                     const ReadyBuffer& buffer) override {}
  void OnFrameDropped(const VideoCaptureControllerID& id,
                      media::VideoCaptureFrameDropReason reason) override {}
  void OnNewSubCaptureTargetVersion(
      const VideoCaptureControllerID& id,
      uint32_t sub_capture_target_version) override {}
  void OnFrameWithEmptyRegionCapture(const VideoCaptureControllerID&) override {
  }
  void OnEnded(const VideoCaptureControllerID& id) override {}

  void OnGotControllerCallback(const VideoCaptureControllerID&) {}
};

// Input argument for testing AddVideoCaptureObserver().
class MockVideoCaptureObserver : public media::VideoCaptureObserver {
 public:
  MOCK_METHOD1(OnVideoCaptureStarted, void(media::VideoFacingMode));
  MOCK_METHOD1(OnVideoCaptureStopped, void(media::VideoFacingMode));
};

// Needed to generate ScreenLocked event to |vcm_|.
class ScreenlockMonitorTestSource : public ScreenlockMonitorSource {
 public:
  ScreenlockMonitorTestSource() = default;
  ~ScreenlockMonitorTestSource() override = default;

  void GenerateScreenLockedEvent() {
    ProcessScreenlockEvent(SCREEN_LOCK_EVENT);
    base::RunLoop().RunUntilIdle();
  }

  void GenerateScreenUnlockedEvent() {
    ProcessScreenlockEvent(SCREEN_UNLOCK_EVENT);
    base::RunLoop().RunUntilIdle();
  }
};

#if !BUILDFLAG(IS_ANDROID)
class MockBrowserClient : public content::ContentBrowserClient {
 public:
  MOCK_METHOD(void,
              BindVideoEffectsManager,
              (const std::string& device_id,
               content::BrowserContext* browser_context,
               mojo::PendingReceiver<media::mojom::VideoEffectsManager>
                   video_effects_manager),
              (override));

  MOCK_METHOD(
      void,
      BindVideoEffectsProcessor,
      (const std::string& device_id,
       content::BrowserContext* browser_context,
       mojo::PendingReceiver<video_effects::mojom::VideoEffectsProcessor>
           video_effects_processor),
      (override));
};
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

// Test class
class VideoCaptureManagerTest : public testing::Test {
 public:
  VideoCaptureManagerTest() {}

  VideoCaptureManagerTest(const VideoCaptureManagerTest&) = delete;
  VideoCaptureManagerTest& operator=(const VideoCaptureManagerTest&) = delete;

  ~VideoCaptureManagerTest() override {}

  void HandleEnumerationResult(
      base::OnceClosure quit_closure,
      media::mojom::DeviceEnumerationResult result,
      const media::VideoCaptureDeviceDescriptors& descriptors) {
    blink::MediaStreamDevices devices;
    for (const auto& descriptor : descriptors) {
      devices.emplace_back(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
                           descriptor.device_id, descriptor.GetNameAndModel());
    }
    devices_ = devices;
    std::move(quit_closure).Run();
  }

  void HandleEnumerationResultAsDisplayMediaDevices(
      base::OnceClosure quit_closure,
      media::mojom::DeviceEnumerationResult result,
      const media::VideoCaptureDeviceDescriptors& descriptors) {
    blink::MediaStreamDevices devices;
    for (const auto& descriptor : descriptors) {
      devices.emplace_back(blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
                           DesktopMediaID(DesktopMediaID::TYPE_SCREEN,
                                          DesktopMediaID::kFakeId, false)
                               .ToString(),
                           descriptor.GetNameAndModel());
    }
    devices_ = devices;
    std::move(quit_closure).Run();
  }

 protected:
  void SetUp() override {
#if !BUILDFLAG(IS_ANDROID)
    content::SetBrowserClientForTesting(&browser_client_);
#endif  // !BUILDFLAG(IS_ANDROID)
    listener_ = std::make_unique<MockMediaStreamProviderListener>();
    auto video_capture_device_factory =
        std::make_unique<WrappedDeviceFactory>();
    video_capture_device_factory_ = video_capture_device_factory.get();
    auto fake_video_capture_provider =
        std::make_unique<FakeVideoCaptureProvider>(
            std::move(video_capture_device_factory));
    auto screencapture_video_capture_provider =
        InProcessVideoCaptureProvider::CreateInstanceForScreenCapture(
            base::SingleThreadTaskRunner::GetCurrentDefault());
    auto video_capture_provider_switcher =
        std::make_unique<VideoCaptureProviderSwitcher>(
            std::move(fake_video_capture_provider),
            std::move(screencapture_video_capture_provider));
    screenlock_monitor_source_ = new ScreenlockMonitorTestSource();
    screenlock_monitor_ = std::make_unique<ScreenlockMonitor>(
        std::unique_ptr<ScreenlockMonitorSource>(screenlock_monitor_source_));

    vcm_ = new VideoCaptureManager(std::move(video_capture_provider_switcher),
                                   base::DoNothing());
    const int32_t kNumberOfFakeDevices = 2;
    video_capture_device_factory_->SetToDefaultDevicesConfig(
        kNumberOfFakeDevices);
    vcm_->RegisterListener(listener_.get());
    frame_observer_ = std::make_unique<MockFrameObserver>();

    base::RunLoop run_loop;
    vcm_->EnumerateDevices(
        base::BindOnce(&VideoCaptureManagerTest::HandleEnumerationResult,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    ASSERT_GE(devices_.size(), 2u);
  }

  void TearDown() override {
    screenlock_monitor_source_ = nullptr;
    task_environment_.RunUntilIdle();
  }

  void OnGotControllerCallback(
      VideoCaptureControllerID id,
      bool expect_success,
      const base::WeakPtr<VideoCaptureController>& controller) {
    if (expect_success) {
      ASSERT_TRUE(controller);
      ASSERT_TRUE(0 == controllers_.count(id));
      controllers_[id] = controller.get();
    } else {
      ASSERT_FALSE(controller);
    }
  }

  VideoCaptureControllerID StartClient(const base::UnguessableToken& session_id,
                                       bool expect_success) {
    media::VideoCaptureParams params;
    params.requested_format = media::VideoCaptureFormat(
        gfx::Size(320, 240), 30, media::PIXEL_FORMAT_I420);

    VideoCaptureControllerID client_id = base::UnguessableToken::Create();
    vcm_->ConnectClient(
        session_id, params, client_id, frame_observer_.get(), std::nullopt,
        base::BindOnce(&VideoCaptureManagerTest::OnGotControllerCallback,
                       base::Unretained(this), client_id, expect_success),
        /*browser_context=*/&browser_context_);
    base::RunLoop().RunUntilIdle();
    return client_id;
  }

  void StopClient(const VideoCaptureControllerID& client_id) {
    ASSERT_TRUE(1 == controllers_.count(client_id));
    vcm_->DisconnectClient(controllers_[client_id], client_id,
                           frame_observer_.get(),
                           media::VideoCaptureError::kNone);
    controllers_.erase(client_id);
  }

  void ResumeClient(const base::UnguessableToken& session_id,
                    const VideoCaptureControllerID& client_id) {
    ASSERT_EQ(1u, controllers_.count(client_id));
    media::VideoCaptureParams params;
    params.requested_format = media::VideoCaptureFormat(
        gfx::Size(320, 240), 30, media::PIXEL_FORMAT_I420);
    vcm_->ResumeCaptureForClient(session_id, params, controllers_[client_id],
                                 client_id, frame_observer_.get());
    // Allow possible VideoCaptureDevice::Resume() task to run.
    base::RunLoop().RunUntilIdle();
  }

  void PauseClient(const VideoCaptureControllerID& client_id) {
    ASSERT_EQ(1u, controllers_.count(client_id));
    vcm_->PauseCaptureForClient(controllers_[client_id], client_id,
                                frame_observer_.get());
    // Allow possible VideoCaptureDevice::MaybeSuspend() task to run.
    base::RunLoop().RunUntilIdle();
  }

#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_DESKTOP_ANDROID)
  void ApplicationStateChange(base::android::ApplicationState state) {
    vcm_->OnApplicationStateChange(state);
  }
#endif

  BrowserTaskEnvironment task_environment_;
  raw_ptr<ScreenlockMonitorTestSource> screenlock_monitor_source_;
  std::unique_ptr<ScreenlockMonitor> screenlock_monitor_;
  std::map<VideoCaptureControllerID,
           raw_ptr<VideoCaptureController, CtnExperimental>>
      controllers_;
  scoped_refptr<VideoCaptureManager> vcm_;
  std::unique_ptr<MockMediaStreamProviderListener> listener_;
  std::unique_ptr<MockFrameObserver> frame_observer_;
  raw_ptr<WrappedDeviceFactory> video_capture_device_factory_;
  blink::MediaStreamDevices devices_;
  content::TestBrowserContext browser_context_;
#if !BUILDFLAG(IS_ANDROID)
  MockBrowserClient browser_client_;
#endif  // !BUILDFLAG(IS_ANDROID)
};

// Test cases

// Try to open, start, stop and close a device.
TEST_F(VideoCaptureManagerTest, CreateAndClose) {
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(browser_client_, BindVideoEffectsManager(_, _, _)).Times(0);
#endif  // !BUILDFLAG(IS_ANDROID)
  InSequence s;
  EXPECT_CALL(*listener_,
              Opened(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _));
  EXPECT_CALL(*frame_observer_, OnStarted(_));
  EXPECT_CALL(*listener_,
              Closed(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _));

  base::UnguessableToken video_session_id = vcm_->Open(devices_.front());
  VideoCaptureControllerID client_id = StartClient(video_session_id, true);

  StopClient(client_id);
  vcm_->Close(video_session_id);

  // Wait to check callbacks before removing the listener.
  base::RunLoop().RunUntilIdle();
  vcm_->UnregisterListener(listener_.get());
}

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)
// Try to start and stop a device with an effects processor
TEST_F(VideoCaptureManagerTest, CreateWithVideoEffectsProcessor) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kCameraMicEffects);
  mojo::PendingReceiver<media::mojom::VideoEffectsManager> receiver;
  EXPECT_CALL(browser_client_, BindVideoEffectsProcessor(devices_.front().id,
                                                         &browser_context_, _))
      .Times(1);

  base::UnguessableToken video_session_id = vcm_->Open(devices_.front());
  auto client_id = StartClient(video_session_id, true);
  StopClient(client_id);
}
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID) &&
        // !BUILDFLAG(IS_FUCHSIA)

TEST_F(VideoCaptureManagerTest, CreateAndCloseMultipleTimes) {
  InSequence s;
  for (int i = 1; i < 3; ++i) {
    EXPECT_CALL(*listener_,
                Opened(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _));
    EXPECT_CALL(*frame_observer_, OnStarted(_));
    EXPECT_CALL(*listener_,
                Closed(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _));
    base::UnguessableToken video_session_id = vcm_->Open(devices_.front());
    VideoCaptureControllerID client_id = StartClient(video_session_id, true);

    StopClient(client_id);
    vcm_->Close(video_session_id);
  }

  // Wait to check callbacks before removing the listener.
  base::RunLoop().RunUntilIdle();
  vcm_->UnregisterListener(listener_.get());
}

// Try to open, start, and abort a device.
TEST_F(VideoCaptureManagerTest, CreateAndAbort) {
  InSequence s;
  EXPECT_CALL(*listener_,
              Opened(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _));
  EXPECT_CALL(*frame_observer_, OnStarted(_));
  EXPECT_CALL(*listener_,
              Aborted(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _));

  base::UnguessableToken video_session_id = vcm_->Open(devices_.front());
  VideoCaptureControllerID client_id = StartClient(video_session_id, true);

  // Wait for device opened.
  base::RunLoop().RunUntilIdle();

  vcm_->DisconnectClient(
      controllers_[client_id], client_id, frame_observer_.get(),
      media::VideoCaptureError::kIntentionalErrorRaisedByUnitTest);

  // Wait to check callbacks before removing the listener.
  base::RunLoop().RunUntilIdle();
  vcm_->UnregisterListener(listener_.get());
}

TEST_F(VideoCaptureManagerTest, AddObserver) {
  InSequence s;
  MockVideoCaptureObserver observer;
  vcm_->AddVideoCaptureObserver(&observer);

  EXPECT_CALL(observer,
              OnVideoCaptureStarted(WrappedDeviceFactory::DEFAULT_FACING));
  EXPECT_CALL(observer,
              OnVideoCaptureStopped(WrappedDeviceFactory::DEFAULT_FACING));

  base::UnguessableToken video_session_id = vcm_->Open(devices_.front());
  VideoCaptureControllerID client_id = StartClient(video_session_id, true);

  StopClient(client_id);
  vcm_->Close(video_session_id);

  // Wait to check callbacks before removing the listener.
  base::RunLoop().RunUntilIdle();
  vcm_->UnregisterListener(listener_.get());
}

// Open the same device twice.
TEST_F(VideoCaptureManagerTest, OpenTwice) {
  InSequence s;
  EXPECT_CALL(*listener_,
              Opened(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _))
      .Times(2);
  EXPECT_CALL(*listener_,
              Closed(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _))
      .Times(2);

  base::UnguessableToken video_session_id_first = vcm_->Open(devices_.front());

  // This should trigger an error callback with error code
  // 'kDeviceAlreadyInUse'.
  base::UnguessableToken video_session_id_second = vcm_->Open(devices_.front());
  EXPECT_NE(video_session_id_first, video_session_id_second);

  vcm_->Close(video_session_id_first);
  vcm_->Close(video_session_id_second);

  // Wait to check callbacks before removing the listener.
  base::RunLoop().RunUntilIdle();
  vcm_->UnregisterListener(listener_.get());
}

// Connect and disconnect devices.
TEST_F(VideoCaptureManagerTest, ConnectAndDisconnectDevices) {
  int number_of_devices_keep =
      video_capture_device_factory_->number_of_devices();

  // Simulate we remove 1 fake device.
  video_capture_device_factory_->SetToDefaultDevicesConfig(1);
  base::RunLoop run_loop;
  vcm_->EnumerateDevices(
      base::BindOnce(&VideoCaptureManagerTest::HandleEnumerationResult,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  ASSERT_EQ(devices_.size(), 1u);

  // Simulate we add 2 fake devices.
  video_capture_device_factory_->SetToDefaultDevicesConfig(3);
  base::RunLoop run_loop2;
  vcm_->EnumerateDevices(
      base::BindOnce(&VideoCaptureManagerTest::HandleEnumerationResult,
                     base::Unretained(this), run_loop2.QuitClosure()));
  run_loop2.Run();
  ASSERT_EQ(devices_.size(), 3u);

  vcm_->UnregisterListener(listener_.get());
  video_capture_device_factory_->SetToDefaultDevicesConfig(
      number_of_devices_keep);
}

// Enumerate devices and open the first, then check the list of supported
// formats. Then start the opened device. The capability list should stay the
// same. Finally stop the device and check that the capabilities stay unchanged.
TEST_F(VideoCaptureManagerTest, ManipulateDeviceAndCheckCapabilities) {
  // Before enumerating the devices, requesting formats should return false.
  base::UnguessableToken video_session_id;
  media::VideoCaptureFormats supported_formats;
  supported_formats.clear();
  EXPECT_FALSE(
      vcm_->GetDeviceSupportedFormats(video_session_id, &supported_formats));

  InSequence s;
  EXPECT_CALL(*listener_,
              Opened(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _));
  video_session_id = vcm_->Open(devices_.front());
  base::RunLoop().RunUntilIdle();

  // Right after opening the device, we should see all its formats.
  supported_formats.clear();
  EXPECT_TRUE(
      vcm_->GetDeviceSupportedFormats(video_session_id, &supported_formats));
  ASSERT_GT(supported_formats.size(), 1u);
  EXPECT_GT(supported_formats[0].frame_size.width(), 1);
  EXPECT_GT(supported_formats[0].frame_size.height(), 1);
  EXPECT_GT(supported_formats[0].frame_rate, 1);
  EXPECT_GT(supported_formats[1].frame_size.width(), 1);
  EXPECT_GT(supported_formats[1].frame_size.height(), 1);
  EXPECT_GT(supported_formats[1].frame_rate, 1);

  EXPECT_CALL(*frame_observer_, OnStarted(_));
  VideoCaptureControllerID client_id = StartClient(video_session_id, true);
  base::RunLoop().RunUntilIdle();
  // After StartClient(), device's supported formats should stay the same.
  supported_formats.clear();
  EXPECT_TRUE(
      vcm_->GetDeviceSupportedFormats(video_session_id, &supported_formats));
  ASSERT_GE(supported_formats.size(), 2u);
  EXPECT_GT(supported_formats[0].frame_size.width(), 1);
  EXPECT_GT(supported_formats[0].frame_size.height(), 1);
  EXPECT_GT(supported_formats[0].frame_rate, 1);
  EXPECT_GT(supported_formats[1].frame_size.width(), 1);
  EXPECT_GT(supported_formats[1].frame_size.height(), 1);
  EXPECT_GT(supported_formats[1].frame_rate, 1);

  EXPECT_CALL(*listener_,
              Closed(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _));
  StopClient(client_id);
  supported_formats.clear();
  EXPECT_TRUE(
      vcm_->GetDeviceSupportedFormats(video_session_id, &supported_formats));
  ASSERT_GE(supported_formats.size(), 2u);
  EXPECT_GT(supported_formats[0].frame_size.width(), 1);
  EXPECT_GT(supported_formats[0].frame_size.height(), 1);
  EXPECT_GT(supported_formats[0].frame_rate, 1);
  EXPECT_GT(supported_formats[1].frame_size.width(), 1);
  EXPECT_GT(supported_formats[1].frame_size.height(), 1);
  EXPECT_GT(supported_formats[1].frame_rate, 1);

  vcm_->Close(video_session_id);
  base::RunLoop().RunUntilIdle();
  vcm_->UnregisterListener(listener_.get());
}

// Enumerate devices, then check the list of supported formats. Then open and
// start the first device. The capability list should stay the same. Finally
// stop the device and check that the capabilities stay unchanged.
TEST_F(VideoCaptureManagerTest,
       ManipulateDeviceAndCheckCapabilitiesWithDeviceId) {
  // Requesting formats should work even before enumerating/opening devices.
  std::string device_id = devices_.front().id;
  media::VideoCaptureFormats supported_formats;
  supported_formats.clear();
  EXPECT_TRUE(vcm_->GetDeviceSupportedFormats(device_id, &supported_formats));
  ASSERT_GE(supported_formats.size(), 2u);
  EXPECT_GT(supported_formats[0].frame_size.width(), 1);
  EXPECT_GT(supported_formats[0].frame_size.height(), 1);
  EXPECT_GT(supported_formats[0].frame_rate, 1);
  EXPECT_GT(supported_formats[1].frame_size.width(), 1);
  EXPECT_GT(supported_formats[1].frame_size.height(), 1);
  EXPECT_GT(supported_formats[1].frame_rate, 1);

  InSequence s;
  EXPECT_CALL(*listener_,
              Opened(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _));
  base::UnguessableToken video_session_id = vcm_->Open(devices_.front());
  base::RunLoop().RunUntilIdle();

  // Right after opening the device, we should see all its formats.
  supported_formats.clear();
  EXPECT_TRUE(vcm_->GetDeviceSupportedFormats(device_id, &supported_formats));
  ASSERT_GE(supported_formats.size(), 2u);
  EXPECT_GT(supported_formats[0].frame_size.width(), 1);
  EXPECT_GT(supported_formats[0].frame_size.height(), 1);
  EXPECT_GT(supported_formats[0].frame_rate, 1);
  EXPECT_GT(supported_formats[1].frame_size.width(), 1);
  EXPECT_GT(supported_formats[1].frame_size.height(), 1);
  EXPECT_GT(supported_formats[1].frame_rate, 1);

  EXPECT_CALL(*frame_observer_, OnStarted(_));
  VideoCaptureControllerID client_id = StartClient(video_session_id, true);
  base::RunLoop().RunUntilIdle();
  // After StartClient(), device's supported formats should stay the same.
  supported_formats.clear();
  EXPECT_TRUE(vcm_->GetDeviceSupportedFormats(device_id, &supported_formats));
  ASSERT_GE(supported_formats.size(), 2u);
  EXPECT_GT(supported_formats[0].frame_size.width(), 1);
  EXPECT_GT(supported_formats[0].frame_size.height(), 1);
  EXPECT_GT(supported_formats[0].frame_rate, 1);
  EXPECT_GT(supported_formats[1].frame_size.width(), 1);
  EXPECT_GT(supported_formats[1].frame_size.height(), 1);
  EXPECT_GT(supported_formats[1].frame_rate, 1);

  EXPECT_CALL(*listener_,
              Closed(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _));
  StopClient(client_id);
  supported_formats.clear();
  EXPECT_TRUE(vcm_->GetDeviceSupportedFormats(device_id, &supported_formats));
  ASSERT_GE(supported_formats.size(), 2u);
  EXPECT_GT(supported_formats[0].frame_size.width(), 1);
  EXPECT_GT(supported_formats[0].frame_size.height(), 1);
  EXPECT_GT(supported_formats[0].frame_rate, 1);
  EXPECT_GT(supported_formats[1].frame_size.width(), 1);
  EXPECT_GT(supported_formats[1].frame_size.height(), 1);
  EXPECT_GT(supported_formats[1].frame_rate, 1);

  vcm_->Close(video_session_id);
  base::RunLoop().RunUntilIdle();
  vcm_->UnregisterListener(listener_.get());
}

// Enumerate devices and open the first, then check the formats currently in
// use, which should be an empty vector. Then start the opened device. The
// format(s) in use should be just one format (the one used when configuring-
// starting the device). Finally stop the device and check that the formats in
// use is an empty vector.
TEST_F(VideoCaptureManagerTest, StartDeviceAndGetDeviceFormatInUse) {
  InSequence s;
  EXPECT_CALL(*listener_,
              Opened(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _));
  base::UnguessableToken video_session_id = vcm_->Open(devices_.front());
  base::RunLoop().RunUntilIdle();

  // Right after opening the device, we should see no format in use.
  media::VideoCaptureFormats formats_in_use;
  EXPECT_TRUE(vcm_->GetDeviceFormatsInUse(video_session_id, &formats_in_use));
  EXPECT_TRUE(formats_in_use.empty());

  EXPECT_CALL(*frame_observer_, OnStarted(_));
  VideoCaptureControllerID client_id = StartClient(video_session_id, true);
  base::RunLoop().RunUntilIdle();
  // After StartClient(), |formats_in_use| should contain one valid format.
  EXPECT_TRUE(vcm_->GetDeviceFormatsInUse(video_session_id, &formats_in_use));
  EXPECT_EQ(formats_in_use.size(), 1u);
  if (formats_in_use.size()) {
    media::VideoCaptureFormat& format_in_use = formats_in_use.front();
    EXPECT_TRUE(format_in_use.IsValid());
    EXPECT_GT(format_in_use.frame_size.width(), 1);
    EXPECT_GT(format_in_use.frame_size.height(), 1);
    EXPECT_GT(format_in_use.frame_rate, 1);
  }
  formats_in_use.clear();

  EXPECT_CALL(*listener_,
              Closed(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _));
  StopClient(client_id);
  base::RunLoop().RunUntilIdle();
  // After StopClient(), the device's formats in use should be empty again.
  EXPECT_TRUE(vcm_->GetDeviceFormatsInUse(video_session_id, &formats_in_use));
  EXPECT_TRUE(formats_in_use.empty());

  vcm_->Close(video_session_id);
  base::RunLoop().RunUntilIdle();
  vcm_->UnregisterListener(listener_.get());
}

// Enumerate devices and open the first, then check the formats currently in
// use, which should be an empty vector. Then start the opened device. The
// format(s) in use should be just one format (the one used when configuring-
// starting the device). Finally stop the device and check that the formats in
// use is an empty vector.
TEST_F(VideoCaptureManagerTest,
       StartDeviceAndGetDeviceFormatInUseWithDeviceId) {
  std::string device_id = devices_.front().id;
  InSequence s;
  EXPECT_CALL(*listener_,
              Opened(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _));
  base::UnguessableToken video_session_id = vcm_->Open(devices_.front());
  base::RunLoop().RunUntilIdle();

  // Right after opening the device, we should see no format in use.
  EXPECT_EQ(
      std::nullopt,
      vcm_->GetDeviceFormatInUse(
          blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, device_id));

  EXPECT_CALL(*frame_observer_, OnStarted(_));
  VideoCaptureControllerID client_id = StartClient(video_session_id, true);
  base::RunLoop().RunUntilIdle();
  // After StartClient(), device's format in use should be valid.
  std::optional<media::VideoCaptureFormat> format_in_use =
      vcm_->GetDeviceFormatInUse(
          blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, device_id);
  EXPECT_TRUE(format_in_use.has_value());
  EXPECT_TRUE(format_in_use->IsValid());
  EXPECT_GT(format_in_use->frame_size.width(), 1);
  EXPECT_GT(format_in_use->frame_size.height(), 1);
  EXPECT_GT(format_in_use->frame_rate, 1);

  EXPECT_CALL(*listener_,
              Closed(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _));
  StopClient(client_id);
  base::RunLoop().RunUntilIdle();
  // After StopClient(), the device's format in use should be empty again.
  EXPECT_EQ(
      std::nullopt,
      vcm_->GetDeviceFormatInUse(
          blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, device_id));

  vcm_->Close(video_session_id);
  base::RunLoop().RunUntilIdle();
  vcm_->UnregisterListener(listener_.get());
}

// Open two different devices.
TEST_F(VideoCaptureManagerTest, OpenTwo) {
  InSequence s;
  EXPECT_CALL(*listener_,
              Opened(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _))
      .Times(2);
  EXPECT_CALL(*listener_,
              Closed(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _))
      .Times(2);

  auto it = devices_.begin();

  base::UnguessableToken video_session_id_first = vcm_->Open(*it);
  ++it;
  base::UnguessableToken video_session_id_second = vcm_->Open(*it);

  vcm_->Close(video_session_id_first);
  vcm_->Close(video_session_id_second);

  // Wait to check callbacks before removing the listener.
  base::RunLoop().RunUntilIdle();
  vcm_->UnregisterListener(listener_.get());
}

// Try open a non-existing device.
TEST_F(VideoCaptureManagerTest, OpenNotExisting) {
  InSequence s;
  EXPECT_CALL(*frame_observer_, OnError(_, _));
  EXPECT_CALL(*listener_,
              Opened(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _));
  EXPECT_CALL(*listener_,
              Closed(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _));

  blink::mojom::MediaStreamType stream_type =
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE;
  std::string device_name("device_doesnt_exist");
  std::string device_id("id_doesnt_exist");
  blink::MediaStreamDevice dummy_device(stream_type, device_id, device_name);

  // This should fail with an error to the controller.
  base::UnguessableToken session_id = vcm_->Open(dummy_device);
  VideoCaptureControllerID client_id = StartClient(session_id, true);
  base::RunLoop().RunUntilIdle();

  StopClient(client_id);
  vcm_->Close(session_id);
  base::RunLoop().RunUntilIdle();

  vcm_->UnregisterListener(listener_.get());
}

// Start a device without calling Open, using a non-magic ID.
TEST_F(VideoCaptureManagerTest, StartInvalidSession) {
  StartClient(base::UnguessableToken::Create(), false);

  // Wait to check callbacks before removing the listener.
  base::RunLoop().RunUntilIdle();
  vcm_->UnregisterListener(listener_.get());
}

// Open and start a device, close it before calling Stop.
TEST_F(VideoCaptureManagerTest, CloseWithoutStop) {
  InSequence s;
  EXPECT_CALL(*listener_,
              Opened(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _));
  EXPECT_CALL(*frame_observer_, OnStarted(_));
  EXPECT_CALL(*listener_,
              Closed(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _));

  base::UnguessableToken video_session_id = vcm_->Open(devices_.front());

  VideoCaptureControllerID client_id = StartClient(video_session_id, true);

  // Close will stop the running device, an assert will be triggered in
  // VideoCaptureManager destructor otherwise.
  vcm_->Close(video_session_id);
  StopClient(client_id);

  // Wait to check callbacks before removing the listener
  base::RunLoop().RunUntilIdle();
  vcm_->UnregisterListener(listener_.get());
}

// Try to open, start, pause and resume a device. Confirm the device is
// paused/resumed at the correct times in both single-client and multiple-client
// scenarios.
TEST_F(VideoCaptureManagerTest, PauseAndResumeClient) {
  EXPECT_CALL(*listener_,
              Opened(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _));
  EXPECT_CALL(*frame_observer_, OnStarted(_));

  const base::UnguessableToken video_session_id = vcm_->Open(devices_.front());
  const VideoCaptureControllerID client_id =
      StartClient(video_session_id, true);

  // Test pause/resume when only one client is present.
  EXPECT_CALL(*video_capture_device_factory_, WillSuspendDevice()).Times(1);
  PauseClient(client_id);
  EXPECT_CALL(*video_capture_device_factory_, WillResumeDevice()).Times(2);
  ResumeClient(video_session_id, client_id);

  // Attempting to resume the same client a second time should not cause any
  // calls to VideoCaptureDevice::Resume() because VideoCaptureManager will
  // not attempt to resume the device if the client is not paused.
  ResumeClient(video_session_id, client_id);

  // Add a second client that is never paused, then pause/resume the first
  // client, and no calls to VideoCaptureDevice::MaybeSuspend() is made.
  // However, a call to VideoCaptureDevice::Resume() is still made because
  // VideoCaptureManager will attempt to resume the device if the client is
  // paused.
  EXPECT_CALL(*frame_observer_, OnStarted(_));
  const VideoCaptureControllerID client_id2 =
      StartClient(video_session_id, true);
  PauseClient(client_id);
  ResumeClient(video_session_id, client_id);

  StopClient(client_id);
  StopClient(client_id2);

  EXPECT_CALL(*listener_,
              Closed(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _));
  vcm_->Close(video_session_id);

  // Wait to check callbacks before removing the listener.
  base::RunLoop().RunUntilIdle();
  vcm_->UnregisterListener(listener_.get());
}

#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_DESKTOP_ANDROID)
// Try to open, start, pause and resume a device.
TEST_F(VideoCaptureManagerTest, PauseAndResumeDevice) {
  InSequence s;
  EXPECT_CALL(*listener_,
              Opened(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _));
  EXPECT_CALL(*frame_observer_, OnStarted(_));
  EXPECT_CALL(*listener_,
              Closed(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _));

  base::UnguessableToken video_session_id = vcm_->Open(devices_.front());
  VideoCaptureControllerID client_id = StartClient(video_session_id, true);

  // Release/ResumeDevices according to ApplicationStatus. Should cause no
  // problem in any order. Check https://crbug.com/615557 for more details.
  ApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
  ApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
  ApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
  ApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
  ApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);

  StopClient(client_id);
  vcm_->Close(video_session_id);

  // Wait to check callbacks before removing the listener.
  base::RunLoop().RunUntilIdle();
  vcm_->UnregisterListener(listener_.get());
}
#elif !BUILDFLAG(IS_DESKTOP_ANDROID)
TEST_F(VideoCaptureManagerTest, PauseAndResumeDeviceOnScreenLock) {
  vcm_->set_idle_close_timeout_for_testing(base::TimeDelta());

  InSequence s;

  EXPECT_CALL(*listener_,
              Opened(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _));
  EXPECT_CALL(*frame_observer_, OnStarted(_));
  auto video_session_id = vcm_->Open(devices_.front());
  auto client_id = StartClient(video_session_id, true);
  ASSERT_TRUE(video_capture_device_factory_->has_active_devices());

  // Pretend screen is locked, which should close the device.
  screenlock_monitor_source_->GenerateScreenLockedEvent();
  ASSERT_FALSE(video_capture_device_factory_->has_active_devices());

  // Starting another client while the screen is locked should defer the actual
  // start of the device, but appear open. Since the device is already started,
  // the OnStarted() will appear before Opened().
  EXPECT_CALL(*frame_observer_, OnStarted(_));
  EXPECT_CALL(*listener_,
              Opened(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _));
  auto video_session_id2 = vcm_->Open(devices_.front());
  auto client_id2 = StartClient(video_session_id2, true);
  ASSERT_FALSE(video_capture_device_factory_->has_active_devices());

  // Unlock the screen now.
  EXPECT_CALL(*frame_observer_, OnStarted(_)).Times(2);
  screenlock_monitor_source_->GenerateScreenUnlockedEvent();
  ASSERT_TRUE(video_capture_device_factory_->has_active_devices());

  EXPECT_CALL(*listener_,
              Closed(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
                     video_session_id));
  EXPECT_CALL(*listener_,
              Closed(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
                     video_session_id2));

  StopClient(client_id);
  vcm_->Close(video_session_id);

  StopClient(client_id2);
  vcm_->Close(video_session_id2);

  // Wait to check callbacks before removing the listener.
  base::RunLoop().RunUntilIdle();
  vcm_->UnregisterListener(listener_.get());
}

TEST_F(VideoCaptureManagerTest, ScreenLockDoesNothingBeforeTimeout) {
  vcm_->set_idle_close_timeout_for_testing(base::TimeDelta::Max());
  InSequence s;
  EXPECT_CALL(*listener_,
              Opened(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _));
  EXPECT_CALL(*frame_observer_, OnStarted(_));
  auto video_session_id = vcm_->Open(devices_.front());
  auto client_id = StartClient(video_session_id, true);
  ASSERT_TRUE(video_capture_device_factory_->has_active_devices());

  // Pretend screen is locked, which should do nothing since timeout hasn't
  // elapsed.
  screenlock_monitor_source_->GenerateScreenLockedEvent();
  EXPECT_TRUE(video_capture_device_factory_->has_active_devices());
  EXPECT_TRUE(vcm_->is_idle_close_timer_running_for_testing());

  screenlock_monitor_source_->GenerateScreenUnlockedEvent();
  ASSERT_TRUE(video_capture_device_factory_->has_active_devices());
  EXPECT_FALSE(vcm_->is_idle_close_timer_running_for_testing());

  EXPECT_CALL(*listener_,
              Closed(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
                     video_session_id));

  StopClient(client_id);
  vcm_->Close(video_session_id);

  // Wait to check callbacks before removing the listener.
  base::RunLoop().RunUntilIdle();
  vcm_->UnregisterListener(listener_.get());
}
#endif

// Try to open, start a device capture device, and confirm it's not affected by
// the ScreenLocked event.
TEST_F(VideoCaptureManagerTest, DeviceCaptureDeviceNotClosedOnScreenlock) {
  InSequence s;
  // ScreenLocked event shouldn't affect camera capture device.
  EXPECT_CALL(*listener_,
              Opened(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _));
  EXPECT_CALL(*frame_observer_, OnStarted(_));
  EXPECT_CALL(*listener_,
              Closed(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _))
      .Times(0);

  base::UnguessableToken video_session_id = vcm_->Open(devices_.front());
  VideoCaptureControllerID client_id = StartClient(video_session_id, true);

  // Pretend screen is locked, which should not close the device.
  screenlock_monitor_source_->GenerateScreenLockedEvent();
  Mock::VerifyAndClearExpectations(listener_.get());

  EXPECT_CALL(*listener_,
              Closed(blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE, _));
  StopClient(client_id);
  vcm_->Close(video_session_id);

  // Wait to check callbacks before removing the listener.
  base::RunLoop().RunUntilIdle();
  vcm_->UnregisterListener(listener_.get());
}

#if BUILDFLAG(ENABLE_SCREEN_CAPTURE) && !BUILDFLAG(IS_ANDROID)
// Try to open, start a desktop capture device, and confirm it's closed on
// ScreenLocked event on desktop platforms.
TEST_F(VideoCaptureManagerTest, DesktopCaptureDeviceClosedOnScreenlock) {
  InSequence s;
  // ScreenLocked event should stop desktop capture device.
  EXPECT_CALL(*listener_,
              Opened(blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE, _));
  EXPECT_CALL(*frame_observer_, OnStarted(_));
  EXPECT_CALL(*listener_,
              Closed(blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE, _));

  // Simulate we add 1 fake display media device.
  video_capture_device_factory_->SetToDefaultDevicesConfig(1);
  base::RunLoop run_loop;
  vcm_->EnumerateDevices(base::BindOnce(
      &VideoCaptureManagerTest::HandleEnumerationResultAsDisplayMediaDevices,
      base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  ASSERT_EQ(devices_.size(), 1u);

  base::UnguessableToken video_session_id = vcm_->Open(devices_.front());
  VideoCaptureControllerID client_id = StartClient(video_session_id, true);

  // Pretend screen is locked, which should close the device.
  screenlock_monitor_source_->GenerateScreenLockedEvent();
  Mock::VerifyAndClearExpectations(listener_.get());

  StopClient(client_id);

  // Wait to check callbacks before removing the listener.
  base::RunLoop().RunUntilIdle();
  vcm_->UnregisterListener(listener_.get());
}
#endif  // ENABLE_SCREEN_CAPTURE && !BUILDFLAG(IS_ANDROID)

// TODO(mcasas): Add a test to check consolidation of the supported formats
// provided by the device when http://crbug.com/323913 is closed.

}  // namespace content
