// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_content_browser_client.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/pattern.h"
#include "cc/base/switches.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "content/public/browser/login_delegate.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/test/data/mojo_web_test_helper_test.mojom.h"
#include "content/test/mock_badge_service.h"
#include "content/test/mock_clipboard_host.h"
#include "content/test/mock_platform_notification_service.h"
#include "content/test/mock_raw_clipboard_host.h"
#include "content/web_test/browser/fake_bluetooth_chooser.h"
#include "content/web_test/browser/fake_bluetooth_chooser_factory.h"
#include "content/web_test/browser/fake_bluetooth_delegate.h"
#include "content/web_test/browser/mojo_echo.h"
#include "content/web_test/browser/mojo_web_test_helper.h"
#include "content/web_test/browser/web_test_bluetooth_fake_adapter_setter_impl.h"
#include "content/web_test/browser/web_test_browser_context.h"
#include "content/web_test/browser/web_test_browser_main_parts.h"
#include "content/web_test/browser/web_test_control_host.h"
#include "content/web_test/browser/web_test_permission_manager.h"
#include "content/web_test/browser/web_test_storage_access_manager.h"
#include "content/web_test/browser/web_test_tts_platform.h"
#include "content/web_test/common/web_test_bluetooth_fake_adapter_setter.mojom.h"
#include "content/web_test/common/web_test_switches.h"
#include "device/bluetooth/public/mojom/test/fake_bluetooth.mojom.h"
#include "device/bluetooth/test/fake_bluetooth.h"
#include "gpu/config/gpu_switches.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/net_buildflags.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "storage/browser/quota/quota_settings.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/base/ui_base_switches.h"
#include "url/origin.h"

#if defined(OS_WIN)
#include "base/strings/utf_string_conversions.h"
#include "sandbox/policy/win/sandbox_win.h"
#include "sandbox/win/src/sandbox.h"
#endif

namespace content {
namespace {

WebTestContentBrowserClient* g_web_test_browser_client;

void BindWebTestHelper(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::MojoWebTestHelper> receiver) {
  MojoWebTestHelper::Create(std::move(receiver));
}

// An OverlayWindow that returns the last given video natural size as the
// window's bounds.
class BoundsMatchVideoSizeOverlayWindow : public OverlayWindow {
 public:
  BoundsMatchVideoSizeOverlayWindow() = default;
  ~BoundsMatchVideoSizeOverlayWindow() override = default;

  BoundsMatchVideoSizeOverlayWindow(const BoundsMatchVideoSizeOverlayWindow&) =
      delete;
  BoundsMatchVideoSizeOverlayWindow& operator=(
      const BoundsMatchVideoSizeOverlayWindow&) = delete;

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
  void SetPlayPauseButtonVisibility(bool is_visible) override {}
  void SetSkipAdButtonVisibility(bool is_visible) override {}
  void SetNextTrackButtonVisibility(bool is_visible) override {}
  void SetPreviousTrackButtonVisibility(bool is_visible) override {}
  void SetSurfaceId(const viz::SurfaceId& surface_id) override {}
  cc::Layer* GetLayerForTesting() override { return nullptr; }

