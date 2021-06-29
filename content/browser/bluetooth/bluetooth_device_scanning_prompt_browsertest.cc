// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_device_scanning_prompt_controller.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "content/browser/bluetooth/bluetooth_adapter_factory_wrapper.h"
#include "content/public/browser/bluetooth_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "content/shell/browser/shell.h"
#include "content/test/test_web_contents.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"
#include "url/gurl.h"

using testing::_;
using testing::Mock;
using testing::Return;

namespace content {

namespace {
using PromptEventCallback =
    base::OnceCallback<void(BluetoothScanningPrompt::Event)>;

class FakeBluetoothScanningPrompt : public BluetoothScanningPrompt {
 public:
  explicit FakeBluetoothScanningPrompt(
      PromptEventCallback prompt_event_callback)
      : prompt_event_callback_(std::move(prompt_event_callback)) {}
  ~FakeBluetoothScanningPrompt() override = default;

  FakeBluetoothScanningPrompt(const FakeBluetoothScanningPrompt&) = delete;
  FakeBluetoothScanningPrompt& operator=(const FakeBluetoothScanningPrompt&) =
      delete;

  void RunPromptEventCallback(Event event) {
    ASSERT_TRUE(prompt_event_callback_);
    std::move(prompt_event_callback_).Run(event);
  }

 private:
  PromptEventCallback prompt_event_callback_;
};

class FakeBluetoothAdapter : public device::MockBluetoothAdapter {
 public:
  FakeBluetoothAdapter() = default;

  FakeBluetoothAdapter(const FakeBluetoothAdapter&) = delete;
  FakeBluetoothAdapter& operator=(const FakeBluetoothAdapter&) = delete;

  // device::BluetoothAdapter:
  void StartScanWithFilter(
      std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
      DiscoverySessionResultCallback callback) override {
    std::move(callback).Run(
        /*is_error=*/false,
        device::UMABluetoothDiscoverySessionOutcome::SUCCESS);
  }
  void StopScan(DiscoverySessionResultCallback callback) override {
    std::move(callback).Run(
        /*is_error=*/true,
        device::UMABluetoothDiscoverySessionOutcome::UNKNOWN);
  }

 private:
  ~FakeBluetoothAdapter() override = default;
};

class TestBluetoothDelegate : public BluetoothDelegate {
 public:
  TestBluetoothDelegate() = default;
  ~TestBluetoothDelegate() override = default;
  TestBluetoothDelegate(const TestBluetoothDelegate&) = delete;
  TestBluetoothDelegate& operator=(const TestBluetoothDelegate&) = delete;

  // BluetoothDelegate:
  std::unique_ptr<BluetoothChooser> RunBluetoothChooser(
      RenderFrameHost* frame,
      const BluetoothChooser::EventHandler& event_handler) override {
    return nullptr;
  }
  std::unique_ptr<BluetoothScanningPrompt> ShowBluetoothScanningPrompt(
      RenderFrameHost* frame,
      const BluetoothScanningPrompt::EventHandler& event_handler) override {
    showed_bluetooth_scanning_prompt_ = true;
    DCHECK_EQ(frame->GetLifecycleState(),
              RenderFrameHost::LifecycleState::kActive);
    if (quit_on_scanning_prompt_)
      std::move(quit_on_scanning_prompt_).Run();
    auto prompt =
        std::make_unique<FakeBluetoothScanningPrompt>(std::move(event_handler));
    prompt_ = prompt.get();
    return std::move(prompt);
  }
  blink::WebBluetoothDeviceId GetWebBluetoothDeviceId(
      RenderFrameHost* frame,
      const std::string& device_address) override {
    return blink::WebBluetoothDeviceId();
  }
  std::string GetDeviceAddress(RenderFrameHost* frame,
                               const blink::WebBluetoothDeviceId&) override {
    return std::string();
  }
  blink::WebBluetoothDeviceId AddScannedDevice(
      RenderFrameHost* frame,
      const std::string& device_address) override {
    return blink::WebBluetoothDeviceId();
  }
  blink::WebBluetoothDeviceId GrantServiceAccessPermission(
      RenderFrameHost* frame,
      const device::BluetoothDevice* device,
      const blink::mojom::WebBluetoothRequestDeviceOptions* options) override {
    return blink::WebBluetoothDeviceId();
  }
  bool HasDevicePermission(
      RenderFrameHost* frame,
      const blink::WebBluetoothDeviceId& device_id) override {
    return false;
  }
  bool IsAllowedToAccessService(RenderFrameHost* frame,
                                const blink::WebBluetoothDeviceId& device_id,
                                const device::BluetoothUUID& service) override {
    return false;
  }
  bool IsAllowedToAccessAtLeastOneService(
      RenderFrameHost* frame,
      const blink::WebBluetoothDeviceId& device_id) override {
    return false;
  }
  bool IsAllowedToAccessManufacturerData(
      RenderFrameHost* frame,
      const blink::WebBluetoothDeviceId& device_id,
      const uint16_t manufacturer_code) override {
    return false;
  }
  std::vector<blink::mojom::WebBluetoothDevicePtr> GetPermittedDevices(
      RenderFrameHost* frame) override {
    return {};
  }

