// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/content_browser_client.h"

#include <utility>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/login_delegate.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/quota_permission_context.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "content/public/browser/vpn_service_proxy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_utils.h"
#include "media/audio/audio_manager.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/client_cert_store.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/device/public/cpp/geolocation/location_provider.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/sandbox/sandbox_type.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/mojom/renderer_preference_watcher.mojom.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

std::unique_ptr<BrowserMainParts> ContentBrowserClient::CreateBrowserMainParts(
    const MainFunctionParams& parameters) {
  return nullptr;
}

void ContentBrowserClient::PostAfterStartupTask(
    const base::Location& from_here,
    const scoped_refptr<base::TaskRunner>& task_runner,
    base::OnceClosure task) {
  task_runner->PostTask(from_here, std::move(task));
}

bool ContentBrowserClient::IsBrowserStartupComplete() {
  return true;
}

void ContentBrowserClient::SetBrowserStartupIsCompleteForTesting() {}

WebContentsViewDelegate* ContentBrowserClient::GetWebContentsViewDelegate(
    WebContents* web_contents) {
  return nullptr;
}

bool ContentBrowserClient::IsShuttingDown() {
  return false;
}

bool ContentBrowserClient::AllowGpuLaunchRetryOnIOThread() {
  return true;
}

GURL ContentBrowserClient::GetEffectiveURL(BrowserContext* browser_context,
                                           const GURL& url) {
  DCHECK(browser_context);
  return url;
}

bool ContentBrowserClient::ShouldCompareEffectiveURLsForSiteInstanceSelection(
    BrowserContext* browser_context,
    content::SiteInstance* candidate_site_instance,
    bool is_main_frame,
    const GURL& candidate_url,
    const GURL& destination_url) {
  DCHECK(browser_context);
  return true;
}

bool ContentBrowserClient::IsExplicitNavigation(ui::PageTransition transition) {
  return transition & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR;
}

bool ContentBrowserClient::ShouldUseMobileFlingCurve() {
  return false;
}

bool ContentBrowserClient::ShouldUseProcessPerSite(
    BrowserContext* browser_context, const GURL& effective_url) {
  DCHECK(browser_context);
  return false;
}

bool ContentBrowserClient::ShouldUseSpareRenderProcessHost(
    BrowserContext* browser_context,
    const GURL& site_url) {
  return true;
}

bool ContentBrowserClient::DoesSiteRequireDedicatedProcess(
    BrowserContext* browser_context,
    const GURL& effective_site_url) {
  DCHECK(browser_context);
  return false;
}

bool ContentBrowserClient::ShouldLockToOrigin(BrowserContext* browser_context,
                                              const GURL& effective_url) {
  DCHECK(browser_context);
  return true;
}

const char*
ContentBrowserClient::GetInitiatorSchemeBypassingDocumentBlocking() {
  return nullptr;
}

bool ContentBrowserClient::ShouldTreatURLSchemeAsFirstPartyWhenTopLevel(
    base::StringPiece scheme,
    bool is_embedded_origin_secure) {
  return false;
}

bool ContentBrowserClient::ShouldIgnoreSameSiteCookieRestrictionsWhenTopLevel(
    base::StringPiece scheme,
    bool is_embedded_origin_secure) {
  return false;
}

mojo::PendingRemote<network::mojom::URLLoaderFactory>
ContentBrowserClient::CreateURLLoaderFactoryForNetworkRequests(
    RenderProcessHost* process,
    network::mojom::NetworkContext* network_context,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
        header_client,
    const url::Origin& origin,
    const url::Origin& main_world_origin,
    const base::Optional<net::NetworkIsolationKey>& network_isolation_key) {
  return mojo::NullRemote();
}

void ContentBrowserClient::GetAdditionalViewSourceSchemes(
    std::vector<std::string>* additional_schemes) {
  GetAdditionalWebUISchemes(additional_schemes);
}

bool ContentBrowserClient::LogWebUIUrl(const GURL& web_ui_url) {
  return false;
}

bool ContentBrowserClient::IsWebUIAllowedToMakeNetworkRequests(
    const url::Origin& origin) {
  return false;
}

bool ContentBrowserClient::IsHandledURL(const GURL& url) {
  return false;
}

bool ContentBrowserClient::CanCommitURL(RenderProcessHost* process_host,
                                        const GURL& site_url) {
  return true;
}

