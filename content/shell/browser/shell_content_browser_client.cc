// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_content_browser_client.h"

#include <stddef.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/custom_handlers/protocol_handler_throttle.h"
#include "components/custom_handlers/simple_protocol_handler_registry_factory.h"
#include "components/embedder_support/switches.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/metrics/client_info.h"
#include "components/metrics/metrics_service.h"
#include "components/network_hints/browser/simple_network_hints_handler_impl.h"
#include "components/performance_manager/embedder/binders.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/login_delegate.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/setup_field_trials.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_browser_main_parts.h"
#include "content/shell/browser/shell_devtools_manager_delegate.h"
#include "content/shell/browser/shell_web_contents_view_delegate_creator.h"
#include "content/shell/common/shell_controller.test-mojom.h"
#include "content/shell/common/shell_paths.h"
#include "content/shell/common/shell_switches.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/media_service.mojom.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/features.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/ssl/client_cert_identity.h"
#include "services/device/public/cpp/geolocation/location_system_permission_status.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_service_buildflags.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/apk_assets.h"
#include "base/android/path_utils.h"
#include "content/shell/android/shell_descriptors.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "components/crash/content/browser/crash_handler_host_linux.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "services/device/public/cpp/test/fake_geolocation_system_permission_manager.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include "components/crash/core/app/crash_switches.h"
#include "components/crash/core/app/crashpad.h"
#include "content/public/common/content_descriptors.h"
#endif

#if BUILDFLAG(ENABLE_CAST_RENDERER)
#include "media/mojo/services/media_service_factory.h"  // nogncheck
#endif

#if BUILDFLAG(IS_CT_SUPPORTED)
#include "services/network/public/mojom/ct_log_info.mojom.h"
#endif

#if BUILDFLAG(IS_IOS)
#include "components/permissions/bluetooth_delegate_impl.h"
#include "content/shell/browser/bluetooth/shell_bluetooth_delegate_impl_client.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "media/mojo/mojom/media_foundation_preferences.mojom.h"
#include "media/mojo/services/media_foundation_preferences.h"
#endif  // BUILDFLAG(IS_WIN)

namespace content {

namespace {

#if BUILDFLAG(IS_IOS)
inline constexpr char kJITEnabled[] = "settings.javascript.jit";
#endif

using PerformanceManagerRegistry =
    performance_manager::PerformanceManagerRegistry;

// Tests may install their own ShellContentBrowserClient, track the list here.
// The list is ordered with oldest first and newer ones added after it.
std::vector<ShellContentBrowserClient*>&
GetShellContentBrowserClientInstancesImpl() {
  static base::NoDestructor<std::vector<ShellContentBrowserClient*>> instances;
  return *instances;
}

#if BUILDFLAG(IS_ANDROID)
int GetCrashSignalFD(const base::CommandLine& command_line) {
  return crashpad::CrashHandlerHost::Get()->GetDeathSignalSocket();
}
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
int GetCrashSignalFD(const base::CommandLine& command_line) {
  int fd;
  pid_t pid;
  return crash_reporter::GetHandlerSocket(&fd, &pid) ? fd : -1;
}
#endif

class ShellControllerImpl : public mojom::ShellController {
 public:
  ShellControllerImpl() = default;
  ~ShellControllerImpl() override = default;

  // mojom::ShellController:
  void GetSwitchValue(const std::string& name,
                      GetSwitchValueCallback callback) override {
    const auto& command_line = *base::CommandLine::ForCurrentProcess();
    if (command_line.HasSwitch(name)) {
      std::move(callback).Run(command_line.GetSwitchValueASCII(name));
    } else {
      std::move(callback).Run(std::nullopt);
    }
  }

  void ExecuteJavaScript(const std::u16string& script,
                         ExecuteJavaScriptCallback callback) override {
    CHECK(!Shell::windows().empty());
    WebContents* contents = Shell::windows()[0]->web_contents();
    contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        script, std::move(callback), ISOLATED_WORLD_ID_GLOBAL);
  }