  void AddFramePermissionObserver(FramePermissionObserver* observer) override {}
  void RemoveFramePermissionObserver(
      FramePermissionObserver* observer) override {}

  void WaitForShowBluetoothScanningPrompt() {
    if (showed_bluetooth_scanning_prompt_)
      return;
    base::RunLoop run_loop;
    quit_on_scanning_prompt_ = run_loop.QuitClosure();
    run_loop.Run();
  }
  void RunBluetoothScanningPromptEventCallback(
      BluetoothScanningPrompt::Event event) {
    ASSERT_TRUE(prompt_);
    prompt_->RunPromptEventCallback(event);
  }

  bool showed_bluetooth_scanning_prompt() {
    return showed_bluetooth_scanning_prompt_;
  }

  void reset_showed_bluetooth_scanning_prompt() {
    showed_bluetooth_scanning_prompt_ = false;
  }

 private:
  FakeBluetoothScanningPrompt* prompt_ = nullptr;
  base::OnceClosure quit_on_scanning_prompt_;
  bool showed_bluetooth_scanning_prompt_ = false;
};

class TestContentBrowserClient : public ContentBrowserClient {
 public:
  TestContentBrowserClient() = default;
  ~TestContentBrowserClient() override = default;
  TestContentBrowserClient(const TestContentBrowserClient&) = delete;
  TestContentBrowserClient& operator=(const TestContentBrowserClient&) = delete;

  TestBluetoothDelegate* bluetooth_delegate() { return &bluetooth_delegate_; }

 protected:
  // ChromeContentBrowserClient:
  BluetoothDelegate* GetBluetoothDelegate() override {
    return &bluetooth_delegate_;
  }

 private:
  TestBluetoothDelegate bluetooth_delegate_;
};

}  // namespace

class BluetoothDeviceChooserControllerBrowserTest : public ContentBrowserTest {
 public:
  BluetoothDeviceChooserControllerBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &BluetoothDeviceChooserControllerBrowserTest::GetWebContents,
            base::Unretained(this))) {}
  ~BluetoothDeviceChooserControllerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    prerender_helper_.SetUpOnMainThread(embedded_test_server());
    ASSERT_TRUE(test_server_handle_ =
                    embedded_test_server()->StartAndReturnHandle());

    // Hook up the test bluetooth delegate.
    old_browser_client_ = SetBrowserClientForTesting(&browser_client_);
    SetFakeBlueboothAdapter();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sets up the blink runtime feature for accessing to navigator.bluetooth.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetFakeBlueboothAdapter() {
    adapter_ = new FakeBluetoothAdapter();
    EXPECT_CALL(*adapter_, IsPresent()).WillRepeatedly(Return(true));
    BluetoothAdapterFactoryWrapper::Get().SetBluetoothAdapterForTesting(
        adapter_);
  }

  WebContents* GetWebContents() { return shell()->web_contents(); }
  TestBluetoothDelegate* GetBluetoothDelegate() {
    return browser_client_.bluetooth_delegate();
  }

  test::PrerenderTestHelper* prerender_helper() { return &prerender_helper_; }
  FakeBluetoothAdapter* adapter() { return adapter_.get(); }

 private:
  test::PrerenderTestHelper prerender_helper_;
  net::test_server::EmbeddedTestServerHandle test_server_handle_;
  scoped_refptr<FakeBluetoothAdapter> adapter_;
  TestContentBrowserClient browser_client_;
  ContentBrowserClient* old_browser_client_ = nullptr;
};