bool ContentBrowserClient::IsURLAcceptableForWebUI(
    BrowserContext* browser_context,
    const GURL& url) {
  DCHECK(browser_context);
  return false;
}

bool ContentBrowserClient::ShouldStayInParentProcessForNTP(
    const GURL& url,
    SiteInstance* parent_site_instance) {
  return false;
}

bool ContentBrowserClient::IsSuitableHost(RenderProcessHost* process_host,
                                          const GURL& site_url) {
  return true;
}

bool ContentBrowserClient::MayReuseHost(RenderProcessHost* process_host) {
  return true;
}

bool ContentBrowserClient::ShouldTryToUseExistingProcessHost(
      BrowserContext* browser_context, const GURL& url) {
  DCHECK(browser_context);
  return false;
}

bool ContentBrowserClient::ShouldSubframesTryToReuseExistingProcess(
    RenderFrameHost* main_frame) {
  return true;
}

bool ContentBrowserClient::ShouldSwapBrowsingInstancesForNavigation(
    SiteInstance* site_instance,
    const GURL& current_effective_url,
    const GURL& destination_effective_url) {
  return false;
}

bool ContentBrowserClient::ShouldIsolateErrorPage(bool in_main_frame) {
  return in_main_frame;
}

std::unique_ptr<media::AudioManager> ContentBrowserClient::CreateAudioManager(
    media::AudioLogFactory* audio_log_factory) {
  return nullptr;
}

bool ContentBrowserClient::OverridesAudioManager() {
  return false;
}

void ContentBrowserClient::GetHardwareSecureDecryptionCaps(
    const std::string& key_system,
    const base::flat_set<media::CdmProxy::Protocol>& cdm_proxy_protocols,
    base::flat_set<media::VideoCodec>* video_codecs,
    base::flat_set<media::EncryptionScheme>* encryption_schemes) {}

bool ContentBrowserClient::ShouldAssignSiteForURL(const GURL& url) {
  return true;
}

std::vector<url::Origin>
ContentBrowserClient::GetOriginsRequiringDedicatedProcess() {
  return std::vector<url::Origin>();
}

bool ContentBrowserClient::ShouldEnableStrictSiteIsolation() {
#if defined(OS_ANDROID)
  return false;
#else
  return true;
#endif
}

bool ContentBrowserClient::ShouldDisableSiteIsolation() {
  return false;
}

std::vector<std::string>
ContentBrowserClient::GetAdditionalSiteIsolationModes() {
  return std::vector<std::string>();
}

bool ContentBrowserClient::IsFileAccessAllowed(
    const base::FilePath& path,
    const base::FilePath& absolute_path,
    const base::FilePath& profile_path) {
  return true;
}

bool ContentBrowserClient::ForceSniffingFileUrlsForHtml() {
  return false;
}

std::string ContentBrowserClient::GetApplicationClientGUIDForQuarantineCheck() {
  return std::string();
}

std::string ContentBrowserClient::GetApplicationLocale() {
  return "en-US";
}

std::string ContentBrowserClient::GetAcceptLangs(BrowserContext* context) {
  DCHECK(context);
  return std::string();
}

gfx::ImageSkia ContentBrowserClient::GetDefaultFavicon() {
  return gfx::ImageSkia();
}

base::FilePath ContentBrowserClient::GetLoggingFileName(
    const base::CommandLine& command_line) {
  return base::FilePath();
}

bool ContentBrowserClient::AllowAppCache(const GURL& manifest_url,
                                         const GURL& first_party,
                                         BrowserContext* context) {
  return true;
}

bool ContentBrowserClient::AllowServiceWorkerOnIO(
    const GURL& scope,
    const GURL& site_for_cookies,
    const base::Optional<url::Origin>& top_frame_origin,
    const GURL& script_url,
    ResourceContext* context,
    base::RepeatingCallback<WebContents*()> wc_getter) {
  return true;
}

bool ContentBrowserClient::AllowServiceWorkerOnUI(
    const GURL& scope,
    const GURL& site_for_cookies,
    const base::Optional<url::Origin>& top_frame_origin,
    const GURL& script_url,
    BrowserContext* context,
    base::RepeatingCallback<WebContents*()> wc_getter) {
  return true;
}