  void ShutDown() override { Shell::Shutdown(); }
};

void BindNetworkHintsHandler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<network_hints::mojom::NetworkHintsHandler> receiver) {
  DCHECK(frame_host);
  network_hints::SimpleNetworkHintsHandlerImpl::Create(frame_host,
                                                       std::move(receiver));
}

#if BUILDFLAG(IS_WIN)
void BindMediaFoundationPreferences(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<media::mojom::MediaFoundationPreferences> receiver) {
  // Passing in a NullCallback since we don't have MediaFoundationServiceMonitor
  // in content.
  MediaFoundationPreferencesImpl::Create(
      frame_host->GetSiteInstance()->GetSiteURL(), base::NullCallback(),
      std::move(receiver));
}
#endif  // BUILDFLAG(IS_WIN)

base::flat_set<url::Origin> GetIsolatedContextOriginSetFromFlag() {
  std::string cmdline_origins(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kIsolatedContextOrigins));

  std::vector<std::string_view> origin_strings = base::SplitStringPiece(
      cmdline_origins, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  base::flat_set<url::Origin> origin_set;
  for (std::string_view origin_string : origin_strings) {
    url::Origin allowed_origin = url::Origin::Create(GURL(origin_string));
    if (!allowed_origin.opaque()) {
      origin_set.insert(allowed_origin);
    } else {
      LOG(ERROR) << "Error parsing IsolatedContext origin: " << origin_string;
    }
  }
  return origin_set;
}

// In content browser tests we allow more than one ShellContentBrowserClient
// to be created (actually, ContentBrowserTestContentBrowserClient). Any state
// needed should be added here so that it's shared between the instances.
struct SharedState {
  SharedState() {
#if BUILDFLAG(IS_MAC)
    location_manager =
        std::make_unique<device::FakeGeolocationSystemPermissionManager>();
    location_manager->SetSystemPermission(
        device::LocationSystemPermissionStatus::kAllowed);
#endif
  }

#if BUILDFLAG(IS_MAC)
  std::unique_ptr<device::FakeGeolocationSystemPermissionManager>
      location_manager;
#endif

  // Owned by content::BrowserMainLoop.
  raw_ptr<ShellBrowserMainParts, DanglingUntriaged> shell_browser_main_parts =
      nullptr;

  std::unique_ptr<PrefService> local_state;
};

SharedState& GetSharedState() {
  static SharedState* g_shared_state = nullptr;
  if (!g_shared_state) {
    g_shared_state = new SharedState();
  }
  return *g_shared_state;
}

std::unique_ptr<PrefService> CreateLocalState() {
  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
  metrics::MetricsService::RegisterPrefs(pref_registry.get());
  variations::VariationsService::RegisterPrefs(pref_registry.get());

#if BUILDFLAG(IS_IOS)
  pref_registry->RegisterBooleanPref(kJITEnabled, true);
#endif

  base::FilePath path;
  CHECK(base::PathService::Get(SHELL_DIR_USER_DATA, &path));
  path = path.AppendASCII("Local State");

  PrefServiceFactory pref_service_factory;
  pref_service_factory.set_user_prefs(
      base::MakeRefCounted<JsonPrefStore>(path));

  return pref_service_factory.Create(pref_registry);
}

bool AreIsolatedWebAppsEnabled() {
  return base::FeatureList::IsEnabled(features::kIsolatedWebApps);
}

}  // namespace

std::string GetShellLanguage() {
  return "en-us,en";
}

blink::UserAgentMetadata GetShellUserAgentMetadata() {
  blink::UserAgentMetadata metadata;

  metadata.brand_version_list.emplace_back("content_shell",
                                           CONTENT_SHELL_MAJOR_VERSION);
  metadata.brand_full_version_list.emplace_back("content_shell",
                                                CONTENT_SHELL_VERSION);
  metadata.full_version = CONTENT_SHELL_VERSION;
  metadata.platform = "Unknown";
  metadata.architecture = embedder_support::GetCpuArchitecture();
  metadata.model = embedder_support::BuildModelInfo();

  metadata.bitness = embedder_support::GetCpuBitness();
  metadata.wow64 = embedder_support::IsWoW64();
  metadata.form_factors = {"Desktop"};

  return metadata;
}

