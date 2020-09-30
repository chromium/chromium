// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_CONTENT_BROWSER_CLIENT_H_
#define CONTENT_SHELL_BROWSER_SHELL_CONTENT_BROWSER_CLIENT_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "content/shell/browser/shell_speech_recognition_manager_delegate.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"

namespace content {
class ShellBrowserContext;
class ShellBrowserMainParts;

std::string GetShellUserAgent();
std::string GetShellLanguage();
blink::UserAgentMetadata GetShellUserAgentMetadata();

class ShellContentBrowserClient : public ContentBrowserClient {
 public:
  // Gets the current instance.
  static ShellContentBrowserClient* Get();

  ShellContentBrowserClient();
  ~ShellContentBrowserClient() override;

  // The value supplied here is set when creating the NetworkContext.
  // Specifically
  // network::mojom::NetworkContext::allow_any_cors_exempt_header_for_browser.
  static void set_allow_any_cors_exempt_header_for_browser(bool value) {
    allow_any_cors_exempt_header_for_browser_ = value;
  }

  // ContentBrowserClient overrides.
  std::unique_ptr<BrowserMainParts> CreateBrowserMainParts(
      const MainFunctionParams& parameters) override;
  bool IsHandledURL(const GURL& url) override;
  bool ShouldTerminateOnServiceQuit(
      const service_manager::Identity& id) override;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;
  std::string GetAcceptLangs(BrowserContext* context) override;
  std::string GetDefaultDownloadName() override;
  WebContentsViewDelegate* GetWebContentsViewDelegate(
      WebContents* web_contents) override;
  scoped_refptr<content::QuotaPermissionContext> CreateQuotaPermissionContext()
      override;
  GeneratedCodeCacheSettings GetGeneratedCodeCacheSettings(
      content::BrowserContext* context) override;
  base::OnceClosure SelectClientCertificate(
      WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      net::ClientCertIdentityList client_certs,
      std::unique_ptr<ClientCertificateDelegate> delegate) override;
  SpeechRecognitionManagerDelegate* CreateSpeechRecognitionManagerDelegate()
      override;
  void OverrideWebkitPrefs(RenderViewHost* render_view_host,
                           blink::web_pref::WebPreferences* prefs) override;
  base::FilePath GetFontLookupTableCacheDir() override;
  DevToolsManagerDelegate* GetDevToolsManagerDelegate() override;
  void ExposeInterfacesToRenderer(
      service_manager::BinderRegistry* registry,
      blink::AssociatedInterfaceRegistry* associated_registry,
      RenderProcessHost* render_process_host) override;
  mojo::Remote<::media::mojom::MediaService> RunSecondaryMediaService()
      override;
  void RegisterBrowserInterfaceBindersForFrame(
      RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<RenderFrameHost*>* map) override;
  void OpenURL(SiteInstance* site_instance,
               const OpenURLParams& params,
               base::OnceCallback<void(WebContents*)> callback) override;
  std::vector<std::unique_ptr<NavigationThrottle>> CreateThrottlesForNavigation(
      NavigationHandle* navigation_handle) override;
  std::unique_ptr<LoginDelegate> CreateLoginDelegate(
      const net::AuthChallengeInfo& auth_info,
      content::WebContents* web_contents,
      const content::GlobalRequestID& request_id,
      bool is_main_frame,
      const GURL& url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      bool first_auth_attempt,
      LoginAuthRequiredCallback auth_required_callback) override;
  base::DictionaryValue GetNetLogConstants() override;
  base::FilePath GetSandboxedStorageServiceDataDirectory() override;
  std::string GetUserAgent() override;
  blink::UserAgentMetadata GetUserAgentMetadata() override;
  void OverrideURLLoaderFactoryParams(
      BrowserContext* browser_context,
      const url::Origin& origin,
      bool is_for_isolated_world,
      network::mojom::URLLoaderFactoryParams* factory_params) override;
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
  void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::PosixFileDescriptorInfo* mappings) override;
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
  void ConfigureNetworkContextParams(
      BrowserContext* context,
      bool in_memory,
      const base::FilePath& relative_partition_path,
      network::mojom::NetworkContextParams* network_context_params,
      network::mojom::CertVerifierCreationParams* cert_verifier_creation_params)
      override;
  std::vector<base::FilePath> GetNetworkContextsParentDirectory() override;
  void BindBrowserControlInterface(mojo::ScopedMessagePipeHandle pipe) override;