bool ContentBrowserClient::AllowSharedWorker(
    const GURL& worker_url,
    const GURL& site_for_cookies,
    const base::Optional<url::Origin>& top_frame_origin,
    const std::string& name,
    const url::Origin& constructor_origin,
    BrowserContext* context,
    int render_process_id,
    int render_frame_id) {
  DCHECK(context);
  return true;
}

bool ContentBrowserClient::DoesSchemeAllowCrossOriginSharedWorker(
    const std::string& scheme) {
  return false;
}

bool ContentBrowserClient::AllowSignedExchange(BrowserContext* context) {
  return true;
}

bool ContentBrowserClient::IsDataSaverEnabled(BrowserContext* context) {
  DCHECK(context);
  return false;
}

void ContentBrowserClient::UpdateRendererPreferencesForWorker(
    BrowserContext* browser_context,
    blink::mojom::RendererPreferences* out_prefs) {
  // |browser_context| may be null (e.g. during shutdown of a service worker).
}

void ContentBrowserClient::AllowWorkerFileSystem(
    const GURL& url,
    BrowserContext* browser_context,
    const std::vector<GlobalFrameRoutingId>& render_frames,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(true);
}

bool ContentBrowserClient::AllowWorkerIndexedDB(
    const GURL& url,
    BrowserContext* browser_context,
    const std::vector<GlobalFrameRoutingId>& render_frames) {
  return true;
}

bool ContentBrowserClient::AllowWorkerCacheStorage(
    const GURL& url,
    BrowserContext* browser_context,
    const std::vector<GlobalFrameRoutingId>& render_frames) {
  return true;
}

bool ContentBrowserClient::AllowWorkerWebLocks(
    const GURL& url,
    BrowserContext* browser_context,
    const std::vector<GlobalFrameRoutingId>& render_frames) {
  return true;
}

ContentBrowserClient::AllowWebBluetoothResult
ContentBrowserClient::AllowWebBluetooth(
    content::BrowserContext* browser_context,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  DCHECK(browser_context);
  return AllowWebBluetoothResult::ALLOW;
}

std::string ContentBrowserClient::GetWebBluetoothBlocklist() {
  return std::string();
}

scoped_refptr<QuotaPermissionContext>
ContentBrowserClient::CreateQuotaPermissionContext() {
  return nullptr;
}

void ContentBrowserClient::GetQuotaSettings(
    BrowserContext* context,
    StoragePartition* partition,
    storage::OptionalQuotaSettingsCallback callback) {
  DCHECK(context);

  // By default, no quota is provided, embedders should override.
  std::move(callback).Run(storage::GetNoQuotaSettings());
}

GeneratedCodeCacheSettings ContentBrowserClient::GetGeneratedCodeCacheSettings(
    BrowserContext* context) {
  // By default, code cache is disabled, embedders should override.
  return GeneratedCodeCacheSettings(false, 0, base::FilePath());
}

void ContentBrowserClient::AllowCertificateError(
    WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    bool is_main_frame_request,
    bool strict_enforcement,
    const base::Callback<void(CertificateRequestResultType)>& callback) {
  callback.Run(CERTIFICATE_REQUEST_RESULT_TYPE_DENY);
}

base::OnceClosure ContentBrowserClient::SelectClientCertificate(
    WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<ClientCertificateDelegate> delegate) {
  return base::OnceClosure();
}

std::unique_ptr<device::LocationProvider>
ContentBrowserClient::OverrideSystemLocationProvider() {
  return nullptr;
}

scoped_refptr<network::SharedURLLoaderFactory>
ContentBrowserClient::GetSystemSharedURLLoaderFactory() {
  return nullptr;
}

network::mojom::NetworkContext*
ContentBrowserClient::GetSystemNetworkContext() {
  return nullptr;
}

std::string ContentBrowserClient::GetGeolocationApiKey() {
  return std::string();
}

#if defined(OS_ANDROID)
bool ContentBrowserClient::ShouldUseGmsCoreGeolocationProvider() {
  return false;
}
#endif

std::string ContentBrowserClient::GetStoragePartitionIdForSite(
    BrowserContext* browser_context,
    const GURL& site) {
  DCHECK(browser_context);
  return std::string();
}

bool ContentBrowserClient::IsValidStoragePartitionId(
    BrowserContext* browser_context,
    const std::string& partition_id) {
  DCHECK(browser_context);

  // Since the GetStoragePartitionIdForChildProcess() only generates empty
  // strings, we should only ever see empty strings coming back.
  return partition_id.empty();
}

