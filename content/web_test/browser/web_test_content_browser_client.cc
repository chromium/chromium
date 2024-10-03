// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_content_browser_client.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/pattern.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/origin_trials/common/features.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "content/public/browser/login_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/test/data/mojo_bindings_web_test.test-mojom.h"
#include "content/test/data/mojo_web_test_helper_test.mojom.h"
#include "content/test/mock_badge_service.h"
#include "content/test/mock_clipboard_host.h"
#include "content/web_test/browser/fake_bluetooth_chooser.h"
#include "content/web_test/browser/fake_bluetooth_chooser_factory.h"
#include "content/web_test/browser/fake_bluetooth_delegate.h"
#include "content/web_test/browser/mojo_echo.h"
#include "content/web_test/browser/mojo_optional_numerics_unittest.h"
#include "content/web_test/browser/mojo_web_test_helper.h"
#include "content/web_test/browser/web_test_bluetooth_fake_adapter_setter_impl.h"
#include "content/web_test/browser/web_test_browser_context.h"
#include "content/web_test/browser/web_test_browser_main_parts.h"
#include "content/web_test/browser/web_test_control_host.h"
#include "content/web_test/browser/web_test_cookie_manager.h"
#include "content/web_test/browser/web_test_device_posture_provider.h"
#include "content/web_test/browser/web_test_fedcm_manager.h"
#include "content/web_test/browser/web_test_origin_trial_throttle.h"
#include "content/web_test/browser/web_test_permission_manager.h"
#include "content/web_test/browser/web_test_sensor_provider_manager.h"
#include "content/web_test/browser/web_test_storage_access_manager.h"
#include "content/web_test/browser/web_test_tts_platform.h"
#include "content/web_test/common/web_test_bluetooth_fake_adapter_setter.mojom.h"
#include "content/web_test/common/web_test_string_util.h"
#include "content/web_test/common/web_test_switches.h"
#include "device/bluetooth/emulation/fake_bluetooth.h"
#include "device/bluetooth/public/mojom/emulation/fake_bluetooth.mojom.h"
#include "gpu/config/gpu_switches.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/net_buildflags.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/device/public/cpp/compute_pressure/buildflags.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/proxy_resolver/proxy_resolver_factory_impl.h"  // nogncheck
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "storage/browser/quota/quota_settings.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/device_posture/device_posture_provider_automation.mojom.h"
#include "ui/base/ui_base_switches.h"
#include "url/origin.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
#include "content/web_test/browser/web_test_pressure_manager.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
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
class BoundsMatchVideoSizeOverlayWindow : public VideoOverlayWindow {
 public:
  BoundsMatchVideoSizeOverlayWindow() = default;
  ~BoundsMatchVideoSizeOverlayWindow() override = default;

  BoundsMatchVideoSizeOverlayWindow(const BoundsMatchVideoSizeOverlayWindow&) =
      delete;
  BoundsMatchVideoSizeOverlayWindow& operator=(
      const BoundsMatchVideoSizeOverlayWindow&) = delete;

  bool IsActive() const override { return false; }
  void Close() override {}
  void ShowInactive() override {}
  void Hide() override {}
  bool IsVisible() const override { return false; }
  gfx::Rect GetBounds() override { return gfx::Rect(size_); }
  void UpdateNaturalSize(const gfx::Size& natural_size) override {
    size_ = natural_size;
  }
  void SetPlaybackState(PlaybackState playback_state) override {}
  void SetPlayPauseButtonVisibility(bool is_visible) override {}
  void SetSkipAdButtonVisibility(bool is_visible) override {}
  void SetNextTrackButtonVisibility(bool is_visible) override {}
  void SetPreviousTrackButtonVisibility(bool is_visible) override {}
  void SetMicrophoneMuted(bool muted) override {}
  void SetCameraState(bool turned_on) override {}
  void SetToggleMicrophoneButtonVisibility(bool is_visible) override {}
  void SetToggleCameraButtonVisibility(bool is_visible) override {}
  void SetHangUpButtonVisibility(bool is_visible) override {}
  void SetNextSlideButtonVisibility(bool is_visible) override {}
  void SetPreviousSlideButtonVisibility(bool is_visible) override {}
  void SetSurfaceId(const viz::SurfaceId& surface_id) override {}

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

class MojoWebTestCounterImpl : public mojo_bindings_test::mojom::Counter {
 public:
  using CounterObserver = mojo_bindings_test::mojom::CounterObserver;

