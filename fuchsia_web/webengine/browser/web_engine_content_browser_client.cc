// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/web_engine_content_browser_client.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/fuchsia/file_utils.h"
#include "base/functional/callback.h"
#include "base/i18n/rtl.h"
#include "base/strings/string_split.h"
#include "base/threading/thread_restrictions.h"
#include "build/chromecast_buildflags.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/policy/content/safe_sites_navigation_throttle.h"
#include "components/site_isolation/features.h"
#include "components/site_isolation/preloaded_isolated_origins.h"
#include "components/strings/grit/components_locale_settings.h"
#include "components/url_rewrite/common/url_loader_throttle.h"
#include "components/url_rewrite/common/url_request_rewrite_rules.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_switches.h"
#include "fuchsia_web/common/fuchsia_dir_scheme.h"
#include "fuchsia_web/common/init_logging.h"
#include "fuchsia_web/webengine/browser/frame_impl.h"
#include "fuchsia_web/webengine/browser/navigation_policy_throttle.h"
#include "fuchsia_web/webengine/browser/web_engine_browser_context.h"
#include "fuchsia_web/webengine/browser/web_engine_browser_interface_binders.h"
#include "fuchsia_web/webengine/browser/web_engine_browser_main_parts.h"
#include "fuchsia_web/webengine/browser/web_engine_devtools_controller.h"
#include "fuchsia_web/webengine/common/cors_exempt_headers.h"
#include "fuchsia_web/webengine/common/web_engine_content_client.h"
#include "fuchsia_web/webengine/switches.h"
#include "media/base/media_switches.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_util.h"
#include "net/ssl/ssl_private_key.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom.h"
#include "third_party/widevine/cdm/buildflags.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
class DevToolsManagerDelegate final : public content::DevToolsManagerDelegate {
 public:
  explicit DevToolsManagerDelegate(WebEngineBrowserMainParts* main_parts)
      : main_parts_(main_parts) {
    DCHECK(main_parts_);
  }
  ~DevToolsManagerDelegate() override = default;

  DevToolsManagerDelegate(const DevToolsManagerDelegate&) = delete;
  DevToolsManagerDelegate& operator=(const DevToolsManagerDelegate&) = delete;

  // content::DevToolsManagerDelegate implementation.
  std::vector<content::BrowserContext*> GetBrowserContexts() override {
    return main_parts_->browser_contexts();
  }
  content::BrowserContext* GetDefaultBrowserContext() override {
    std::vector<content::BrowserContext*> contexts = GetBrowserContexts();
    return contexts.empty() ? nullptr : contexts.front();
  }
  content::DevToolsAgentHost::List RemoteDebuggingTargets() override {
    return main_parts_->devtools_controller()->RemoteDebuggingTargets();
  }

 private:
  WebEngineBrowserMainParts* const main_parts_;
};

std::vector<std::string> GetCorsExemptHeaders() {
  return base::SplitString(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          switches::kCorsExemptHeaders),
      ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}

static constexpr char const* kRendererSwitchesToCopy[] = {
    blink::switches::kSharedArrayBufferAllowedOrigins,
    switches::kCorsExemptHeaders,
    switches::kEnableCastStreamingReceiver,
    switches::kEnableProtectedVideoBuffers,
    switches::kUseOverlaysForVideo,
    switches::kMinVideoDecoderOutputBufferSize,

// TODO(crbug/1013412): Delete these two switches when fixed.
#if BUILDFLAG(ENABLE_WIDEVINE)
    switches::kEnableWidevine,
#if BUILDFLAG(ENABLE_CAST_RECEIVER)
    switches::kPlayreadyKeySystem,
#endif
#endif

    // Pass to the renderer process for consistency with Chrome.
    network::switches::kUnsafelyTreatInsecureOriginAsSecure,
};

static constexpr char const* kUtilitySwitchesToCopy[] = {
    network::switches::kUnsafelyTreatInsecureOriginAsSecure,
    switches::kDataQuotaBytes,
};

// These are passed to every child process and should only be used when it is
// not possible to narrow the scope down to a subset of processes.
static constexpr char const* kAllProcessSwitchesToCopy[] = {
    // This is used by every child process in WebEngineContentClient.
    switches::kEnableContentDirectories,
};