void ContentBrowserClient::GetStoragePartitionConfigForSite(
    BrowserContext* browser_context,
    const GURL& site,
    bool can_be_default,
    std::string* partition_domain,
    std::string* partition_name,
    bool* in_memory) {
  DCHECK(browser_context);

  partition_domain->clear();
  partition_name->clear();
  *in_memory = false;
}

MediaObserver* ContentBrowserClient::GetMediaObserver() {
  return nullptr;
}

LockObserver* ContentBrowserClient::GetLockObserver() {
  return nullptr;
}

PlatformNotificationService*
ContentBrowserClient::GetPlatformNotificationService(
    BrowserContext* browser_context) {
  return nullptr;
}

bool ContentBrowserClient::CanCreateWindow(
    RenderFrameHost* opener,
    const GURL& opener_url,
    const GURL& opener_top_level_frame_url,
    const url::Origin& source_origin,
    content::mojom::WindowContainerType container_type,
    const GURL& target_url,
    const Referrer& referrer,
    const std::string& frame_name,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& features,
    bool user_gesture,
    bool opener_suppressed,
    bool* no_javascript_access) {
  *no_javascript_access = false;
  return true;
}

SpeechRecognitionManagerDelegate*
    ContentBrowserClient::CreateSpeechRecognitionManagerDelegate() {
  return nullptr;
}

TtsControllerDelegate* ContentBrowserClient::GetTtsControllerDelegate() {
  return nullptr;
}

TtsPlatform* ContentBrowserClient::GetTtsPlatform() {
  return nullptr;
}

base::FilePath ContentBrowserClient::GetDefaultDownloadDirectory() {
  return base::FilePath();
}

std::string ContentBrowserClient::GetDefaultDownloadName() {
  return std::string();
}

base::FilePath ContentBrowserClient::GetFontLookupTableCacheDir() {
  return base::FilePath();
}

base::FilePath ContentBrowserClient::GetShaderDiskCacheDirectory() {
  return base::FilePath();
}

base::FilePath ContentBrowserClient::GetGrShaderDiskCacheDirectory() {
  return base::FilePath();
}

BrowserPpapiHost*
    ContentBrowserClient::GetExternalBrowserPpapiHost(int plugin_process_id) {
  return nullptr;
}

bool ContentBrowserClient::AllowPepperSocketAPI(
    BrowserContext* browser_context,
    const GURL& url,
    bool private_api,
    const SocketPermissionRequest* params) {
  DCHECK(browser_context);
  return false;
}

bool ContentBrowserClient::IsPepperVpnProviderAPIAllowed(
    BrowserContext* browser_context,
    const GURL& url) {
  DCHECK(browser_context);
  return false;
}

std::unique_ptr<VpnServiceProxy> ContentBrowserClient::GetVpnServiceProxy(
    BrowserContext* browser_context) {
  DCHECK(browser_context);
  return nullptr;
}

std::unique_ptr<ui::SelectFilePolicy>
ContentBrowserClient::CreateSelectFilePolicy(WebContents* web_contents) {
  return std::unique_ptr<ui::SelectFilePolicy>();
}

DevToolsManagerDelegate* ContentBrowserClient::GetDevToolsManagerDelegate() {
  return nullptr;
}

void ContentBrowserClient::UpdateDevToolsBackgroundServiceExpiration(
    BrowserContext* browser_context,
    int service,
    base::Time expiration_time) {}

base::flat_map<int, base::Time>
ContentBrowserClient::GetDevToolsBackgroundServiceExpirations(
    BrowserContext* browser_context) {
  return {};
}

TracingDelegate* ContentBrowserClient::GetTracingDelegate() {
  return nullptr;
}

bool ContentBrowserClient::IsPluginAllowedToCallRequestOSFileHandle(
    BrowserContext* browser_context,
    const GURL& url) {
  DCHECK(browser_context);
  return false;
}

bool ContentBrowserClient::IsPluginAllowedToUseDevChannelAPIs(
    BrowserContext* browser_context,
    const GURL& url) {
  // |browser_context| may be null (e.g. when called from
  // PpapiPluginProcessHost::PpapiPluginProcessHost).

  return false;
}

