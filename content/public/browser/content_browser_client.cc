// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/content_browser_client.h"

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/guid.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/anchor_element_preconnect_delegate.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/browser/login_delegate.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/prefetch_service_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/sms_fetcher.h"
#include "content/public/browser/speculation_host_delegate.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "content/public/browser/vpn_service_proxy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/common/alternative_error_page_override_info.mojom.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_utils.h"
#include "media/audio/audio_manager.h"
#include "media/capture/content/screen_enumerator.h"
#include "media/mojo/mojom/media_service.mojom.h"
#include "net/cookies/site_for_cookies.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/client_cert_store.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/device/public/cpp/geolocation/geolocation_manager.h"
#include "services/device/public/cpp/geolocation/location_provider.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/web_transport.mojom.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/mojom/browsing_topics/browsing_topics.mojom.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/public/browser/tts_environment_android.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "content/public/browser/firewall_hole_proxy.h"
#endif

namespace content {

std::unique_ptr<BrowserMainParts> ContentBrowserClient::CreateBrowserMainParts(
    bool /* is_integration_test */) {
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

std::unique_ptr<WebContentsViewDelegate>
ContentBrowserClient::GetWebContentsViewDelegate(WebContents* web_contents) {
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
    bool is_outermost_main_frame,
    const GURL& candidate_url,
    const GURL& destination_url) {
  DCHECK(browser_context);
  return true;
}

bool ContentBrowserClient::IsExplicitNavigation(ui::PageTransition transition) {
  return transition & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR;
}

bool ContentBrowserClient::ShouldUseMobileFlingCurve() {
#if BUILDFLAG(IS_ANDROID)
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

bool ContentBrowserClient::ShouldLockProcessToSite(
    BrowserContext* browser_context,
    const GURL& effective_url) {
  DCHECK(browser_context);
  return true;
}

bool ContentBrowserClient::DoesWebUIUrlRequireProcessLock(const GURL& url) {
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

network::mojom::IPAddressSpace
ContentBrowserClient::DetermineAddressSpaceFromURL(const GURL& url) {
  return network::mojom::IPAddressSpace::kUnknown;
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

bool ContentBrowserClient::HasCustomSchemeHandler(
    content::BrowserContext* browser_context,
    const std::string& scheme) {
  return false;
}

bool ContentBrowserClient::CanCommitURL(RenderProcessHost* process_host,
                                        const GURL& site_url) {
  return true;
}

bool ContentBrowserClient::ShouldStayInParentProcessForNTP(
    const GURL& url,
    const GURL& parent_site_url) {
  return false;
}

bool ContentBrowserClient::IsSuitableHost(RenderProcessHost* process_host,
                                          const GURL& site_url) {
  return true;
}

bool ContentBrowserClient::MayReuseHost(RenderProcessHost* process_host) {
  return true;
}

size_t ContentBrowserClient::GetProcessCountToIgnoreForLimit() {
  return 0;
}

absl::optional<blink::ParsedPermissionsPolicy>
ContentBrowserClient::GetPermissionsPolicyForIsolatedWebApp(
    content::BrowserContext* browser_context,
    const url::Origin& app_origin) {
  return blink::ParsedPermissionsPolicy();
}

bool ContentBrowserClient::ShouldTryToUseExistingProcessHost(
    BrowserContext* browser_context,
    const GURL& url) {
  DCHECK(browser_context);
  return false;
}

bool ContentBrowserClient::ShouldEmbeddedFramesTryToReuseExistingProcess(
    RenderFrameHost* outermost_main_frame) {
  return true;
}

bool ContentBrowserClient::ShouldAllowNoLongerUsedProcessToExit() {
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

std::unique_ptr<media::ScreenEnumerator>
ContentBrowserClient::CreateScreenEnumerator() const {
  return nullptr;
}

bool ContentBrowserClient::OverridesAudioManager() {
  return false;
}

bool ContentBrowserClient::ShouldAssignSiteForURL(const GURL& url) {
  return true;
}

std::vector<url::Origin>
ContentBrowserClient::GetOriginsRequiringDedicatedProcess() {
  return std::vector<url::Origin>();
}

bool ContentBrowserClient::ShouldEnableStrictSiteIsolation() {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  return true;
#endif
}

bool ContentBrowserClient::ShouldDisableSiteIsolation(
    SiteIsolationMode site_isolation_mode) {
  return false;
}

std::vector<std::string>
ContentBrowserClient::GetAdditionalSiteIsolationModes() {
  return std::vector<std::string>();
}

bool ContentBrowserClient::ShouldUrlUseApplicationIsolationLevel(
    BrowserContext* browser_context,
    const GURL& url,
    bool origin_matches_flag) {
  return false;
}

bool ContentBrowserClient::IsIsolatedContextAllowedForUrl(
    BrowserContext* browser_context,
    const GURL& lock_url) {
  return false;
}

bool ContentBrowserClient::IsGetDisplayMediaSetSelectAllScreensAllowed(
    content::BrowserContext* context,
    const url::Origin& origin) {
  return false;
}

size_t ContentBrowserClient::GetMaxRendererProcessCountOverride() {
  return 0u;
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

AllowServiceWorkerResult ContentBrowserClient::AllowServiceWorker(
    const GURL& scope,
    const net::SiteForCookies& site_for_cookies,
    const absl::optional<url::Origin>& top_frame_origin,
    const GURL& script_url,
    BrowserContext* context) {
  return AllowServiceWorkerResult::Yes();
}

void ContentBrowserClient::UpdateEnabledBlinkRuntimeFeaturesInIsolatedWorker(
    BrowserContext* context,
    const GURL& script_url,
    std::vector<std::string>& out_forced_enabled_runtime_features) {}

bool ContentBrowserClient::AllowSharedWorker(
    const GURL& worker_url,
    const net::SiteForCookies& site_for_cookies,
    const absl::optional<url::Origin>& top_frame_origin,
    const std::string& name,
    const blink::StorageKey& storage_key,
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
    blink::RendererPreferences* out_prefs) {
  // |browser_context| may be null (e.g. during shutdown of a service worker).
}

void ContentBrowserClient::RequestFilesAccess(
    const std::vector<base::FilePath>& files,
    const GURL& destination_url,
    base::OnceCallback<void(file_access::ScopedFileAccess)>
        continuation_callback) {
  std::move(continuation_callback)
      .Run(file_access::ScopedFileAccess::Allowed());
}

void ContentBrowserClient::AllowWorkerFileSystem(
    const GURL& url,
    BrowserContext* browser_context,
    const std::vector<GlobalRenderFrameHostId>& render_frames,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(true);
}

bool ContentBrowserClient::AllowWorkerIndexedDB(
    const GURL& url,
    BrowserContext* browser_context,
    const std::vector<GlobalRenderFrameHostId>& render_frames) {
  return true;
}

bool ContentBrowserClient::AllowWorkerCacheStorage(
    const GURL& url,
    BrowserContext* browser_context,
    const std::vector<GlobalRenderFrameHostId>& render_frames) {
  return true;
}

bool ContentBrowserClient::AllowWorkerWebLocks(
    const GURL& url,
    BrowserContext* browser_context,
    const std::vector<GlobalRenderFrameHostId>& render_frames) {
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

bool ContentBrowserClient::IsInterestGroupAPIAllowed(
    content::RenderFrameHost* render_frame_host,
    InterestGroupApiOperation operation,
    const url::Origin& top_frame_origin,
    const url::Origin& api_origin) {
  return false;
}

bool ContentBrowserClient::IsAttributionReportingOperationAllowed(
    content::BrowserContext* browser_context,
    AttributionReportingOperation operation,
    const url::Origin* source_origin,
    const url::Origin* destination_origin,
    const url::Origin* reporting_origin) {
  return true;
}

bool ContentBrowserClient::IsSharedStorageAllowed(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* rfh,
    const url::Origin& top_frame_origin,
    const url::Origin& accessing_origin) {
  // TODO(crbug.com/1325103): Change this to false and override in
  // relevant content_browsertests and web_tests.
  return true;
}

bool ContentBrowserClient::IsSharedStorageSelectURLAllowed(
    content::BrowserContext* browser_context,
    const url::Origin& top_frame_origin,
    const url::Origin& accessing_origin) {
  // TODO(crbug.com/1325103): Change this to false and override in
  // relevant content_browsertests and web_tests.
  return true;
}

bool ContentBrowserClient::IsPrivateAggregationAllowed(
    content::BrowserContext* browser_context,
    const url::Origin& top_frame_origin,
    const url::Origin& reporting_origin) {
  return true;
}

bool ContentBrowserClient::CanSendSCTAuditingReport(
    BrowserContext* browser_context) {
  return false;
}

GeneratedCodeCacheSettings ContentBrowserClient::GetGeneratedCodeCacheSettings(
    BrowserContext* context) {
  // By default, code cache is disabled, embedders should override.
  return GeneratedCodeCacheSettings(false, 0, base::FilePath());
}

cert_verifier::mojom::CertVerifierServiceParamsPtr
ContentBrowserClient::GetCertVerifierServiceParams() {
  return nullptr;
}

void ContentBrowserClient::AllowCertificateError(
    WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    bool is_primary_main_frame_request,
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

device::GeolocationManager* ContentBrowserClient::GetGeolocationManager() {
  return nullptr;
}

#if BUILDFLAG(IS_ANDROID)
bool ContentBrowserClient::ShouldUseGmsCoreGeolocationProvider() {
  return false;
}
#endif

StoragePartitionConfig ContentBrowserClient::GetStoragePartitionConfigForSite(
    BrowserContext* browser_context,
    const GURL& site) {
  DCHECK(browser_context);

  return StoragePartitionConfig::CreateDefault(browser_context);
}

MediaObserver* ContentBrowserClient::GetMediaObserver() {
  return nullptr;
}

FeatureObserverClient* ContentBrowserClient::GetFeatureObserverClient() {
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
TtsControllerDelegate* ContentBrowserClient::GetTtsControllerDelegate() {
  return nullptr;
}
#endif

TtsPlatform* ContentBrowserClient::GetTtsPlatform() {
  return nullptr;
}

#if !BUILDFLAG(IS_ANDROID)
DirectSocketsDelegate* ContentBrowserClient::GetDirectSocketsDelegate() {
  return nullptr;
}
#endif

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

base::FilePath ContentBrowserClient::GetNetLogDefaultDirectory() {
  return base::FilePath();
}

base::FilePath ContentBrowserClient::GetFirstPartySetsDirectory() {
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
  return nullptr;
}

std::unique_ptr<content::DevToolsManagerDelegate>
ContentBrowserClient::CreateDevToolsManagerDelegate() {
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

void ContentBrowserClient::RegisterAssociatedInterfaceBindersForRenderFrameHost(
    RenderFrameHost& render_frame_host,
    blink::AssociatedInterfaceRegistry& associated_registry) {}

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

void ContentBrowserClient::AddPresentationObserver(
    PresentationObserver* observer,
    WebContents* web_contents) {}

void ContentBrowserClient::RemovePresentationObserver(
    PresentationObserver* observer,
    WebContents* web_contents) {}

void ContentBrowserClient::OpenURL(
    content::SiteInstance* site_instance,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::WebContents*)> callback) {
  DCHECK(site_instance);
  std::move(callback).Run(nullptr);
}

std::vector<std::unique_ptr<NavigationThrottle>>
content::ContentBrowserClient::CreateThrottlesForNavigation(
    NavigationHandle* navigation_handle) {
  return std::vector<std::unique_ptr<NavigationThrottle>>();
}

std::vector<std::unique_ptr<CommitDeferringCondition>>
ContentBrowserClient::CreateCommitDeferringConditionsForNavigation(
    NavigationHandle* navigation_handle,
    content::CommitDeferringCondition::NavigationType navigation_type) {
  DCHECK(navigation_handle);
  return std::vector<std::unique_ptr<CommitDeferringCondition>>();
}

std::unique_ptr<NavigationUIData> ContentBrowserClient::GetNavigationUIData(
    NavigationHandle* navigation_handle) {
  return nullptr;
}

#if BUILDFLAG(IS_WIN)
bool ContentBrowserClient::PreSpawnChild(sandbox::TargetPolicy* policy,
                                         sandbox::mojom::Sandbox sandbox_type,
                                         ChildSpawnFlags flags) {
  return true;
}

bool ContentBrowserClient::IsUtilityCetCompatible(
    const std::string& utility_sub_type) {
  return true;
}

std::wstring ContentBrowserClient::GetAppContainerSidForSandboxType(
    sandbox::mojom::Sandbox sandbox_type,
    AppContainerFlags flags) {
  // Embedders should override this method and return different SIDs for each
  // sandbox type. Note: All content level tests will run child processes in the
  // same AppContainer.
  return std::wstring(
      L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
      L"924012148-129201922");
}

bool ContentBrowserClient::IsRendererAppContainerDisabled() {
  return false;
}

std::wstring ContentBrowserClient::GetLPACCapabilityNameForNetworkService() {
  // Embedders should override this method and return different LPAC capability
  // name. This will be used to secure the user data files required for the
  // network service.
  return std::wstring(L"lpacContentNetworkService");
}

bool ContentBrowserClient::IsRendererCodeIntegrityEnabled() {
  return false;
}

bool ContentBrowserClient::ShouldEnableAudioProcessHighPriority() {
  // TODO(crbug.com/1374069): Delete this method when the
  // kAudioProcessHighPriorityEnabled enterprise policy is deprecated.
  return false;
}

#endif  // BUILDFLAG(IS_WIN)

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
    ukm::SourceIdObj ukm_source_id,
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
    const absl::optional<url::Origin>& request_initiator_origin,
    NonNetworkURLLoaderFactoryMap* factories) {}

bool ContentBrowserClient::WillCreateURLLoaderFactory(
    BrowserContext* browser_context,
    RenderFrameHost* frame,
    int render_process_id,
    URLLoaderFactoryType type,
    const url::Origin& request_initiator,
    absl::optional<int64_t> navigation_id,
    ukm::SourceIdObj ukm_source_id,
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
    const absl::optional<std::string>& user_agent,
    mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
        handshake_client) {
  // NOTREACHED because WillInterceptWebSocket returns false.
  NOTREACHED();
}

void ContentBrowserClient::WillCreateWebTransport(
    int process_id,
    int frame_routing_id,
    const GURL& url,
    const url::Origin& initiator_origin,
    mojo::PendingRemote<network::mojom::WebTransportHandshakeClient>
        handshake_client,
    WillCreateWebTransportCallback callback) {
  std::move(callback).Run(std::move(handshake_client), absl::nullopt);
}

bool ContentBrowserClient::WillCreateRestrictedCookieManager(
    network::mojom::RestrictedCookieManagerRole role,
    BrowserContext* browser_context,
    const url::Origin& origin,
    const net::IsolationInfo& isolation_info,
    bool is_service_worker,
    int process_id,
    int frame_id,
    mojo::PendingReceiver<network::mojom::RestrictedCookieManager>* receiver) {
  return false;
}

std::vector<std::unique_ptr<URLLoaderRequestInterceptor>>
ContentBrowserClient::WillCreateURLLoaderRequestInterceptors(
    content::NavigationUIData* navigation_ui_data,
    int frame_tree_node_id) {
  return std::vector<std::unique_ptr<URLLoaderRequestInterceptor>>();
}

ContentBrowserClient::URLLoaderRequestHandler
ContentBrowserClient::CreateURLLoaderHandlerForServiceWorkerNavigationPreload(
    int frame_tree_node_id,
    const network::ResourceRequest& resource_request) {
  return ContentBrowserClient::URLLoaderRequestHandler();
}

void ContentBrowserClient::OnNetworkServiceCreated(
    network::mojom::NetworkService* network_service) {}

void ContentBrowserClient::ConfigureNetworkContextParams(
    BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* network_context_params,
    cert_verifier::mojom::CertVerifierCreationParams*
        cert_verifier_creation_params) {
  network_context_params->user_agent = GetUserAgentBasedOnPolicy(context);
  network_context_params->accept_language = "en-us,en";
}

std::vector<base::FilePath>
ContentBrowserClient::GetNetworkContextsParentDirectory() {
  return {};
}

base::Value::Dict ContentBrowserClient::GetNetLogConstants() {
  return base::Value::Dict();
}

#if BUILDFLAG(IS_ANDROID)
bool ContentBrowserClient::ShouldOverrideUrlLoading(
    int frame_tree_node_id,
    bool browser_initiated,
    const GURL& gurl,
    const std::string& request_method,
    bool has_user_gesture,
    bool is_redirect,
    bool is_outermost_main_frame,
    ui::PageTransition transition,
    bool* ignore_navigation) {
  return true;
}

bool ContentBrowserClient::SupportsAvoidUnnecessaryBeforeUnloadCheckSync() {
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

void ContentBrowserClient::CreateDeviceInfoService(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::DeviceAPIService> receiver) {}

void ContentBrowserClient::CreateManagedConfigurationService(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::ManagedConfigurationService> receiver) {
}

#if !BUILDFLAG(IS_ANDROID)
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

UsbDelegate* ContentBrowserClient::GetUsbDelegate() {
  return nullptr;
}

FontAccessDelegate* ContentBrowserClient::GetFontAccessDelegate() {
  return nullptr;
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)
SmartCardDelegate* ContentBrowserClient::GetSmartCardDelegate(
    BrowserContext* browser_context) {
  return nullptr;
}
#endif

bool ContentBrowserClient::ShowPaymentHandlerWindow(
    content::BrowserContext* browser_context,
    const GURL& url,
    base::OnceCallback<void(bool, int, int)> callback) {
  DCHECK(browser_context);
  return false;
}

bool ContentBrowserClient::CreateThreadPool(base::StringPiece name) {
  base::ThreadPoolInstance::Create(name);
  return true;
}

WebAuthenticationDelegate*
ContentBrowserClient::GetWebAuthenticationDelegate() {
  static base::NoDestructor<WebAuthenticationDelegate> delegate;
  return delegate.get();
}

#if !BUILDFLAG(IS_ANDROID)
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
    bool is_request_for_primary_main_frame,
    const GURL& url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    bool first_auth_attempt,
    LoginAuthRequiredCallback auth_required_callback) {
  return nullptr;
}

bool ContentBrowserClient::HandleExternalProtocol(
    const GURL& url,
    WebContents::Getter web_contents_getter,
    int frame_tree_node_id,
    NavigationUIData* navigation_data,
    bool is_primary_main_frame,
    bool is_in_fenced_frame_tree,
    network::mojom::WebSandboxFlags sandbox_flags,
    ui::PageTransition page_transition,
    bool has_user_gesture,
    const absl::optional<url::Origin>& initiating_origin,
    RenderFrameHost* initiator_document,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory) {
  return true;
}

std::unique_ptr<VideoOverlayWindow>
ContentBrowserClient::CreateWindowForVideoPictureInPicture(
    VideoPictureInPictureWindowController* controller) {
  return nullptr;
}

void ContentBrowserClient::RegisterRendererPreferenceWatcher(
    BrowserContext* browser_context,
    mojo::PendingRemote<blink::mojom::RendererPreferenceWatcher> watcher) {
  // |browser_context| may be null (e.g. during shutdown of a service worker).
}

bool ContentBrowserClient::CanAcceptUntrustedExchangesIfNeeded() {
  return false;
}

void ContentBrowserClient::OnNetworkServiceDataUseUpdate(
    GlobalRenderFrameHostId render_frame_host_id,
    int32_t network_traffic_annotation_id_hash,
    int64_t recv_bytes,
    int64_t sent_bytes) {}

base::FilePath ContentBrowserClient::GetSandboxedStorageServiceDataDirectory() {
  return base::FilePath();
}

bool ContentBrowserClient::ShouldSandboxAudioService() {
  return base::FeatureList::IsEnabled(features::kAudioServiceSandbox);
}

bool ContentBrowserClient::ShouldSandboxNetworkService() {
  return sandbox::policy::features::IsNetworkSandboxEnabled();
}

bool ContentBrowserClient::ShouldRunOutOfProcessSystemDnsResolution() {
  return base::FeatureList::IsEnabled(
      network::features::kOutOfProcessSystemDnsResolution);
}

std::string ContentBrowserClient::GetProduct() {
  return std::string();
}

std::string ContentBrowserClient::GetUserAgent() {
  return std::string();
}

std::string ContentBrowserClient::GetUserAgentBasedOnPolicy(
    content::BrowserContext* content) {
  return GetUserAgent();
}

std::string ContentBrowserClient::GetFullUserAgent() {
  return GetUserAgent();
}

std::string ContentBrowserClient::GetReducedUserAgent() {
  return GetUserAgent();
}

blink::UserAgentMetadata ContentBrowserClient::GetUserAgentMetadata() {
  return blink::UserAgentMetadata();
}

absl::optional<gfx::ImageSkia> ContentBrowserClient::GetProductLogo() {
  return absl::nullopt;
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

#if BUILDFLAG(IS_ANDROID)
ContentBrowserClient::WideColorGamutHeuristic
ContentBrowserClient::GetWideColorGamutHeuristic() {
  return WideColorGamutHeuristic::kNone;
}

std::unique_ptr<TtsEnvironmentAndroid>
ContentBrowserClient::CreateTtsEnvironmentAndroid() {
  return nullptr;
}

bool ContentBrowserClient::
    ShouldObserveContainerViewLocationForDialogOverlays() {
  return false;
}
#endif

base::flat_set<std::string>
ContentBrowserClient::GetPluginMimeTypesWithExternalHandlers(
    BrowserContext* browser_context) {
  return base::flat_set<std::string>();
}

void ContentBrowserClient::AugmentNavigationDownloadPolicy(
    RenderFrameHost* frame_host,
    bool user_gesture,
    blink::NavigationDownloadPolicy* download_policy) {}

bool ContentBrowserClient::HandleTopicsWebApi(
    const url::Origin& context_origin,
    content::RenderFrameHost* main_frame,
    browsing_topics::ApiCallerSource caller_source,
    bool get_topics,
    bool observe,
    std::vector<blink::mojom::EpochTopicPtr>& topics) {
  return false;
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

bool ContentBrowserClient::ArePersistentMediaDeviceIDsAllowed(
    content::BrowserContext* browser_context,
    const GURL& scope,
    const net::SiteForCookies& site_for_cookies,
    const absl::optional<url::Origin>& top_frame_origin) {
  return false;
}

base::OnceClosure ContentBrowserClient::FetchRemoteSms(
    content::WebContents* web_contents,
    const std::vector<url::Origin>& origin_list,
    base::OnceCallback<void(absl::optional<std::vector<url::Origin>>,
                            absl::optional<std::string>,
                            absl::optional<content::SmsFetchFailureType>)>
        callback) {
  return base::NullCallback();
}

bool ContentBrowserClient::IsClipboardPasteAllowed(
    content::RenderFrameHost* render_frame_host) {
  return true;
}

void ContentBrowserClient::IsClipboardPasteContentAllowed(
    content::WebContents* web_contents,
    const GURL& url,
    const ui::ClipboardFormatType& data_type,
    const std::string& data,
    IsClipboardPasteContentAllowedCallback callback) {
  std::move(callback).Run(data);
}

bool ContentBrowserClient::IsClipboardCopyAllowed(
    content::BrowserContext* browser_context,
    const GURL& url,
    size_t data_size_in_bytes,
    std::u16string& replacement_data) {
  return true;
}

bool ContentBrowserClient::CanEnterFullscreenWithoutUserActivation() {
  return false;
}

#if BUILDFLAG(ENABLE_VR)
XrIntegrationClient* ContentBrowserClient::GetXrIntegrationClient() {
  return nullptr;
}
#endif

void ContentBrowserClient::BindBrowserControlInterface(
    mojo::ScopedMessagePipeHandle pipe) {}

bool ContentBrowserClient::ShouldInheritCrossOriginEmbedderPolicyImplicitly(
    const GURL& url) {
  return false;
}

bool ContentBrowserClient::ShouldServiceWorkerInheritPolicyContainerFromCreator(
    const GURL& url) {
  return url.SchemeIsLocal();
}

bool ContentBrowserClient::ShouldAllowInsecurePrivateNetworkRequests(
    BrowserContext* browser_context,
    const url::Origin& origin) {
  return false;
}

bool ContentBrowserClient::IsJitDisabledForSite(BrowserContext* browser_context,
                                                const GURL& site_url) {
  return false;
}

ukm::UkmService* ContentBrowserClient::GetUkmService() {
  return nullptr;
}

void ContentBrowserClient::OnKeepaliveRequestStarted(BrowserContext*) {}

void ContentBrowserClient::OnKeepaliveRequestFinished() {}

#if BUILDFLAG(IS_MAC)
bool ContentBrowserClient::SetupEmbedderSandboxParameters(
    sandbox::mojom::Sandbox sandbox_type,
    sandbox::SandboxCompiler* compiler) {
  return false;
}
#endif  // BUILDFLAG(IS_MAC)

void ContentBrowserClient::GetHyphenationDictionary(
    base::OnceCallback<void(const base::FilePath&)>) {}

bool ContentBrowserClient::HasErrorPage(int http_status_code) {
  return false;
}

std::unique_ptr<IdentityRequestDialogController>
ContentBrowserClient::CreateIdentityRequestDialogController() {
  return std::make_unique<IdentityRequestDialogController>();
}

bool ContentBrowserClient::SuppressDifferentOriginSubframeJSDialogs(
    BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(
      features::kSuppressDifferentOriginSubframeJSDialogs);
}

std::unique_ptr<AnchorElementPreconnectDelegate>
ContentBrowserClient::CreateAnchorElementPreconnectDelegate(
    RenderFrameHost& render_frame_host) {
  return nullptr;
}

std::unique_ptr<SpeculationHostDelegate>
ContentBrowserClient::CreateSpeculationHostDelegate(
    RenderFrameHost& render_frame_host) {
  return nullptr;
}

std::unique_ptr<PrefetchServiceDelegate>
ContentBrowserClient::CreatePrefetchServiceDelegate(
    BrowserContext* browser_context) {
  return nullptr;
}

bool ContentBrowserClient::IsFindInPageDisabledForOrigin(
    const url::Origin& origin) {
  return false;
}

void ContentBrowserClient::OnWebContentsCreated(WebContents* web_contents) {}

bool ContentBrowserClient::ShouldDisableOriginAgentClusterDefault(
    BrowserContext* browser_context) {
  return false;
}

bool ContentBrowserClient::ShouldPreconnectNavigation(
    BrowserContext* browser_context) {
  return false;
}

bool ContentBrowserClient::IsFirstPartySetsEnabled() {
  return base::FeatureList::IsEnabled(features::kFirstPartySets);
}

bool ContentBrowserClient::WillProvidePublicFirstPartySets() {
  return false;
}

mojom::AlternativeErrorPageOverrideInfoPtr
ContentBrowserClient::GetAlternativeErrorPageOverrideInfo(
    const GURL& url,
    content::RenderFrameHost* render_frame_host,
    content::BrowserContext* browser_context,
    int32_t error_code) {
  return nullptr;
}

bool ContentBrowserClient::OpenExternally(RenderFrameHost* opener,
                                          const GURL& url,
                                          WindowOpenDisposition disposition) {
  return false;
}

bool ContentBrowserClient::ShouldSendOutermostOriginToRenderer(
    const url::Origin& outermost_origin) {
  return false;
}

bool ContentBrowserClient::IsFileSystemURLNavigationAllowed(
    content::BrowserContext* browser_context,
    const GURL& url) {
  return false;
}

#if BUILDFLAG(IS_MAC)
base::FilePath ContentBrowserClient::GetChildProcessPath(
    int child_flags,
    const base::FilePath& helpers_path) {
  NOTIMPLEMENTED();
  return base::FilePath();
}
#endif

}  // namespace content