ShellContentBrowserClient* ShellContentBrowserClient::Get() {
  auto& instances = GetShellContentBrowserClientInstancesImpl();
  return instances.empty() ? nullptr : instances.back();
}

ShellContentBrowserClient::ShellContentBrowserClient() {
  GetShellContentBrowserClientInstancesImpl().push_back(this);
}

ShellContentBrowserClient::~ShellContentBrowserClient() {
  std::erase(GetShellContentBrowserClientInstancesImpl(), this);
}

std::unique_ptr<BrowserMainParts>
ShellContentBrowserClient::CreateBrowserMainParts(
    bool /* is_integration_test */) {
  auto browser_main_parts = std::make_unique<ShellBrowserMainParts>();
  DCHECK(!GetSharedState().shell_browser_main_parts);
  GetSharedState().shell_browser_main_parts = browser_main_parts.get();
  return browser_main_parts;
}

bool ShellContentBrowserClient::HasCustomSchemeHandler(
    content::BrowserContext* browser_context,
    const std::string& scheme) {
  if (custom_handlers::ProtocolHandlerRegistry* protocol_handler_registry =
          custom_handlers::SimpleProtocolHandlerRegistryFactory::
              GetForBrowserContext(browser_context)) {
    return protocol_handler_registry->IsHandledProtocol(scheme);
  }
  return false;
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
ShellContentBrowserClient::CreateURLLoaderThrottles(
    const network::ResourceRequest& request,
    BrowserContext* browser_context,
    const base::RepeatingCallback<WebContents*()>& wc_getter,
    NavigationUIData* navigation_ui_data,
    FrameTreeNodeId frame_tree_node_id,
    std::optional<int64_t> navigation_id) {
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> result;

  auto* factory = custom_handlers::SimpleProtocolHandlerRegistryFactory::
      GetForBrowserContext(browser_context);
  // null in unit tests.
  if (factory) {
    result.push_back(
        std::make_unique<custom_handlers::ProtocolHandlerThrottle>(*factory));
  }

  return result;
}

bool ShellContentBrowserClient::IsHandledURL(const GURL& url) {
  if (!url.is_valid()) {
    return false;
  }
  constexpr auto kProtocolList = base::MakeFixedFlatSet<std::string_view>({
      url::kHttpScheme,
      url::kHttpsScheme,
      url::kWsScheme,
      url::kWssScheme,
      url::kBlobScheme,
      url::kFileSystemScheme,
      kChromeUIScheme,
      kChromeUIUntrustedScheme,
      kChromeDevToolsScheme,
      url::kDataScheme,
      url::kFileScheme,
  });

  return kProtocolList.contains(url.scheme());
}

bool ShellContentBrowserClient::AreIsolatedWebAppsEnabled(
    BrowserContext* browser_context) {
  return ::content::AreIsolatedWebAppsEnabled();
}

void ShellContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  static const char* const kForwardSwitches[] = {
#if BUILDFLAG(IS_MAC)
      // Needed since on Mac, content_browsertests doesn't use
      // content_test_launcher.cc and instead uses shell_main.cc. So give a
      // signal
      // to shell_main.cc that it's a browser test.
      switches::kBrowserTest,
#endif
      switches::kCrashDumpsDir,
      switches::kEnableCrashReporter,
      switches::kExposeInternalsForTesting,
      switches::kRunWebTests,
      switches::kTestRegisterStandardScheme,
  };

  command_line->CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                                 kForwardSwitches);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableCrashReporter)) {
    int fd;
    pid_t pid;
    if (crash_reporter::GetHandlerSocket(&fd, &pid)) {
      command_line->AppendSwitchASCII(
          crash_reporter::switches::kCrashpadHandlerPid,
          base::NumberToString(pid));
    }
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  if (command_line->GetSwitchValueASCII(switches::kProcessType) ==
          switches::kRendererProcess &&
      ::content::AreIsolatedWebAppsEnabled()) {
    command_line->AppendSwitch(switches::kEnableIsolatedWebAppsInRenderer);
  }

#if BUILDFLAG(IS_IOS)
  if (command_line->GetSwitchValueASCII(switches::kProcessType) ==
          switches::kRendererProcess &&
      !IsJITEnabled()) {
    command_line->AppendSwitchASCII(blink::switches::kJavaScriptFlags,
                                    "--jitless");
  }
#endif
}