bool ContentBrowserClient::BindAssociatedReceiverFromFrame(
    RenderFrameHost* render_frame_host,
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle* handle) {
  return false;
}

ControllerPresentationServiceDelegate*
ContentBrowserClient::GetControllerPresentationServiceDelegate(
    WebContents* web_contents) {
  return nullptr;
}

ReceiverPresentationServiceDelegate*
ContentBrowserClient::GetReceiverPresentationServiceDelegate(
    WebContents* web_contents) {
  return nullptr;
}

void ContentBrowserClient::OpenURL(
    content::SiteInstance* site_instance,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::WebContents*)> callback) {
  DCHECK(site_instance);
  std::move(callback).Run(nullptr);
}

std::string ContentBrowserClient::GetMetricSuffixForURL(const GURL& url) {
  return std::string();
}

std::vector<std::unique_ptr<NavigationThrottle>>
ContentBrowserClient::CreateThrottlesForNavigation(
    NavigationHandle* navigation_handle) {
  return std::vector<std::unique_ptr<NavigationThrottle>>();
}

std::unique_ptr<NavigationUIData> ContentBrowserClient::GetNavigationUIData(
    NavigationHandle* navigation_handle) {
  return nullptr;
}

#if defined(OS_WIN)
bool ContentBrowserClient::PreSpawnRenderer(sandbox::TargetPolicy* policy,
                                            RendererSpawnFlags flags) {
  return true;
}

base::string16 ContentBrowserClient::GetAppContainerSidForSandboxType(
    int sandbox_type) {
  // Embedders should override this method and return different SIDs for each
  // sandbox type. Note: All content level tests will run child processes in the
  // same AppContainer.
  return base::string16(
      L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
      L"924012148-129201922");
}

bool ContentBrowserClient::IsRendererCodeIntegrityEnabled() {
  return false;
}

#endif  // defined(OS_WIN)

void ContentBrowserClient::RunServiceInstance(
    const service_manager::Identity& identity,
    mojo::PendingReceiver<service_manager::mojom::Service>* receiver) {}

bool ContentBrowserClient::ShouldTerminateOnServiceQuit(
    const service_manager::Identity& id) {
  return false;
}

base::Optional<service_manager::Manifest>
ContentBrowserClient::GetServiceManifestOverlay(base::StringPiece name) {
  return base::nullopt;
}

std::vector<service_manager::Manifest>
ContentBrowserClient::GetExtraServiceManifests() {
  return std::vector<service_manager::Manifest>();
}

std::vector<std::string> ContentBrowserClient::GetStartupServices() {
  return std::vector<std::string>();
}