  MojoWebTestCounterImpl() {
    additional_receivers_.set_disconnect_handler(base::BindRepeating(
        &MojoWebTestCounterImpl::OnCloneDisconnected, base::Unretained(this)));
  }

  ~MojoWebTestCounterImpl() override = default;

  static void Bind(mojo::PendingReceiver<Counter> receiver) {
    mojo::MakeSelfOwnedReceiver(std::make_unique<MojoWebTestCounterImpl>(),
                                std::move(receiver));
  }

  // mojo_bindings_test::mojom::Counter:
  void AddObserver(
      mojo::PendingAssociatedRemote<CounterObserver> observer) override {
    observers_.Add(std::move(observer));
  }

  void AddNewObserver(AddNewObserverCallback callback) override {
    mojo::PendingAssociatedRemote<CounterObserver> observer;
    std::move(callback).Run(observer.InitWithNewEndpointAndPassReceiver());
    observers_.Add(std::move(observer));
  }

  void RemoveAllObservers() override { observers_.Clear(); }

  void Clone(mojo::PendingAssociatedReceiver<Counter> receiver) override {
    additional_receivers_.Add(this, std::move(receiver));
  }

  void CloneToNewRemote(CloneToNewRemoteCallback callback) override {
    mojo::PendingAssociatedRemote<Counter> new_remote;
    additional_receivers_.Add(this,
                              new_remote.InitWithNewEndpointAndPassReceiver());
    std::move(callback).Run(std::move(new_remote));
  }

  void Increment(IncrementCallback callback) override {
    ++count_;
    for (const auto& observer : observers_)
      observer->OnCountChanged(count_);
    std::move(callback).Run(count_);
  }

 private:
  void OnCloneDisconnected() {
    for (const auto& observer : observers_)
      observer->OnCloneDisconnected();
  }

  int count_ = 0;
  mojo::AssociatedReceiverSet<Counter> additional_receivers_;
  mojo::AssociatedRemoteSet<CounterObserver> observers_;
};

class MojoWebTestProxyResolverFactory
    : public proxy_resolver::mojom::ProxyResolverFactory {
 public:
  MojoWebTestProxyResolverFactory() = default;

  static mojo::PendingRemote<proxy_resolver::mojom::ProxyResolverFactory>
  CreateWithSelfOwnedReceiver() {
    mojo::PendingRemote<proxy_resolver::mojom::ProxyResolverFactory> remote;
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<MojoWebTestProxyResolverFactory>(),
        remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  void CreateResolver(
      const std::string& pac_script,
      mojo::PendingReceiver<proxy_resolver::mojom::ProxyResolver> receiver,
      mojo::PendingRemote<
          proxy_resolver::mojom::ProxyResolverFactoryRequestClient> client)
      override {
    static base::NoDestructor<
        mojo::Remote<proxy_resolver::mojom::ProxyResolverFactory>>
        remote;
    if (!remote->is_bound()) {
      static base::NoDestructor<proxy_resolver::ProxyResolverFactoryImpl>
          factory(remote->BindNewPipeAndPassReceiver());
    }

    remote->get()->CreateResolver(pac_script, std::move(receiver),
                                  std::move(client));
  }
};

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
      GetUIThreadTaskRunner({});
  ui_task_runner->PostTask(FROM_HERE,
                           base::BindOnce(&CreateChildProcessCrashWatcher));
}

void WebTestContentBrowserClient::ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    RenderProcessHost* render_process_host) {
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner =
      GetUIThreadTaskRunner({});
  registry->AddInterface(base::BindRepeating(&MojoWebTestCounterImpl::Bind),
                         ui_task_runner);
  registry->AddInterface(base::BindRepeating(&MojoEcho::Bind), ui_task_runner);
  registry->AddInterface(
      base::BindRepeating(&optional_numerics_unittest::Params::Bind),
      ui_task_runner);
  registry->AddInterface(
      base::BindRepeating(&optional_numerics_unittest::ResponseParams::Bind),
      ui_task_runner);
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

  registry->AddInterface(
      base::BindRepeating(
          &WebTestContentBrowserClient::BindNonAssociatedWebTestControlHost,
          base::Unretained(this)),
      ui_task_runner);
}