#if BUILDFLAG(IS_IOS)
bool ShellContentBrowserClient::IsJITEnabled() {
  return GetSharedState().local_state->GetBoolean(kJITEnabled);
}

void ShellContentBrowserClient::SetJITEnabled(bool value) {
  GetSharedState().local_state->SetBoolean(kJITEnabled, value);
  GetSharedState().local_state->CommitPendingWrite();
}
#endif

device::GeolocationSystemPermissionManager*
ShellContentBrowserClient::GetGeolocationSystemPermissionManager() {
#if BUILDFLAG(IS_MAC)
  return GetSharedState().location_manager.get();
#elif BUILDFLAG(IS_IOS)
  // TODO(crbug.com/1431447, 1411704): Unify this to
  // FakeGeolocationSystemPermissionManager once exploring browser features in
  // ContentShell on iOS is done.
  return GetSharedState()
      .shell_browser_main_parts->GetGeolocationSystemPermissionManager();
#else
  return nullptr;
#endif
}

std::string ShellContentBrowserClient::GetAcceptLangs(BrowserContext* context) {
  return GetShellLanguage();
}

std::string ShellContentBrowserClient::GetDefaultDownloadName() {
  return "download";
}

std::unique_ptr<WebContentsViewDelegate>
ShellContentBrowserClient::GetWebContentsViewDelegate(
    WebContents* web_contents) {
  return CreateShellWebContentsViewDelegate(web_contents);
}

bool ShellContentBrowserClient::IsIsolatedContextAllowedForUrl(
    BrowserContext* browser_context,
    const GURL& lock_url) {
  static base::flat_set<url::Origin> isolated_context_origins =
      GetIsolatedContextOriginSetFromFlag();
  return isolated_context_origins.contains(url::Origin::Create(lock_url));
}

bool ShellContentBrowserClient::IsSharedStorageAllowed(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* rfh,
    const url::Origin& top_frame_origin,
    const url::Origin& accessing_origin,
    std::string* out_debug_message,
    bool* out_block_is_site_setting_specific) {
  return true;
}

bool ShellContentBrowserClient::IsSharedStorageSelectURLAllowed(
    content::BrowserContext* browser_context,
    const url::Origin& top_frame_origin,
    const url::Origin& accessing_origin,
    std::string* out_debug_message,
    bool* out_block_is_site_setting_specific) {
  return true;
}

bool ShellContentBrowserClient::IsFencedStorageReadAllowed(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* rfh,
    const url::Origin& top_frame_origin,
    const url::Origin& accessing_origin) {
  return true;
}

bool ShellContentBrowserClient::IsCookieDeprecationLabelAllowed(
    content::BrowserContext* browser_context) {
  return true;
}

bool ShellContentBrowserClient::IsCookieDeprecationLabelAllowedForContext(
    content::BrowserContext* browser_context,
    const url::Origin& top_frame_origin,
    const url::Origin& context_origin) {
  return true;
}

GeneratedCodeCacheSettings
ShellContentBrowserClient::GetGeneratedCodeCacheSettings(
    content::BrowserContext* context) {
  // If we pass 0 for size, disk_cache will pick a default size using the
  // heuristics based on available disk size. These are implemented in
  // disk_cache::PreferredCacheSize in net/disk_cache/cache_util.cc.
  return GeneratedCodeCacheSettings(true, 0, context->GetPath());
}

base::OnceClosure ShellContentBrowserClient::SelectClientCertificate(
    BrowserContext* browser_context,
    int process_id,
    WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<ClientCertificateDelegate> delegate) {
  if (select_client_certificate_callback_ && web_contents) {
    return std::move(select_client_certificate_callback_)
        .Run(web_contents, cert_request_info, std::move(client_certs),
             std::move(delegate));
  }
  return base::OnceClosure();
}

SpeechRecognitionManagerDelegate*
ShellContentBrowserClient::CreateSpeechRecognitionManagerDelegate() {
  return new ShellSpeechRecognitionManagerDelegate();
}

