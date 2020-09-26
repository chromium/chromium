// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_CONTENT_BROWSER_CLIENT_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_CONTENT_BROWSER_CLIENT_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

class GURL;

namespace base {
class CommandLine;
}

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
class ShellBrowserMainDelegate;
class ShellBrowserMainParts;

// Content module browser process support for app_shell.
class ShellContentBrowserClient : public content::ContentBrowserClient {
 public:
  explicit ShellContentBrowserClient(
      ShellBrowserMainDelegate* browser_main_delegate);
  ~ShellContentBrowserClient() override;

  // Returns the single instance.
  static ShellContentBrowserClient* Get();

  // Returns the single browser context for app_shell.
  content::BrowserContext* GetBrowserContext();

  // content::ContentBrowserClient overrides.
  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) override;
  void RenderProcessWillLaunch(content::RenderProcessHost* host) override;
  bool ShouldUseProcessPerSite(content::BrowserContext* browser_context,
                               const GURL& site_url) override;
  bool IsHandledURL(const GURL& url) override;
  void SiteInstanceGotProcess(content::SiteInstance* site_instance) override;
  void SiteInstanceDeleting(content::SiteInstance* site_instance) override;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;
  content::SpeechRecognitionManagerDelegate*
  CreateSpeechRecognitionManagerDelegate() override;
  content::BrowserPpapiHost* GetExternalBrowserPpapiHost(
      int plugin_process_id) override;
  void GetAdditionalAllowedSchemesForFileSystem(
      std::vector<std::string>* additional_schemes) override;
  content::DevToolsManagerDelegate* GetDevToolsManagerDelegate() override;
  std::vector<std::unique_ptr<content::NavigationThrottle>>
  CreateThrottlesForNavigation(
      content::NavigationHandle* navigation_handle) override;
  std::unique_ptr<content::NavigationUIData> GetNavigationUIData(
      content::NavigationHandle* navigation_handle) override;
  void RegisterNonNetworkNavigationURLLoaderFactories(
      int frame_tree_node_id,
      base::UkmSourceId ukm_source_id,
      NonNetworkURLLoaderFactoryDeprecatedMap* uniquely_owned_factories,
      NonNetworkURLLoaderFactoryMap* factories) override;
  void RegisterNonNetworkWorkerMainResourceURLLoaderFactories(
      content::BrowserContext* browser_context,
      NonNetworkURLLoaderFactoryMap* factories) override;
  void RegisterNonNetworkServiceWorkerUpdateURLLoaderFactories(
      content::BrowserContext* browser_context,
      NonNetworkURLLoaderFactoryMap* factories) override;
  void RegisterNonNetworkSubresourceURLLoaderFactories(
      int render_process_id,
      int render_frame_id,
      NonNetworkURLLoaderFactoryDeprecatedMap* uniquely_owned_factories,
      NonNetworkURLLoaderFactoryMap* factories) override;
  bool WillCreateURLLoaderFactory(
      content::BrowserContext* browser_context,
      content::RenderFrameHost* frame_host,
      int render_process_id,
      URLLoaderFactoryType type,
      const url::Origin& request_initiator,
      base::Optional<int64_t> navigation_id,
      base::UkmSourceId ukm_source_id,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>* factory_receiver,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
          header_client,
      bool* bypass_redirect_checks,
      bool* disable_secure_dns,
      network::mojom::URLLoaderFactoryOverridePtr* factory_override) override;
  bool HandleExternalProtocol(
      const GURL& url,
      content::WebContents::OnceGetter web_contents_getter,
      int child_id,
      content::NavigationUIData* navigation_data,
      bool is_main_frame,
      ui::PageTransition page_transition,
      bool has_user_gesture,
      const base::Optional<url::Origin>& initiating_origin,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory)
      override;
  void OverrideURLLoaderFactoryParams(
      content::BrowserContext* browser_context,
      const url::Origin& origin,
      bool is_for_isolated_world,
      network::mojom::URLLoaderFactoryParams* factory_params) override;
  base::FilePath GetSandboxedStorageServiceDataDirectory() override;
  std::string GetUserAgent() override;

 protected:
  // Subclasses may wish to provide their own ShellBrowserMainParts.
  virtual std::unique_ptr<ShellBrowserMainParts> CreateShellBrowserMainParts(
      const content::MainFunctionParams& parameters,
      ShellBrowserMainDelegate* browser_main_delegate);

 private:
  // Appends command line switches for a renderer process.
  void AppendRendererSwitches(base::CommandLine* command_line);

  // Returns the extension or app associated with |site_instance| or NULL.
  const Extension* GetExtension(content::SiteInstance* site_instance);

  // Owned by content::BrowserMainLoop.
  ShellBrowserMainParts* browser_main_parts_;

  // Owned by ShellBrowserMainParts.
  ShellBrowserMainDelegate* browser_main_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ShellContentBrowserClient);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_CONTENT_BROWSER_CLIENT_H_
