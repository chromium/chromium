// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/video_capture_service.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/video_capture/public/cpp/mock_producer.h"
#include "services/video_capture/public/mojom/devices_changed_observer.mojom.h"
#include "services/video_capture/public/mojom/producer.mojom.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"
#include "services/video_capture/public/mojom/virtual_device.mojom.h"

namespace content {

namespace {

enum class VirtualDeviceType { kSharedMemory, kTexture };

struct TestParams {
  VirtualDeviceType device_type;
};

static const char kVideoCaptureHtmlFile[] = "/media/video_capture_test.html";
static const char kEnumerateVideoCaptureDevicesAndVerify[] =
    "enumerateVideoCaptureDevicesAndVerifyCount(%d)";
static const char kRegisterForDeviceChangeEvent[] =
    "registerForDeviceChangeEvent()";
static const char kWaitForDeviceChangeEvent[] = "waitForDeviceChangeEvent()";
static const char kResetHasReceivedChangedEventFlag[] =
    "resetHasReceivedChangedEventFlag()";

}  // anonymous namespace

// Integration test that obtains a connection to the video capture service via
// the Browser process' service manager. It then registers and unregisters
// virtual devices at the service and checks in JavaScript that the list of
// enumerated devices changes correspondingly.
class WebRtcVideoCaptureServiceEnumerationBrowserTest
    : public ContentBrowserTest,
      public testing::WithParamInterface<TestParams>,
      public video_capture::mojom::DevicesChangedObserver {
 public:
  WebRtcVideoCaptureServiceEnumerationBrowserTest() = default;

  WebRtcVideoCaptureServiceEnumerationBrowserTest(
      const WebRtcVideoCaptureServiceEnumerationBrowserTest&) = delete;
  WebRtcVideoCaptureServiceEnumerationBrowserTest& operator=(
      const WebRtcVideoCaptureServiceEnumerationBrowserTest&) = delete;

  ~WebRtcVideoCaptureServiceEnumerationBrowserTest() override {}

  void ConnectToService() {
    mojo::PendingRemote<video_capture::mojom::DevicesChangedObserver> observer;
    devices_changed_observer_receiver_.Bind(
        observer.InitWithNewPipeAndPassReceiver());
    GetVideoCaptureService().ConnectToVideoSourceProvider(
        video_source_provider_.BindNewPipeAndPassReceiver());
    video_source_provider_->RegisterVirtualDevicesChangedObserver(
        std::move(observer),
        false /*raise_event_if_virtual_devices_already_present*/);
  }

  void AddVirtualDevice(const std::string& device_id) {
    media::VideoCaptureDeviceInfo info;
    info.descriptor.device_id = device_id;
    info.descriptor.set_display_name(device_id);
    info.descriptor.capture_api = media::VideoCaptureApi::VIRTUAL_DEVICE;

    base::RunLoop wait_loop;
    closure_to_be_called_on_devices_changed_ = wait_loop.QuitClosure();
    switch (GetParam().device_type) {
      case VirtualDeviceType::kSharedMemory: {
        mojo::PendingRemote<video_capture::mojom::SharedMemoryVirtualDevice>
            virtual_device;
        mojo::PendingRemote<video_capture::mojom::Producer> producer;
        auto mock_producer = std::make_unique<video_capture::MockProducer>(
            producer.InitWithNewPipeAndPassReceiver());
        video_source_provider_->AddSharedMemoryVirtualDevice(
            info, std::move(producer),
            virtual_device.InitWithNewPipeAndPassReceiver());
        shared_memory_devices_by_id_.insert(std::make_pair(
            device_id, std::make_pair(std::move(virtual_device),
                                      std::move(mock_producer))));
        break;
      }
      case VirtualDeviceType::kTexture: {
        mojo::PendingRemote<video_capture::mojom::TextureVirtualDevice>
            virtual_device;
        video_source_provider_->AddTextureVirtualDevice(
            info, virtual_device.InitWithNewPipeAndPassReceiver());
        texture_devices_by_id_.insert(
            std::make_pair(device_id, std::move(virtual_device)));
        break;
      }
    }
    // Wait for confirmation from the service.
    wait_loop.Run();
  }

  void RemoveVirtualDevice(const std::string& device_id) {
    base::RunLoop wait_loop;
    closure_to_be_called_on_devices_changed_ = wait_loop.QuitClosure();
    switch (GetParam().device_type) {
      case VirtualDeviceType::kSharedMemory:
        shared_memory_devices_by_id_.erase(device_id);
        break;
      case VirtualDeviceType::kTexture:
        texture_devices_by_id_.erase(device_id);
        break;
    }

    // Wait for confirmation from the service.
    wait_loop.Run();
  }

  void DisconnectFromService() { video_source_provider_.reset(); }

  void EnumerateDevicesInRendererAndVerifyDeviceCount(
      int expected_device_count) {
    const std::string javascript_to_execute = base::StringPrintf(
        kEnumerateVideoCaptureDevicesAndVerify, expected_device_count);
    ASSERT_TRUE(ExecJs(shell(), javascript_to_execute));
  }

  void RegisterForDeviceChangeEventInRenderer() {
    ASSERT_TRUE(ExecJs(shell(), kRegisterForDeviceChangeEvent));
  }

  void WaitForDeviceChangeEventInRenderer() {
    ASSERT_TRUE(ExecJs(shell(), kWaitForDeviceChangeEvent));
  }

  void ResetHasReceivedChangedEventFlag() {
    ASSERT_TRUE(ExecJs(shell(), kResetHasReceivedChangedEventFlag));
  }

  // Implementation of video_capture::mojom::DevicesChangedObserver:
  void OnDevicesChanged() override {
    if (closure_to_be_called_on_devices_changed_) {
      std::move(closure_to_be_called_on_devices_changed_).Run();
    }
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Note: We are not planning to actually use any fake device, but we want
    // to avoid enumerating or otherwise calling into real capture devices.
    command_line->RemoveSwitch(switches::kUseFakeDeviceForMediaStream);
    command_line->AppendSwitchASCII(switches::kUseFakeDeviceForMediaStream,
                                    "device-count=0");
    command_line->AppendSwitch(switches::kUseFakeUIForMediaStream);
  }

  void Initialize() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->StartAcceptingConnections();

    EXPECT_TRUE(NavigateToURL(
        shell(), GURL(embedded_test_server()->GetURL(kVideoCaptureHtmlFile))));
  }