void ShellContentBrowserClient::OverrideWebPreferences(
    WebContents* web_contents,
    SiteInstance& main_frame_site,
    blink::web_pref::WebPreferences* prefs) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceDarkMode)) {
    prefs->preferred_color_scheme = blink::mojom::PreferredColorScheme::kDark;
  } else {
    prefs->preferred_color_scheme = blink::mojom::PreferredColorScheme::kLight;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceHighContrast)) {
    prefs->in_forced_colors = true;
    prefs->preferred_contrast = blink::mojom::PreferredContrast::kMore;
  } else {
    prefs->in_forced_colors = false;
    prefs->preferred_contrast = blink::mojom::PreferredContrast::kNoPreference;
  }
}

std::unique_ptr<content::DevToolsManagerDelegate>
ShellContentBrowserClient::CreateDevToolsManagerDelegate() {
  return std::make_unique<ShellDevToolsManagerDelegate>(browser_context());
}

void ShellContentBrowserClient::ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    RenderProcessHost* render_process_host) {
  PerformanceManagerRegistry::GetInstance()->CreateProcessNode(
      render_process_host);
  PerformanceManagerRegistry::GetInstance()
      ->GetBinders()
      .ExposeInterfacesToRendererProcess(registry, render_process_host);
}

void ShellContentBrowserClient::ExposeInterfacesToChild(
    mojo::BinderMapWithContext<content::BrowserChildProcessHost*>* map) {
  PerformanceManagerRegistry::GetInstance()
      ->GetBinders()
      .ExposeInterfacesToBrowserChildProcess(map);
}

mojo::Remote<::media::mojom::MediaService>
ShellContentBrowserClient::RunSecondaryMediaService() {
  mojo::Remote<::media::mojom::MediaService> remote;
#if BUILDFLAG(ENABLE_CAST_RENDERER)
  static base::SequenceLocalStorageSlot<std::unique_ptr<::media::MediaService>>
      service;
  service.emplace(::media::CreateMediaServiceForTesting(
      remote.BindNewPipeAndPassReceiver()));
#endif
  return remote;
}

void ShellContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<RenderFrameHost*>* map) {
  PerformanceManagerRegistry::GetInstance()
      ->GetBinders()
      .ExposeInterfacesToRenderFrame(map);
  map->Add<network_hints::mojom::NetworkHintsHandler>(&BindNetworkHintsHandler);
#if BUILDFLAG(IS_WIN)
  map->Add<media::mojom::MediaFoundationPreferences>(
      &BindMediaFoundationPreferences);
#endif  // BUILDFLAG(IS_WIN)
}

void ShellContentBrowserClient::OpenURL(
    SiteInstance* site_instance,
    const OpenURLParams& params,
    base::OnceCallback<void(WebContents*)> callback) {
  std::move(callback).Run(
      Shell::CreateNewWindow(site_instance->GetBrowserContext(), params.url,
                             nullptr, gfx::Size())
          ->web_contents());
}

void ShellContentBrowserClient::CreateThrottlesForNavigation(
    NavigationThrottleRegistry& registry) {
  if (create_throttles_for_navigation_callback_) {
    create_throttles_for_navigation_callback_.Run(registry);
  }
}

std::unique_ptr<LoginDelegate> ShellContentBrowserClient::CreateLoginDelegate(
    const net::AuthChallengeInfo& auth_info,
    WebContents* web_contents,
    BrowserContext* browser_context,
    const GlobalRequestID& request_id,
    bool is_request_for_primary_main_frame_navigation,
    bool is_request_for_navigation,
    const GURL& url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    bool first_auth_attempt,
    GuestPageHolder* guest,
    LoginDelegate::LoginAuthRequiredCallback auth_required_callback) {
  if (!login_request_callback_.is_null()) {
    std::move(login_request_callback_)
        .Run(is_request_for_primary_main_frame_navigation,
             is_request_for_navigation);
  }
  return nullptr;
}