void WebTestContentBrowserClient::
    RegisterAssociatedInterfaceBindersForRenderFrameHost(
        RenderFrameHost& render_frame_host,
        blink::AssociatedInterfaceRegistry& associated_registry) {
  associated_registry.AddInterface<mojom::WebTestControlHost>(
      base::BindRepeating(&WebTestContentBrowserClient::BindWebTestControlHost,
                          base::Unretained(this),
                          render_frame_host.GetProcess()->GetID()));
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
    WebContents* web_contents,
    blink::web_pref::WebPreferences* prefs) {
  if (WebTestControlHost::Get())
    WebTestControlHost::Get()->OverrideWebkitPrefs(prefs);
}

std::vector<std::unique_ptr<content::NavigationThrottle>>
WebTestContentBrowserClient::CreateThrottlesForNavigation(
    content::NavigationHandle* navigation_handle) {
  std::vector<std::unique_ptr<content::NavigationThrottle>> throttles =
      ShellContentBrowserClient::CreateThrottlesForNavigation(
          navigation_handle);
  if (origin_trials::features::IsPersistentOriginTrialsEnabled()) {
    throttles.push_back(std::make_unique<WebTestOriginTrialThrottle>(
        navigation_handle, navigation_handle->GetWebContents()
                               ->GetBrowserContext()
                               ->GetOriginTrialsControllerDelegate()));
  }
  return throttles;
}

void WebTestContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  ShellContentBrowserClient::AppendExtraCommandLineSwitches(command_line,
                                                            child_process_id);

  static const char* const kForwardSwitches[] = {
      // Switches from web_test_switches.h that are used in the renderer.
      switches::kEnableAccelerated2DCanvas,
      switches::kEnableFontAntialiasing,
      switches::kAlwaysUseComplexText,
      switches::kStableReleaseMode,
  };

  command_line->CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                                 kForwardSwitches);
}

std::unique_ptr<BrowserMainParts>
WebTestContentBrowserClient::CreateBrowserMainParts(
    bool /* is_integration_test */) {
  auto browser_main_parts = std::make_unique<WebTestBrowserMainParts>();

  set_browser_main_parts(browser_main_parts.get());

  return browser_main_parts;
}