 private:
  gfx::Size size_;
};

void CreateChildProcessCrashWatcher() {
  class ChildProcessCrashWatcher : public BrowserChildProcessObserver {
   public:
    ChildProcessCrashWatcher() { BrowserChildProcessObserver::Add(this); }
    ~ChildProcessCrashWatcher() override {
      BrowserChildProcessObserver::Remove(this);
    }

   private:
    // BrowserChildProcessObserver implementation.
    void BrowserChildProcessCrashed(
        const ChildProcessData& data,
        const ChildProcessTerminationInfo& info) override {
      // Child processes should not crash in web tests.
      LOG(ERROR) << "Child process crashed with\n"
                    "   process_type: "
                 << data.process_type << "\n"
                 << "   name: " << data.name;
      CHECK(false);
    }
  };

  // This creates the singleton object which will now watch for crashes from
  // any BrowserChildProcessHost.
  static base::NoDestructor<ChildProcessCrashWatcher> watcher;
}

}  // namespace

WebTestContentBrowserClient::WebTestContentBrowserClient() {
  DCHECK(!g_web_test_browser_client);

  g_web_test_browser_client = this;

  // The 1GB limit is intended to give a large headroom to tests that need to
  // build up a large data set and issue many concurrent reads or writes.
  static storage::QuotaSettings quota_settings(
      storage::GetHardCodedSettings(1024 * 1024 * 1024));
  StoragePartition::SetDefaultQuotaSettingsForTesting(&quota_settings);
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

void WebTestContentBrowserClient::ResetMockClipboardHosts() {
  if (mock_clipboard_host_)
    mock_clipboard_host_->Reset();
  if (mock_raw_clipboard_host_)
    mock_raw_clipboard_host_->Reset();
}

void WebTestContentBrowserClient::SetScreenOrientationChanged(
    bool screen_orientation_changed) {
  screen_orientation_changed_ = screen_orientation_changed;
}

std::unique_ptr<FakeBluetoothChooser>
WebTestContentBrowserClient::GetNextFakeBluetoothChooser() {
  if (!fake_bluetooth_chooser_factory_)
    return nullptr;
  return fake_bluetooth_chooser_factory_->GetNextFakeBluetoothChooser();
}

void WebTestContentBrowserClient::BrowserChildProcessHostCreated(
    BrowserChildProcessHost* host) {
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner =
      content::GetUIThreadTaskRunner({});
  ui_task_runner->PostTask(FROM_HERE,
                           base::BindOnce(&CreateChildProcessCrashWatcher));
}

void WebTestContentBrowserClient::ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    RenderProcessHost* render_process_host) {
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner =
      content::GetUIThreadTaskRunner({});
  registry->AddInterface(base::BindRepeating(&MojoEcho::Bind), ui_task_runner);
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
          &WebTestContentBrowserClient::BindPermissionAutomation,
          base::Unretained(this)),
      ui_task_runner);

  registry->AddInterface(
      base::BindRepeating(
          &WebTestContentBrowserClient::BindStorageAccessAutomation,
          base::Unretained(this)),
      ui_task_runner);

  associated_registry->AddInterface(base::BindRepeating(
      &WebTestContentBrowserClient::BindWebTestControlHost,
      base::Unretained(this), render_process_host->GetID()));
}

void WebTestContentBrowserClient::BindPermissionAutomation(
    mojo::PendingReceiver<blink::test::mojom::PermissionAutomation> receiver) {
  GetWebTestBrowserContext()->GetWebTestPermissionManager()->Bind(
      std::move(receiver));
}

void WebTestContentBrowserClient::BindStorageAccessAutomation(
    mojo::PendingReceiver<blink::test::mojom::StorageAccessAutomation>
        receiver) {
  GetWebTestBrowserContext()->GetWebTestStorageAccessManager()->Bind(
      std::move(receiver));
}

void WebTestContentBrowserClient::OverrideWebkitPrefs(
    RenderViewHost* render_view_host,
    blink::web_pref::WebPreferences* prefs) {
  if (WebTestControlHost::Get())
    WebTestControlHost::Get()->OverrideWebkitPrefs(prefs);
}

void WebTestContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  ShellContentBrowserClient::AppendExtraCommandLineSwitches(command_line,
                                                            child_process_id);

  static const char* kForwardSwitches[] = {
    // Switches from web_test_switches.h that are used in the renderer.
    switches::kEnableAccelerated2DCanvas,
    switches::kEnableFontAntialiasing,
    switches::kAlwaysUseComplexText,
    switches::kStableReleaseMode,
#if defined(OS_WIN)
    switches::kRegisterFontFiles,
#endif
  };

  command_line->CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                                 kForwardSwitches,
                                 base::size(kForwardSwitches));
}

std::unique_ptr<BrowserMainParts>
WebTestContentBrowserClient::CreateBrowserMainParts(
    const MainFunctionParams& parameters) {
  auto browser_main_parts =
      std::make_unique<WebTestBrowserMainParts>(parameters);

  set_browser_main_parts(browser_main_parts.get());

  return browser_main_parts;
}

std::unique_ptr<OverlayWindow>
WebTestContentBrowserClient::CreateWindowForPictureInPicture(
    PictureInPictureWindowController* controller) {
  return std::make_unique<BoundsMatchVideoSizeOverlayWindow>();
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

  // When appropriate, we isolate WPT origins for additional OOPIF coverage.
  //
  // We don't isolate WPT origins:
  // 1) on platforms where strict Site Isolation is not the default.
  // 2) in web tests under virtual/not-site-per-process where the
  //    --disable-site-isolation-trials switch is used.
  // 3) in web tests under virtual/no-auto-wpt-origin-isolation where the
  //    --disable-auto-wpt-origin-isolation switch is used.
  if (SiteIsolationPolicy::UseDedicatedProcessesForAllSites() &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAutoWPTOriginIsolation)) {
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

void WebTestContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  ShellContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
      render_frame_host, map);
  map->Add<mojom::MojoWebTestHelper>(base::BindRepeating(&BindWebTestHelper));
  map->Add<blink::mojom::ClipboardHost>(base::BindRepeating(
      &WebTestContentBrowserClient::BindClipboardHost, base::Unretained(this)));
  map->Add<blink::mojom::RawClipboardHost>(
      base::BindRepeating(&WebTestContentBrowserClient::BindRawClipboardHost,
                          base::Unretained(this)));
  map->Add<blink::mojom::BadgeService>(base::BindRepeating(
      &WebTestContentBrowserClient::BindBadgeService, base::Unretained(this)));
}

