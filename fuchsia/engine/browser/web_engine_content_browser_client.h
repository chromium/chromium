// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_CONTENT_BROWSER_CLIENT_H_
#define FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_CONTENT_BROWSER_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "content/public/browser/content_browser_client.h"
#include "fuchsia/engine/browser/content_directory_loader_factory.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class WebEngineBrowserMainParts;

class WebEngineContentBrowserClient : public content::ContentBrowserClient {
 public:
  WebEngineContentBrowserClient();
  ~WebEngineContentBrowserClient() final;

  WebEngineContentBrowserClient(const WebEngineContentBrowserClient&) = delete;
  WebEngineContentBrowserClient& operator=(
      const WebEngineContentBrowserClient&) = delete;

  // ContentBrowserClient overrides.
  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) final;
  std::unique_ptr<content::DevToolsManagerDelegate>
  CreateDevToolsManagerDelegate() final;
  std::string GetProduct() final;
  std::string GetUserAgent() final;
  void OverrideWebkitPrefs(content::WebContents* web_contents,
                           blink::web_pref::WebPreferences* web_prefs) final;
  void RegisterBrowserInterfaceBindersForFrame(
      content::RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<content::RenderFrameHost*>* map) final;
  void RegisterNonNetworkNavigationURLLoaderFactories(
      int frame_tree_node_id,
      ukm::SourceIdObj ukm_source_id,
      NonNetworkURLLoaderFactoryMap* factories) final;
  void RegisterNonNetworkSubresourceURLLoaderFactories(
      int render_process_id,
      int render_frame_id,
      NonNetworkURLLoaderFactoryMap* factories) final;
  bool ShouldEnableStrictSiteIsolation() final;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) final;
  std::string GetApplicationLocale() final;
  std::string GetAcceptLangs(content::BrowserContext* context) final;
  base::OnceClosure SelectClientCertificate(
      content::WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      net::ClientCertIdentityList client_certs,
      std::unique_ptr<content::ClientCertificateDelegate> delegate) final;
  std::vector<std::unique_ptr<content::NavigationThrottle>>
  CreateThrottlesForNavigation(
      content::NavigationHandle* navigation_handle) final;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      content::BrowserContext* browser_context,
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      content::NavigationUIData* navigation_ui_data,
      int frame_tree_node_id) final;
  void ConfigureNetworkContextParams(
      content::BrowserContext* context,
      bool in_memory,
      const base::FilePath& relative_partition_path,
      network::mojom::NetworkContextParams* network_context_params,
      cert_verifier::mojom::CertVerifierCreationParams*
          cert_verifier_creation_params) final;
  std::vector<url::Origin> GetOriginsRequiringDedicatedProcess() final;

  WebEngineBrowserMainParts* main_parts_for_test() const { return main_parts_; }

 private:
  const std::vector<std::string> cors_exempt_headers_;
  const bool allow_insecure_content_;

  // Owned by content::BrowserMainLoop.
  WebEngineBrowserMainParts* main_parts_;
};

#endif  // FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_CONTENT_BROWSER_CLIENT_H_