// Returns an absolute path to a storage subdirectory specific to a
// browser instance, hosted under `base_path`.
// If `relative_partition_path` is empty (i.e. for the root partition), then the
// subpath "Default" will be used instead.
base::FilePath GetStoragePath(const base::FilePath& base_path,
                              const base::FilePath& relative_partition_path) {
  DCHECK(base_path.IsAbsolute());

  base::FilePath partition_path =
      base_path.Append(relative_partition_path.empty()
                           ? base::FilePath(FILE_PATH_LITERAL("Default"))
                           : relative_partition_path);
  if (!base::CreateDirectory(partition_path)) {
    return {};
  }
  return partition_path;
}

// Convenience method for reading a size_t from the commandline args.
// Returns nullopt if the switch is unset or invalid.
absl::optional<size_t> GetSwitchAsSize(base::StringPiece switch_name) {
  std::string str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(switch_name);
  size_t value;
  if (str.empty()) {
    return absl::nullopt;
  }
  if (!base::StringToSizeT(str, &value)) {
    LOG(FATAL) << "Invalid size switch --" << switch_name << ".";
    return absl::nullopt;
  }
  return value;
}

}  // namespace

WebEngineContentBrowserClient::WebEngineContentBrowserClient()
    : cors_exempt_headers_(GetCorsExemptHeaders()) {
  // Logging in this class ensures this is logged once per web_instance.
  LogComponentStartWithVersion("WebEngine web_instance");
}

WebEngineContentBrowserClient::~WebEngineContentBrowserClient() = default;

std::unique_ptr<content::BrowserMainParts>
WebEngineContentBrowserClient::CreateBrowserMainParts(
    bool /* is_integration_test */) {
  auto browser_main_parts = std::make_unique<WebEngineBrowserMainParts>(this);
  main_parts_ = browser_main_parts.get();
  return browser_main_parts;
}

std::unique_ptr<content::DevToolsManagerDelegate>
WebEngineContentBrowserClient::CreateDevToolsManagerDelegate() {
  DCHECK(main_parts_);
  return std::make_unique<DevToolsManagerDelegate>(main_parts_);
}

std::string WebEngineContentBrowserClient::GetProduct() {
  return version_info::GetProductNameAndVersionForUserAgent();
}

std::string WebEngineContentBrowserClient::GetUserAgent() {
  std::string user_agent = embedder_support::GetUserAgent();
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUserAgentProductAndVersion)) {
    user_agent +=
        " " + base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
                  switches::kUserAgentProductAndVersion);
  }
  return user_agent;
}

blink::UserAgentMetadata WebEngineContentBrowserClient::GetUserAgentMetadata() {
  return embedder_support::GetUserAgentMetadata();
}

void WebEngineContentBrowserClient::OverrideWebkitPrefs(
    content::WebContents* web_contents,
    blink::web_pref::WebPreferences* web_prefs) {
  // Disable WebSQL support since it is being removed from the web platform
  // and does not work. See crbug.com/1317431.
  web_prefs->databases_enabled = false;

  // TODO(crbug.com/1382970): Remove once supported in WebEngine.
  web_prefs->disable_webauthn = true;

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
  static bool allow_insecure_content =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAllowRunningInsecureContent);
  if (allow_insecure_content) {
    web_prefs->allow_running_insecure_content = true;
  }
#endif

  FrameImpl* frame = FrameImpl::FromWebContents(web_contents);
  // This method may be called when a |web_contents| is instantiated but an
  // associated frame has not been created.
  if (frame != nullptr) {
    frame->OverrideWebPreferences(web_prefs);
  }
}

void WebEngineContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  PopulateFuchsiaFrameBinders(map);
}

void WebEngineContentBrowserClient::
    RegisterNonNetworkNavigationURLLoaderFactories(
        int frame_tree_node_id,
        ukm::SourceIdObj ukm_source_id,
        NonNetworkURLLoaderFactoryMap* factories) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableContentDirectories)) {
    factories->emplace(kFuchsiaDirScheme,
                       ContentDirectoryLoaderFactory::Create());
  }
}

void WebEngineContentBrowserClient::
    RegisterNonNetworkSubresourceURLLoaderFactories(
        int render_process_id,
        int render_frame_id,
        const absl::optional<url::Origin>& request_initiator_origin,
        NonNetworkURLLoaderFactoryMap* factories) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableContentDirectories)) {
    factories->emplace(kFuchsiaDirScheme,
                       ContentDirectoryLoaderFactory::Create());
  }
}

bool WebEngineContentBrowserClient::ShouldEnableStrictSiteIsolation() {
  static BASE_FEATURE(kSitePerProcess, "site-per-process",
                      base::FEATURE_ENABLED_BY_DEFAULT);
  static bool enable_strict_isolation =
      base::FeatureList::IsEnabled(kSitePerProcess);
  return enable_strict_isolation;
}

void WebEngineContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();

  command_line->CopySwitchesFrom(browser_command_line,
                                 kAllProcessSwitchesToCopy,
                                 std::size(kAllProcessSwitchesToCopy));

  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);

  if (process_type == switches::kRendererProcess) {
    command_line->CopySwitchesFrom(browser_command_line,
                                   kRendererSwitchesToCopy,
                                   std::size(kRendererSwitchesToCopy));
  } else if (process_type == switches::kUtilityProcess) {
    // Although only the Network process needs
    // kUnsafelyTreatInsecureOriginAsSecureSwitchToCopy and kDataQuotaBytes,
    // differentiating utility process sub-types is non-trivial.
    // ChromeContentBrowserClient appends this switch to all Utility processes
    // so do the same here.
    command_line->CopySwitchesFrom(browser_command_line, kUtilitySwitchesToCopy,
                                   std::size(kUtilitySwitchesToCopy));
  }
}

std::string WebEngineContentBrowserClient::GetApplicationLocale() {
  // ICU is configured with the system locale by WebEngineBrowserMainParts.
  return base::i18n::GetConfiguredLocale();
}

std::string WebEngineContentBrowserClient::GetAcceptLangs(
    content::BrowserContext* context) {
  // Returns a comma-separated list of language codes, in preference order.
  // This is passed to net::HttpUtil::GenerateAcceptLanguageHeader() to
  // generate a legacy "accept-language" header value.
  return l10n_util::GetStringUTF8(IDS_ACCEPT_LANGUAGES);
}

base::OnceClosure WebEngineContentBrowserClient::SelectClientCertificate(
    content::WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<content::ClientCertificateDelegate> delegate) {
  // Continue without a certificate.
  delegate->ContinueWithCertificate(nullptr, nullptr);
  return base::OnceClosure();
}