base::Value::Dict ShellContentBrowserClient::GetNetLogConstants() {
  base::Value::Dict client_constants;
  client_constants.Set("name", "content_shell");
  base::CommandLine::StringType command_line =
      base::CommandLine::ForCurrentProcess()->GetCommandLineString();
#if BUILDFLAG(IS_WIN)
  client_constants.Set("command_line", base::WideToUTF8(command_line));
#else
  client_constants.Set("command_line", command_line);
#endif
  base::Value::Dict constants;
  constants.Set("clientInfo", std::move(client_constants));
  return constants;
}

base::FilePath
ShellContentBrowserClient::GetSandboxedStorageServiceDataDirectory() {
  return browser_context()->GetPath();
}

base::FilePath ShellContentBrowserClient::GetFirstPartySetsDirectory() {
  return browser_context()->GetPath();
}

std::optional<base::FilePath>
ShellContentBrowserClient::GetLocalTracesDirectory() {
  return browser_context()->GetPath();
}

std::string ShellContentBrowserClient::GetUserAgent() {
  const auto custom_ua = embedder_support::GetUserAgentFromCommandLine();
  if (custom_ua.has_value()) {
    return custom_ua.value();
  }

  std::string product =
      base::StringPrintf("Chrome/%s.0.0.0", CONTENT_SHELL_MAJOR_VERSION);
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          embedder_support::kUseMobileUserAgent)) {
    product += " Mobile";
  }
#endif

  return embedder_support::BuildUnifiedPlatformUserAgentFromProduct(product);
}

blink::UserAgentMetadata ShellContentBrowserClient::GetUserAgentMetadata() {
  return GetShellUserAgentMetadata();
}

void ShellContentBrowserClient::OverrideURLLoaderFactoryParams(
    BrowserContext* browser_context,
    const url::Origin& origin,
    bool is_for_isolated_world,
    bool is_for_service_worker,
    network::mojom::URLLoaderFactoryParams* factory_params) {
  if (url_loader_factory_params_callback_) {
    url_loader_factory_params_callback_.Run(
        factory_params, origin, is_for_isolated_world, is_for_service_worker);
  }
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
void ShellContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    content::PosixFileDescriptorInfo* mappings) {
#if BUILDFLAG(IS_ANDROID)
  mappings->ShareWithRegion(
      kShellPakDescriptor,
      base::GlobalDescriptors::GetInstance()->Get(kShellPakDescriptor),
      base::GlobalDescriptors::GetInstance()->GetRegion(kShellPakDescriptor));
#endif
  int crash_signal_fd = GetCrashSignalFD(command_line);
  if (crash_signal_fd >= 0) {
    mappings->Share(kCrashDumpSignal, crash_signal_fd);
  }
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)

// Note that ShellContentBrowserClient overrides this method to work around
// test flakiness that happens when NetworkService::SetTestDohConfigForTesting()
// is used.
// TODO(crbug.com/41494161): Remove that override once the flakiness is fixed.
void ShellContentBrowserClient::OnNetworkServiceCreated(
    network::mojom::NetworkService* network_service) {
  // TODO(bashi): Consider enabling this for Android. Excluded because the
  // built-in resolver may not work on older SDK versions.
#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(net::features::kAsyncDns)) {
    network_service->ConfigureStubHostResolver(
        /*insecure_dns_client_enabled=*/true,
        base::FeatureList::IsEnabled(net::features::kHappyEyeballsV3),
        /*secure_dns_mode=*/net::SecureDnsMode::kAutomatic,
        net::DnsOverHttpsConfig(),
        /*additional_dns_types_enabled=*/true,
        /*fallback_doh_nameservers=*/{});
  }
#endif
}

void ShellContentBrowserClient::ConfigureNetworkContextParams(
    BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* network_context_params,
    cert_verifier::mojom::CertVerifierCreationParams*
        cert_verifier_creation_params) {
  ConfigureNetworkContextParamsForShell(context, network_context_params,
                                        cert_verifier_creation_params);
}

std::vector<base::FilePath>
ShellContentBrowserClient::GetNetworkContextsParentDirectory() {
  return {browser_context()->GetPath()};
}