  std::map<std::string,
           mojo::PendingRemote<video_capture::mojom::TextureVirtualDevice>>
      texture_devices_by_id_;
  std::map<std::string,
           std::pair<mojo::PendingRemote<
                         video_capture::mojom::SharedMemoryVirtualDevice>,
                     std::unique_ptr<video_capture::MockProducer>>>
      shared_memory_devices_by_id_;

 private:
  mojo::Receiver<video_capture::mojom::DevicesChangedObserver>
      devices_changed_observer_receiver_{this};
  mojo::Remote<video_capture::mojom::VideoSourceProvider>
      video_source_provider_;
  base::OnceClosure closure_to_be_called_on_devices_changed_;
};

IN_PROC_BROWSER_TEST_P(WebRtcVideoCaptureServiceEnumerationBrowserTest,
                       SingleAddedVirtualDeviceGetsEnumerated) {
  Initialize();
  ConnectToService();

  // Exercise
  AddVirtualDevice("test");
  EnumerateDevicesInRendererAndVerifyDeviceCount(1);

  // Tear down
  RemoveVirtualDevice("test");
  DisconnectFromService();
}

// TODO: crbug.com/352672009 - Fix the flakiness on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_RemoveVirtualDeviceAfterItHasBeenEnumerated \
  DISABLED_RemoveVirtualDeviceAfterItHasBeenEnumerated
#else
#define MAYBE_RemoveVirtualDeviceAfterItHasBeenEnumerated \
  RemoveVirtualDeviceAfterItHasBeenEnumerated
#endif
IN_PROC_BROWSER_TEST_P(WebRtcVideoCaptureServiceEnumerationBrowserTest,
                       MAYBE_RemoveVirtualDeviceAfterItHasBeenEnumerated) {
  Initialize();
  ConnectToService();

  AddVirtualDevice("test_1");
  AddVirtualDevice("test_2");
  EnumerateDevicesInRendererAndVerifyDeviceCount(2);
  RemoveVirtualDevice("test_1");
  EnumerateDevicesInRendererAndVerifyDeviceCount(1);
  RemoveVirtualDevice("test_2");
  EnumerateDevicesInRendererAndVerifyDeviceCount(0);

  // Tear down
  DisconnectFromService();
}

// The mediadevices.ondevicechange event is currently not supported on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_AddingAndRemovingVirtualDeviceTriggersMediaElementOnDeviceChange \
  DISABLED_AddingAndRemovingVirtualDeviceTriggersMediaElementOnDeviceChange
#else
#define MAYBE_AddingAndRemovingVirtualDeviceTriggersMediaElementOnDeviceChange \
  AddingAndRemovingVirtualDeviceTriggersMediaElementOnDeviceChange
#endif

IN_PROC_BROWSER_TEST_P(
    WebRtcVideoCaptureServiceEnumerationBrowserTest,
    MAYBE_AddingAndRemovingVirtualDeviceTriggersMediaElementOnDeviceChange) {
  Initialize();
  ConnectToService();
  RegisterForDeviceChangeEventInRenderer();
  // Waiting for enumeration ensures that the browser everything is primed for
  // device change events.
  EnumerateDevicesInRendererAndVerifyDeviceCount(0);

  // Exercise
  AddVirtualDevice("test");

  WaitForDeviceChangeEventInRenderer();
  ResetHasReceivedChangedEventFlag();

  RemoveVirtualDevice("test");
  WaitForDeviceChangeEventInRenderer();

  // Tear down
  DisconnectFromService();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebRtcVideoCaptureServiceEnumerationBrowserTest,
    ::testing::Values(TestParams{VirtualDeviceType::kSharedMemory},
                      TestParams{VirtualDeviceType::kTexture}));

}  // namespace content
