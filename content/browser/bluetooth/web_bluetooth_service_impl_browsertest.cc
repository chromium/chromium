// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/web_bluetooth_service_impl.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "content/browser/bluetooth/bluetooth_adapter_factory_wrapper.h"
#include "content/browser/bluetooth/web_bluetooth_service_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/bluetooth_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/test_web_contents.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_service.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"
#include "url/gurl.h"

using testing::_;
using testing::Mock;
using testing::Return;

namespace content {

namespace {
constexpr char kDeviceAddress[] = "00:00:00:00:00:00";
constexpr char kHeartRateUUIDString[] = "0000180d-0000-1000-8000-00805f9b34fb";

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
  device::BluetoothAdapter::ConstDeviceList GetDevices() const override {
    device::BluetoothAdapter::ConstDeviceList devices;
    for (const auto& it : mock_devices_)
      devices.push_back(it.get());
    return devices;
  }

  device::BluetoothDevice* GetDevice(const std::string& address) override {
    device::MockBluetoothAdapter::GetDevice(address);
    for (const auto& it : mock_devices_) {
      if (it->GetAddress() == address)
        return it.get();
    }
    return nullptr;
  }

  void StartScanWithFilter(
      std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
      DiscoverySessionResultCallback callback) override {
    std::move(callback).Run(
        /*is_error=*/false,
        device::UMABluetoothDiscoverySessionOutcome::SUCCESS);
  }
  void StopScan(DiscoverySessionResultCallback callback) override {
    std::move(callback).Run(
        /*is_error=*/false,
        device::UMABluetoothDiscoverySessionOutcome::SUCCESS);
  }

 private:
  ~FakeBluetoothAdapter() override = default;
};

class FakeBluetoothChooser : public content::BluetoothChooser {
 public:
  FakeBluetoothChooser(content::BluetoothChooser::EventHandler event_handler,
                       const std::string& device_to_select)
      : event_handler_(event_handler), device_to_select_(device_to_select) {}
  FakeBluetoothChooser(const FakeBluetoothChooser&) = delete;
  FakeBluetoothChooser& operator=(const FakeBluetoothChooser&) = delete;
  ~FakeBluetoothChooser() override = default;

  // content::BluetoothChooser implementation:
  void AddOrUpdateDevice(const std::string& device_id,
                         bool should_update_name,
                         const std::u16string& device_name,
                         bool is_gatt_connected,
                         bool is_paired,
                         int signal_strength_level) override {
    // Select the added device if its device ID matches |device_to_select_|.
    if (device_to_select_ == device_id)
      event_handler_.Run(content::BluetoothChooserEvent::SELECTED, device_id);
  }

 private:
  content::BluetoothChooser::EventHandler event_handler_;
  std::string device_to_select_;
};

class TestBluetoothDelegate : public BluetoothDelegate {
 public:
  TestBluetoothDelegate() = default;
  ~TestBluetoothDelegate() override = default;
  TestBluetoothDelegate(const TestBluetoothDelegate&) = delete;
  TestBluetoothDelegate& operator=(const TestBluetoothDelegate&) = delete;

  void SetDeviceToSelect(const std::string& device_address) {
    device_to_select_ = device_address;
  }

