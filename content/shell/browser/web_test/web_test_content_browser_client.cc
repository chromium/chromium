// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/web_test_content_browser_client.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/pattern.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "content/public/browser/login_delegate.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/web_test/blink_test_controller.h"
#include "content/shell/browser/web_test/fake_bluetooth_chooser.h"
#include "content/shell/browser/web_test/fake_bluetooth_chooser_factory.h"
#include "content/shell/browser/web_test/mojo_web_test_helper.h"
#include "content/shell/browser/web_test/web_test_bluetooth_fake_adapter_setter_impl.h"
#include "content/shell/browser/web_test/web_test_browser_context.h"
#include "content/shell/browser/web_test/web_test_browser_main_parts.h"
#include "content/shell/browser/web_test/web_test_message_filter.h"
#include "content/shell/browser/web_test/web_test_tts_controller_delegate.h"
#include "content/shell/browser/web_test/web_test_tts_platform.h"
#include "content/shell/common/web_test/web_test_switches.h"
#include "content/shell/renderer/web_test/blink_test_helpers.h"
#include "content/test/mock_clipboard_host.h"
#include "content/test/mock_platform_notification_service.h"
#include "device/bluetooth/test/fake_bluetooth.h"
#include "gpu/config/gpu_switches.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "url/origin.h"

namespace content {
namespace {

WebTestContentBrowserClient* g_web_test_browser_client;

void BindWebTestHelper(mojo::PendingReceiver<mojom::MojoWebTestHelper> receiver,
                       RenderFrameHost* render_frame_host) {
  MojoWebTestHelper::Create(std::move(receiver));
}

class TestOverlayWindow : public OverlayWindow {
 public:
  TestOverlayWindow() = default;
  ~TestOverlayWindow() override {}

  static std::unique_ptr<OverlayWindow> Create(
      PictureInPictureWindowController* controller) {
    return std::unique_ptr<OverlayWindow>(new TestOverlayWindow());
  }

  bool IsActive() override { return false; }
  void Close() override {}
  void ShowInactive() override {}
  void Hide() override {}
  bool IsVisible() override { return false; }
  bool IsAlwaysOnTop() override { return false; }
  gfx::Rect GetBounds() override { return gfx::Rect(size_); }
  void UpdateVideoSize(const gfx::Size& natural_size) override {
    size_ = natural_size;
  }
  void SetPlaybackState(PlaybackState playback_state) override {}
  void SetAlwaysHidePlayPauseButton(bool is_visible) override {}
  void SetSkipAdButtonVisibility(bool is_visible) override {}
  void SetNextTrackButtonVisibility(bool is_visible) override {}
  void SetPreviousTrackButtonVisibility(bool is_visible) override {}
  void SetSurfaceId(const viz::SurfaceId& surface_id) override {}
  cc::Layer* GetLayerForTesting() override { return nullptr; }

 private:
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(TestOverlayWindow);
};

}  // namespace

WebTestContentBrowserClient::WebTestContentBrowserClient() {
  DCHECK(!g_web_test_browser_client);

  g_web_test_browser_client = this;
}

WebTestContentBrowserClient::~WebTestContentBrowserClient() {
  g_web_test_browser_client = nullptr;
}

WebTestContentBrowserClient* WebTestContentBrowserClient::Get() {
  return g_web_test_browser_client;
}

WebTestBrowserContext* WebTestContentBrowserClient::GetWebTestBrowserContext() {
  return static_cast<WebTestBrowserContext*>(browser_context());
}

void WebTestContentBrowserClient::SetPopupBlockingEnabled(bool block_popups) {
  block_popups_ = block_popups;
}

void WebTestContentBrowserClient::ResetMockClipboardHost() {
  if (mock_clipboard_host_)
    mock_clipboard_host_->Reset();
}

std::unique_ptr<FakeBluetoothChooser>
WebTestContentBrowserClient::GetNextFakeBluetoothChooser() {
  if (!fake_bluetooth_chooser_factory_)
    return nullptr;
  return fake_bluetooth_chooser_factory_->GetNextFakeBluetoothChooser();
}

void WebTestContentBrowserClient::RenderProcessWillLaunch(
    RenderProcessHost* host) {
  ShellContentBrowserClient::RenderProcessWillLaunch(host);

  StoragePartition* partition =
      BrowserContext::GetDefaultStoragePartition(browser_context());
  host->AddFilter(new WebTestMessageFilter(
      host->GetID(), partition->GetDatabaseTracker(),
      partition->GetQuotaManager(), partition->GetNetworkContext()));
}

void WebTestContentBrowserClient::ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    RenderProcessHost* render_process_host) {
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner =
      base::CreateSingleThreadTaskRunner({content::BrowserThread::UI});
  registry->AddInterface(
      base::BindRepeating(&WebTestBluetoothFakeAdapterSetterImpl::Create),
      ui_task_runner);

  registry->AddInterface(base::BindRepeating(&bluetooth::FakeBluetooth::Create),
                         ui_task_runner);
  // This class outlives |render_process_host|, which owns |registry|. Since
  // any binders will not be called after |registry| is deleted
  // and |registry| is outlived by this class, it is safe to use
  // base::Unretained in all binders.
  registry->AddInterface(
      base::BindRepeating(
          &WebTestContentBrowserClient::CreateFakeBluetoothChooserFactory,
          base::Unretained(this)),
      ui_task_runner);
  registry->AddInterface(base::BindRepeating(&MojoWebTestHelper::Create));
  registry->AddInterface(
      base::BindRepeating(
          &WebTestContentBrowserClient::BindClipboardHostForRequest,
          base::Unretained(this)),
      ui_task_runner);

  registry->AddInterface(
      base::BindRepeating(
          &WebTestContentBrowserClient::BindClientHintsControllerDelegate,
          base::Unretained(this)),
      ui_task_runner);
}

