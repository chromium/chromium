// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_CONTENT_BROWSER_CLIENT_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_CONTENT_BROWSER_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "content/public/browser/content_browser_client.h"
#include "fuchsia_web/webengine/browser/content_directory_loader_factory.h"
#include "mojo/public/cpp/bindings/binder_map.h"

class WebEngineBrowserMainParts;

class WebEngineContentBrowserClient final
    : public content::ContentBrowserClient {
 public:
  WebEngineContentBrowserClient();
  ~WebEngineContentBrowserClient() override;

  WebEngineContentBrowserClient(const WebEngineContentBrowserClient&) = delete;
  WebEngineContentBrowserClient& operator=(
      const WebEngineContentBrowserClient&) = delete;

  // ContentBrowserClient overrides.
  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      bool is_integration_test) override;
  std::unique_ptr<content::DevToolsManagerDelegate>
  CreateDevToolsManagerDelegate() override;
  std::string GetProduct() override;
  std::string GetUserAgent() override;
  blink::UserAgentMetadata GetUserAgentMetadata() override;
  void OverrideWebkitPrefs(content::WebContents* web_contents,
                           blink::web_pref::WebPreferences* web_prefs) override;
  void RegisterBrowserInterfaceBindersForFrame(
      content::RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<content::RenderFrameHost*>* map) override;
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
  CreateNonNetworkNavigationURLLoaderFactory(
      const std::string& scheme,
      content::FrameTreeNodeId frame_tree_node_id) override;
  void RegisterNonNetworkSubresourceURLLoaderFactories(
      int render_process_id,
      int render_frame_id,
      const std::optional<url::Origin>& request_initiator_origin,
      NonNetworkURLLoaderFactoryMap* factories) override;
  bool ShouldEnableStrictSiteIsolation() override;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;
  std::string GetApplicationLocale() override;
  std::string GetAcceptLangs(content::BrowserContext* context) override;
  base::OnceClosure SelectClientCertificate(
      content::BrowserContext* browser_context,
      int process_id,
      content::WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      net::ClientCertIdentityList client_certs,
      std::unique_ptr<content::ClientCertificateDelegate> delegate) override;
  std::vector<std::unique_ptr<content::NavigationThrottle>>
  CreateThrottlesForNavigation(
      content::NavigationHandle* navigation_handle) override;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      content::BrowserContext* browser_context,
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      content::NavigationUIData* navigation_ui_data,
      content::FrameTreeNodeId frame_tree_node_id,
      std::optional<int64_t> navigation_id) override;
  void ConfigureNetworkContextParams(
      content::BrowserContext* context,
      bool in_memory,
      const base::FilePath& relative_partition_path,
      network::mojom::NetworkContextParams* network_context_params,
      cert_verifier::mojom::CertVerifierCreationParams*
          cert_verifier_creation_params) override;
  std::vector<url::Origin> GetOriginsRequiringDedicatedProcess() override;

  WebEngineBrowserMainParts* main_parts_for_test() const { return main_parts_; }

 private:
  const std::vector<std::string> cors_exempt_headers_;

  // Owned by content::BrowserMainLoop.
  WebEngineBrowserMainParts* main_parts_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_CONTENT_BROWSER_CLIENT_H_
