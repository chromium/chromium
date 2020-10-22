// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_CONTENT_BROWSER_CLIENT_H_
#define FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_CONTENT_BROWSER_CLIENT_H_

#include <lib/zx/channel.h>

#include <fuchsia/web/cpp/fidl.h>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "content/public/browser/content_browser_client.h"
#include "fuchsia/engine/browser/content_directory_loader_factory.h"
#include "fuchsia/engine/browser/media_resource_provider_service.h"
#include "mojo/public/cpp/bindings/binder_map.h"

class WebEngineBrowserMainParts;

class WebEngineContentBrowserClient : public content::ContentBrowserClient {
 public:
  explicit WebEngineContentBrowserClient(
      fidl::InterfaceRequest<fuchsia::web::Context> request);
  ~WebEngineContentBrowserClient() final;

  WebEngineBrowserMainParts* main_parts_for_test() const { return main_parts_; }

  // ContentBrowserClient overrides.
  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) final;
  content::DevToolsManagerDelegate* GetDevToolsManagerDelegate() final;
  std::string GetProduct() final;
  std::string GetUserAgent() final;
  void OverrideWebkitPrefs(content::RenderViewHost* rvh,
                           blink::web_pref::WebPreferences* web_prefs) final;
  void RegisterBrowserInterfaceBindersForFrame(
      content::RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<content::RenderFrameHost*>* map) final;
  void RegisterNonNetworkNavigationURLLoaderFactories(
      int frame_tree_node_id,
      base::UkmSourceId ukm_source_id,
      NonNetworkURLLoaderFactoryDeprecatedMap* uniquely_owned_factories,
      NonNetworkURLLoaderFactoryMap* factories) final;
  void RegisterNonNetworkSubresourceURLLoaderFactories(
      int render_process_id,
      int render_frame_id,
      NonNetworkURLLoaderFactoryDeprecatedMap* uniquely_owned_factories,
      NonNetworkURLLoaderFactoryMap* factories) final;
  bool ShouldEnableStrictSiteIsolation() final;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) final;
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
      network::mojom::CertVerifierCreationParams* cert_verifier_creation_params)
      override;

 private:
  fidl::InterfaceRequest<fuchsia::web::Context> request_;

  const std::vector<std::string> cors_exempt_headers_;
  const bool allow_insecure_content_;

  // Owned by content::BrowserMainLoop.
  WebEngineBrowserMainParts* main_parts_;

  MediaResourceProviderService media_resource_provider_service_;

  DISALLOW_COPY_AND_ASSIGN(WebEngineContentBrowserClient);
};

#endif  // FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_CONTENT_BROWSER_CLIENT_H_