std::vector<std::unique_ptr<content::NavigationThrottle>>
WebEngineContentBrowserClient::CreateThrottlesForNavigation(
    content::NavigationHandle* navigation_handle) {
  std::vector<std::unique_ptr<content::NavigationThrottle>> throttles;
  auto* frame_impl =
      FrameImpl::FromWebContents(navigation_handle->GetWebContents());
  DCHECK(frame_impl);

  // Only create throttle if FrameImpl has a NavigationPolicyProvider,
  // indicating an interest in navigations.
  if (frame_impl->navigation_policy_handler()) {
    throttles.push_back(std::make_unique<NavigationPolicyThrottle>(
        navigation_handle, frame_impl->navigation_policy_handler()));
  }

  const absl::optional<std::string>& explicit_sites_filter_error_page =
      frame_impl->explicit_sites_filter_error_page();

  if (explicit_sites_filter_error_page) {
    throttles.push_back(std::make_unique<SafeSitesNavigationThrottle>(
        navigation_handle,
        navigation_handle->GetWebContents()->GetBrowserContext(),
        *explicit_sites_filter_error_page));
  }

  return throttles;
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
WebEngineContentBrowserClient::CreateURLLoaderThrottles(
    const network::ResourceRequest& request,
    content::BrowserContext* browser_context,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    content::NavigationUIData* navigation_ui_data,
    int frame_tree_node_id) {
  if (frame_tree_node_id == content::RenderFrameHost::kNoFrameTreeNodeId) {
    // TODO(crbug.com/1378791): Add support for workers.
    return {};
  }

  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;
  auto* frame_impl = FrameImpl::FromWebContents(wc_getter.Run());
  DCHECK(frame_impl);
  const auto& rules =
      frame_impl->url_request_rewrite_rules_manager()->GetCachedRules();
  if (rules) {
    throttles.emplace_back(std::make_unique<url_rewrite::URLLoaderThrottle>(
        rules, base::BindRepeating(&IsHeaderCorsExempt)));
  }
  return throttles;
}

void WebEngineContentBrowserClient::MaybeConfigurePersistentData(
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* network_context_params) {
  if (!base::DirectoryExists(
          base::FilePath(base::kPersistedDataDirectoryPath))) {
    return;
  }
  // Filenames are copied from chrome/common/chrome_constants.cc
  // to maintain the convention established by Chrome.
  static const base::FilePath kHttpServerPropertiesFile(
      FILE_PATH_LITERAL("Network Persistent State"));
  static const base::FilePath kCookiesFile(FILE_PATH_LITERAL("Cookies"));
  static const base::FilePath kTrustTokenFile(
      FILE_PATH_LITERAL("Trust Tokens"));
  static const base::FilePath kTransportSecurityPersisterFile(
      FILE_PATH_LITERAL("TransportSecurity"));
  static const base::FilePath kSCTAuditingPendingReportsFileName(
      FILE_PATH_LITERAL("SCT Auditing Pending Reports"));

  auto data_path =
      GetStoragePath(base::FilePath(base::kPersistedDataDirectoryPath),
                     relative_partition_path);
  if (data_path.empty()) {
    return;
  }
  network_context_params->file_paths =
      ::network::mojom::NetworkContextFilePaths::New();
  network_context_params->file_paths->data_directory = data_path;

  // Configure the network process' database file locations under
  // the "Network" subdirectory of the browser /data subpartition.
  network_context_params->file_paths->http_server_properties_file_name =
      base::FilePath(kHttpServerPropertiesFile);
  network_context_params->file_paths->cookie_database_name =
      base::FilePath(kCookiesFile);
  network_context_params->file_paths->trust_token_database_name =
      base::FilePath(kTrustTokenFile);
  network_context_params->file_paths->transport_security_persister_file_name =
      base::FilePath(kTransportSecurityPersisterFile);
  network_context_params->file_paths->sct_auditing_pending_reports_file_name =
      base::FilePath(kSCTAuditingPendingReportsFileName);
}

void WebEngineContentBrowserClient::MaybeConfigureHttpCache(
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* network_context_params) {
  // If no flags are specified, then the browser default HTTP cache behavior
  // is used (in-memory; sized up to 2% of RAM).
  absl::optional<size_t> mem_cache_maxsize =
      GetSwitchAsSize(switches::kInMemoryHttpCacheSize);
  absl::optional<size_t> disk_cache_maxsize =
      GetSwitchAsSize(switches::kOnDiskHttpCacheSize);
  bool is_cache_dir_available =
      base::DirectoryExists(base::FilePath(base::kPersistedCacheDirectoryPath));
  if (!in_memory && is_cache_dir_available && disk_cache_maxsize &&
      *disk_cache_maxsize > 0u) {
    // Configure the on-disk HTTP cache if the browser context is non-OTR,
    // /cache is present, and a nonzero disk quota is set.
    network_context_params->http_cache_directory =
        GetStoragePath(base::FilePath(base::kPersistedCacheDirectoryPath),
                       relative_partition_path);
    network_context_params->http_cache_max_size = *disk_cache_maxsize;
  } else if (mem_cache_maxsize) {
    // Configure the in-memory HTTP cache.
    if (*mem_cache_maxsize == 0u) {
      // Disable the in-memory cache if the memory cache cap is set to zero.
      network_context_params->http_cache_enabled = false;
    } else {
      // Otherwise, if there is a RAM cache quota set, then an in-memory cache
      // can be used.
      network_context_params->http_cache_max_size = *mem_cache_maxsize;
    }
  }
}

void WebEngineContentBrowserClient::ConfigureNetworkContextParams(
    content::BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* network_context_params,
    cert_verifier::mojom::CertVerifierCreationParams*
        cert_verifier_creation_params) {
  network_context_params->user_agent = GetUserAgent();
  network_context_params->accept_language =
      net::HttpUtil::GenerateAcceptLanguageHeader(GetAcceptLangs(context));

  // Set the list of cors_exempt_headers which may be specified in a URLRequest,
  // starting with the headers passed in via
  // |CreateContextParams.cors_exempt_headers|.
  network_context_params->cors_exempt_header_list = cors_exempt_headers_;

  {
    base::ScopedAllowBlocking allow_blocking;
    if (!in_memory) {
      MaybeConfigurePersistentData(relative_partition_path,
                                   network_context_params);
    }
    MaybeConfigureHttpCache(in_memory, relative_partition_path,
                            network_context_params);
  }
}

std::vector<url::Origin>
WebEngineContentBrowserClient::GetOriginsRequiringDedicatedProcess() {
  std::vector<url::Origin> isolated_origin_list;

  // Include additional origins preloaded with specific browser configurations,
  // if any.
  auto built_in_origins =
      site_isolation::GetBrowserSpecificBuiltInIsolatedOrigins();
  std::move(std::begin(built_in_origins), std::end(built_in_origins),
            std::back_inserter(isolated_origin_list));

  return isolated_origin_list;
}
