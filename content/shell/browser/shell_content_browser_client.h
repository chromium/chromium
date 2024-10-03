// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_CONTENT_BROWSER_CLIENT_H_
#define CONTENT_SHELL_BROWSER_SHELL_CONTENT_BROWSER_CLIENT_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "content/shell/browser/shell_speech_recognition_manager_delegate.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"

class PrefService;

#if BUILDFLAG(IS_IOS)
namespace permissions {
class BluetoothDelegateImpl;
}
#endif

namespace content {
class ShellBrowserContext;
class ShellBrowserMainParts;

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
      bool is_integration_test) override;
  bool IsHandledURL(const GURL& url) override;
  bool HasCustomSchemeHandler(content::BrowserContext* browser_context,
                              const std::string& scheme) override;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      BrowserContext* browser_context,
      const base::RepeatingCallback<WebContents*()>& wc_getter,
      NavigationUIData* navigation_ui_data,
      FrameTreeNodeId frame_tree_node_id,
      std::optional<int64_t> navigation_id) override;
  bool AreIsolatedWebAppsEnabled(BrowserContext* browser_context) override;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;
  std::string GetAcceptLangs(BrowserContext* context) override;
  std::string GetDefaultDownloadName() override;
  std::unique_ptr<WebContentsViewDelegate> GetWebContentsViewDelegate(
      WebContents* web_contents) override;
  bool IsIsolatedContextAllowedForUrl(BrowserContext* browser_context,
                                      const GURL& lock_url) override;
  bool IsSharedStorageAllowed(
      content::BrowserContext* browser_context,
      content::RenderFrameHost* rfh,
      const url::Origin& top_frame_origin,
      const url::Origin& accessing_origin,
      std::string* out_debug_message,
      bool* out_block_is_site_setting_specific) override;
  bool IsSharedStorageSelectURLAllowed(
      content::BrowserContext* browser_context,
      const url::Origin& top_frame_origin,
      const url::Origin& accessing_origin,
      std::string* out_debug_message,
      bool* out_block_is_site_setting_specific) override;
  bool IsCookieDeprecationLabelAllowed(
      content::BrowserContext* browser_context) override;
  bool IsCookieDeprecationLabelAllowedForContext(
      content::BrowserContext* browser_context,
      const url::Origin& top_frame_origin,
      const url::Origin& context_origin) override;
  GeneratedCodeCacheSettings GetGeneratedCodeCacheSettings(
      content::BrowserContext* context) override;
  base::OnceClosure SelectClientCertificate(
      BrowserContext* browser_context,
      int process_id,
      WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      net::ClientCertIdentityList client_certs,
      std::unique_ptr<ClientCertificateDelegate> delegate) override;
  SpeechRecognitionManagerDelegate* CreateSpeechRecognitionManagerDelegate()
      override;
  void OverrideWebkitPrefs(WebContents* web_contents,
                           blink::web_pref::WebPreferences* prefs) override;
  std::unique_ptr<content::DevToolsManagerDelegate>
  CreateDevToolsManagerDelegate() override;
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
      content::BrowserContext* browser_context,
      const content::GlobalRequestID& request_id,
      bool is_main_frame,
      bool is_navigation,
      const GURL& url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      bool first_auth_attempt,
      LoginAuthRequiredCallback auth_required_callback) override;
  base::Value::Dict GetNetLogConstants() override;
  base::FilePath GetSandboxedStorageServiceDataDirectory() override;
  base::FilePath GetFirstPartySetsDirectory() override;
  std::optional<base::FilePath> GetLocalTracesDirectory() override;
  std::string GetUserAgent() override;
  blink::UserAgentMetadata GetUserAgentMetadata() override;
  void OverrideURLLoaderFactoryParams(
      BrowserContext* browser_context,
      const url::Origin& origin,
      bool is_for_isolated_world,
      network::mojom::URLLoaderFactoryParams* factory_params) override;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::PosixFileDescriptorInfo* mappings) override;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)
  device::GeolocationSystemPermissionManager*
  GetGeolocationSystemPermissionManager() override;
  void OnNetworkServiceCreated(
      network::mojom::NetworkService* network_service) override;
  void ConfigureNetworkContextParams(
      BrowserContext* context,
      bool in_memory,
      const base::FilePath& relative_partition_path,
      network::mojom::NetworkContextParams* network_context_params,
      cert_verifier::mojom::CertVerifierCreationParams*
          cert_verifier_creation_params) override;
  std::vector<base::FilePath> GetNetworkContextsParentDirectory() override;