// Tests that the scanning prompt is not shown in the prerendering.
IN_PROC_BROWSER_TEST_F(BluetoothDeviceChooserControllerBrowserTest,
                       NoShowBluetoothScanningPromptInPrerendering) {
  GURL url = embedded_test_server()->GetURL("/hello.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_CALL(*adapter(), AddObserver(_));
  ASSERT_TRUE(content::ExecJs(GetWebContents()->GetMainFrame(), R"(
      var requestLEScanPromise = navigator.bluetooth.requestLEScan({
        acceptAllAdvertisements: true});
  )"));
  // Waits for ShowBluetoothScanningPrompt().
  GetBluetoothDelegate()->WaitForShowBluetoothScanningPrompt();
  // It should show the scanning prompt.
  EXPECT_TRUE(GetBluetoothDelegate()->showed_bluetooth_scanning_prompt());
  GetBluetoothDelegate()->reset_showed_bluetooth_scanning_prompt();

  // Posts a task to simulate a prompt event during a call to
  // RequestScanningStart().
  GetBluetoothDelegate()->RunBluetoothScanningPromptEventCallback(
      BluetoothScanningPrompt::Event::kAllow);

  // Loads a page in the prerender.
  auto prerender_url = embedded_test_server()->GetURL("/empty.html");
  // The prerendering doesn't affect the current scanning.
  int host_id = prerender_helper()->AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     host_id);
  RenderFrameHost* prerendered_frame_host =
      prerender_helper()->GetPrerenderedMainFrameHost(host_id);

  // Runs JS asynchronously since Mojo calls is deferred on the prerendering.
  prerendered_frame_host->ExecuteJavaScriptWithUserGestureForTests(
      u"var requestLEScanPromise = navigator.bluetooth.requestLEScan({"
      u"acceptAllAdvertisements: true});");
  // The prerendering doesn't show the bluetoothscanning prompt.
  EXPECT_FALSE(GetBluetoothDelegate()->showed_bluetooth_scanning_prompt());

  // Loading a new primary page removes observer and stops scanning.
  EXPECT_CALL(*adapter(), RemoveObserver(_));

  // Navigates the primary page to the URL.
  prerender_helper()->NavigatePrimaryPage(prerender_url);
  // The page should be activated from the prerendering.
  EXPECT_TRUE(host_observer.was_activated());
  // Sets BlueboothAdapter for the new primary page since the previous
  // adapter is released by BluetoothAdapterFactoryWrapper::ReleaseAdapter().
  BluetoothAdapterFactoryWrapper::Get().SetBluetoothAdapterForTesting(
      adapter());

  EXPECT_CALL(*adapter(), AddObserver(_));
  // Waits for ShowBluetoothScanningPrompt() since the page is activated.
  GetBluetoothDelegate()->WaitForShowBluetoothScanningPrompt();
  // It should show the scanning prompt.
  EXPECT_TRUE(GetBluetoothDelegate()->showed_bluetooth_scanning_prompt());
  // Post a task to simulate a prompt event during a call to
  // RequestScanningStart().
  GetBluetoothDelegate()->RunBluetoothScanningPromptEventCallback(
      BluetoothScanningPrompt::Event::kAllow);
  EXPECT_CALL(*adapter(), RemoveObserver(_));
}

}  // namespace content
