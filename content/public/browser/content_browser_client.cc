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
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/login_delegate.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "content/public/browser/vpn_service_proxy.h"
#include "content/public/common/url_loader_throttle.h"
#include "content/public/common/url_utils.h"
#include "media/audio/audio_manager.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/client_cert_store.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/device/public/cpp/geolocation/location_provider.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/service_manager/sandbox/sandbox_type.h"
#include "storage/browser/quota/quota_manager.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

void OverrideOnBindInterface(const service_manager::BindSourceInfo& remote_info,
                             const std::string& name,
                             mojo::ScopedMessagePipeHandle* handle) {
  GetContentClient()->browser()->OverrideOnBindInterface(remote_info, name,
                                                         handle);
}

BrowserMainParts* ContentBrowserClient::CreateBrowserMainParts(
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

bool ContentBrowserClient::ShouldUseMobileFlingCurve() const {
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

void ContentBrowserClient::LogInitiatorSchemeBypassingDocumentBlocking(
    const url::Origin& initiator_origin,
    int render_process_id,
    ResourceType resource_type) {}

network::mojom::URLLoaderFactoryPtrInfo
ContentBrowserClient::CreateURLLoaderFactoryForNetworkRequests(
    RenderProcessHost* process,
    network::mojom::NetworkContext* network_context,
    const url::Origin& request_initiator) {
  return network::mojom::URLLoaderFactoryPtrInfo();
}

void ContentBrowserClient::GetAdditionalViewSourceSchemes(
    std::vector<std::string>* additional_schemes) {
  GetAdditionalWebUISchemes(additional_schemes);
}

bool ContentBrowserClient::LogWebUIUrl(const GURL& web_ui_url) const {
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

bool ContentBrowserClient::ShouldAllowOpenURL(SiteInstance* site_instance,
                                              const GURL& url) {
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

bool ContentBrowserClient::ShouldSwapBrowsingInstancesForNavigation(
    SiteInstance* site_instance,
    const GURL& current_url,
    const GURL& new_url) {
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
    base::flat_set<media::EncryptionMode>* encryption_schemes) {}

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

bool ContentBrowserClient::IsFileAccessAllowed(
    const base::FilePath& path,
    const base::FilePath& absolute_path,
    const base::FilePath& profile_path) {
  return true;
}

bool ContentBrowserClient::ForceSniffingFileUrlsForHtml() {
  return false;
}

std::string ContentBrowserClient::GetApplicationLocale() {
  return "en-US";
}

std::string ContentBrowserClient::GetAcceptLangs(BrowserContext* context) {
  DCHECK(context);
  return std::string();
}

const gfx::ImageSkia* ContentBrowserClient::GetDefaultFavicon() {
  static gfx::ImageSkia* empty = new gfx::ImageSkia();
  return empty;
}

base::FilePath ContentBrowserClient::GetLoggingFileName(
    const base::CommandLine& command_line) {
  return base::FilePath();
}

bool ContentBrowserClient::AllowAppCache(const GURL& manifest_url,
                                         const GURL& first_party,
                                         ResourceContext* context) {
  return true;
}

bool ContentBrowserClient::AllowServiceWorker(
    const GURL& scope,
    const GURL& first_party,
    ResourceContext* context,
    base::RepeatingCallback<WebContents*()> wc_getter) {
  return true;
}

bool ContentBrowserClient::AllowSharedWorker(
    const GURL& worker_url,
    const GURL& main_frame_url,
    const std::string& name,
    const url::Origin& constructor_origin,
    BrowserContext* context,
    int render_process_id,
    int render_frame_id) {
  DCHECK(context);
  return true;
}

bool ContentBrowserClient::IsDataSaverEnabled(BrowserContext* context) {
  DCHECK(context);
  return false;
}

void ContentBrowserClient::UpdateRendererPreferencesForWorker(
    BrowserContext* browser_context,
    RendererPreferences* out_prefs) {
  // |browser_context| may be null (e.g. during shutdown of a service worker).
}

bool ContentBrowserClient::AllowGetCookie(const GURL& url,
                                          const GURL& first_party,
                                          const net::CookieList& cookie_list,
                                          ResourceContext* context,
                                          int render_process_id,
                                          int render_frame_id) {
  return true;
}

bool ContentBrowserClient::AllowSetCookie(const GURL& url,
                                          const GURL& first_party,
                                          const net::CanonicalCookie& cookie,
                                          ResourceContext* context,
                                          int render_process_id,
                                          int render_frame_id) {
  return true;
}

void ContentBrowserClient::OnCookiesRead(int process_id,
                                         int routing_id,
                                         const GURL& url,
                                         const GURL& first_party_url,
                                         const net::CookieList& cookie_list,
                                         bool blocked_by_policy) {}

void ContentBrowserClient::OnCookieChange(int process_id,
                                          int routing_id,
                                          const GURL& url,
                                          const GURL& first_party_url,
                                          const net::CanonicalCookie& cookie,
                                          bool blocked_by_policy) {}

void ContentBrowserClient::AllowWorkerFileSystem(
    const GURL& url,
    ResourceContext* context,
    const std::vector<GlobalFrameRoutingId>& render_frames,
    base::Callback<void(bool)> callback) {
  std::move(callback).Run(true);
}

bool ContentBrowserClient::AllowWorkerIndexedDB(
    const GURL& url,
    const base::string16& name,
    ResourceContext* context,
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

QuotaPermissionContext* ContentBrowserClient::CreateQuotaPermissionContext() {
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
    ResourceType resource_type,
    bool strict_enforcement,
    bool expired_previous_decision,
    const base::Callback<void(CertificateRequestResultType)>& callback) {
  callback.Run(CERTIFICATE_REQUEST_RESULT_TYPE_DENY);
}

void ContentBrowserClient::SelectClientCertificate(
    WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<ClientCertificateDelegate> delegate) {}

net::CookieStore* ContentBrowserClient::OverrideCookieStoreForURL(
    const GURL& url,
    ResourceContext* context) {
  return nullptr;
}

std::unique_ptr<device::LocationProvider>
ContentBrowserClient::OverrideSystemLocationProvider() {
  return nullptr;
}

scoped_refptr<network::SharedURLLoaderFactory>
ContentBrowserClient::GetSystemSharedURLLoaderFactory() {
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

PlatformNotificationService*
ContentBrowserClient::GetPlatformNotificationService() {
  return nullptr;
}

bool ContentBrowserClient::CanCreateWindow(
    RenderFrameHost* opener,
    const GURL& opener_url,
    const GURL& opener_top_level_frame_url,
    const GURL& source_origin,
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

net::NetLog* ContentBrowserClient::GetNetLog() {
  return nullptr;
}

base::FilePath ContentBrowserClient::GetDefaultDownloadDirectory() {
  return base::FilePath();
}

std::string ContentBrowserClient::GetDefaultDownloadName() {
  return std::string();
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

std::string ContentBrowserClient::GetServiceUserIdForBrowserContext(
    BrowserContext* browser_context) {
  DCHECK(browser_context);
  return base::GenerateGUID();
}

bool ContentBrowserClient::BindAssociatedInterfaceRequestFromFrame(
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
    content::BrowserContext* browser_context,
    const content::OpenURLParams& params,
    const base::Callback<void(content::WebContents*)>& callback) {
  DCHECK(browser_context);
  callback.Run(nullptr);
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
bool ContentBrowserClient::PreSpawnRenderer(sandbox::TargetPolicy* policy) {
  return true;
}

base::string16 ContentBrowserClient::GetAppContainerSidForSandboxType(
    int sandbox_type) const {
  // Embedders should override this method and return different SIDs for each
  // sandbox type. Note: All content level tests will run child processes in the
  // same AppContainer.
  return base::string16(
      L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
      L"924012148-129201922");
}
#endif  // defined(OS_WIN)

std::unique_ptr<base::Value> ContentBrowserClient::GetServiceManifestOverlay(
    base::StringPiece name) {
  return nullptr;
}

ContentBrowserClient::OutOfProcessServiceInfo::OutOfProcessServiceInfo() =
    default;

ContentBrowserClient::OutOfProcessServiceInfo::OutOfProcessServiceInfo(
    const ProcessNameCallback& process_name_callback)
    : process_name_callback(process_name_callback) {}

ContentBrowserClient::OutOfProcessServiceInfo::OutOfProcessServiceInfo(
    const ProcessNameCallback& process_name_callback,
    const std::string& process_group)
    : process_name_callback(process_name_callback),
      process_group(process_group) {
  DCHECK(!process_group.empty());
}

ContentBrowserClient::OutOfProcessServiceInfo::~OutOfProcessServiceInfo() =
    default;

bool ContentBrowserClient::ShouldTerminateOnServiceQuit(
    const service_manager::Identity& id) {
  return false;
}

std::vector<ContentBrowserClient::ServiceManifestInfo>
ContentBrowserClient::GetExtraServiceManifests() {
  return std::vector<ContentBrowserClient::ServiceManifestInfo>();
}

std::vector<service_manager::Identity>
ContentBrowserClient::GetStartupServices() {
  return std::vector<service_manager::Identity>();
}

::rappor::RapporService* ContentBrowserClient::GetRapporService() {
  return nullptr;
}

std::unique_ptr<base::TaskScheduler::InitParams>
ContentBrowserClient::GetTaskSchedulerInitParams() {
  return nullptr;
}

std::vector<std::unique_ptr<URLLoaderThrottle>>
ContentBrowserClient::CreateURLLoaderThrottles(
    const network::ResourceRequest& request,
    ResourceContext* resource_context,
    const base::RepeatingCallback<WebContents*()>& wc_getter,
    NavigationUIData* navigation_ui_data,
    int frame_tree_node_id) {
  return std::vector<std::unique_ptr<URLLoaderThrottle>>();
}

void ContentBrowserClient::RegisterNonNetworkNavigationURLLoaderFactories(
    int frame_tree_node_id,
    NonNetworkURLLoaderFactoryMap* factories) {}

void ContentBrowserClient::RegisterNonNetworkSubresourceURLLoaderFactories(
    int render_process_id,
    int render_frame_id,
    NonNetworkURLLoaderFactoryMap* factories) {}

bool ContentBrowserClient::WillCreateURLLoaderFactory(
    BrowserContext* browser_context,
    RenderFrameHost* frame,
    bool is_navigation,
    const url::Origin& request_initiator,
    network::mojom::URLLoaderFactoryRequest* factory_request,
    bool* bypass_redirect_checks) {
  DCHECK(browser_context);
  return false;
}

void ContentBrowserClient::WillCreateWebSocket(
    RenderFrameHost* frame,
    network::mojom::WebSocketRequest* request,
    network::mojom::AuthenticationHandlerPtr* auth_handler) {}

std::vector<std::unique_ptr<URLLoaderRequestInterceptor>>
ContentBrowserClient::WillCreateURLLoaderRequestInterceptors(
    NavigationUIData* navigation_ui_data,
    int frame_tree_node_id) {
  return std::vector<std::unique_ptr<URLLoaderRequestInterceptor>>();
}

void ContentBrowserClient::OnNetworkServiceCreated(
    network::mojom::NetworkService* network_service) {}

network::mojom::NetworkContextPtr ContentBrowserClient::CreateNetworkContext(
    BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path) {
  DCHECK(context);
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService))
    return nullptr;

  network::mojom::NetworkContextPtr network_context;
  network::mojom::NetworkContextParamsPtr context_params =
      network::mojom::NetworkContextParams::New();
  context_params->user_agent = GetContentClient()->GetUserAgent();
  context_params->accept_language = "en-us,en";
  context_params->enable_data_url_support = true;
  GetNetworkService()->CreateNetworkContext(MakeRequest(&network_context),
                                            std::move(context_params));
  return network_context;
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
    mojo::InterfaceRequest<blink::mojom::WebUsbService> request) {}

bool ContentBrowserClient::ShowPaymentHandlerWindow(
    content::BrowserContext* browser_context,
    const GURL& url,
    base::OnceCallback<void(bool, int, int)> callback) {
  DCHECK(browser_context);
  return false;
}

bool ContentBrowserClient::ShouldCreateTaskScheduler() {
  return true;
}

std::unique_ptr<AuthenticatorRequestClientDelegate>
ContentBrowserClient::GetWebAuthenticationRequestDelegate(
    RenderFrameHost* render_frame_host) {
  return std::make_unique<AuthenticatorRequestClientDelegate>();
}

#if defined(OS_MACOSX)
bool ContentBrowserClient::IsWebAuthenticationTouchIdAuthenticatorSupported() {
  return false;
}
#endif

std::unique_ptr<net::ClientCertStore>
ContentBrowserClient::CreateClientCertStore(ResourceContext* resource_context) {
  return nullptr;
}

scoped_refptr<LoginDelegate> ContentBrowserClient::CreateLoginDelegate(
    net::AuthChallengeInfo* auth_info,
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
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
    ResourceRequestInfo::WebContentsGetter web_contents_getter,
    int child_id,
    NavigationUIData* navigation_data,
    bool is_main_frame,
    ui::PageTransition page_transition,
    bool has_user_gesture) {
  return true;
}

std::unique_ptr<OverlayWindow>
ContentBrowserClient::CreateWindowForPictureInPicture(
    PictureInPictureWindowController* controller) {
  return nullptr;
}

bool ContentBrowserClient::IsSafeRedirectTarget(const GURL& url,
                                                ResourceContext* context) {
  return true;
}

void ContentBrowserClient::RegisterRendererPreferenceWatcherForWorkers(
    BrowserContext* browser_context,
    mojom::RendererPreferenceWatcherPtr watcher) {
  // |browser_context| may be null (e.g. during shutdown of a service worker).
}

base::Optional<std::string> ContentBrowserClient::GetOriginPolicyErrorPage(
    OriginPolicyErrorReason error_reason,
    const url::Origin& origin,
    const GURL& url) {
  return base::nullopt;
}

bool ContentBrowserClient::CanIgnoreCertificateErrorIfNeeded() {
  return false;
}

void ContentBrowserClient::OnNetworkServiceDataUseUpdate(
    int32_t network_traffic_annotation_id_hash,
    int64_t recv_bytes,
    int64_t sent_bytes) {}

content::PreviewsState ContentBrowserClient::DetermineAllowedPreviews(
    content::PreviewsState initial_state,
    content::NavigationHandle* navigation_handle) {
  return content::PREVIEWS_OFF;
}

content::PreviewsState ContentBrowserClient::DetermineCommittedPreviews(
    content::PreviewsState initial_state,
    content::NavigationHandle* navigation_handle,
    const net::HttpResponseHeaders* response_headers) {
  return content::PREVIEWS_OFF;
}

}  // namespace content