  // BluetoothDelegate:
  std::unique_ptr<BluetoothChooser> RunBluetoothChooser(
      RenderFrameHost* frame,
      const BluetoothChooser::EventHandler& event_handler) override {
    return std::make_unique<FakeBluetoothChooser>(event_handler,
                                                  device_to_select_);
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

  void ShowDevicePairPrompt(content::RenderFrameHost* frame,
                            const std::u16string& device_identifier,
                            PairPromptCallback callback,
                            PairingKind pairing_kind,
                            const std::optional<std::u16string>& pin) override {
    NOTREACHED_IN_MIGRATION();
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
  void RevokeDevicePermissionWebInitiated(
      RenderFrameHost* frame,
      const blink::WebBluetoothDeviceId& device_id) override {}
  bool IsAllowedToAccessService(RenderFrameHost* frame,
                                const blink::WebBluetoothDeviceId& device_id,
                                const device::BluetoothUUID& service) override {
    return false;
  }
  bool MayUseBluetooth(RenderFrameHost* rfh) override { return true; }
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
  std::string device_to_select_;
  raw_ptr<FakeBluetoothScanningPrompt, DanglingUntriaged> prompt_ = nullptr;
  base::OnceClosure quit_on_scanning_prompt_;
  bool showed_bluetooth_scanning_prompt_ = false;
};

class TestContentBrowserClient : public ContentBrowserTestContentBrowserClient {
 public:
  TestContentBrowserClient() = default;
  ~TestContentBrowserClient() override = default;
  TestContentBrowserClient(const TestContentBrowserClient&) = delete;
  TestContentBrowserClient& operator=(const TestContentBrowserClient&) = delete;

  TestBluetoothDelegate* bluetooth_delegate() { return &bluetooth_delegate_; }

  AllowWebBluetoothResult AllowWebBluetooth(
      content::BrowserContext* browser_context,
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin) override {
    checked_allow_web_bluetooth_ = true;

    if (block_globally_disabled_)
      return AllowWebBluetoothResult::BLOCK_GLOBALLY_DISABLED;

    return ContentBrowserClient::AllowWebBluetooth(
        browser_context, requesting_origin, embedding_origin);
  }

  void block_globally_disabled() { block_globally_disabled_ = true; }

  bool checked_allow_web_bluetooth() { return checked_allow_web_bluetooth_; }

 protected:
  // ChromeContentBrowserClient:
  BluetoothDelegate* GetBluetoothDelegate() override {
    return &bluetooth_delegate_;
  }

 private:
  TestBluetoothDelegate bluetooth_delegate_;
  bool checked_allow_web_bluetooth_ = false;
  bool block_globally_disabled_ = false;
};

}  // namespace

class WebBluetoothServiceImplBrowserTest : public ContentBrowserTest {
 public:
  WebBluetoothServiceImplBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &WebBluetoothServiceImplBrowserTest::GetWebContents,
            base::Unretained(this))) {}
  ~WebBluetoothServiceImplBrowserTest() override = default;

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(test_server_handle_ =
                    embedded_test_server()->StartAndReturnHandle());

    // Hook up the test bluetooth delegate.
    browser_client_ = std::make_unique<TestContentBrowserClient>();
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
    BluetoothAdapterFactoryWrapper::Get().SetBluetoothAdapterOverride(adapter_);
  }

  void AddFakeDevice(const std::string& device_address) {
    const device::BluetoothUUID kHeartRateUUID(kHeartRateUUIDString);
    auto fake_device =
        std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
            adapter_.get(), /*bluetooth_class=*/0u,
            /*name=*/"Test Device", device_address,
            /*paired=*/true,
            /*connected=*/true);
    fake_device->AddUUID(kHeartRateUUID);
    fake_device->AddMockService(
        std::make_unique<testing::NiceMock<device::MockBluetoothGattService>>(
            fake_device.get(), kHeartRateUUIDString, kHeartRateUUID,
            /*is_primary=*/true));
    adapter_->AddMockDevice(std::move(fake_device));
  }

  void SetDeviceToSelect(const std::string& device_address) {
    browser_client_->bluetooth_delegate()->SetDeviceToSelect(device_address);
  }

  bool CheckedAllowWebBluetooth() {
    return browser_client_->checked_allow_web_bluetooth();
  }

  void BlockGloballyDisabled() { browser_client_->block_globally_disabled(); }

  WebBluetoothServiceImpl* GetWebBluetoothServiceOverride(
      RenderFrameHost* render_frame_host) {
    return WebBluetoothServiceImpl::GetForCurrentDocument(render_frame_host);
  }

  WebContents* GetWebContents() { return shell()->web_contents(); }
  TestBluetoothDelegate* GetBluetoothDelegate() {
    return browser_client_->bluetooth_delegate();
  }

  test::PrerenderTestHelper* prerender_helper() { return &prerender_helper_; }
  FakeBluetoothAdapter* adapter() { return adapter_.get(); }

 private:
  test::PrerenderTestHelper prerender_helper_;
  net::test_server::EmbeddedTestServerHandle test_server_handle_;
  scoped_refptr<FakeBluetoothAdapter> adapter_;
  std::unique_ptr<TestContentBrowserClient> browser_client_;
};