#if BUILDFLAG(IS_IOS)
BluetoothDelegate* ShellContentBrowserClient::GetBluetoothDelegate() {
  if (!bluetooth_delegate_) {
    bluetooth_delegate_ = std::make_unique<permissions::BluetoothDelegateImpl>(
        std::make_unique<ShellBluetoothDelegateImplClient>());
  }
  return bluetooth_delegate_.get();
}
#endif

void ShellContentBrowserClient::BindBrowserControlInterface(
    mojo::ScopedMessagePipeHandle pipe) {
  if (!pipe.is_valid()) {
    return;
  }
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ShellControllerImpl>(),
      mojo::PendingReceiver<mojom::ShellController>(std::move(pipe)));
}

void ShellContentBrowserClient::set_browser_main_parts(
    ShellBrowserMainParts* parts) {
  GetSharedState().shell_browser_main_parts = parts;
}

ShellBrowserContext* ShellContentBrowserClient::browser_context() {
  return GetSharedState().shell_browser_main_parts->browser_context();
}

ShellBrowserContext*
ShellContentBrowserClient::off_the_record_browser_context() {
  return GetSharedState()
      .shell_browser_main_parts->off_the_record_browser_context();
}

ShellBrowserMainParts* ShellContentBrowserClient::shell_browser_main_parts() {
  return GetSharedState().shell_browser_main_parts;
}

void ShellContentBrowserClient::ConfigureNetworkContextParamsForShell(
    BrowserContext* context,
    network::mojom::NetworkContextParams* context_params,
    cert_verifier::mojom::CertVerifierCreationParams*
        cert_verifier_creation_params) {
  context_params->user_agent = GetUserAgent();
  context_params->accept_language = GetAcceptLangs(context);
  context_params->enable_zstd = true;
  auto exempt_header =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          "cors_exempt_header_list");
  if (!exempt_header.empty()) {
    context_params->cors_exempt_header_list.push_back(exempt_header);
  }
  context_params->device_bound_sessions_enabled =
      base::FeatureList::IsEnabled(net::features::kDeviceBoundSessions);
}

void ShellContentBrowserClient::GetHyphenationDictionary(
    base::OnceCallback<void(const base::FilePath&)> callback) {
  // If we have the source tree, return the dictionary files in the tree.
  base::FilePath dir;
  if (base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &dir)) {
    dir = dir.AppendASCII("third_party")
              .AppendASCII("hyphenation-patterns")
              .AppendASCII("hyb");
    std::move(callback).Run(dir);
  }
  // No need to callback if there were no dictionaries.
}

bool ShellContentBrowserClient::HasErrorPage(int http_status_code) {
  return http_status_code >= 400 && http_status_code < 600;
}

void ShellContentBrowserClient::OnWebContentsCreated(
    WebContents* web_contents) {
  PerformanceManagerRegistry::GetInstance()->MaybeCreatePageNodeForWebContents(
      web_contents);
}

void ShellContentBrowserClient::CreateFeatureListAndFieldTrials() {
  GetSharedState().local_state = CreateLocalState();
  SetupFieldTrials();
}

std::optional<network::ParsedPermissionsPolicy>
ShellContentBrowserClient::GetPermissionsPolicyForIsolatedWebApp(
    WebContents* web_contents,
    const url::Origin& app_origin) {
  network::ParsedPermissionsPolicyDeclaration coi_decl(
      network::mojom::PermissionsPolicyFeature::kCrossOriginIsolated,
      /*allowed_origins=*/{},
      /*self_if_matches=*/std::nullopt,
      /*matches_all_origins=*/true, /*matches_opaque_src=*/false);

  network::ParsedPermissionsPolicyDeclaration socket_decl(
      network::mojom::PermissionsPolicyFeature::kDirectSockets,
      /*allowed_origins=*/{}, app_origin,
      /*matches_all_origins=*/false, /*matches_opaque_src=*/false);
  return {{coi_decl, socket_decl}};
}

// Tests may install their own ShellContentBrowserClient, track the list here.
// The list is ordered with oldest first and newer ones added after it.
// static
const std::vector<ShellContentBrowserClient*>&
ShellContentBrowserClient::GetShellContentBrowserClientInstances() {
  return GetShellContentBrowserClientInstancesImpl();
}

}  // namespace content
