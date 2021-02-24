// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/web_engine_content_browser_client.h"

#include <fuchsia/web/cpp/fidl.h>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "components/policy/content/safe_sites_navigation_throttle.h"
#include "components/site_isolation/features.h"
#include "components/site_isolation/preloaded_isolated_origins.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "fuchsia/base/fuchsia_dir_scheme.h"
#include "fuchsia/engine/browser/frame_impl.h"
#include "fuchsia/engine/browser/navigation_policy_throttle.h"
#include "fuchsia/engine/browser/url_request_rewrite_rules_manager.h"
#include "fuchsia/engine/browser/web_engine_browser_context.h"
#include "fuchsia/engine/browser/web_engine_browser_interface_binders.h"
#include "fuchsia/engine/browser/web_engine_browser_main_parts.h"
#include "fuchsia/engine/browser/web_engine_devtools_controller.h"
#include "fuchsia/engine/common/web_engine_content_client.h"
#include "fuchsia/engine/common/web_engine_url_loader_throttle.h"
#include "fuchsia/engine/switches.h"
#include "media/base/media_switches.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom.h"

namespace {

class DevToolsManagerDelegate : public content::DevToolsManagerDelegate {
 public:
  DevToolsManagerDelegate(content::BrowserContext* browser_context,
                          WebEngineDevToolsController* controller)
      : browser_context_(browser_context), controller_(controller) {
    DCHECK(browser_context_);
    DCHECK(controller_);
  }
  ~DevToolsManagerDelegate() final = default;

  // content::DevToolsManagerDelegate implementation.
  content::BrowserContext* GetDefaultBrowserContext() final {
    return browser_context_;
  }
  content::DevToolsAgentHost::List RemoteDebuggingTargets() final {
    return controller_->RemoteDebuggingTargets();
  }

 private:
  content::BrowserContext* const browser_context_;
  WebEngineDevToolsController* const controller_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsManagerDelegate);
};

std::vector<std::string> GetCorsExemptHeaders() {
  return base::SplitString(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          switches::kCorsExemptHeaders),
      ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}

}  // namespace

WebEngineContentBrowserClient::WebEngineContentBrowserClient(
    fidl::InterfaceRequest<fuchsia::web::Context> request)
    : request_(std::move(request)),
      cors_exempt_headers_(GetCorsExemptHeaders()),
      allow_insecure_content_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAllowRunningInsecureContent)) {}

WebEngineContentBrowserClient::~WebEngineContentBrowserClient() = default;

std::unique_ptr<content::BrowserMainParts>
WebEngineContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  DCHECK(request_);
  auto browser_main_parts = std::make_unique<WebEngineBrowserMainParts>(
      parameters, std::move(request_));

  main_parts_ = browser_main_parts.get();

  return browser_main_parts;
}

content::DevToolsManagerDelegate*
WebEngineContentBrowserClient::GetDevToolsManagerDelegate() {
  DCHECK(main_parts_);
  return new DevToolsManagerDelegate(main_parts_->browser_context(),
                                     main_parts_->devtools_controller());
}

std::string WebEngineContentBrowserClient::GetProduct() {
  return version_info::GetProductNameAndVersionForUserAgent();
}

std::string WebEngineContentBrowserClient::GetUserAgent() {
  std::string user_agent;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseLegacyAndroidUserAgent)) {
    user_agent =
        content::BuildUserAgentFromOSAndProduct("Linux; Android", GetProduct());
  } else {
    user_agent = content::BuildUserAgentFromProduct(GetProduct());
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUserAgentProductAndVersion)) {
    user_agent +=
        " " + base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
                  switches::kUserAgentProductAndVersion);
  }

  return user_agent;
}

void WebEngineContentBrowserClient::OverrideWebkitPrefs(
    content::RenderViewHost* rvh,
    blink::web_pref::WebPreferences* web_prefs) {
  // Disable WebSQL support since it's being removed from the web platform.
  web_prefs->databases_enabled = false;

  if (allow_insecure_content_)
    web_prefs->allow_running_insecure_content = true;
}

void WebEngineContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  MediaResourceProviderService* const provider =
      main_parts_->media_resource_provider_service();
  DCHECK(provider);
  PopulateFuchsiaFrameBinders(map, provider);
}

void WebEngineContentBrowserClient::
    RegisterNonNetworkNavigationURLLoaderFactories(
        int frame_tree_node_id,
        ukm::SourceIdObj ukm_source_id,
        NonNetworkURLLoaderFactoryMap* factories) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kContentDirectories)) {
    factories->emplace(cr_fuchsia::kFuchsiaDirScheme,
                       ContentDirectoryLoaderFactory::Create());
  }
}

void WebEngineContentBrowserClient::
    RegisterNonNetworkSubresourceURLLoaderFactories(
        int render_process_id,
        int render_frame_id,
        NonNetworkURLLoaderFactoryMap* factories) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kContentDirectories)) {
    factories->emplace(cr_fuchsia::kFuchsiaDirScheme,
                       ContentDirectoryLoaderFactory::Create());
  }
}

bool WebEngineContentBrowserClient::ShouldEnableStrictSiteIsolation() {
  constexpr base::Feature kSitePerProcess{"site-per-process",
                                          base::FEATURE_ENABLED_BY_DEFAULT};
  static bool enable_strict_isolation =
      base::FeatureList::IsEnabled(kSitePerProcess);
  return enable_strict_isolation;
}

void WebEngineContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  // TODO(https://crbug.com/1083520): Pass based on process type.
  constexpr char const* kSwitchesToCopy[] = {
      switches::kContentDirectories,
      switches::kCorsExemptHeaders,
      switches::kDisableSoftwareVideoDecoders,
      switches::kEnableCastStreamingReceiver,
      switches::kEnableProtectedVideoBuffers,
      switches::kEnableWidevine,
      switches::kForceProtectedVideoOutputBuffers,
      switches::kMaxDecodedImageSizeMb,
      switches::kPlayreadyKeySystem,
      network::switches::kUnsafelyTreatInsecureOriginAsSecure,
      switches::kUseOverlaysForVideo,
  };

  command_line->CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                                 kSwitchesToCopy, base::size(kSwitchesToCopy));
}

std::string WebEngineContentBrowserClient::GetApplicationLocale() {
  // ICU is configured with the system locale by WebEngineBrowserMainParts.
  return base::i18n::GetConfiguredLocale();
}

std::string WebEngineContentBrowserClient::GetAcceptLangs(
    content::BrowserContext* context) {
  DCHECK_EQ(main_parts_->browser_context(), context);
  return static_cast<WebEngineBrowserContext*>(context)
      ->GetPreferredLanguages();
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

  // Only create throttle if FrameImpl has a NavigationPolicyProvider,
  // indicating an interest in navigations.
  if (frame_impl->navigation_policy_handler()) {
    throttles.push_back(std::make_unique<NavigationPolicyThrottle>(
        navigation_handle, frame_impl->navigation_policy_handler()));
  }

  const base::Optional<std::string>& explicit_sites_filter_error_page =
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
    // TODO(crbug.com/976975): Add support for service workers.
    return {};
  }

  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;
  throttles.emplace_back(std::make_unique<WebEngineURLLoaderThrottle>(
      FrameImpl::FromWebContents(wc_getter.Run())
          ->url_request_rewrite_rules_manager()));
  return throttles;
}

void WebEngineContentBrowserClient::ConfigureNetworkContextParams(
    content::BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* network_context_params,
    network::mojom::CertVerifierCreationParams* cert_verifier_creation_params) {
  network_context_params->user_agent = GetUserAgent();
  network_context_params
      ->accept_language = net::HttpUtil::GenerateAcceptLanguageHeader(
      static_cast<WebEngineBrowserContext*>(context)->GetPreferredLanguages());

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