#if BUILDFLAG(IS_IOS)
  BluetoothDelegate* GetBluetoothDelegate() override;
#endif
  void BindBrowserControlInterface(mojo::ScopedMessagePipeHandle pipe) override;
  void GetHyphenationDictionary(
      base::OnceCallback<void(const base::FilePath&)>) override;
  bool HasErrorPage(int http_status_code) override;
  void OnWebContentsCreated(WebContents* web_contents) override;

  // Turns on features via permissions policy for Isolated App
  // Web Platform Tests.
  std::optional<blink::ParsedPermissionsPolicy>
  GetPermissionsPolicyForIsolatedWebApp(WebContents* web_contents,
                                        const url::Origin& app_origin) override;

  void CreateFeatureListAndFieldTrials();

  ShellBrowserContext* browser_context();
  ShellBrowserContext* off_the_record_browser_context();
  ShellBrowserMainParts* shell_browser_main_parts();

  // Used for content_browsertests.
  using SelectClientCertificateCallback = base::OnceCallback<base::OnceClosure(
      content::WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      net::ClientCertIdentityList client_certs,
      std::unique_ptr<content::ClientCertificateDelegate> delegate)>;

  void set_select_client_certificate_callback(
      SelectClientCertificateCallback select_client_certificate_callback) {
    select_client_certificate_callback_ =
        std::move(select_client_certificate_callback);
  }
  void set_login_request_callback(
      base::OnceCallback<void(bool is_primary_main_frame, bool is_navigation)>
          login_request_callback) {
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
  void set_browser_main_parts(ShellBrowserMainParts* parts);

  // Used by ConfigureNetworkContextParams(), and can be overridden to change
  // the parameters used there.
  virtual void ConfigureNetworkContextParamsForShell(
      BrowserContext* context,
      network::mojom::NetworkContextParams* context_params,
      cert_verifier::mojom::CertVerifierCreationParams*
          cert_verifier_creation_params);

 private:
  // For GetShellContentBrowserClientInstances().
  friend class ContentBrowserTestContentBrowserClient;

  // Needed so that content_shell can use fieldtrial_testing_config.
  void SetUpFieldTrials();

  // Returns the list of ShellContentBrowserClients ordered by time created.
  // If a test overrides ContentBrowserClient, this list will have more than
  // one item.
  static const std::vector<ShellContentBrowserClient*>&
  GetShellContentBrowserClientInstances();

  static bool allow_any_cors_exempt_header_for_browser_;

  SelectClientCertificateCallback select_client_certificate_callback_;
  base::OnceCallback<void(bool is_main_frame, bool is_navigation)>
      login_request_callback_;
  base::RepeatingCallback<void(const network::mojom::URLLoaderFactoryParams*,
                               const url::Origin&,
                               bool is_for_isolated_world)>
      url_loader_factory_params_callback_;
  base::RepeatingCallback<std::vector<std::unique_ptr<NavigationThrottle>>(
      NavigationHandle*)>
      create_throttles_for_navigation_callback_;
  base::RepeatingCallback<void(blink::web_pref::WebPreferences*)>
      override_web_preferences_callback_;
#if BUILDFLAG(IS_IOS)
  std::unique_ptr<permissions::BluetoothDelegateImpl> bluetooth_delegate_;
#endif

  // NOTE: Tests may install a second ShellContentBrowserClient that becomes
  // the ContentBrowserClient used by content. This has subtle implications
  // for any state (variables) that are needed by ShellContentBrowserClient.
  // Any state that needs to be the same across all ShellContentBrowserClients
  // should be placed in SharedState (declared in
  // shell_content_browser_client.cc). Any state that is specific to a
  // particular instance should be placed here.
};

// The delay for sending reports when running with --run-web-tests
constexpr base::TimeDelta kReportingDeliveryIntervalTimeForWebTests =
    base::Milliseconds(100);

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_CONTENT_BROWSER_CLIENT_H_