void WebTestContentBrowserClient::BindClipboardHostForRequest(
    blink::mojom::ClipboardHostRequest request) {
  // Implicit conversion from ClipboardHostRequest to
  // mojo::PendingReceiver<blink::mojom::ClipboardHost>.
  BindClipboardHost(std::move(request));
}

void WebTestContentBrowserClient::BindClipboardHost(
    mojo::PendingReceiver<blink::mojom::ClipboardHost> receiver) {
  if (!mock_clipboard_host_)
    mock_clipboard_host_ = std::make_unique<MockClipboardHost>();
  mock_clipboard_host_->Bind(std::move(receiver));
}

void WebTestContentBrowserClient::BindClientHintsControllerDelegate(
    mojo::PendingReceiver<client_hints::mojom::ClientHints> receiver) {
  ClientHintsControllerDelegate* delegate =
      browser_context()->GetClientHintsControllerDelegate();
  DCHECK(delegate);
  delegate->Bind(std::move(receiver));
}

void WebTestContentBrowserClient::OverrideWebkitPrefs(
    RenderViewHost* render_view_host,
    WebPreferences* prefs) {
  if (BlinkTestController::Get())
    BlinkTestController::Get()->OverrideWebkitPrefs(prefs);
}

void WebTestContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  command_line->AppendSwitch(switches::kRunWebTests);
  ShellContentBrowserClient::AppendExtraCommandLineSwitches(command_line,
                                                            child_process_id);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAlwaysUseComplexText)) {
    command_line->AppendSwitch(switches::kAlwaysUseComplexText);
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableFontAntialiasing)) {
    command_line->AppendSwitch(switches::kEnableFontAntialiasing);
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kStableReleaseMode)) {
    command_line->AppendSwitch(switches::kStableReleaseMode);
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableLeakDetection)) {
    command_line->AppendSwitchASCII(
        switches::kEnableLeakDetection,
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kEnableLeakDetection));
  }
}

std::unique_ptr<BrowserMainParts>
WebTestContentBrowserClient::CreateBrowserMainParts(
    const MainFunctionParams& parameters) {
  auto browser_main_parts =
      std::make_unique<WebTestBrowserMainParts>(parameters);

  set_browser_main_parts(browser_main_parts.get());

  return browser_main_parts;
}

void WebTestContentBrowserClient::GetQuotaSettings(
    BrowserContext* context,
    StoragePartition* partition,
    storage::OptionalQuotaSettingsCallback callback) {
  // The 1GB limit is intended to give a large headroom to tests that need to
  // build up a large data set and issue many concurrent reads or writes.
  std::move(callback).Run(storage::GetHardCodedSettings(1024 * 1024 * 1024));
}

std::unique_ptr<OverlayWindow>
WebTestContentBrowserClient::CreateWindowForPictureInPicture(
    PictureInPictureWindowController* controller) {
  return TestOverlayWindow::Create(controller);
}

