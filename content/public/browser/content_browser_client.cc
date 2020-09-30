// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/content_browser_client.h"

// content_browser_client.h is a widely included header and its size impacts
// build time significantly. If you run into this limit, try using forward
// declarations instead of including more headers. If that is infeasible, adjust
// the limit. For more info, see
// https://chromium.googlesource.com/chromium/src/+/HEAD/docs/wmax_tokens.md
#pragma clang max_tokens_here 890000

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/sequenced_task_runner.h"
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
#include "content/public/common/content_features.h"
#include "content/public/common/url_utils.h"
#include "media/audio/audio_manager.h"
#include "media/mojo/mojom/media_service.mojom.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/client_cert_store.h"
#include "sandbox/policy/sandbox_type.h"
#include "services/device/public/cpp/geolocation/location_provider.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/mojom/renderer_preference_watcher.mojom.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_ANDROID)
#include "content/public/browser/tts_environment_android.h"
#endif

namespace content {

std::unique_ptr<BrowserMainParts> ContentBrowserClient::CreateBrowserMainParts(
    const MainFunctionParams& parameters) {
  return nullptr;
}

void ContentBrowserClient::PostAfterStartupTask(
    const base::Location& from_here,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
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

bool ContentBrowserClient::CanShutdownGpuProcessNowOnIOThread() {
  return false;
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
#if defined(OS_ANDROID)
  return true;
#else
  return false;
#endif
}

bool ContentBrowserClient::ShouldUseProcessPerSite(
    BrowserContext* browser_context,
    const GURL& site_url) {
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

bool ContentBrowserClient::ShouldLockProcess(BrowserContext* browser_context,
                                             const GURL& effective_url) {
  DCHECK(browser_context);
  return true;
}

bool ContentBrowserClient::DoesWebUISchemeRequireProcessLock(
    base::StringPiece scheme) {
  return true;
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

std::string ContentBrowserClient::GetSiteDisplayNameForCdmProcess(
    BrowserContext* browser_context,
    const GURL& site_url) {
  return site_url.spec();
}

void ContentBrowserClient::OverrideURLLoaderFactoryParams(
    BrowserContext* browser_context,
    const url::Origin& origin,
    bool is_for_isolated_world,
    network::mojom::URLLoaderFactoryParams* factory_params) {}

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
    BrowserContext* browser_context,
    const GURL& url) {
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

download::QuarantineConnectionCallback
ContentBrowserClient::GetQuarantineConnectionCallback() {
  return base::NullCallback();
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

bool ContentBrowserClient::AllowAppCache(
    const GURL& manifest_url,
    const GURL& site_for_cookies,
    const base::Optional<url::Origin>& top_frame_origin,
    BrowserContext* context) {
  return true;
}

AllowServiceWorkerResult ContentBrowserClient::AllowServiceWorkerOnIO(
    const GURL& scope,
    const GURL& site_for_cookies,
    const base::Optional<url::Origin>& top_frame_origin,
    const GURL& script_url,
    ResourceContext* context) {
  return AllowServiceWorkerResult::Yes();
}

AllowServiceWorkerResult ContentBrowserClient::AllowServiceWorkerOnUI(
    const GURL& scope,
    const GURL& site_for_cookies,
    const base::Optional<url::Origin>& top_frame_origin,
    const GURL& script_url,
    BrowserContext* context) {
  return AllowServiceWorkerResult::Yes();
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

bool ContentBrowserClient::OverrideWebPreferencesAfterNavigation(
    WebContents* web_contents,
    blink::web_pref::WebPreferences* prefs) {
  return false;
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

bool ContentBrowserClient::AllowConversionMeasurement(
    content::BrowserContext* browser_context) {
  return true;
}

scoped_refptr<QuotaPermissionContext>
ContentBrowserClient::CreateQuotaPermissionContext() {
  return nullptr;
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
    base::OnceCallback<void(CertificateRequestResultType)> callback) {
  std::move(callback).Run(CERTIFICATE_REQUEST_RESULT_TYPE_DENY);
}

bool ContentBrowserClient::ShouldDenyRequestOnCertificateError(
    const GURL main_frame_url) {
  // Generally we shouldn't deny all certificate errors, but individual
  // subclasses may override this for special cases.
  return false;
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

StoragePartitionConfig ContentBrowserClient::GetStoragePartitionConfigForSite(
    BrowserContext* browser_context,
    const GURL& site) {
  DCHECK(browser_context);

  return StoragePartitionConfig::CreateDefault();
}

MediaObserver* ContentBrowserClient::GetMediaObserver() {
  return nullptr;
}

FeatureObserverClient* ContentBrowserClient::GetFeatureObserverClient() {
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

#if defined(OS_CHROMEOS)
TtsControllerDelegate* ContentBrowserClient::GetTtsControllerDelegate() {
  return nullptr;
}
#endif

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

BrowserPpapiHost* ContentBrowserClient::GetExternalBrowserPpapiHost(
    int plugin_process_id) {
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

mojo::Remote<media::mojom::MediaService>
ContentBrowserClient::RunSecondaryMediaService() {
  return mojo::Remote<media::mojom::MediaService>();
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
    sandbox::policy::SandboxType sandbox_type) {
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
    base::UkmSourceId ukm_source_id,
    NonNetworkURLLoaderFactoryDeprecatedMap* uniquely_owned_factories,
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
    NonNetworkURLLoaderFactoryDeprecatedMap* uniquely_owned_factories,
    NonNetworkURLLoaderFactoryMap* factories) {}

bool ContentBrowserClient::WillCreateURLLoaderFactory(
    BrowserContext* browser_context,
    RenderFrameHost* frame,
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
    network::mojom::URLLoaderFactoryOverridePtr* factory_override) {
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
    const net::SiteForCookies& site_for_cookies,
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
    const net::SiteForCookies& site_for_cookies,
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

void ContentBrowserClient::ConfigureNetworkContextParams(
    BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* network_context_params,
    network::mojom::CertVerifierCreationParams* cert_verifier_creation_params) {
  network_context_params->user_agent = GetUserAgent();
  network_context_params->accept_language = "en-us,en";
}

std::vector<base::FilePath>
ContentBrowserClient::GetNetworkContextsParentDirectory() {
  return {};
}

base::DictionaryValue ContentBrowserClient::GetNetLogConstants() {
  return base::DictionaryValue();
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

BluetoothDelegate* ContentBrowserClient::GetBluetoothDelegate() {
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

#if !defined(OS_ANDROID)
std::unique_ptr<AuthenticatorRequestClientDelegate>
ContentBrowserClient::GetWebAuthenticationRequestDelegate(
    RenderFrameHost* render_frame_host) {
  return std::make_unique<AuthenticatorRequestClientDelegate>();
}
#endif

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
    WebContents::OnceGetter web_contents_getter,
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

base::FilePath ContentBrowserClient::GetSandboxedStorageServiceDataDirectory() {
  return base::FilePath();
}

bool ContentBrowserClient::ShouldSandboxAudioService() {
  return base::FeatureList::IsEnabled(features::kAudioServiceSandbox);
}

blink::PreviewsState ContentBrowserClient::DetermineAllowedPreviews(
    blink::PreviewsState initial_state,
    content::NavigationHandle* navigation_handle,
    const GURL& current_navigation_url) {
  return blink::PreviewsTypes::PREVIEWS_OFF;
}

blink::PreviewsState ContentBrowserClient::DetermineCommittedPreviews(
    blink::PreviewsState initial_state,
    content::NavigationHandle* navigation_handle,
    const net::HttpResponseHeaders* response_headers) {
  return blink::PreviewsTypes::PREVIEWS_OFF;
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

bool ContentBrowserClient::ShouldBlockRendererDebugURL(
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

std::unique_ptr<TtsEnvironmentAndroid>
ContentBrowserClient::CreateTtsEnvironmentAndroid() {
  return nullptr;
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

std::string ContentBrowserClient::GetInterestCohortForJsApi(
    content::BrowserContext* browser_context,
    const url::Origin& requesting_origin,
    const net::SiteForCookies& site_for_cookies) {
  return std::string();
}

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

void ContentBrowserClient::IsClipboardPasteAllowed(
    content::WebContents* web_contents,
    const GURL& url,
    const ui::ClipboardFormatType& data_type,
    const std::string& data,
    IsClipboardPasteAllowedCallback callback) {
  std::move(callback).Run(ClipboardPasteAllowed(true));
}

bool ContentBrowserClient::CanEnterFullscreenWithoutUserActivation() {
  return false;
}

#if BUILDFLAG(ENABLE_PLUGINS)
bool ContentBrowserClient::ShouldAllowPluginCreation(
    const url::Origin& embedder_origin,
    const content::PepperPluginInfo& plugin_info) {
  return true;
}
#endif

#if BUILDFLAG(ENABLE_VR)
XrIntegrationClient* ContentBrowserClient::GetXrIntegrationClient() {
  return nullptr;
}
#endif

bool ContentBrowserClient::IsOriginTrialRequiredForAppCache(
    content::BrowserContext* browser_context) {
  // In Chrome proper, this also considers Profile preferences.
  // As a default, only consider the base::Feature.
  // Compare this with the ChromeContentBrowserClient implementation.
  return base::FeatureList::IsEnabled(
      blink::features::kAppCacheRequireOriginTrial);
}

void ContentBrowserClient::BindBrowserControlInterface(
    mojo::ScopedMessagePipeHandle pipe) {}

bool ContentBrowserClient::ShouldInheritCrossOriginEmbedderPolicyImplicitly(
    const GURL& url) {
  return false;
}

bool ContentBrowserClient::ShouldAllowInsecurePrivateNetworkRequests(
    BrowserContext* browser_context,
    const GURL& url) {
  return false;
}

ukm::UkmService* ContentBrowserClient::GetUkmService() {
  return nullptr;
}

#if defined(OS_MAC)
bool ContentBrowserClient::SetupEmbedderSandboxParameters(
    sandbox::policy::SandboxType sandbox_type,
    sandbox::SeatbeltExecClient* client) {
  return false;
}
#endif  // defined(OS_MAC)

}  // namespace content
