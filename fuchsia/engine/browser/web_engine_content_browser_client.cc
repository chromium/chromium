// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/web_engine_content_browser_client.h"

#include <fuchsia/web/cpp/fidl.h>
#include <string>
#include <utility>

#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "fuchsia/base/fuchsia_dir_scheme.h"
#include "fuchsia/engine/browser/url_request_rewrite_rules_manager.h"
#include "fuchsia/engine/browser/web_engine_browser_context.h"
#include "fuchsia/engine/browser/web_engine_browser_interface_binders.h"
#include "fuchsia/engine/browser/web_engine_browser_main_parts.h"
#include "fuchsia/engine/browser/web_engine_devtools_controller.h"
#include "fuchsia/engine/common/web_engine_content_client.h"
#include "fuchsia/engine/common/web_engine_url_loader_throttle.h"
#include "fuchsia/engine/switches.h"
#include "media/base/media_switches.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

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
  std::string user_agent = content::BuildUserAgentFromProduct(GetProduct());
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

  // Allow media to autoplay.
  // TODO(crbug.com/1067101): Provide a FIDL API to configure AutoplayPolicy.
  web_prefs->autoplay_policy =
      blink::web_pref::AutoplayPolicy::kNoUserGestureRequired;
}

void WebEngineContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  PopulateFuchsiaFrameBinders(map, &media_resource_provider_service_);
}

void WebEngineContentBrowserClient::
    RegisterNonNetworkNavigationURLLoaderFactories(
        int frame_tree_node_id,
        base::UkmSourceId ukm_source_id,
        NonNetworkURLLoaderFactoryDeprecatedMap* uniquely_owned_factories,
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
        NonNetworkURLLoaderFactoryDeprecatedMap* uniquely_owned_factories,
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
      switches::kUseOverlaysForVideo,
  };

  command_line->CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                                 kSwitchesToCopy, base::size(kSwitchesToCopy));
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

  UrlRequestRewriteRulesManager* adapter =
      UrlRequestRewriteRulesManager::ForFrameTreeNodeId(frame_tree_node_id);
  if (!adapter) {
    // No popup support for rules rewriter.
    return {};
  }

  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;
  throttles.emplace_back(std::make_unique<WebEngineURLLoaderThrottle>(
      UrlRequestRewriteRulesManager::ForFrameTreeNodeId(frame_tree_node_id)));
  return throttles;
}

void WebEngineContentBrowserClient::ConfigureNetworkContextParams(
    content::BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* network_context_params,
    network::mojom::CertVerifierCreationParams* cert_verifier_creation_params) {
  // Same as ContentBrowserClient::ConfigureNetworkContextParams().
  network_context_params->user_agent = GetUserAgent();
  network_context_params->accept_language = "en-us,en";

  // Set the list of cors_exempt_headers which may be specified in a URLRequest,
  // starting with the headers passed in via
  // |CreateContextParams.cors_exempt_headers|.
  network_context_params->cors_exempt_header_list = cors_exempt_headers_;
}