std::vector<url::Origin>
WebTestContentBrowserClient::GetOriginsRequiringDedicatedProcess() {
  // Unconditionally (with and without --site-per-process) isolate some origins
  // that may be used by tests that only make sense in presence of an OOPIF.
  std::vector<std::string> origins_to_isolate = {
      "http://devtools.oopif.test/",
      "http://devtools-extensions.oopif.test/",
      "https://devtools.oopif.test/",
  };

  // On platforms with strict Site Isolation, the also isolate WPT origins for
  // additional OOPIF coverage.
  //
  // Don't isolate WPT origins on
  // 1) platforms where strict Site Isolation is not the default.
  // 2) in web tests under virtual/not-site-per-process where
  //    --disable-site-isolation-trials switch is used.
  if (SiteIsolationPolicy::UseDedicatedProcessesForAllSites()) {
    // The list of hostnames below is based on
    // https://web-platform-tests.org/writing-tests/server-features.html
    const char* kWptHostnames[] = {
        "www.web-platform.test",
        "www1.web-platform.test",
        "www2.web-platform.test",
        "xn--n8j6ds53lwwkrqhv28a.web-platform.test",
        "xn--lve-6lad.web-platform.test",
    };

    // The list of schemes below is based on
    // third_party/blink/tools/blinkpy/third_party/wpt/wpt.config.json
    const char* kOriginTemplates[] = {
        "http://%s/",
        "https://%s/",
    };

    origins_to_isolate.reserve(origins_to_isolate.size() +
                               base::size(kWptHostnames) *
                                   base::size(kOriginTemplates));
    for (const char* kWptHostname : kWptHostnames) {
      for (const char* kOriginTemplate : kOriginTemplates) {
        std::string origin = base::StringPrintf(kOriginTemplate, kWptHostname);
        origins_to_isolate.push_back(origin);
      }
    }
  }

  // Translate std::vector<std::string> into std::vector<url::Origin>.
  std::vector<url::Origin> result;
  result.reserve(origins_to_isolate.size());
  for (const std::string& s : origins_to_isolate)
    result.push_back(url::Origin::Create(GURL(s)));
  return result;
}

PlatformNotificationService*
WebTestContentBrowserClient::GetPlatformNotificationService(
    content::BrowserContext* browser_context) {
  if (!mock_platform_notification_service_) {
    mock_platform_notification_service_.reset(
        new MockPlatformNotificationService(browser_context));
  }

  return mock_platform_notification_service_.get();
}

bool WebTestContentBrowserClient::CanCreateWindow(
    content::RenderFrameHost* opener,
    const GURL& opener_url,
    const GURL& opener_top_level_frame_url,
    const url::Origin& source_origin,
    content::mojom::WindowContainerType container_type,
    const GURL& target_url,
    const content::Referrer& referrer,
    const std::string& frame_name,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& features,
    bool user_gesture,
    bool opener_suppressed,
    bool* no_javascript_access) {
  *no_javascript_access = false;
  return !block_popups_ || user_gesture;
}

bool WebTestContentBrowserClient::CanAcceptUntrustedExchangesIfNeeded() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kRunWebTests);
}

content::TtsControllerDelegate*
WebTestContentBrowserClient::GetTtsControllerDelegate() {
  return WebTestTtsControllerDelegate::GetInstance();
}

content::TtsPlatform* WebTestContentBrowserClient::GetTtsPlatform() {
  return WebTestTtsPlatform::GetInstance();
}

void WebTestContentBrowserClient::ExposeInterfacesToFrame(
    service_manager::BinderRegistryWithArgs<content::RenderFrameHost*>*
        registry) {
  registry->AddInterface(base::BindRepeating(&BindWebTestHelper));
}

std::unique_ptr<LoginDelegate> WebTestContentBrowserClient::CreateLoginDelegate(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    const content::GlobalRequestID& request_id,
    bool is_main_frame,
    const GURL& url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    bool first_auth_attempt,
    LoginAuthRequiredCallback auth_required_callback) {
  return nullptr;
}

// private
void WebTestContentBrowserClient::CreateFakeBluetoothChooserFactory(
    mojo::PendingReceiver<mojom::FakeBluetoothChooserFactory> receiver) {
  DCHECK(!fake_bluetooth_chooser_factory_);
  fake_bluetooth_chooser_factory_ =
      FakeBluetoothChooserFactory::Create(std::move(receiver));
}

}  // namespace content