std::unique_ptr<VideoOverlayWindow>
WebTestContentBrowserClient::CreateWindowForVideoPictureInPicture(
    VideoPictureInPictureWindowController* controller) {
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
    // //third_party/blink/web_tests/external/wpt/config.json
    const char* kSchemes[] = {
        url::kHttpScheme,
        url::kHttpsScheme,
    };

    origins_to_isolate.reserve(origins_to_isolate.size() +
                               std::size(kWptHostnames) * std::size(kSchemes));
    for (const char* hostname : kWptHostnames) {
      for (const char* scheme : kSchemes) {
        origins_to_isolate.push_back(
            base::StringPrintf("%s://%s/", scheme, hostname));
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

  WebTestControlHost* control_host = WebTestControlHost::Get();
  bool dump_navigation_policy =
      control_host->web_test_runtime_flags().dump_navigation_policy();

  if (dump_navigation_policy) {
    static_cast<mojom::WebTestControlHost*>(control_host)
        ->PrintMessage(
            "Default policy for createView for '" +
            web_test_string_util::URLDescription(target_url) + "' is '" +
            web_test_string_util::WindowOpenDispositionToString(disposition) +
            "'\n");
  }
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
  map->Add<blink::mojom::BadgeService>(base::BindRepeating(
      &WebTestContentBrowserClient::BindBadgeService, base::Unretained(this)));
  map->Add<blink::test::mojom::CookieManagerAutomation>(base::BindRepeating(
      &WebTestContentBrowserClient::BindCookieManagerAutomation,
      base::Unretained(this)));
  map->Add<blink::test::mojom::DevicePostureProviderAutomation>(
      base::BindRepeating(
          &WebTestContentBrowserClient::BindDevicePostureProviderAutomation,
          base::Unretained(this)));
  map->Add<blink::test::mojom::FederatedAuthRequestAutomation>(
      base::BindRepeating(&WebTestContentBrowserClient::BindFedCmAutomation,
                          base::Unretained(this)));
  map->Add<blink::test::mojom::WebSensorProviderAutomation>(base::BindRepeating(
      &WebTestContentBrowserClient::BindWebSensorProviderAutomation,
      base::Unretained(this)));

#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
  map->Add<blink::test::mojom::WebPressureManagerAutomation>(
      base::BindRepeating(
          &WebTestContentBrowserClient::BindWebPressureManagerAutomation,
          base::Unretained(this)));
#endif  // BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
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

void WebTestContentBrowserClient::BindClipboardHost(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::ClipboardHost> receiver) {
  if (!mock_clipboard_host_)
    mock_clipboard_host_ = std::make_unique<MockClipboardHost>();
  mock_clipboard_host_->Bind(std::move(receiver));
}

void WebTestContentBrowserClient::BindBadgeService(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::BadgeService> receiver) {
  if (!mock_badge_service_)
    mock_badge_service_ = std::make_unique<MockBadgeService>();
  mock_badge_service_->Bind(std::move(receiver));
}

void WebTestContentBrowserClient::BindCookieManagerAutomation(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::test::mojom::CookieManagerAutomation>
        receiver) {
  cookie_managers_.Add(std::make_unique<WebTestCookieManager>(
                           GetWebTestBrowserContext()
                               ->GetDefaultStoragePartition()
                               ->GetCookieManagerForBrowserProcess(),
                           render_frame_host->GetLastCommittedURL()),
                       std::move(receiver));
}

void WebTestContentBrowserClient::BindDevicePostureProviderAutomation(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::test::mojom::DevicePostureProviderAutomation>
        receiver) {
  device_posture_provider_managers_.Add(
      std::make_unique<WebTestDevicePostureProvider>(
          static_cast<RenderFrameHostImpl*>(render_frame_host)->GetWeakPtr()),
      std::move(receiver));
}

void WebTestContentBrowserClient::BindFedCmAutomation(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::test::mojom::FederatedAuthRequestAutomation>
        receiver) {
  fedcm_managers_.Add(std::make_unique<WebTestFedCmManager>(render_frame_host),
                      std::move(receiver));
}

void WebTestContentBrowserClient::BindWebSensorProviderAutomation(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::test::mojom::WebSensorProviderAutomation>
        receiver) {
  if (!sensor_provider_manager_) {
    sensor_provider_manager_ = std::make_unique<WebTestSensorProviderManager>(
        WebContents::FromRenderFrameHost(render_frame_host));
  }
  sensor_provider_manager_->Bind(std::move(receiver));
}

void WebTestContentBrowserClient::ResetWebSensorProviderAutomation() {
  sensor_provider_manager_.reset();
}

#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
void WebTestContentBrowserClient::BindWebPressureManagerAutomation(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::test::mojom::WebPressureManagerAutomation>
        receiver) {
  WebTestPressureManager::GetOrCreate(
      WebContents::FromRenderFrameHost(render_frame_host))
      ->Bind(std::move(receiver));
}
#endif  // BUILDFLAG(ENABLE_COMPUTE_PRESSURE)

std::unique_ptr<LoginDelegate> WebTestContentBrowserClient::CreateLoginDelegate(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context,
    const content::GlobalRequestID& request_id,
    bool is_request_for_primary_main_frame,
    bool is_request_for_navigation,
    const GURL& url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    bool first_auth_attempt,
    LoginAuthRequiredCallback auth_required_callback) {
  return nullptr;
}

void WebTestContentBrowserClient::ConfigureNetworkContextParamsForShell(
    BrowserContext* context,
    network::mojom::NetworkContextParams* context_params,
    cert_verifier::mojom::CertVerifierCreationParams*
        cert_verifier_creation_params) {
  ShellContentBrowserClient::ConfigureNetworkContextParamsForShell(
      context, context_params, cert_verifier_creation_params);

  // Configure the Reporting service in a manner expected by certain Web
  // Platform Tests (network-error-logging and reporting-api).
  //
  //   (1) Always send reports (irrespective of BACKGROUND_SYNC permission)
  //   (2) Lower the timeout for sending reports.
  context_params->reporting_delivery_interval =
      kReportingDeliveryIntervalTimeForWebTests;
  context_params->skip_reporting_send_permission_check = true;

  const char* kProxyPacUrl = "proxy-pac-url";
  auto pac_url =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(kProxyPacUrl);

  if (!pac_url.empty()) {
    auto proxy_config = net::ProxyConfig::CreateFromCustomPacURL(GURL(pac_url));
    context_params->proxy_resolver_factory =
        MojoWebTestProxyResolverFactory::CreateWithSelfOwnedReceiver();
    context_params->initial_proxy_config = net::ProxyConfigWithAnnotation(
        proxy_config, TRAFFIC_ANNOTATION_FOR_TESTS);
  }
}

void WebTestContentBrowserClient::CreateFakeBluetoothChooserFactory(
    mojo::PendingReceiver<mojom::FakeBluetoothChooserFactory> receiver) {
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

void WebTestContentBrowserClient::BindNonAssociatedWebTestControlHost(
    mojo::PendingReceiver<mojom::NonAssociatedWebTestControlHost> receiver) {
  if (WebTestControlHost::Get()) {
    WebTestControlHost::Get()->BindNonAssociatedWebTestControlHost(
        std::move(receiver));
  }
}

#if BUILDFLAG(IS_WIN)
bool WebTestContentBrowserClient::PreSpawnChild(
    sandbox::TargetConfig* config,
    sandbox::mojom::Sandbox sandbox_type,
    ChildSpawnFlags flags) {
  return true;
}
#endif  // BUILDFLAG(IS_WIN)

std::string WebTestContentBrowserClient::GetAcceptLangs(
    BrowserContext* context) {
  return content::GetShellLanguage();
}

bool WebTestContentBrowserClient::IsInterestGroupAPIAllowed(
    content::RenderFrameHost* render_frame_host,
    InterestGroupApiOperation operation,
    const url::Origin& top_frame_origin,
    const url::Origin& api_origin) {
  return true;
}

bool WebTestContentBrowserClient::IsPrivacySandboxReportingDestinationAttested(
    content::BrowserContext* browser_context,
    const url::Origin& destination_origin,
    content::PrivacySandboxInvokingAPI invoking_api) {
  return true;
}

void WebTestContentBrowserClient::GetHyphenationDictionary(
    base::OnceCallback<void(const base::FilePath&)> callback) {
  // Use the dictionaries in the runtime deps instead of the repository. The
  // build infrastructure takes only the list of files that GN determines to be
  // the runtime deps, not the whole repository.
  base::FilePath dir;
  if (base::PathService::Get(base::DIR_EXE, &dir)) {
    dir = dir.AppendASCII("gen/hyphen-data");
    std::move(callback).Run(dir);
  }
  // No need to callback if there were no dictionaries.
}

void WebTestContentBrowserClient::
    RegisterMojoBinderPoliciesForSameOriginPrerendering(
        MojoBinderPolicyMap& policy_map) {
  policy_map.SetAssociatedPolicy<mojom::WebTestControlHost>(
      content::MojoBinderAssociatedPolicy::kGrant);
}

void WebTestContentBrowserClient::RegisterMojoBinderPoliciesForPreview(
    MojoBinderPolicyMap& policy_map) {
  policy_map.SetAssociatedPolicy<mojom::WebTestControlHost>(
      content::MojoBinderAssociatedPolicy::kGrant);
}

}  // namespace content