  ShellBrowserContext* browser_context();
  ShellBrowserContext* off_the_record_browser_context();
  ShellBrowserMainParts* shell_browser_main_parts() {
    return shell_browser_main_parts_;
  }

  // Used for content_browsertests.
  void set_select_client_certificate_callback(
      base::OnceClosure select_client_certificate_callback) {
    select_client_certificate_callback_ =
        std::move(select_client_certificate_callback);
  }
  void set_should_terminate_on_service_quit_callback(
      base::OnceCallback<bool(const service_manager::Identity&)> callback) {
    should_terminate_on_service_quit_callback_ = std::move(callback);
  }
  void set_login_request_callback(
      base::OnceCallback<void(bool is_main_frame)> login_request_callback) {
    login_request_callback_ = std::move(login_request_callback);
  }
  void set_url_loader_factory_params_callback(
      base::RepeatingCallback<void(
          const network::mojom::URLLoaderFactoryParams*,
          const url::Origin&,
          bool is_for_isolated_world)> url_loader_factory_params_callback) {
    url_loader_factory_params_callback_ =
        std::move(url_loader_factory_params_callback);
  }
  void set_create_throttles_for_navigation_callback(
      base::RepeatingCallback<std::vector<std::unique_ptr<NavigationThrottle>>(
          NavigationHandle*)> create_throttles_for_navigation_callback) {
    create_throttles_for_navigation_callback_ =
        create_throttles_for_navigation_callback;
  }

  void set_override_web_preferences_callback(
      base::RepeatingCallback<void(blink::web_pref::WebPreferences*)>
          callback) {
    override_web_preferences_callback_ = std::move(callback);
  }

 protected:
  // Call this if CreateBrowserMainParts() is overridden in a subclass.
  void set_browser_main_parts(ShellBrowserMainParts* parts) {
    shell_browser_main_parts_ = parts;
  }

  // Used by ConfigureNetworkContextParams(), and can be overridden to change
  // the parameters used there.
  virtual void ConfigureNetworkContextParamsForShell(
      BrowserContext* context,
      network::mojom::NetworkContextParams* context_params,
      network::mojom::CertVerifierCreationParams*
          cert_verifier_creation_params);

 private:
  static bool allow_any_cors_exempt_header_for_browser_;

  base::OnceClosure select_client_certificate_callback_;
  base::OnceCallback<bool(const service_manager::Identity&)>
      should_terminate_on_service_quit_callback_;
  base::OnceCallback<void(bool is_main_frame)> login_request_callback_;
  base::RepeatingCallback<void(const network::mojom::URLLoaderFactoryParams*,
                               const url::Origin&,
                               bool is_for_isolated_world)>
      url_loader_factory_params_callback_;
  base::RepeatingCallback<std::vector<std::unique_ptr<NavigationThrottle>>(
      NavigationHandle*)>
      create_throttles_for_navigation_callback_;
  base::RepeatingCallback<void(blink::web_pref::WebPreferences*)>
      override_web_preferences_callback_;

  // Owned by content::BrowserMainLoop.
  ShellBrowserMainParts* shell_browser_main_parts_ = nullptr;
};

// The delay for sending reports when running with --run-web-tests
constexpr base::TimeDelta kReportingDeliveryIntervalTimeForWebTests =
    base::TimeDelta::FromMilliseconds(100);

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_CONTENT_BROWSER_CLIENT_H_
