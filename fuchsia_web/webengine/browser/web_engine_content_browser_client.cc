// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/web_engine_content_browser_client.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/i18n/rtl.h"
#include "base/strings/string_split.h"
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
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom.h"
#include "third_party/widevine/cdm/buildflags.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

constexpr net::NetworkTrafficAnnotationTag kProxyConfigTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("webview_proxy_config", R"(
      semantics {
        sender: "Proxy configuration via a command line flag"
        description:
          "Used to fetch HTTP/HTTPS/SOCKS5/PAC proxy configuration when "
          "proxy is configured by the --proxy-server command line flag. "
          "When proxy implies automatic configuration, it can send network "
          "requests in the scope of this annotation."
        trigger:
          "Whenever a network request is made when the system proxy settings "
          "are used, and they indicate to use a proxy server."
        data:
          "Proxy configuration."
        destination: OTHER
        destination_other: "The proxy server specified in the configuration."
      }
      policy {
        cookies_allowed: NO
        setting:
          "This request cannot be disabled in settings. However it will never "
          "be made if user does not run with the '--proxy-server' switch."
        policy_exception_justification:
          "Not implemented, behaviour only available behind a switch."
      })");

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
  content::DevToolsAgentHost::List RemoteDebuggingTargets(
      DevToolsManagerDelegate::TargetType target_type) override {
    LOG_IF(WARNING, target_type != DevToolsManagerDelegate::kFrame)
        << "Ignoring unsupported remote target type: " << target_type;
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
    switches::kForceProtectedVideoOutputBuffers,
    switches::kMinVideoDecoderOutputBufferSize,

// TODO(crbug.com/42050020): Delete these two switches when fixed.
#if BUILDFLAG(ENABLE_WIDEVINE)
    switches::kEnableWidevine,
#if BUILDFLAG(ENABLE_CAST_RECEIVER)
    switches::kPlayreadyKeySystem,
#endif
#endif

    // Pass to the renderer process for consistency with Chrome.
    network::switches::kUnsafelyTreatInsecureOriginAsSecure,
};

static constexpr char const*
    kUnsafelyTreatInsecureOriginAsSecureSwitchToCopy[] = {
        network::switches::kUnsafelyTreatInsecureOriginAsSecure,
};

// These are passed to every child process and should only be used when it is
// not possible to narrow the scope down to a subset of processes.
static constexpr char const* kAllProcessSwitchesToCopy[] = {
    // This is used by every child process in WebEngineContentClient.
    switches::kEnableContentDirectories,
};

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
  return std::string(version_info::GetProductNameAndVersionForUserAgent());
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

  // TODO(crbug.com/40245916): Remove once supported in WebEngine.
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

mojo::PendingRemote<network::mojom::URLLoaderFactory>
WebEngineContentBrowserClient::CreateNonNetworkNavigationURLLoaderFactory(
    const std::string& scheme,
    content::FrameTreeNodeId frame_tree_node_id) {
  if (scheme == kFuchsiaDirScheme) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableContentDirectories)) {
      return ContentDirectoryLoaderFactory::Create();
    }
  }
  return {};
}

void WebEngineContentBrowserClient::
    RegisterNonNetworkSubresourceURLLoaderFactories(
        int render_process_id,
        int render_frame_id,
        const std::optional<url::Origin>& request_initiator_origin,
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
                                 kAllProcessSwitchesToCopy);

  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);

  if (process_type == switches::kRendererProcess) {
    command_line->CopySwitchesFrom(browser_command_line,
                                   kRendererSwitchesToCopy);
  } else if (process_type == switches::kUtilityProcess) {
    // Although only the Network process needs
    // kUnsafelyTreatInsecureOriginAsSecureSwitchToCopy, differentiating utility
    // process sub-types is non-trivial. ChromeContentBrowserClient appends this
    // switch to all Utility processes so do the same here.
    // Do not add other switches here.
    command_line->CopySwitchesFrom(
        browser_command_line, kUnsafelyTreatInsecureOriginAsSecureSwitchToCopy);
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
    content::BrowserContext* browser_context,
    int process_id,
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

  const std::optional<std::string>& explicit_sites_filter_error_page =
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
    content::FrameTreeNodeId frame_tree_node_id,
    std::optional<int64_t> navigation_id) {
  if (frame_tree_node_id.is_null()) {
    // TODO(crbug.com/40244093): Add support for Shared and Service Workers.
    return {};
  }

  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;
  auto* frame_impl = FrameImpl::FromWebContents(wc_getter.Run());
  DCHECK(frame_impl);
  auto rules =
      frame_impl->url_request_rewrite_rules_manager()->GetCachedRules();
  if (rules) {
    throttles.emplace_back(std::make_unique<url_rewrite::URLLoaderThrottle>(
        rules, base::BindRepeating(&IsHeaderCorsExempt)));
  }
  return throttles;
}

void WebEngineContentBrowserClient::ConfigureNetworkContextParams(
    content::BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* network_context_params,
    cert_verifier::mojom::CertVerifierCreationParams*
        cert_verifier_creation_params) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string proxy = command_line.GetSwitchValueASCII(switches::kProxyServer);
  if (!proxy.empty()) {
    net::ProxyConfig proxy_config;
    proxy_config.proxy_rules().ParseFromString(proxy);
    std::string bypass_list =
        command_line.GetSwitchValueASCII(switches::kProxyBypassList);
    if (!bypass_list.empty()) {
      proxy_config.proxy_rules().bypass_rules.ParseFromString(bypass_list);
    }

    network_context_params->initial_proxy_config =
        net::ProxyConfigWithAnnotation(proxy_config,
                                       kProxyConfigTrafficAnnotation);
  }

  network_context_params->user_agent = GetUserAgent();
  network_context_params->accept_language =
      net::HttpUtil::GenerateAcceptLanguageHeader(GetAcceptLangs(context));

  // Set the list of cors_exempt_headers which may be specified in a URLRequest,
  // starting with the headers passed in via
  // |CreateContextParams.cors_exempt_headers|.
  network_context_params->cors_exempt_header_list = cors_exempt_headers_;
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