::rappor::RapporService* ContentBrowserClient::GetRapporService() {
  return nullptr;
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
ContentBrowserClient::CreateURLLoaderThrottles(
    const network::ResourceRequest& request,
    BrowserContext* browser_context,
    const base::RepeatingCallback<WebContents*()>& wc_getter,
    NavigationUIData* navigation_ui_data,
    int frame_tree_node_id) {
  return std::vector<std::unique_ptr<blink::URLLoaderThrottle>>();
}

void ContentBrowserClient::RegisterNonNetworkNavigationURLLoaderFactories(
    int frame_tree_node_id,
    NonNetworkURLLoaderFactoryMap* factories) {}

void ContentBrowserClient::
    RegisterNonNetworkWorkerMainResourceURLLoaderFactories(
        BrowserContext* browser_context,
        NonNetworkURLLoaderFactoryMap* factories) {}

void ContentBrowserClient::
    RegisterNonNetworkServiceWorkerUpdateURLLoaderFactories(
        BrowserContext* browser_context,
        NonNetworkURLLoaderFactoryMap* factories) {}

void ContentBrowserClient::RegisterNonNetworkSubresourceURLLoaderFactories(
    int render_process_id,
    int render_frame_id,
    NonNetworkURLLoaderFactoryMap* factories) {}

bool ContentBrowserClient::WillCreateURLLoaderFactory(
    BrowserContext* browser_context,
    RenderFrameHost* frame,
    int render_process_id,
    URLLoaderFactoryType type,
    const url::Origin& request_initiator,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>* factory_receiver,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
        header_client,
    bool* bypass_redirect_checks) {
  DCHECK(browser_context);
  return false;
}

bool ContentBrowserClient::WillInterceptWebSocket(RenderFrameHost*) {
  return false;
}

uint32_t ContentBrowserClient::GetWebSocketOptions(RenderFrameHost* frame) {
  return network::mojom::kWebSocketOptionNone;
}

void ContentBrowserClient::CreateWebSocket(
    RenderFrameHost* frame,
    WebSocketFactory factory,
    const GURL& url,
    const GURL& site_for_cookies,
    const base::Optional<std::string>& user_agent,
    mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
        handshake_client) {
  // NOTREACHED because WillInterceptWebSocket returns false.
  NOTREACHED();
}

bool ContentBrowserClient::WillCreateRestrictedCookieManager(
    network::mojom::RestrictedCookieManagerRole role,
    BrowserContext* browser_context,
    const url::Origin& origin,
    const GURL& site_for_cookies,
    const url::Origin& top_frame_origin,
    bool is_service_worker,
    int process_id,
    int frame_id,
    mojo::PendingReceiver<network::mojom::RestrictedCookieManager>* receiver) {
  return false;
}

std::vector<std::unique_ptr<URLLoaderRequestInterceptor>>
ContentBrowserClient::WillCreateURLLoaderRequestInterceptors(
    content::NavigationUIData* navigation_ui_data,
    int frame_tree_node_id,
    const scoped_refptr<network::SharedURLLoaderFactory>&
        network_loader_factory) {
  return std::vector<std::unique_ptr<URLLoaderRequestInterceptor>>();
}

void ContentBrowserClient::OnNetworkServiceCreated(
    network::mojom::NetworkService* network_service) {}

mojo::Remote<network::mojom::NetworkContext>
ContentBrowserClient::CreateNetworkContext(
    BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path) {
  DCHECK(context);
  mojo::Remote<network::mojom::NetworkContext> network_context;
  network::mojom::NetworkContextParamsPtr context_params =
      network::mojom::NetworkContextParams::New();
  context_params->user_agent = GetUserAgent();
  context_params->accept_language = "en-us,en";
  GetNetworkService()->CreateNetworkContext(
      network_context.BindNewPipeAndPassReceiver(), std::move(context_params));
  return network_context;
}

std::vector<base::FilePath>
ContentBrowserClient::GetNetworkContextsParentDirectory() {
  return {};
}

#if defined(OS_ANDROID)
bool ContentBrowserClient::ShouldOverrideUrlLoading(
    int frame_tree_node_id,
    bool browser_initiated,
    const GURL& gurl,
    const std::string& request_method,
    bool has_user_gesture,
    bool is_redirect,
    bool is_main_frame,
    ui::PageTransition transition,
    bool* ignore_navigation) {
  return true;
}
#endif

bool ContentBrowserClient::AllowRenderingMhtmlOverHttp(
    NavigationUIData* navigation_ui_data) {
  return false;
}

bool ContentBrowserClient::ShouldForceDownloadResource(
    const GURL& url,
    const std::string& mime_type) {
  return false;
}

void ContentBrowserClient::CreateWebUsbService(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) {}

#if !defined(OS_ANDROID)
SerialDelegate* ContentBrowserClient::GetSerialDelegate() {
  return nullptr;
}
#endif

HidDelegate* ContentBrowserClient::GetHidDelegate() {
  return nullptr;
}

bool ContentBrowserClient::ShowPaymentHandlerWindow(
    content::BrowserContext* browser_context,
    const GURL& url,
    base::OnceCallback<void(bool, int, int)> callback) {
  DCHECK(browser_context);
  return false;
}

bool ContentBrowserClient::ShouldCreateThreadPool() {
  return true;
}

std::unique_ptr<AuthenticatorRequestClientDelegate>
ContentBrowserClient::GetWebAuthenticationRequestDelegate(
    RenderFrameHost* render_frame_host,
    const std::string& relying_party_id) {
  return std::make_unique<AuthenticatorRequestClientDelegate>();
}

std::unique_ptr<net::ClientCertStore>
ContentBrowserClient::CreateClientCertStore(BrowserContext* browser_context) {
  return nullptr;
}

std::unique_ptr<LoginDelegate> ContentBrowserClient::CreateLoginDelegate(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    const GlobalRequestID& request_id,
    bool is_request_for_main_frame,
    const GURL& url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    bool first_auth_attempt,
    LoginAuthRequiredCallback auth_required_callback) {
  return nullptr;
}

bool ContentBrowserClient::HandleExternalProtocol(
    const GURL& url,
    WebContents::Getter web_contents_getter,
    int child_id,
    NavigationUIData* navigation_data,
    bool is_main_frame,
    ui::PageTransition page_transition,
    bool has_user_gesture,
    const base::Optional<url::Origin>& initiating_origin,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory) {
  return true;
}

std::unique_ptr<OverlayWindow>
ContentBrowserClient::CreateWindowForPictureInPicture(
    PictureInPictureWindowController* controller) {
  return nullptr;
}

void ContentBrowserClient::RegisterRendererPreferenceWatcher(
    BrowserContext* browser_context,
    mojo::PendingRemote<blink::mojom::RendererPreferenceWatcher> watcher) {
  // |browser_context| may be null (e.g. during shutdown of a service worker).
}

base::Optional<std::string> ContentBrowserClient::GetOriginPolicyErrorPage(
    network::OriginPolicyState error_reason,
    content::NavigationHandle* handle) {
  return base::nullopt;
}

bool ContentBrowserClient::CanAcceptUntrustedExchangesIfNeeded() {
  return false;
}

void ContentBrowserClient::OnNetworkServiceDataUseUpdate(
    int32_t network_traffic_annotation_id_hash,
    int64_t recv_bytes,
    int64_t sent_bytes) {}

content::PreviewsState ContentBrowserClient::DetermineAllowedPreviews(
    content::PreviewsState initial_state,
    content::NavigationHandle* navigation_handle,
    const GURL& current_navigation_url) {
  return content::PREVIEWS_OFF;
}

content::PreviewsState ContentBrowserClient::DetermineCommittedPreviews(
    content::PreviewsState initial_state,
    content::NavigationHandle* navigation_handle,
    const net::HttpResponseHeaders* response_headers) {
  return content::PREVIEWS_OFF;
}

std::string ContentBrowserClient::GetProduct() {
  return std::string();
}

std::string ContentBrowserClient::GetUserAgent() {
  return std::string();
}

blink::UserAgentMetadata ContentBrowserClient::GetUserAgentMetadata() {
  return blink::UserAgentMetadata();
}

base::Optional<gfx::ImageSkia> ContentBrowserClient::GetProductLogo() {
  return base::nullopt;
}

bool ContentBrowserClient::IsBuiltinComponent(BrowserContext* browser_context,
                                              const url::Origin& origin) {
  return false;
}

bool ContentBrowserClient::IsRendererDebugURLBlacklisted(
    const GURL& url,
    BrowserContext* context) {
  return false;
}

ui::AXMode ContentBrowserClient::GetAXModeForBrowserContext(
    BrowserContext* browser_context) {
  return BrowserAccessibilityState::GetInstance()->GetAccessibilityMode();
}

#if defined(OS_ANDROID)
ContentBrowserClient::WideColorGamutHeuristic
ContentBrowserClient::GetWideColorGamutHeuristic() {
  return WideColorGamutHeuristic::kNone;
}
#endif

base::flat_set<std::string>
ContentBrowserClient::GetPluginMimeTypesWithExternalHandlers(
    BrowserContext* browser_context) {
  return base::flat_set<std::string>();
}

void ContentBrowserClient::AugmentNavigationDownloadPolicy(
    const WebContents* web_contents,
    const RenderFrameHost* frame_host,
    bool user_gesture,
    NavigationDownloadPolicy* download_policy) {}

bool ContentBrowserClient::IsBluetoothScanningBlocked(
    content::BrowserContext* browser_context,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  return false;
}

void ContentBrowserClient::BlockBluetoothScanning(
    content::BrowserContext* browser_context,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {}

bool ContentBrowserClient::ShouldLoadExtraIcuDataFile() {
  return false;
}

bool ContentBrowserClient::ArePersistentMediaDeviceIDsAllowed(
    content::BrowserContext* browser_context,
    const GURL& scope,
    const GURL& site_for_cookies,
    const base::Optional<url::Origin>& top_frame_origin) {
  return false;
}

void ContentBrowserClient::FetchRemoteSms(
    content::BrowserContext* browser_context,
    const url::Origin& origin,
    base::OnceCallback<void(base::Optional<std::string>)> callback) {}

}  // namespace content