// Tests that the scanning prompt is not shown in the prerendering. It also
// ensures that ScanningClient is not created in the prerendering.
IN_PROC_BROWSER_TEST_F(WebBluetoothServiceImplBrowserTest,
                       NoShowBluetoothScanningPromptInPrerendering) {
  GURL url = embedded_test_server()->GetURL("/hello.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_CALL(*adapter(), AddObserver(_));
  ASSERT_TRUE(content::ExecJs(GetWebContents()->GetPrimaryMainFrame(), R"(
      var requestLEScanPromise = navigator.bluetooth.requestLEScan({
        acceptAllAdvertisements: true});
  )"));
  // Waits for ShowBluetoothScanningPrompt().
  GetBluetoothDelegate()->WaitForShowBluetoothScanningPrompt();
  // It should show the scanning prompt.
  EXPECT_TRUE(GetBluetoothDelegate()->showed_bluetooth_scanning_prompt());

  WebBluetoothServiceImpl* service_for_main_frame =
      GetWebBluetoothServiceOverride(GetWebContents()->GetPrimaryMainFrame());
  // ScanningClient with the main frame is created.
  EXPECT_EQ(service_for_main_frame->scanning_clients_.size(), 1u);

  GetBluetoothDelegate()->reset_showed_bluetooth_scanning_prompt();

  // Posts a task to simulate a prompt event during a call to
  // RequestScanningStart().
  GetBluetoothDelegate()->RunBluetoothScanningPromptEventCallback(
      BluetoothScanningPrompt::Event::kAllow);

  // Loads a page in the prerender.
  auto prerender_url = embedded_test_server()->GetURL("/empty.html");
  // The prerendering doesn't affect the current scanning.
  FrameTreeNodeId host_id = prerender_helper()->AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     host_id);
  RenderFrameHost* prerendered_frame_host =
      prerender_helper()->GetPrerenderedMainFrameHost(host_id);

  // A SecurityError is thrown when there is no user gesture.
  constexpr char kUserGestureError[] =
      "Must be handling a user gesture to show a permission request.";
  auto result = EvalJs(prerendered_frame_host, R"(
      navigator.bluetooth.requestLEScan({acceptAllAdvertisements: true});)",
                       content::EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE);
  EXPECT_THAT(result.error, ::testing::HasSubstr(kUserGestureError));

  // The prerendering doesn't show the bluetoothscanning prompt.
  EXPECT_FALSE(GetBluetoothDelegate()->showed_bluetooth_scanning_prompt());
  // ScanningClient is not created in the prerendering.
  EXPECT_EQ(service_for_main_frame->scanning_clients_.size(), 1u);

  // Loading a new primary page removes observer and stops scanning.
  EXPECT_CALL(*adapter(), RemoveObserver(_));

  RenderFrameDeletedObserver rfh_observer(
      GetWebContents()->GetPrimaryMainFrame());

  // Navigates the primary page to the URL.
  prerender_helper()->NavigatePrimaryPage(prerender_url);
  // The page should be activated from the prerendering.
  EXPECT_TRUE(host_observer.was_activated());

  // Wait until the previous RFH to be disposed of, so a new bluetooth adapter
  // can be set after that.
  rfh_observer.WaitUntilDeleted();

  // Sets BluetoothAdapter for the new primary page since the previous
  // adapter is released by BluetoothAdapterFactoryWrapper::ReleaseAdapter().
  BluetoothAdapterFactoryWrapper::Get().SetBluetoothAdapterOverride(adapter());

  EXPECT_CALL(*adapter(), AddObserver(_));

  // Scanning after the prerendering activation to ensure it shows the prompt on
  // the activated page.
  EXPECT_TRUE(ExecJs(GetWebContents()->GetPrimaryMainFrame(), R"(
      var requestLEScanPromise = navigator.bluetooth.requestLEScan({
        acceptAllAdvertisements: true});)"));
  // Waits for ShowBluetoothScanningPrompt() since the page is activated.
  GetBluetoothDelegate()->WaitForShowBluetoothScanningPrompt();
  // It should show the scanning prompt.
  EXPECT_TRUE(GetBluetoothDelegate()->showed_bluetooth_scanning_prompt());

  WebBluetoothServiceImpl* service_for_activated_frame =
      GetWebBluetoothServiceOverride(GetWebContents()->GetPrimaryMainFrame());
  // ScanningClient is created after the prerendering activation.
  EXPECT_EQ(service_for_activated_frame->scanning_clients_.size(), 1u);

  // Post a task to simulate a prompt event during a call to
  // RequestScanningStart().
  GetBluetoothDelegate()->RunBluetoothScanningPromptEventCallback(
      BluetoothScanningPrompt::Event::kAllow);
  EXPECT_CALL(*adapter(), RemoveObserver(_));
}

// Tests that navigator.bluetooth.requestDevice() has an error without a user
// gesture in the prerendering and works in the prerendering activation.
IN_PROC_BROWSER_TEST_F(WebBluetoothServiceImplBrowserTest,
                       RequestDeviceInPrerendering) {
  GURL url = embedded_test_server()->GetURL("/hello.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Setup the fake device.
  AddFakeDevice(kDeviceAddress);
  SetDeviceToSelect(kDeviceAddress);

  EXPECT_CALL(*adapter(), AddObserver(_));
  EXPECT_CALL(*adapter(), GetDevice(kDeviceAddress));

  EXPECT_EQ("", content::EvalJs(GetWebContents(), R"(
    (async() => {
      try {
        let device = await navigator.bluetooth.requestDevice({
          filters: [{name: 'Test Device', services: ['heart_rate']}]});
        return "";
      } catch(e) {
        return `${e.name}: ${e.message}`;
      }
    })()
  )"));

  // WebBluetoothService is created for the main frame.
  EXPECT_NE(
      GetWebBluetoothServiceOverride(GetWebContents()->GetPrimaryMainFrame()),
      nullptr);

  // Loads a page in the prerender.
  auto prerender_url = embedded_test_server()->GetURL("/empty.html");
  FrameTreeNodeId host_id = prerender_helper()->AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     host_id);
  content::RenderFrameHost* prerendered_frame_host =
      prerender_helper()->GetPrerenderedMainFrameHost(host_id);

  // A SecurityError is thrown when there is no user gesture.
  constexpr char kUserGestureError[] =
      "Must be handling a user gesture to show a permission request.";
  auto result =
      content::EvalJs(prerendered_frame_host, R"(
      navigator.bluetooth.requestDevice({
          filters: [{name: 'Test Device', services: ['heart_rate']}]}))",
                      content::EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE);
  EXPECT_THAT(result.error, ::testing::HasSubstr(kUserGestureError));

  // WebBluetoothService is not created for `prerendered_frame_host`.
  EXPECT_EQ(GetWebBluetoothServiceOverride(prerendered_frame_host), nullptr);

  // Loading a new primary page removes observer.
  EXPECT_CALL(*adapter(), RemoveObserver(_));

  RenderFrameDeletedObserver rfh_observer(
      GetWebContents()->GetPrimaryMainFrame());

  // Navigate to the prerendered page.
  prerender_helper()->NavigatePrimaryPage(prerender_url);
  // The page should be activated from the prerendering.
  EXPECT_TRUE(host_observer.was_activated());

  // Wait until the previous RFH to be disposed of, so a new bluetooth adapter
  // can be set after that.
  rfh_observer.WaitUntilDeleted();

  // Sets BluetoothAdapter for the new primary page since the previous
  // adapter is released by BluetoothAdapterFactoryWrapper::ReleaseAdapter().
  BluetoothAdapterFactoryWrapper::Get().SetBluetoothAdapterOverride(adapter());
  EXPECT_CALL(*adapter(), AddObserver(_));
  EXPECT_CALL(*adapter(), GetDevice(kDeviceAddress));

  EXPECT_TRUE(content::ExecJs(GetWebContents()->GetPrimaryMainFrame(), R"(
      navigator.bluetooth.requestDevice({
          filters: [{name: 'Test Device', services: ['heart_rate']}]}))"));

  // WebBluetoothService is created for the activated page.
  EXPECT_NE(
      GetWebBluetoothServiceOverride(GetWebContents()->GetPrimaryMainFrame()),
      nullptr);

  EXPECT_CALL(*adapter(), RemoveObserver(_));
}

// Tests that GetBluetoothAllowed() only works with the main page in order to
// ensure that it's no problem to get the main frame from the WebContents.
IN_PROC_BROWSER_TEST_F(WebBluetoothServiceImplBrowserTest,
                       GetBluetoothAllowedNotCalledInPrerendering) {
  GURL url = embedded_test_server()->GetURL("/hello.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Loads a page in the prerender.
  auto prerender_url = embedded_test_server()->GetURL("/empty.html");
  // The prerendering doesn't affect the current scanning.
  FrameTreeNodeId host_id = prerender_helper()->AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     host_id);
  RenderFrameHost* prerendered_frame_host =
      prerender_helper()->GetPrerenderedMainFrameHost(host_id);

  // Runs JS asynchronously since Mojo calls are deferred during prerendering.
  content::DOMMessageQueue message_queue(prerendered_frame_host);
  content::ExecuteScriptAsync(prerendered_frame_host, R"(
    navigator.bluetooth.getAvailability()
    .then(isBluetoothAvailable => {
      window.domAutomationController.send('Done');
    });
  )");

  // WebBluetoothService is not created for `prerendered_frame_host`.
  EXPECT_EQ(GetWebBluetoothServiceOverride(prerendered_frame_host), nullptr);
  // It should not be called in the prerendering.
  EXPECT_FALSE(CheckedAllowWebBluetooth());

  // Navigates the primary page to the URL.
  prerender_helper()->NavigatePrimaryPage(prerender_url);
  // The page should be activated from the prerendering.
  EXPECT_TRUE(host_observer.was_activated());

  // Sets BlueboothAdapter for the new primary page since the previous
  // adapter is released by BluetoothAdapterFactoryWrapper::ReleaseAdapter().
  BluetoothAdapterFactoryWrapper::Get().SetBluetoothAdapterOverride(adapter());
  EXPECT_CALL(*adapter(), AddObserver(_));

  std::string message;
  do {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
  } while (message != "\"Done\"");

  // It should be called when activated.
  EXPECT_TRUE(CheckedAllowWebBluetooth());
  EXPECT_NE(GetWebBluetoothServiceOverride(prerendered_frame_host), nullptr);
  EXPECT_CALL(*adapter(), RemoveObserver(_));
}

// Tests that console messages have correct source frames.
IN_PROC_BROWSER_TEST_F(WebBluetoothServiceImplBrowserTest,
                       ConsoleLogFromSourceFrame) {
  WebContentsConsoleObserver console_observer(GetWebContents());
  constexpr char kConsoleLog[] = "Bluetooth permission has been blocked.";
  console_observer.SetPattern(kConsoleLog);

  // Block Web Bluetooth to get the console message.
  BlockGloballyDisabled();

  GURL url = embedded_test_server()->GetURL("/page_with_blank_iframe.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  EXPECT_CALL(*adapter(), AddObserver(_));

  RenderFrameHost* sub_frame = ChildFrameAt(GetWebContents(), 0);
  ASSERT_TRUE(sub_frame);

  constexpr char kErrorMessage[] =
      "NotFoundError: Web Bluetooth API globally disabled.";

  EXPECT_EQ(kErrorMessage, content::EvalJs(sub_frame, R"(
    (async() => {
      try {
        let device = await navigator.bluetooth.requestDevice({
          filters: [{name: 'Test Device', services: ['heart_rate']}]});
        return "";
      } catch(e) {
        return `${e.name}: ${e.message}`;
      }
    })()
  )"));

  ASSERT_TRUE(console_observer.Wait());
  std::vector<WebContentsConsoleObserver::Message> messages =
      console_observer.messages();
  EXPECT_EQ(messages.size(), 1u);
  EXPECT_EQ(messages.back().source_frame, sub_frame);
  EXPECT_CALL(*adapter(), RemoveObserver(_));
}

class WebBluetoothServiceImplFencedFramesBrowserTest
    : public WebBluetoothServiceImplBrowserTest {
 public:
  WebBluetoothServiceImplFencedFramesBrowserTest() = default;
  ~WebBluetoothServiceImplFencedFramesBrowserTest() override = default;
  WebBluetoothServiceImplFencedFramesBrowserTest(
      const WebBluetoothServiceImplFencedFramesBrowserTest&) = delete;

  WebBluetoothServiceImplFencedFramesBrowserTest& operator=(
      const WebBluetoothServiceImplFencedFramesBrowserTest&) = delete;

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(WebBluetoothServiceImplFencedFramesBrowserTest,
                       BlockFromFencedFrame) {
  const GURL kInitialUrl = embedded_test_server()->GetURL("/hello.html");
  EXPECT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Setup the fake device.
  AddFakeDevice(kDeviceAddress);
  SetDeviceToSelect(kDeviceAddress);

  EXPECT_CALL(*adapter(), AddObserver(_));
  EXPECT_CALL(*adapter(), GetDevice(kDeviceAddress));

  EXPECT_EQ("", content::EvalJs(GetWebContents(), R"(
    (async() => {
      try {
        let device = await navigator.bluetooth.requestDevice({
          filters: [{name: 'Test Device', services: ['heart_rate']}]});
        return "";
      } catch(e) {
        return `${e.name}: ${e.message}`;
      }
    })()
  )"));

  // WebBluetoothService is created for the main frame.
  EXPECT_NE(
      GetWebBluetoothServiceOverride(GetWebContents()->GetPrimaryMainFrame()),
      nullptr);

  // Loads a fenced frame
  const GURL kFencedFrameUrl =
      embedded_test_server()->GetURL("/fenced_frames/empty.html");
  content::RenderFrameHost* render_frame_host =
      fenced_frame_test_helper().CreateFencedFrame(
          GetWebContents()->GetPrimaryMainFrame(), kFencedFrameUrl);
  EXPECT_NE(nullptr, render_frame_host);

  // Tries to request a device from the fenced, which must cause an error.
  constexpr char kFencedFrameError[] =
      "Web Bluetooth is not allowed in a fenced frame tree.";
  auto result = content::EvalJs(render_frame_host, R"(
      navigator.bluetooth.requestDevice({
          filters: [{name: 'Test Device', services: ['heart_rate']}]}))");
  EXPECT_THAT(result.error, ::testing::HasSubstr(kFencedFrameError));

  // No service should be created, as this is a fenced-frame
  EXPECT_EQ(nullptr, GetWebBluetoothServiceOverride(render_frame_host));

  EXPECT_CALL(*adapter(), RemoveObserver(GetWebBluetoothServiceOverride(
                              GetWebContents()->GetPrimaryMainFrame())));
}

}  // namespace content