bool WebTestContentBrowserClient::CanAcceptUntrustedExchangesIfNeeded() {
  return true;
}

BluetoothDelegate* WebTestContentBrowserClient::GetBluetoothDelegate() {
  if (!fake_bluetooth_delegate_)
    fake_bluetooth_delegate_ = std::make_unique<FakeBluetoothDelegate>();
  return fake_bluetooth_delegate_.get();
}

void WebTestContentBrowserClient::ResetFakeBluetoothDelegate() {
  fake_bluetooth_delegate_.reset();
}

content::TtsPlatform* WebTestContentBrowserClient::GetTtsPlatform() {
  return WebTestTtsPlatform::GetInstance();
}

bool WebTestContentBrowserClient::CanEnterFullscreenWithoutUserActivation() {
  return screen_orientation_changed_;
}

void WebTestContentBrowserClient::BindClipboardHost(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::ClipboardHost> receiver) {
  if (!mock_clipboard_host_)
    mock_clipboard_host_ = std::make_unique<MockClipboardHost>();
  mock_clipboard_host_->Bind(std::move(receiver));
}

void WebTestContentBrowserClient::BindRawClipboardHost(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::RawClipboardHost> receiver) {
  if (!mock_clipboard_host_)
    mock_clipboard_host_ = std::make_unique<MockClipboardHost>();
  if (!mock_raw_clipboard_host_) {
    mock_raw_clipboard_host_ =
        std::make_unique<MockRawClipboardHost>(mock_clipboard_host_.get());
  }
  mock_raw_clipboard_host_->Bind(std::move(receiver));
}

void WebTestContentBrowserClient::BindBadgeService(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::BadgeService> receiver) {
  if (!mock_badge_service_)
    mock_badge_service_ = std::make_unique<MockBadgeService>();
  mock_badge_service_->Bind(std::move(receiver));
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

void WebTestContentBrowserClient::ConfigureNetworkContextParamsForShell(
    BrowserContext* context,
    network::mojom::NetworkContextParams* context_params,
    network::mojom::CertVerifierCreationParams* cert_verifier_creation_params) {
  ShellContentBrowserClient::ConfigureNetworkContextParamsForShell(
      context, context_params, cert_verifier_creation_params);

#if BUILDFLAG(ENABLE_REPORTING)
  // Configure the Reporting service in a manner expected by certain Web
  // Platform Tests (network-error-logging and reporting-api).
  //
  //   (1) Always send reports (irrespective of BACKGROUND_SYNC permission)
  //   (2) Lower the timeout for sending reports.
  context_params->reporting_delivery_interval =
      kReportingDeliveryIntervalTimeForWebTests;
  context_params->skip_reporting_send_permission_check = true;
#endif
}

void WebTestContentBrowserClient::CreateFakeBluetoothChooserFactory(
    mojo::PendingReceiver<mojom::FakeBluetoothChooserFactory> receiver) {
  DCHECK(!fake_bluetooth_chooser_factory_);
  fake_bluetooth_chooser_factory_ =
      FakeBluetoothChooserFactory::Create(std::move(receiver));
}

void WebTestContentBrowserClient::BindWebTestControlHost(
    int render_process_id,
    mojo::PendingAssociatedReceiver<mojom::WebTestControlHost> receiver) {
  if (WebTestControlHost::Get())
    WebTestControlHost::Get()->BindWebTestControlHostForRenderer(
        render_process_id, std::move(receiver));
}

#if defined(OS_WIN)
bool WebTestContentBrowserClient::PreSpawnRenderer(
    sandbox::TargetPolicy* policy,
    RendererSpawnFlags flags) {
  // Add sideloaded font files for testing. See also DIR_WINDOWS_FONTS
  // addition in |StartSandboxedProcess|.
  std::vector<std::string> font_files = switches::GetSideloadFontFiles();
  for (std::vector<std::string>::const_iterator i(font_files.begin());
       i != font_files.end(); ++i) {
    policy->AddRule(sandbox::TargetPolicy::SUBSYS_FILES,
                    sandbox::TargetPolicy::FILES_ALLOW_READONLY,
                    base::UTF8ToWide(*i).c_str());
  }
  return true;
}
#endif  // OS_WIN

std::string WebTestContentBrowserClient::GetAcceptLangs(
    BrowserContext* context) {
  return content::GetShellLanguage();
}

}  // namespace content
