// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CONTENT_BROWSER_CLIENT_H_
#define CONTENT_PUBLIC_BROWSER_CONTENT_BROWSER_CLIENT_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece_forward.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "components/browsing_topics/common/common_types.h"
#include "components/download/public/common/quarantine_connection.h"
#include "components/file_access/scoped_file_access.h"
#include "content/common/content_export.h"
#include "content/public/browser/allow_service_worker_result.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/commit_deferring_condition.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "content/public/browser/generated_code_cache_settings.h"
#include "content/public/browser/interest_group_api_operation.h"
#include "content/public/browser/interest_group_manager.h"
#include "content/public/browser/login_delegate.h"
#include "content/public/browser/mojo_binder_policy_map.h"
#include "content/public/browser/privacy_sandbox_invoking_api.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/common/alternative_error_page_override_info.mojom-forward.h"
#include "content/public/common/page_visibility_state.h"
#include "content/public/common/window_container_type.mojom-forward.h"
#include "device/vr/buildflags/buildflags.h"
#include "media/mojo/mojom/media_service.mojom-forward.h"
#include "media/mojo/mojom/remoting.mojom-forward.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "net/cookies/cookie_setting_override.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom-forward.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/ip_address_space.mojom-forward.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom-forward.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-forward.h"
#include "services/network/public/mojom/web_transport.mojom-forward.h"
#include "services/network/public/mojom/websocket.mojom-forward.h"
#include "storage/browser/file_system/file_system_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/mojom/browsing_topics/browsing_topics.mojom-forward.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_cloud_identifier.mojom-forward.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom-forward.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "third_party/blink/public/mojom/origin_trials/origin_trials_settings.mojom-forward.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

#if (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)) || BUILDFLAG(IS_FUCHSIA)
#include "base/posix/global_descriptors.h"
#endif

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include "content/public/browser/posix_file_descriptor_info.h"
#endif

namespace net {
class SiteForCookies;
class IsolationInfo;
}  // namespace net

class GURL;

// TODO(ellyjones): This synonym shouldn't need to exist - call sites should get
// the definition from LoginDelegate directly. Migrate all users over, then
// delete this.
using LoginAuthRequiredCallback =
    content::LoginDelegate::LoginAuthRequiredCallback;

namespace base {
class CommandLine;
class FilePath;
class Location;
class SequencedTaskRunner;
}  // namespace base

namespace blink {
namespace mojom {
class DeviceAPIService;
class ManagedConfigurationService;
class RendererPreferenceWatcher;
class WindowFeatures;
enum class WebFeature : int32_t;
}  // namespace mojom
namespace web_pref {
struct WebPreferences;
}  // namespace web_pref
class AssociatedInterfaceRegistry;
struct NavigationDownloadPolicy;
struct RendererPreferences;
class StorageKey;
class URLLoaderThrottle;
}  // namespace blink

namespace device {
class GeolocationManager;
class LocationProvider;
}  // namespace device

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace media {
class AudioLogFactory;
class AudioManager;
class ScreenEnumerator;
}  // namespace media

namespace mojo {
template <typename>
class BinderMapWithContext;
}  // namespace mojo

namespace network {
class SharedURLLoaderFactory;
namespace mojom {
class TrustedHeaderClient;
class URLLoader;
class URLLoaderClient;
}  // namespace mojom
}  // namespace network

namespace service_manager {
template <typename...>
class BinderRegistryWithArgs;
using BinderRegistry = BinderRegistryWithArgs<>;
}  // namespace service_manager

namespace net {
class AuthChallengeInfo;
class ClientCertIdentity;
using ClientCertIdentityList = std::vector<std::unique_ptr<ClientCertIdentity>>;
class ClientCertStore;
class HttpResponseHeaders;
class SSLCertRequestInfo;
class SSLInfo;
class URLRequest;
}  // namespace net

namespace network {
namespace mojom {
class NetworkContext;
class NetworkService;
class TrustedURLLoaderHeaderClient;
}  // namespace mojom
struct ResourceRequest;
}  // namespace network

namespace sandbox {
class SandboxCompiler;
class TargetConfig;
namespace mojom {
enum class Sandbox;
}  // namespace mojom
}  // namespace sandbox

namespace ui {
class SelectFilePolicy;
class ClipboardFormatType;
}  // namespace ui

namespace ukm {
class UkmService;
}  // namespace ukm

namespace url {
class Origin;
}  // namespace url

namespace storage {
class FileSystemBackend;
}  // namespace storage

namespace content {
enum class SiteIsolationMode;
enum class SmsFetchFailureType;
class AnchorElementPreconnectDelegate;
class AuthenticatorRequestClientDelegate;
class BluetoothDelegate;
class BrowserChildProcessHost;
class BrowserContext;
class BrowserMainParts;
class BrowserPpapiHost;
class BrowserURLHandler;
class ClientCertificateDelegate;
class ControllerPresentationServiceDelegate;
class DevToolsManagerDelegate;
class DirectSocketsDelegate;
class FeatureObserverClient;
class FontAccessDelegate;
class HidDelegate;
class IdentityRequestDialogController;
class LoginDelegate;
class MDocProvider;
class MediaObserver;
class NavigationHandle;
class NavigationThrottle;
class NavigationUIData;
class PrefetchServiceDelegate;
class PrerenderWebContentsDelegate;
class PresentationObserver;
class PrivateNetworkDeviceDelegate;
class ReceiverPresentationServiceDelegate;
class RenderFrameHost;
class RenderProcessHost;
class ResponsivenessCalculatorDelegate;
class SerialDelegate;
class SiteInstance;
class SpeculationHostDelegate;
class SpeechRecognitionManagerDelegate;
class StoragePartition;
class TracingDelegate;
class TtsPlatform;
class URLLoaderRequestInterceptor;
class UsbDelegate;
class VideoOverlayWindow;
class VideoPictureInPictureWindowController;
class VpnServiceProxy;
class WebAuthenticationDelegate;
class WebContents;
class WebContentsViewDelegate;
class WebUIBrowserInterfaceBrokerRegistry;
class XrIntegrationClient;
struct GlobalRenderFrameHostId;
struct GlobalRequestID;
struct OpenURLParams;
struct Referrer;
struct ServiceWorkerVersionBaseInfo;
struct SocketPermissionRequest;

#if BUILDFLAG(IS_ANDROID)
class TtsEnvironmentAndroid;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
class TtsControllerDelegate;
#endif

#if BUILDFLAG(IS_CHROMEOS)
class SmartCardDelegate;
#endif

// Structure of data pasted from clipboard.
struct CONTENT_EXPORT ClipboardPasteData {
  ClipboardPasteData(std::string text,
                     std::string image,
                     std::vector<std::string> file_paths);
  ClipboardPasteData();
  ClipboardPasteData(const ClipboardPasteData&);
  ClipboardPasteData(ClipboardPasteData&&);
  ClipboardPasteData& operator=(ClipboardPasteData&&);
  bool isEmpty();
  ~ClipboardPasteData();

  // UTF-8 encoded text data to scan, such as plain text, URLs, HTML, etc.
  std::string text;

  // Binary image data to scan, such as png (here we assume the data
  // struct holds one image only).
  std::string image;

  // A list of full file paths to scan.
  std::vector<std::string> file_paths;
};

// Embedder API (or SPI) for participating in browser logic, to be implemented
// by the client of the content browser. See ChromeContentBrowserClient for the
// principal implementation. The methods are assumed to be called on the UI
// thread unless otherwise specified. Use this "escape hatch" sparingly, to
// avoid the embedder interface ballooning and becoming very specific to Chrome.
// (Often, the call out to the client can happen in a different part of the code
// that either already has a hook out to the embedder, or calls out to one of
// the observer interfaces.)
class CONTENT_EXPORT ContentBrowserClient {
 public:
  // Callback used with IsClipboardPasteContentAllowed() method.  If the paste
  // is not allowed, nullopt is passed to the callback.  Otherwise, the data
  // that should be pasted is passed in.
  using IsClipboardPasteContentAllowedCallback = base::OnceCallback<void(
      absl::optional<ClipboardPasteData> clipboard_paste_data)>;

  virtual ~ContentBrowserClient() = default;

  // Allows the embedder to set any number of custom BrowserMainParts
  // implementations for the browser startup code. See comments in
  // browser_main_parts.h.
  // |is_integration_test| is true iff MainFunctionParams::ui_task was set and
  // will thus intercept MainMessageLoopRun (i.e. in integration tests -- ref.
  // BrowserMainParts::ShouldInterceptMainMessageLoopRun for additional control
  // the embedder has over that).
  virtual std::unique_ptr<BrowserMainParts> CreateBrowserMainParts(
      bool is_integration_test);

  // Queues `task` until after the startup phase (for whatever definition of
  // "startup" the embedder has in mind). This may be called on any thread.
  // Note: prefer to simply post a task with BEST_EFFORT priority. This will
  // delay the task until higher priority tasks are finished, which includes
  // critical startup tasks. The BrowserThread::PostBestEffortTask() helper can
  // post a BEST_EFFORT task to an arbitrary task runner.
  virtual void PostAfterStartupTask(
      const base::Location& from_here,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      base::OnceClosure task);

  // Allows the embedder to indicate whether it considers startup to be
  // complete. May be called on any thread. This should be called on a one-off
  // basis; if you need to poll this function constantly, use the above
  // PostAfterStartupTask() API instead.
  virtual bool IsBrowserStartupComplete();

  // Allows the embedder to handle a request from unit tests running in the
  // content layer to consider startup complete (for the sake of
  // PostAfterStartupTask()).
  virtual void SetBrowserStartupIsCompleteForTesting();

  // Returns true if the embedder is in the process of shutting down, whether
  // because of the user closing the browser or the OS is shutting down.
  virtual bool IsShuttingDown();

  // If content creates the WebContentsView implementation, it will ask the
  // embedder to return an (optional) delegate to customize it.
  virtual std::unique_ptr<WebContentsViewDelegate> GetWebContentsViewDelegate(
      WebContents* web_contents);

  // Allow embedder control GPU process launch retry on failure behavior.
  virtual bool AllowGpuLaunchRetryOnIOThread();

  // Called when GPU process is not used for compositing. Allow embedder to
  // control whether to shut down the GPU process to save memory, at the cost
  // of slower start up the next time GPU process is needed.
  // Note this only ensures the GPU process is not used for compositing. It is
  // the embedder's responsibility to ensure there are no other services hosted
  // by the GPU process being used; examples include accelerated media decoders
  // and encoders.
  virtual bool CanShutdownGpuProcessNowOnIOThread();

  // Notifies that a render process will be created. This is called before
  // the content layer adds its own BrowserMessageFilters, so that the
  // embedder's IPC filters have priority.
  //
  // If the client provides a service request, the content layer will ask the
  // corresponding embedder renderer-side component to bind it to an
  // implementation at the appropriate moment during initialization.
  virtual void RenderProcessWillLaunch(RenderProcessHost* host) {}

  // Notifies that a BrowserChildProcessHost has been created.
  virtual void BrowserChildProcessHostCreated(BrowserChildProcessHost* host) {}

  // Get the effective URL for the given actual URL, to allow an embedder to
  // group different url schemes in the same SiteInstance.
  virtual GURL GetEffectiveURL(BrowserContext* browser_context,
                               const GURL& url);

  // Returns true if effective URLs should be compared when choosing a
  // SiteInstance for a navigation to |destination_url|.
  // |is_outermost_main_frame| is true if the navigation will take place in an
  // outermost main frame.
  virtual bool ShouldCompareEffectiveURLsForSiteInstanceSelection(
      BrowserContext* browser_context,
      content::SiteInstance* candidate_site_instance,
      bool is_outermost_main_frame,
      const GURL& candidate_url,
      const GURL& destination_url);

  // Returns true if the user intentionally initiated the navigation. This is
  // used to determine whether we should process debug URLs like chrome://crash.
  // The default implementation checks whether the transition was initiated via
  // the address bar (rather than whether it was typed) to permit the pasting of
  // debug URLs.
  virtual bool IsExplicitNavigation(ui::PageTransition transition);

  // Returns whether gesture fling events should use the mobile-behavior gesture
  // curve for scrolling.
  virtual bool ShouldUseMobileFlingCurve();

  // Returns whether all instances of the specified site URL should be
  // rendered by the same process, rather than using process-per-site-instance.
  virtual bool ShouldUseProcessPerSite(BrowserContext* browser_context,
                                       const GURL& site_url);

  // Returns whether a spare RenderProcessHost should be used for navigating to
  // the specified site URL.
  //
  // Using the spare RenderProcessHost is advisable, because it can improve
  // performance of a navigation that requires a new process.  On the other
  // hand, sometimes the spare RenderProcessHost cannot be used - for example
  // some embedders might override
  // ContentBrowserClient::AppendExtraCommandLineSwitches and add some cmdline
  // switches at navigation time (and this won't work for the spare, because the
  // spare RenderProcessHost is launched ahead of time).
  virtual bool ShouldUseSpareRenderProcessHost(BrowserContext* browser_context,
                                               const GURL& site_url);

  // Returns true if site isolation should be enabled for |effective_site_url|.
  // This call allows the embedder to supplement the site isolation policy
  // enforced by the content layer. Will only be called if the content layer
  // didn't decide to isolate |effective_site_url| according to its internal
  // policy (e.g. because of --site-per-process).
  virtual bool DoesSiteRequireDedicatedProcess(BrowserContext* browser_context,
                                               const GURL& effective_site_url);

  // Returns true unless the effective URL is part of a site that cannot live in
  // a process restricted to just that site.  This is only called if site
  // isolation is enabled for this URL.
  virtual bool ShouldLockProcessToSite(BrowserContext* browser_context,
                                       const GURL& effective_url);

  // Returns a boolean indicating whether the WebUI |url| requires its process
  // to be locked to the WebUI origin. Note: This method can be called from
  // multiple threads. It is not safe to assume it runs only on the UI thread.
  //
  // TODO(crbug.com/566091): Remove this exception once most visited tiles can
  // load in OOPIFs on the NTP.  Ideally, all WebUI urls should load in locked
  // processes.
  virtual bool DoesWebUIUrlRequireProcessLock(const GURL& url);

  // Returns true if everything embedded inside a document with given scheme
  // should be treated as first-party content. |scheme| will be in canonical
  // (lowercased) form. |is_embedded_origin_secure| refers to whether the origin
  // that is embedded in a document with the given scheme is secure.
  //
  // See also WebSecurityPolicy::RegisterURLSchemeAsFirstPartyWhenTopLevel() in
  // the renderer, and the network::mojom::CookieManagerParams fields:
  //  1. third_party_cookies_allowed_schemes (corresponding to schemes where
  //     this returns true regardless of |is_embedded_origin_secure|), and
  //  2. secure_origins_cookies_allowed_schemes (corresponding to schemes where
  //     this returns true if |is_embedded_origin_secure| is true),
  // which should both be synchronized with the output of this function.
  //
  // TODO(chlily): This doesn't take into account the
  // matching_scheme_cookies_allowed_schemes, but maybe we should remove that
  // anyway.
  virtual bool ShouldTreatURLSchemeAsFirstPartyWhenTopLevel(
      base::StringPiece scheme,
      bool is_embedded_origin_secure);

  // Similar to the above. Returns whether SameSite cookie restrictions should
  // be ignored when the site_for_cookies's scheme is |scheme|.
  // |is_embedded_origin_secure| refers to whether the origin that is embedded
  // in a document with the given scheme is secure.
  // This is a separate function from the above because the allowed schemes can
  // be different, as SameSite restrictions and third-party cookie blocking are
  // related but have different semantics.
  virtual bool ShouldIgnoreSameSiteCookieRestrictionsWhenTopLevel(
      base::StringPiece scheme,
      bool is_embedded_origin_secure);

  // Gets a user friendly display name for a given |site_url| to be used in the
  // CDM process name.
  virtual std::string GetSiteDisplayNameForCdmProcess(
      BrowserContext* browser_context,
      const GURL& site_url);

  // This method allows the //content embedder to override |factory_params| with
  // |origin|-specific properties (e.g. with relaxed Cross-Origin Read Blocking
  // enforcement as needed by some extensions, or with extension-specific CORS
  // exclusions).
  //
  // This method may be called in the following cases:
  // - The default factory to be used by a frame or worker.  In this case both
  //   the |origin| and |request_initiator_origin_lock| are the origin of the
  //   frame or worker (or the origin that is being committed in the frame).
  // - An isolated-world-specific factory for origins covered via
  //   RenderFrameHost::MarkIsolatedWorldAsRequiringSeparateURLLoaderFactory.
  //   In this case |origin| is the origin of the isolated world and the
  //   |request_initiator_origin_lock| is the origin committed in the frame.
  virtual void OverrideURLLoaderFactoryParams(
      BrowserContext* browser_context,
      const url::Origin& origin,
      bool is_for_isolated_world,
      network::mojom::URLLoaderFactoryParams* factory_params);

  // Returns a list of additional WebUI schemes, if any.  These additional
  // schemes act as aliases to the chrome: scheme.  The additional schemes may
  // or may not serve specific WebUI pages depending on the particular
  // URLDataSource and its override of URLDataSource::ShouldServiceRequest.
  virtual void GetAdditionalWebUISchemes(
      std::vector<std::string>* additional_schemes) {}

  // Returns a list of additional schemes allowed for view-source.  Defaults to
  // the list of WebUI schemes returned by GetAdditionalWebUISchemes.
  virtual void GetAdditionalViewSourceSchemes(
      std::vector<std::string>* additional_schemes);

  // Computes the IPAddressSpace of the given URL with embedder knowledge.
  // This is used to assign values to special schemes recognized only by the
  // embedders of content/. Returns kUnknown if no such scheme was found.
  // See https://wicg.github.io/private-network-access/ for details on what
  // the IPAddressSpace represents.
  virtual network::mojom::IPAddressSpace DetermineAddressSpaceFromURL(
      const GURL& url);

  // Called when WebUI objects are created to get aggregate usage data (i.e. is
  // chrome://downloads used more than chrome://bookmarks?). Only internal (e.g.
  // chrome://) URLs are logged. Returns whether the URL was actually logged.
  virtual bool LogWebUIUrl(const GURL& web_ui_url);

  // http://crbug.com/829412
  // Renderers with WebUI bindings shouldn't make http(s) requests for security
  // reasons (e.g. to avoid malicious responses being able to run code in
  // priviliged renderers). Fix these webui's to make requests through C++
  // code instead.
  virtual bool IsWebUIAllowedToMakeNetworkRequests(const url::Origin& origin);

  // Returns whether a specified URL is handled by the embedder's internal
  // protocol handlers.
  virtual bool IsHandledURL(const GURL& url);

  // Returns whether a custom handler is registered for the scheme of the
  // specified URL scheme.
  // https://html.spec.whatwg.org/multipage/system-state.html#custom-handlers
  // TODO(crbug.com/1139176) Move custom protocol handler code to content.
  virtual bool HasCustomSchemeHandler(content::BrowserContext* browser_context,
                                      const std::string& scheme);

  // Returns whether the given process is allowed to commit |url|.  This is a
  // more conservative check than IsSuitableHost, since it is used after a
  // navigation has committed to ensure that the process did not exceed its
  // authority.
  // This is called on the UI thread.
  virtual bool CanCommitURL(RenderProcessHost* process_host, const GURL& url);

  // Allows the embedder to override parameters when navigating. Called for both
  // opening new URLs and when transferring URLs across processes.
  // If the initiator of the navigation is set, `source_process_site_url` is
  // the site URL that the initiator's process is locked to. Generally the
  // initiator is set on renderer-initiated navigations, but not on
  // browser-initiated navigations.
  virtual void OverrideNavigationParams(
      absl::optional<GURL> source_process_site_url,
      ui::PageTransition* transition,
      bool* is_renderer_initiated,
      content::Referrer* referrer,
      absl::optional<url::Origin>* initiator_origin) {}

  // Temporary hack to determine whether to skip OOPIFs on the new tab page.
  // TODO(creis): Remove when https://crbug.com/566091 is fixed.
  virtual bool ShouldStayInParentProcessForNTP(const GURL& url,
                                               const GURL& parent_site_url);

  // Returns whether a new view for a given |site_url| can be launched in a
  // given |process_host|.
  virtual bool IsSuitableHost(RenderProcessHost* process_host,
                              const GURL& site_url);

  // Returns whether a new view for a new site instance can be added to a
  // given |process_host|.
  virtual bool MayReuseHost(RenderProcessHost* process_host);

  // Returns a number of processes to ignore when deciding whether to reuse
  // processes when over the process limit. This is useful for embedders that
  // may want to partly delay when normal pages start reusing processes (e.g.,
  // if another process type has a large number of processes). Defaults to 0.
  // Must be less than or equal to the total number of RenderProcessHosts.
  virtual size_t GetProcessCountToIgnoreForLimit();

  // Returns the base permissions policy that is declared in an isolated app's
  // Web App Manifest. The embedder might choose to return an absl::nullopt in
  // specific cases -- then the default non-isolated permissions policy will be
  // applied.
  virtual absl::optional<blink::ParsedPermissionsPolicy>
  GetPermissionsPolicyForIsolatedWebApp(
      content::BrowserContext* browser_context,
      const url::Origin& app_origin);

  // Returns whether a new process should be created or an existing one should
  // be reused based on the URL we want to load. This should return false,
  // unless there is a good reason otherwise.
  virtual bool ShouldTryToUseExistingProcessHost(
      BrowserContext* browser_context,
      const GURL& url);

  // Returns whether or not embedded frames (subframes or embedded main frames)
  // of |outermost_main_frame| should try to aggressively reuse existing
  // processes, even when below process limit.  This gets called when navigating
  // a subframe to a URL that requires a dedicated process and defaults to true,
  // which minimizes the process count.  The embedder can choose to override
  // this if there is a reason to avoid the reuse.
  virtual bool ShouldEmbeddedFramesTryToReuseExistingProcess(
      RenderFrameHost* outermost_main_frame);

  // Returns whether a process that no longer has active RenderFrameHosts (or
  // other reasons to be kept alive) can safely exit. This should return true,
  // unless the embedder cannot easily handle a process exit in non-live frames.
  virtual bool ShouldAllowNoLongerUsedProcessToExit();

  // Called when a site instance is first associated with a process.
  virtual void SiteInstanceGotProcess(SiteInstance* site_instance) {}

  // Returns true if for the navigation from |current_effective_url| to
  // |destination_effective_url| in |site_instance|, a new SiteInstance and
  // BrowsingInstance should be created (even if we are in a process model that
  // doesn't usually swap.) This forces a process swap and severs script
  // connections with existing tabs.
  virtual bool ShouldSwapBrowsingInstancesForNavigation(
      SiteInstance* site_instance,
      const GURL& current_effective_url,
      const GURL& destination_effective_url);

  // Returns true if error page should be isolated in its own process.
  virtual bool ShouldIsolateErrorPage(bool in_main_frame);

  // Allows the embedder to programmatically provide some origins that should be
  // opted into --isolate-origins mode of Site Isolation.
  virtual std::vector<url::Origin> GetOriginsRequiringDedicatedProcess();

  // Allows the embedder to programmatically control whether the
  // --site-per-process mode of Site Isolation should be used.  Note that
  // returning true here will only take effect if ShouldDisableSiteIsolation()
  // below returns false.
  //
  // Note that for correctness, the same value should be consistently returned.
  // See also https://crbug.com/825369
  virtual bool ShouldEnableStrictSiteIsolation();

  // Allows the embedder to programmatically control whether Site Isolation
  // should be disabled.  Note that this takes precedence over
  // ShouldEnableStrictSiteIsolation() if both return true.
  // `site_isolation_mode` specifies the site isolation mode to check; this
  // allows strict site isolation and partial site isolation to be disabled
  // according to different policies (e.g., different memory thresholds) on
  // Android.
  //
  // Note that for correctness, the same value should be consistently returned.
  virtual bool ShouldDisableSiteIsolation(
      SiteIsolationMode site_isolation_mode);

  // Retrieves names of any additional site isolation modes from the embedder.
  virtual std::vector<std::string> GetAdditionalSiteIsolationModes();

  // Called when a new dynamic isolated origin was added in |context|, and the
  // origin desires to be persisted across restarts, to give the embedder an
  // opportunity to save this isolated origin to disk.
  virtual void PersistIsolatedOrigin(
      BrowserContext* context,
      const url::Origin& origin,
      ChildProcessSecurityPolicy::IsolatedOriginSource source) {}

  // Returns true if the given URL needs be loaded with the "isolated
  // application" isolation level. COOP/COEP headers must also be properly set
  // in order to enable the application isolation level.
  virtual bool ShouldUrlUseApplicationIsolationLevel(
      BrowserContext* browser_context,
      const GURL& url);

  // Allows the embedder to enable access to Isolated Context Web APIs for the
  // given |lock_url| -- the URL to which the renderer process is locked.
  // See [IsolatedContext] IDL attribute for more details.
  virtual bool IsIsolatedContextAllowedForUrl(BrowserContext* browser_context,
                                              const GURL& lock_url);

  // Check if applications whose origin is |origin| are allowed to perform
  // all-screens-auto-selection, which allows automatic capturing of all
  // screens with the getAllScreensMedia API.
  virtual bool IsGetAllScreensMediaAllowed(content::BrowserContext* context,
                                           const url::Origin& origin);

  // Allow the embedder to control the maximum renderer process count. Only
  // applies if it is set to a non-zero value.  Once this limit is exceeded,
  // existing processes will be reused whenever possible, see
  // `ShouldTryToUseExistingProcessHost()`.
  virtual size_t GetMaxRendererProcessCountOverride();

  // Indicates whether a file path should be accessible via file URL given a
  // request from a browser context which lives within |profile_path|.
  //
  // On POSIX platforms, |absolute_path| is the path after resolving all
  // symboling links. On Windows, if the file URL is a shortcut,
  // IsFileAccessAllowed will be called twice: Once for the shortcut, which is
  // treated like a redirect, and once for the destination path after following
  // the shortcut, assuming access to the shortcut path was allowed.
  virtual bool IsFileAccessAllowed(const base::FilePath& path,
                                   const base::FilePath& absolute_path,
                                   const base::FilePath& profile_path);

  // Indicates whether to force the MIME sniffer to sniff file URLs for HTML.
  // By default, disabled. May be called on either the UI or IO threads.
  // See https://crbug.com/777737
  virtual bool ForceSniffingFileUrlsForHtml();

  // Allows the embedder to pass extra command line flags.
  // switches::kProcessType will already be set at this point.
  virtual void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                              int child_process_id) {}

  // Returns a client GUID used for virus scanning.
  virtual std::string GetApplicationClientGUIDForQuarantineCheck();

  // Gets a callback which can connect to a Quarantine Service instance if
  // available.
  virtual download::QuarantineConnectionCallback
  GetQuarantineConnectionCallback();

  // Returns the locale used by the application.
  // This is called on the UI and IO threads.
  virtual std::string GetApplicationLocale();

  // Returns a comma-separate list of language codes, in order of preference.
  // The legacy "accept-language" header is not affected by this setting (see
  // |ConfigureNetworkContextParams()| below).
  // (Not called GetAcceptLanguages so it doesn't clash with win32).
  virtual std::string GetAcceptLangs(BrowserContext* context);

  // Returns the default favicon.
  virtual gfx::ImageSkia GetDefaultFavicon();

  // Returns the fully qualified path to the log file name, or an empty path.
  // This function is used by the sandbox to allow write access to the log.
  virtual base::FilePath GetLoggingFileName(
      const base::CommandLine& command_line);

  // Allows the embedder to control if a service worker is allowed at the given
  // `scope` and can be accessed from `site_for_cookies` and `top_frame_origin`.
  // `site_for_cookies` is used to determine whether the request is done in a
  // third-party context. `top_frame_origin` is used to check if any
  // content_setting affects this request. Only calls that are made within the
  // context of a tab can provide a proper `top_frame_origin`, otherwise the
  // scope of the service worker is used.
  // This function is called whenever an attempt is made to create or access the
  // persistent state of the registration, or to start the service worker.
  //
  // If non-empty, `script_url` is the script of the service worker that is
  // attempted to be registered or started. If it's empty, an attempt is being
  // made to access the registration but there is no specific service worker in
  // the registration being acted on.
  //
  // This is called on the UI thread.
  virtual AllowServiceWorkerResult AllowServiceWorker(
      const GURL& scope,
      const net::SiteForCookies& site_for_cookies,
      const absl::optional<url::Origin>& top_frame_origin,
      const GURL& script_url,
      BrowserContext* context);

  // Returns true if the service worker associated with the given `scope` may be
  // deleted. This can return false if the service worker is tied to another
  // service that fundamentally should not be allowed to be removed (today, this
  // is limited to extensions).
  virtual bool MayDeleteServiceWorkerRegistration(
      const GURL& scope,
      BrowserContext* browser_context);

  // Returns true if the content layer should attempt to update the service
  // worker associated with the given `scope`. This can return false if the
  // service worker is tied to another service that handles the update flow
  // (today, this is limited to extensions).
  virtual bool ShouldTryToUpdateServiceWorkerRegistration(
      const GURL& scope,
      BrowserContext* browser_context);

  // Allows the embedder to enable process-wide blink features before starting a
  // service worker. This is similar to
  // `blink.mojom.CommitNavigationParams.force_enabled_origin_trials` but for
  // RuntimeFeatures instead of Origin Trials.
  //
  // This method is only called when the process that will run the Service
  // Worker is isolated. These features can be highly privileged, so the
  // renderer process with such features enabled shouldn't be used for other
  // sites.
  virtual void UpdateEnabledBlinkRuntimeFeaturesInIsolatedWorker(
      BrowserContext* context,
      const GURL& script_url,
      std::vector<std::string>& out_forced_enabled_runtime_features);

  // Allow the embedder to control if a Shared Worker can be connected from a
  // given tab.
  // This is called on the UI thread.
  virtual bool AllowSharedWorker(
      const GURL& worker_url,
      const net::SiteForCookies& site_for_cookies,
      const absl::optional<url::Origin>& top_frame_origin,
      const std::string& name,
      const blink::StorageKey& storage_key,
      BrowserContext* context,
      int render_process_id,
      int render_frame_id);

  // Allow the embedder to control if a page/worker with |scheme| URL can create
  // a cross-origin shared workers.
  virtual bool DoesSchemeAllowCrossOriginSharedWorker(
      const std::string& scheme);

  // Allows the embedder to control whether Signed HTTP Exchanges (SXG) can be
  // loaded. This is called on the UI thread.
  virtual bool AllowSignedExchange(BrowserContext* context);

  virtual bool IsDataSaverEnabled(BrowserContext* context);

  // Updates the given prefs for Service Worker and Shared Worker. The prefs
  // are to be sent to the renderer process when a worker is created. Note that
  // We don't use this method for Dedicated Workers as they inherit preferences
  // from their closest ancestor frame.
  virtual void UpdateRendererPreferencesForWorker(
      BrowserContext* browser_context,
      blink::RendererPreferences* out_prefs);

  // Requests access to |files| in order to be sent to |destination_url|.
  // |continuation_callback| is called with a token that should be held until
  // `open()` operation on the files is finished.
  virtual void RequestFilesAccess(
      const std::vector<base::FilePath>& files,
      const GURL& destination_url,
      base::OnceCallback<void(file_access::ScopedFileAccess)>
          continuation_callback);

  // Allow the embedder to control if access to file system by a shared worker
  // is allowed.
  virtual void AllowWorkerFileSystem(
      const GURL& url,
      BrowserContext* browser_context,
      const std::vector<GlobalRenderFrameHostId>& render_frames,
      base::OnceCallback<void(bool)> callback);

  // Allow the embedder to control if access to IndexedDB by a shared worker
  // is allowed.
  virtual bool AllowWorkerIndexedDB(
      const GURL& url,
      BrowserContext* browser_context,
      const std::vector<GlobalRenderFrameHostId>& render_frames);

  // Allow the embedder to control if access to Web Locks by a shared worker
  // is allowed.
  virtual bool AllowWorkerWebLocks(
      const GURL& url,
      BrowserContext* browser_context,
      const std::vector<GlobalRenderFrameHostId>& render_frames);

  // Allow the embedder to control if access to CacheStorage by a shared worker
  // is allowed.
  virtual bool AllowWorkerCacheStorage(
      const GURL& url,
      BrowserContext* browser_context,
      const std::vector<GlobalRenderFrameHostId>& render_frames);

  // Allow the embedder to control whether we can use Web Bluetooth.
  // TODO(crbug.com/589228): Replace this with a use of the permission system.
  enum class AllowWebBluetoothResult {
    ALLOW,
    BLOCK_POLICY,
    BLOCK_GLOBALLY_DISABLED,
  };
  virtual AllowWebBluetoothResult AllowWebBluetooth(
      content::BrowserContext* browser_context,
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin);

  // Returns a blocklist of UUIDs that have restrictions when accessed
  // via Web Bluetooth. Parsed by BluetoothBlocklist::Add().
  //
  // The blocklist string must be a comma-separated list of UUID:exclusion
  // pairs. The pairs may be separated by whitespace. Pair components are
  // colon-separated and must not have whitespace around the colon.
  //
  // UUIDs are a string that BluetoothUUID can parse (See BluetoothUUID
  // constructor comment). Exclusion values are a single lower case character
  // string "e", "r", or "w" for EXCLUDE, EXCLUDE_READS, or EXCLUDE_WRITES.
  //
  // Example:
  // "1812:e, 00001800-0000-1000-8000-00805f9b34fb:w, ignored:1, alsoignored."
  virtual std::string GetWebBluetoothBlocklist();

  using InterestGroupApiOperation = content::InterestGroupApiOperation;

  // Returns whether |api_origin| on |top_frame_origin| can perform
  // |operation| within the interest group API.
  virtual bool IsInterestGroupAPIAllowed(
      content::RenderFrameHost* render_frame_host,
      InterestGroupApiOperation operation,
      const url::Origin& top_frame_origin,
      const url::Origin& api_origin);

  // Returns whether |destination_origin| can receive beacons sent through
  // window.fence.reportEvent() or automatic beacons.
  virtual bool IsPrivacySandboxReportingDestinationAttested(
      content::BrowserContext* browser_context,
      const url::Origin& destination_origin,
      content::PrivacySandboxInvokingAPI invoking_api);

  virtual void OnAuctionComplete(
      RenderFrameHost* render_frame_host,
      InterestGroupManager::InterestGroupDataKey data_key);

  enum class AttributionReportingOperation {
    kSource,
    kTrigger,
    kReport,
    kSourceVerboseDebugReport,
    kTriggerVerboseDebugReport,
    kOsSource,
    kOsTrigger,
    kOsSourceVerboseDebugReport,
    kOsTriggerVerboseDebugReport,
    kAny,
  };

  // Allows the embedder to control if Attribution Reporting API operations can
  // happen in a given context. Origins must be provided for a given operation
  // as follows:
  //   - `kSource` and `kOsSource` must provide a non-null `source_origin` and
  //   `reporting_origin`
  //   - `kTrigger` and `kOsTrigger` must provide a non-null
  //   `destination_origin` and `reporting_origin`
  //   - `kReport` must provide all non-null origins
  //   - `kAny` may provide all null origins. It checks whether conversion
  //   measurement is allowed anywhere in `browser_context`, returning false if
  //   Attribution Reporting is not allowed by default on any origin.
  virtual bool IsAttributionReportingOperationAllowed(
      content::BrowserContext* browser_context,
      AttributionReportingOperation operation,
      content::RenderFrameHost* rfh,
      const url::Origin* source_origin,
      const url::Origin* destination_origin,
      const url::Origin* reporting_origin);

  // Allows the embedder to control if web attribution reporting is allowed.
  // This method must be idempotent.
  virtual bool IsWebAttributionReportingAllowed();

  // Allows the embedder to control if an Os source event should register as
  // a Web Os or Os (App) source.
  // This method must be idempotent.
  virtual bool ShouldUseOsWebSourceAttributionReporting();

  // Allows the embedder to control if Shared Storage API operations can happen
  // in a given context.
  //
  // Note that `rfh` can be nullptr.
  virtual bool IsSharedStorageAllowed(content::BrowserContext* browser_context,
                                      content::RenderFrameHost* rfh,
                                      const url::Origin& top_frame_origin,
                                      const url::Origin& accessing_origin);

  // Allows the embedder to control if Shared Storage API `selectURL()` can
  // happen in a given context.
  virtual bool IsSharedStorageSelectURLAllowed(
      content::BrowserContext* browser_context,
      const url::Origin& top_frame_origin,
      const url::Origin& accessing_origin);

  // Allows the embedder to control if Private Aggregation API operations can
  // happen in a given context.
  virtual bool IsPrivateAggregationAllowed(
      content::BrowserContext* browser_context,
      const url::Origin& top_frame_origin,
      const url::Origin& reporting_origin);

#if BUILDFLAG(IS_CHROMEOS)
  // Notification that a trust anchor was used by the given user.
  virtual void OnTrustAnchorUsed(BrowserContext* browser_context) {}
#endif

  // Allows the embedder to implement policy for whether an SCT auditing report
  // should be sent.
  virtual bool CanSendSCTAuditingReport(BrowserContext* browser_context);

  // Notification that a new SCT auditing report has been sent.
  virtual void OnNewSCTAuditingReportSent(BrowserContext* browser_context) {}

  // Allows the embedder to override the LocationProvider implementation.
  // Return nullptr to indicate the default one for the platform should be
  // created. This is used by Qt, see
  // https://bugs.chromium.org/p/chromium/issues/detail?id=725057#c7
  virtual std::unique_ptr<device::LocationProvider>
  OverrideSystemLocationProvider();

  // Returns a SharedURLLoaderFactory attached to the system network context.
  // Must be called on the UI thread. The default implementation returns
  // nullptr.
  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetSystemSharedURLLoaderFactory();

  // Returns the system network context. This shouldn't be used to create a
  // URLLoaderFactory, instead use GetSystemSharedURLLoaderFactory(). Must be
  // called on the UI thread. The default implementation returns nullptr.
  virtual network::mojom::NetworkContext* GetSystemNetworkContext();

  // Allows an embedder to provide a Google API Key to use for network
  // geolocation queries.
  // * May be called from any thread.
  // * Default implementation returns empty string, meaning send no API key.
  virtual std::string GetGeolocationApiKey();

  // Returns the global BrowserProcessPlatformParts' GeolocationManager on
  // macOS and returns nullptr otherwise. For tests this should return a
  // FakeGeolocationManager with the LocationSystemPermissionStatus set to
  // allow.
  virtual device::GeolocationManager* GetGeolocationManager();

#if BUILDFLAG(IS_ANDROID)
  // Allows an embedder to decide whether to use the GmsCoreLocationProvider.
  virtual bool ShouldUseGmsCoreGeolocationProvider();
#endif

  // Allows the embedder to provide a storage partition configuration for a
  // site. A storage partition configuration includes a domain of the embedder's
  // choice, an optional name within that domain, and whether the partition is
  // in-memory only.
  virtual StoragePartitionConfig GetStoragePartitionConfigForSite(
      BrowserContext* browser_context,
      const GURL& site);

  // Allows the embedder to provide settings that determine if generated code
  // can be cached and the amount of disk space used for caching generated code.
  virtual GeneratedCodeCacheSettings GetGeneratedCodeCacheSettings(
      BrowserContext* context);

  // Informs the embedder that a certificate error has occurred. If
  // |overridable| is true and if |strict_enforcement| is false, the user
  // can ignore the error and continue. The embedder can call the callback
  // asynchronously.
  virtual void AllowCertificateError(
      WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      bool is_primary_main_frame_request,
      bool strict_enforcement,
      base::OnceCallback<void(CertificateRequestResultType)> callback);

  // Returns true if all requests with certificate errors should be blocked
  // for a given |main_frame_url|, regardless of any other security settings.
  virtual bool ShouldDenyRequestOnCertificateError(const GURL main_frame_url);

  // Selects a SSL client certificate and returns it to the |delegate|. Note:
  // |delegate| may be called synchronously or asynchronously.
  //
  // Returns a callback that cancels the UI element corresponding to this
  // request. The callback should expect to be invoked on the UI thread. The
  // callback may be null. The callback is not required to be invoked.
  // `web_contents` may be null if the requestor was called from something
  // without an associated WebContents, like a service worker. In this case, UI
  // should not be shown, but a certificate may still be provided (such as when
  // the certificate is auto-selected by policy).
  virtual base::OnceClosure SelectClientCertificate(
      BrowserContext* browser_context,
      WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      net::ClientCertIdentityList client_certs,
      std::unique_ptr<ClientCertificateDelegate> delegate);

  // Returns a class to get notifications about media event. The embedder can
  // return nullptr if they're not interested.
  virtual MediaObserver* GetMediaObserver();

  // Returns a class to observe usage of features. The embedder can return
  // nullptr if they're not interested. The returned FeatureObserverClient must
  // stay alive until BrowserMainParts::PostDestroyThreads() is called and
  // threads are destroyed.  Its interface will always be called from the same
  // sequence.
  virtual FeatureObserverClient* GetFeatureObserverClient();

  // Returns true if the given page is allowed to open a window of the given
  // type. If true is returned, |no_javascript_access| will indicate whether
  // the window that is created should be scriptable/in the same process.
  // This is called on the UI thread.
  virtual bool CanCreateWindow(
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
      bool* no_javascript_access);

  // Allows the embedder to return a delegate for the SpeechRecognitionManager.
  // The delegate will be owned by the manager. It's valid to return nullptr.
  virtual SpeechRecognitionManagerDelegate*
  CreateSpeechRecognitionManagerDelegate();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Allows the embedder to return a delegate for the TtsController.
  virtual TtsControllerDelegate* GetTtsControllerDelegate();
#endif

  // Applies policy-dictated changes to the manifest that was loaded from the
  // provided render_frame_host.
  virtual void MaybeOverrideManifest(RenderFrameHost* render_frame_host,
                                     blink::mojom::ManifestPtr& manifest) {}

  // Allows the embedder to return a TTS platform implementation.
  virtual TtsPlatform* GetTtsPlatform();

#if !BUILDFLAG(IS_ANDROID)
  // Allows the embedder to return a DirectSocketsDelegate
  // implementation.
  virtual DirectSocketsDelegate* GetDirectSocketsDelegate();
#endif

  // Called by WebContents to override the WebKit preferences that are used by
  // the renderer. The content layer will add its own settings, and then it's up
  // to the embedder to update it if it wants.
  virtual void OverrideWebkitPrefs(WebContents* web_contents,
                                   blink::web_pref::WebPreferences* prefs) {}

  // Similar to OverrideWebkitPrefs, but is only called after navigations. Some
  // attributes in WebPreferences might need its value updated after navigation,
  // and this method will give the opportunity for embedder to update them.
  // Returns true if some values |prefs| changed due to embedder override.
  virtual bool OverrideWebPreferencesAfterNavigation(
      WebContents* web_contents,
      blink::web_pref::WebPreferences* prefs);

  // Notifies that BrowserURLHandler has been created, so that the embedder can
  // optionally add their own handlers.
  virtual void BrowserURLHandlerCreated(BrowserURLHandler* handler) {}

  // Returns the default download directory.
  // This can be called on any thread.
  virtual base::FilePath GetDefaultDownloadDirectory();

  // Returns the default filename used in downloads when we have no idea what
  // else we should do with the file.
  virtual std::string GetDefaultDownloadName();

  // Returns the path to the browser shader disk cache root.
  virtual base::FilePath GetShaderDiskCacheDirectory();

  // Returns the path to the shader disk cache root for shaders generated by
  // skia.
  virtual base::FilePath GetGrShaderDiskCacheDirectory();

  // Returns the path to the shader disk cache root for shaders generated by
  // graphite dawn.
  virtual base::FilePath GetGraphiteDawnDiskCacheDirectory();

  // Returns the path to the net log default directory.
  virtual base::FilePath GetNetLogDefaultDirectory();

  // Returns the path to the First-Party Sets directory.
  virtual base::FilePath GetFirstPartySetsDirectory();

  // Returns the path to Local Traces directory.
  virtual absl::optional<base::FilePath> GetLocalTracesDirectory();

  // Notification that a pepper plugin has just been spawned. This allows the
  // embedder to add filters onto the host to implement interfaces.
  // This is called on the IO thread.
  virtual void DidCreatePpapiPlugin(BrowserPpapiHost* browser_host) {}

  // Gets the host for an external out-of-process plugin.
  virtual BrowserPpapiHost* GetExternalBrowserPpapiHost(int plugin_child_id);

  // Returns true if the socket operation specified by |params| is allowed from
  // the given |browser_context| and |url|. If |params| is nullptr, this method
  // checks the basic "socket" permission, which is for those operations that
  // don't require a specific socket permission rule.
  // |private_api| indicates whether this permission check is for the private
  // Pepper socket API or the public one.
  virtual bool AllowPepperSocketAPI(BrowserContext* browser_context,
                                    const GURL& url,
                                    bool private_api,
                                    const SocketPermissionRequest* params);

  // Returns true if the "vpnProvider" permission is allowed from the given
  // |browser_context| and |url|.
  virtual bool IsPepperVpnProviderAPIAllowed(BrowserContext* browser_context,
                                             const GURL& url);

  // Creates a new VpnServiceProxy. The caller owns the returned value. It's
  // valid to return nullptr.
  virtual std::unique_ptr<VpnServiceProxy> GetVpnServiceProxy(
      BrowserContext* browser_context);

  // Returns an implementation of a file selecition policy. Can return null.
  virtual std::unique_ptr<ui::SelectFilePolicy> CreateSelectFilePolicy(
      WebContents* web_contents);

  // Returns additional allowed scheme set which can access files in
  // FileSystem API.
  virtual void GetAdditionalAllowedSchemesForFileSystem(
      std::vector<std::string>* additional_schemes) {}

  // |schemes| is a return value parameter that gets an allowlist of schemes
  // that should bypass the Is Privileged Context check.
  // See http://www.w3.org/TR/powerful-features/#settings-privileged
  virtual void GetSchemesBypassingSecureContextCheckAllowlist(
      std::set<std::string>* schemes) {}

  // Returns auto mount handlers for URL requests for FileSystem APIs.
  virtual void GetURLRequestAutoMountHandlers(
      std::vector<storage::URLRequestAutoMountHandler>* handlers) {}

  // Returns additional file system backends for FileSystem API.
  // |browser_context| is needed in the additional FileSystemBackends.
  // It has mount points to create objects returned by additional
  // FileSystemBackends, and SpecialStoragePolicy for permission granting.
  virtual void GetAdditionalFileSystemBackends(
      BrowserContext* browser_context,
      const base::FilePath& storage_partition_path,
      std::vector<std::unique_ptr<storage::FileSystemBackend>>*
          additional_backends) {}

  // Creates a new DevToolsManagerDelegate. It's valid to return nullptr.
  virtual std::unique_ptr<content::DevToolsManagerDelegate>
  CreateDevToolsManagerDelegate();

  // Stores the new expiration time up until which events related to |service|
  // can still be logged. |service| is the int value of the
  // DevToolsBackgroundService enum. |expiration_time| can be null, denoting
  // that nothing should be recorded any more.
  virtual void UpdateDevToolsBackgroundServiceExpiration(
      BrowserContext* browser_context,
      int service,
      base::Time expiration_time);

  // Returns a mapping from a background service to the time up until which
  // recording the background service's events is still allowed.
  virtual base::flat_map<int, base::Time>
  GetDevToolsBackgroundServiceExpirations(BrowserContext* browser_context);

  // Creates a new TracingDelegate. The caller owns the returned value.
  // It's valid to return nullptr.
  virtual TracingDelegate* GetTracingDelegate();

  // Returns true if plugin referred to by the url can use
  // pp::FileIO::RequestOSFileHandle.
  virtual bool IsPluginAllowedToCallRequestOSFileHandle(
      BrowserContext* browser_context,
      const GURL& url);

  // Returns true if dev channel APIs are available for plugins.
  virtual bool IsPluginAllowedToUseDevChannelAPIs(
      BrowserContext* browser_context,
      const GURL& url);

  // Allows to register browser interfaces exposed through the
  // RenderProcessHost. Note that interface factory callbacks added to
  // |registry| will by default be run immediately on the IO thread, unless a
  // task runner is provided.
  virtual void ExposeInterfacesToRenderer(
      service_manager::BinderRegistry* registry,
      blink::AssociatedInterfaceRegistry* associated_registry,
      RenderProcessHost* render_process_host) {}

  // Called to bind additional frame-bound media interfaces to the renderer.
  virtual void BindMediaServiceReceiver(RenderFrameHost* render_frame_host,
                                        mojo::GenericPendingReceiver receiver) {
  }

  // The Media Service can run in many different types of configurations
  // (typically in the GPU process or its own isolated process), but some
  // clients want an additional dedicated instance to use for specific
  // operations (e.g. for rendering protected content). Clients should override
  // this to create such an instance, and return a bound Remote to control it.
  // See MediaInterfaceProxy and MediaInterfaceFactoryHolder for usage.
  virtual mojo::Remote<media::mojom::MediaService> RunSecondaryMediaService();

  // Allows to register browser interfaces exposed through the RenderFrameHost.
  // This mechanism will replace interface registries and binders used for
  // handling InterfaceProvider's GetInterface() calls (see crbug.com/718652).
  virtual void RegisterBrowserInterfaceBindersForFrame(
      RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<RenderFrameHost*>* map) {}

  // Allows the embedder to control when Mojo interface binders are run for a
  // frame that is being prerendered.
  //
  // Prerender2 limits inactivated pages' capabilities by controlling when to
  // bind Mojo interfaces. See content/browser/preloading/prerender/README.md
  // for more about capability control.
  //
  // The embedder can add entries to `policy_map` for interfaces that it
  // registers in `RegisterBrowserInterfaceBindersForFrame()` and
  // `RegisterAssociatedInterfaceBindersForRenderFrameHost ()`. It should not
  // change or remove existing entries.
  //
  // This function is called at most once, when the first RenderFrameHost is
  // created for prerendering a page that is same-origin to the page that
  // triggered the prerender.
  virtual void RegisterMojoBinderPoliciesForSameOriginPrerendering(
      MojoBinderPolicyMap& policy_map) {}

  // Allows to register browser interfaces which are exposed to a service worker
  // execution context.
  virtual void RegisterBrowserInterfaceBindersForServiceWorker(
      BrowserContext* browser_context,
      const ServiceWorkerVersionBaseInfo& service_worker_version_info,
      mojo::BinderMapWithContext<const ServiceWorkerVersionBaseInfo&>* map) {}

  // Allows the embedder to register per-WebUI interface brokers that are used
  // for handling Mojo.bindInterface in WebUI JavaScript.
  //
  // The exposed interfaces are grouped by the WebUI controller type. For any
  // given WebUI page, only the interfaces corresponding to its controller type
  // will be exposed.
  virtual void RegisterWebUIInterfaceBrokers(
      WebUIBrowserInterfaceBrokerRegistry& registry) {}

  // Allows the embedder to register browser channel-associated interfaces that
  // are exposed through the RenderFrameHost.
  virtual void RegisterAssociatedInterfaceBindersForRenderFrameHost(
      RenderFrameHost& render_frame_host,
      blink::AssociatedInterfaceRegistry& associated_registry);

  // Handles an unhandled incoming interface binding request from the GPU
  // process. Called on the IO thread.
  virtual void BindGpuHostReceiver(mojo::GenericPendingReceiver receiver) {}

  // Handles an unhandled incoming interface binding request from a Utility
  // process. Called on the IO thread.
  virtual void BindUtilityHostReceiver(mojo::GenericPendingReceiver receiver) {}

  // Called on the main thread to handle an unhandled interface receiver binding
  // request from a render process. See |RenderThread::BindHostReceiver()|.
  virtual void BindHostReceiverForRenderer(
      RenderProcessHost* render_process_host,
      mojo::GenericPendingReceiver receiver) {}

  // Allows to override the visibility state of a RenderFrameHost.
  // |visibility_state| should not be null. It will only be set if needed.
  virtual void OverridePageVisibilityState(
      RenderFrameHost* render_frame_host,
      PageVisibilityState* visibility_state) {}

  // Allows an embedder to provide its own ControllerPresentationServiceDelegate
  // implementation. Returns nullptr if unavailable.
  virtual ControllerPresentationServiceDelegate*
  GetControllerPresentationServiceDelegate(WebContents* web_contents);

  // Allows an embedder to provide its own ReceiverPresentationServiceDelegate
  // implementation. Returns nullptr if unavailable. Only WebContents created
  // for offscreen presentations should be passed to this API. The WebContents
  // must belong to an incognito profile.
  virtual ReceiverPresentationServiceDelegate*
  GetReceiverPresentationServiceDelegate(WebContents* web_contents);

  // Add or remove an observer for presentations associated with `web_contents`.
  virtual void AddPresentationObserver(PresentationObserver* observer,
                                       WebContents* web_contents);
  virtual void RemovePresentationObserver(PresentationObserver* observer,
                                          WebContents* web_contents);

  // Allows programmatic opening of a new tab/window without going through
  // another WebContents. For example, from a Worker. |site_instance|
  // describes the context initiating the navigation. |callback| will be
  // invoked with the appropriate WebContents* when available.
  virtual void OpenURL(SiteInstance* site_instance,
                       const OpenURLParams& params,
                       base::OnceCallback<void(WebContents*)> callback);

  // Allows the embedder to register one or more NavigationThrottles for the
  // navigation indicated by |navigation_handle|.  A NavigationThrottle is used
  // to control the flow of a navigation on the UI thread. The embedder is
  // guaranteed that the throttles will be executed in the order they were
  // provided. NavigationThrottles are run only for document loading
  // navigations; they are specifically not run for page activating navigations
  // such as prerender activation and back-forward cache restores or for
  // navigations that don't use a URLLoader like same-document navigations.
  virtual std::vector<std::unique_ptr<NavigationThrottle>>
  CreateThrottlesForNavigation(NavigationHandle* navigation_handle);

  // Allows the embedder to register one or more CommitDeferringConditions for
  // the navigation indicated by |navigation_handle|. A
  // CommitDeferringCondition is used to delay committing a navigation until an
  // embedder-defined condition is met. Similar to NavigationThrottles,
  // CommitDeferringConditions are not used in navigations that don't use a
  // URLLoader, like same-document navigations. Unlike NavigationThrottles,
  // CommitDeferringConditions are also run on page activating navigations such
  // as prerender activation and back-forward cache restores.
  virtual std::vector<std::unique_ptr<CommitDeferringCondition>>
  CreateCommitDeferringConditionsForNavigation(
      NavigationHandle* navigation_handle,
      content::CommitDeferringCondition::NavigationType type);

  // Called at the start of the navigation to get opaque data the embedder
  // wants to see passed to the corresponding URLRequest on the IO thread.
  virtual std::unique_ptr<NavigationUIData> GetNavigationUIData(
      NavigationHandle* navigation_handle);

  // Allows the embedder to provide its own AudioManager implementation.
  // If this function returns nullptr, a default platform implementation
  // will be used.
  virtual std::unique_ptr<media::AudioManager> CreateAudioManager(
      media::AudioLogFactory* audio_log_factory);

  // The ScreenEnumerator object can be used to query all attached screens
  // at once. This function should be called on the IO thread.
  virtual std::unique_ptr<media::ScreenEnumerator> CreateScreenEnumerator()
      const;

  // Returns true if (and only if) CreateAudioManager() is implemented and
  // returns a non-null value.
  virtual bool OverridesAudioManager();

  // Returns true if the system audio echo cancellation shall be enforced.
  virtual bool EnforceSystemAudioEchoCancellation();

  // Populates |mappings| with all files that need to be mapped before launching
  // a child process.
#if (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)) || BUILDFLAG(IS_FUCHSIA)
  virtual void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::PosixFileDescriptorInfo* mappings) {}
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Populates |mappings| with all files that need to be mapped before launching
  // a Zygote process.
  virtual void GetAdditionalMappedFilesForZygote(
      base::CommandLine* command_line,
      content::PosixFileDescriptorInfo* mappings) {}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_WIN)
  // Defines flags that can be passed to PreSpawnChild.
  enum ChildSpawnFlags {
    kChildSpawnFlagNone = 0,
    kChildSpawnFlagRendererCodeIntegrity = 1 << 0,
  };

  // Defines flags that can be passed to GetAppContainerSidForSandboxType.
  enum AppContainerFlags {
    kAppContainerFlagNone = 0,
    kAppContainerFlagDisableAppContainer = 1 << 0,
  };

  // This may be called on the PROCESS_LAUNCHER thread before the child process
  // configuration is set. It gives the embedder a chance to modify the sandbox
  // configuration. Returns false if configuration is invalid and the child
  // should not spawn. Only use this for embedder-specific policies, since the
  // bulk of sandbox policies should go inside the relevant
  // SandboxedProcessLauncherDelegate.
  virtual bool PreSpawnChild(sandbox::TargetConfig* config,
                             sandbox::mojom::Sandbox sandbox_type,
                             ChildSpawnFlags flags);

  // This may be called on the PROCESS_LAUNCHER thread before the child process
  // is launched. It gives the embedder a chance to indicate that a process will
  // not be compatible with Hardware-enforced Stack Protection (CET).
  // |utility_sub_type| should match that provided on the command line to the
  // child process. Only use this for embedder-specific processes, and prefer to
  // key off Sandbox in the relevant SandboxedProcessLauncherDelegate.
  virtual bool IsUtilityCetCompatible(const std::string& utility_sub_type);

  // Returns the AppContainer SID for the specified sandboxed process type, or
  // empty string if this sandboxed process type does not support living inside
  // an AppContainer. Called on PROCESS_LAUNCHER thread.
  // `flags` can signal to the embedder any special behavior that should happen
  // for the `sandbox_type`.
  virtual std::wstring GetAppContainerSidForSandboxType(
      sandbox::mojom::Sandbox sandbox_type,
      AppContainerFlags flags);

  // Returns true if renderer App Container should be disabled.
  // This is called on the UI thread.
  virtual bool IsRendererAppContainerDisabled();

  // Returns the LPAC capability name to use for file data that the network
  // service needs to access to when running within LPAC sandbox. Embedders
  // should override this with their own unique name to ensure security of the
  // network service data.
  virtual std::wstring GetLPACCapabilityNameForNetworkService();

  // Returns whether renderer code integrity is enabled.
  // This is called on the UI thread.
  virtual bool IsRendererCodeIntegrityEnabled();

  // Performs a fast and orderly shutdown of the browser.
  virtual void SessionEnding() {}

  // Returns true if the audio process should run with high priority. false
  // otherwise.
  virtual bool ShouldEnableAudioProcessHighPriority();
#endif

  // Binds a new media remoter service to |receiver|, if supported by the
  // embedder, for the |source| that lives in the render frame represented
  // by |render_frame_host|. This may be called multiple times if there is more
  // than one source candidate in the same render frame.
  virtual void CreateMediaRemoter(
      RenderFrameHost* render_frame_host,
      mojo::PendingRemote<media::mojom::RemotingSource> source,
      mojo::PendingReceiver<media::mojom::Remoter> receiver) {}

  // Allows the embedder to register one or more URLLoaderThrottles for a
  // browser-initiated request. These include navigation requests and requests
  // for web worker scripts.
  //
  // |wc_getter| returns the WebContents of the context of the |request| when
  // available. It can return nullptr for requests for which it there are no
  // WebContents (e.g., requests for web workers).
  //
  // |navigation_ui_data| is only valid if this is a navigation request.
  //
  // |frame_tree_node_id| is also invalid (kNoFrameTreeNodeId) in some cases
  // (e.g., requests for web workers).
  //
  // This is called on the UI thread.
  virtual std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      BrowserContext* browser_context,
      const base::RepeatingCallback<WebContents*()>& wc_getter,
      NavigationUIData* navigation_ui_data,
      int frame_tree_node_id);

  // Allows the embedder to register one or more URLLoaderThrottles for handling
  // a user-initiated `fetch(url, {keepalive: true})` request from documents or
  // from worker scripts in the renderer. Note this is different from
  // `CreateURLLoaderThrottles` and `CreateThrottlesForNavigation` that are
  // created for requests initiated in the browser.
  //
  // Keepalive requests are initiated in the renderer, and have throttles
  // created by `ContentRendererClient::CreateURLLoaderThrottleProvider`.
  // However, these requests may live longer than the renderer process itself.
  // Therefore, this method is used to create browser-side throttles in addition
  // to renderer-side throttles.
  //
  // All the `URLLoaderThrottle` methods up to `WillProcessResponse` are called
  // on the browser-side throttles for keepalive requests, but are effectively
  // ignored while the renderer is still alive. However, if the renderer
  // terminates before the request has finished, subsequent calls to
  // browser-side throttle take effect, for example any throttling applied to
  // redirects. Response on the browser side is ignored, so response-related
  // throttle methods, e.g. `WillProcessResponse` itself, will not be called.
  //
  // See this section for the difference between the renderer-side throttles and
  // browser-side throttles for keepalive requests:
  // https://docs.google.com/document/d/1ZzxMMBvpqn8VZBZKnb7Go8TWjnrGcXuLS_USwVVRUvY/edit#heading=h.eu8mlvut479
  //
  // |wc_getter| returns the WebContents of the context of the |request| when
  // available. It can return nullptr for requests for which it there are no
  // WebContents (e.g., requests for web workers).
  //
  // |frame_tree_node_id| is also invalid (kNoFrameTreeNodeId) in some cases
  // (e.g., requests for web workers).
  //
  // This is called on the UI thread.
  virtual std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottlesForKeepAlive(
      const network::ResourceRequest& request,
      BrowserContext* browser_context,
      const base::RepeatingCallback<WebContents*()>& wc_getter,
      int frame_tree_node_id);

  // Allows the embedder to register per-scheme URLLoaderFactory implementations
  // to handle navigation URL requests for schemes not handled by the Network
  // Service.
  //
  // Note that a RenderFrameHost or RenderProcessHost aren't passed in because
  // these can change during a navigation (e.g. depending on redirects).
  //
  // |ukm_source_id| can be used to record UKM events associated with the
  // navigation.
  using NonNetworkURLLoaderFactoryMap =
      std::map<std::string,
               mojo::PendingRemote<network::mojom::URLLoaderFactory>>;
  virtual void RegisterNonNetworkNavigationURLLoaderFactories(
      int frame_tree_node_id,
      ukm::SourceIdObj ukm_source_id,
      NonNetworkURLLoaderFactoryMap* factories);

  // Allows the embedder to register per-scheme URLLoaderFactory
  // implementations to handle dedicated/shared worker main script requests
  // initiated by the browser process for schemes not handled by the Network
  // Service. The resulting |factories| must be used only by the browser
  // process. The caller must not send any of |factories| to any other process.
  virtual void RegisterNonNetworkWorkerMainResourceURLLoaderFactories(
      BrowserContext* browser_context,
      NonNetworkURLLoaderFactoryMap* factories);

  // Allows the embedder to register per-scheme URLLoaderFactory
  // implementations to handle service worker main/imported script requests
  // initiated by the browser process for schemes not handled by the Network
  // Service. Only called for service worker update check.
  // The resulting |factories| must be used only by the browser process. The
  // caller must not send any of |factories| to any other process.
  virtual void RegisterNonNetworkServiceWorkerUpdateURLLoaderFactories(
      BrowserContext* browser_context,
      NonNetworkURLLoaderFactoryMap* factories);

  // Allows the embedder to register per-scheme URLLoaderFactory implementations
  // to handle subresource URL requests for schemes not handled by the Network
  // Service. This function can also be used to make a factory for other
  // non-subresource requests, such as:
  //   -downloads
  //   -service worker script when starting a service worker. In that case, the
  //    frame id will be MSG_ROUTING_NONE
  virtual void RegisterNonNetworkSubresourceURLLoaderFactories(
      int render_process_id,
      int render_frame_id,
      const absl::optional<url::Origin>& request_initiator_origin,
      NonNetworkURLLoaderFactoryMap* factories);

  // Describes the purpose of the factory in WillCreateURLLoaderFactory().
  enum class URLLoaderFactoryType {
    // For navigations.
    kNavigation,

    // For downloads.
    kDownload,

    // For subresource requests from a document.
    kDocumentSubResource,

    // For the main script request of a dedicated worker or shared worker.
    kWorkerMainResource,

    // For the subresource requests from a dedicated worker or shared worker,
    // including importScripts().
    kWorkerSubResource,

    // For fetching a service worker main script or subresource scripts,
    // including importScripts().
    kServiceWorkerScript,

    // For regular fetches from a service worker (e.g., fetch(), XHR), not
    // including importScripts().
    kServiceWorkerSubResource,

    // For prefetches.
    kPrefetch,
  };

  // Allows the embedder to intercept URLLoaderFactory interfaces used by the
  // content layer to request resources from (usually) the network service.
  //
  // The parameters for URLLoaderFactory creation, namely |header_client| and
  // |factory_override|, are used in the network service where the resulting
  // factory is bound to |factory_receiver|. Note that |factory_receiver| that's
  // passed to the network service might be different from the original factory
  // receiver that was given to the embedder if the embedder had replaced it.
  //
  // |type| indicates the type of requests the factory will be used for.
  //
  // |frame| is nullptr for type kWorkerSubResource, kServiceWorkerSubResource
  // and kServiceWorkerScript. For kNavigation type, it's the RenderFrameHost
  // the navigation might commit in. Else it's the initiating frame.
  //
  // |render_process_id| is the id of a render process host in which the
  // URLLoaderFactory will be used.
  //
  // |request_initiator| indicates which origin will be the initiator of
  // requests that will use the URLLoaderFactory. It's set when this factory is
  // created a) for a renderer to use to fetch subresources
  // (kDocumentSubResource, kWorkerSubResource, kServiceWorkerSubResource), or
  // b) for the browser to use to fetch a worker (kWorkerMainResource) or
  // service worker scripts (kServiceWorkerScript).
  // An opaque origin is passed currently for navigation (kNavigation) and
  // download (kDownload) factories even though requests from these factories
  // can have a valid |network::ResourceRequest::request_initiator|.
  // Note: For the kDocumentSubResource case, the |request_initiator| may be
  // incorrect in some cases:
  // - Sandboxing flags of the frame are not taken into account, which may mean
  //   that |request_initiator| might need to be opaque but isn't.
  // - For about:blank frames, the |request_initiator| might be opaque or might
  //   be the process lock.
  // TODO(lukasza): https://crbug.com/888079: Ensure that |request_initiator| is
  // always accurate.
  //
  // |navigation_id| is valid iff |type| is |kNavigation|. It corresponds to the
  // Navigation ID returned by NavigationHandle::GetNavigationId().
  //
  // |ukm_source_id| can be used to record UKM events associated with the
  // page or worker this URLLoaderFactory is intended for (it may be
  // kInvalidUkmSourceId if there is no such ID available).
  //
  // |*factory_receiver| is always valid upon entry and MUST be valid upon
  // return. The embedder may swap out the value of |*factory_receiver| for its
  // own, in which case it must return |true| to indicate that it's proxying
  // requests for the URLLoaderFactory. Otherwise |*factory_receiver| is left
  // unmodified and this must return |false|.
  //
  // |header_client| may be bound within this call. This can be used in
  // URLLoaderFactoryParams when creating the factory.
  //
  // |bypass_redirect_checks| will be set to true when the embedder will be
  // handling redirect security checks.
  //
  // |disable_secure_dns| will be set to true when the URLLoaderFactory will be
  // used exclusively within a window that requires secure DNS to be turned off,
  // such as a window created for captive portal resolution.
  //
  // |factory_override| gives the embedder a chance to replace the network
  // service's "internal" URLLoaderFactory. See more details in the
  // documentation for URLLoaderFactoryOverride in network_context.mojom.
  // The point is to allow the embedder to override network behavior without
  // losing the security features of the network service. The embedder should
  // use |factory_override| instead of swapping out |*factory_receiver| if such
  // security features are desired.
  //
  // Prefer |factory_receiver| to this parameter if both work, as it is less
  // error-prone.
  //
  // |factory_override| may be nullptr when this WillCreateURLLoaderFactory()
  // call is for a factory that will be used for requests where such security
  // features are no-op (e.g., for navigations). Otherwise, |*factory_override|
  // is nullptr by default, and the embedder can elect to set
  // |*factory_override| to a valid override.
  //
  // |navigation_response_task_runner| is a task runner that may be used for
  // navigation request blocking tasks. Null when the URLLoaderFactory is not
  // being created for a navigation request.
  //
  // Always called on the UI thread.
  virtual bool WillCreateURLLoaderFactory(
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
      network::mojom::URLLoaderFactoryOverridePtr* factory_override,
      scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner);

  // Returns true when the embedder wants to intercept a websocket connection.
  virtual bool WillInterceptWebSocket(RenderFrameHost* frame);

  // Returns the WebSocket creation options.
  virtual uint32_t GetWebSocketOptions(RenderFrameHost* frame);

  using WebSocketFactory = base::OnceCallback<void(
      const GURL& /* url */,
      std::vector<network::mojom::HttpHeaderPtr> /* additional_headers */,
      mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>,
      mojo::PendingRemote<network::mojom::WebSocketAuthenticationHandler>,
      mojo::PendingRemote<network::mojom::TrustedHeaderClient>)>;

  // Allows the embedder to intercept a WebSocket connection. This is called
  // only when WillInterceptWebSocket returns true.
  //
  // |factory| provides a way to create a usual WebSocket connection. |url|,
  // |requested_protocols|, |user_agent|, |handshake_client| are arguments
  // given from the renderer.
  //
  // Always called on the UI thread and only when the Network Service is
  // enabled.
  virtual void CreateWebSocket(
      RenderFrameHost* frame,
      WebSocketFactory factory,
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const absl::optional<std::string>& user_agent,
      mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
          handshake_client);

  // Allows the embedder to control if establishing a WebTransport connection is
  // allowed.
  //
  // `process_id` is the ID of the process which hosts the initiator context.
  // `frame_routing_id` is the ID of the frame with which the initiator context
  // is associated, or MSG_ROUTING_NONE if there is no associated frame.
  // `url` is the destination URL and
  // `initiator_origin` is the origin of the initiator context.
  // When the connection is blocked, `callback` is called with `error`.
  // `handshake_client` will be proxied to block the connection while
  // handshaking.
  using WillCreateWebTransportCallback = base::OnceCallback<void(
      mojo::PendingRemote<network::mojom::WebTransportHandshakeClient>
          handshake_client,
      absl::optional<network::mojom::WebTransportErrorPtr> error)>;
  virtual void WillCreateWebTransport(
      int process_id,
      int frame_routing_id,
      const GURL& url,
      const url::Origin& initiator_origin,
      mojo::PendingRemote<network::mojom::WebTransportHandshakeClient>
          handshake_client,
      WillCreateWebTransportCallback callback);

  // Allows the embedder to intercept or replace the mojo objects used for
  // preference-following access to cookies. This is primarily used for objects
  // vended to renderer processes for limited, origin-locked (to |origin|),
  // access to script-accessible cookies from JavaScript, so returned objects
  // should treat their inputs as untrusted. |site_for_cookies| held by
  // |isolation_info| represents which domains the cookie manager should
  // consider to be first-party, for purposes of SameSite cookies and any
  // third-party cookie blocking the embedder may implement (if
  // |site_for_cookies| is empty, no domains are first-party).
  // |top_frame_origin| held by |isolation_info| represents the domain for
  // top-level frame, and can be used to look up preferences that are dependent
  // on that. |party_context| hold by |isolation_info| is for the purposes of
  // SameParty cookies inclusion calculation.
  //
  // |*receiver| is always valid upon entry.
  //
  // If this methods returns true, it should have created an object bound to
  // |*receiver|, and the value of |*receiver| afterwards is unusable.
  //
  // If the method returns false, it's the caller's responsibility to create
  // an appropriate RestrictedCookieManager bound to the value of |*receiver|
  // after it finishes executing --- the implementation is permitted to swap out
  // the value of |*receiver| for a different one (which permits interposition
  // of a proxy object).
  //
  // If |is_service_worker| is false, then |process_id| and |routing_id|
  // describe the frame the result is to be used from. If it's true, operations
  // are not bound to a particular frame, but are in context of a service worker
  // appropriate for |origin|.
  //
  // This is called on the UI thread.
  virtual bool WillCreateRestrictedCookieManager(
      network::mojom::RestrictedCookieManagerRole role,
      BrowserContext* browser_context,
      const url::Origin& origin,
      const net::IsolationInfo& isolation_info,
      bool is_service_worker,
      int process_id,
      int routing_id,
      mojo::PendingReceiver<network::mojom::RestrictedCookieManager>* receiver);

  // Allows the embedder to returns a list of request interceptors that can
  // intercept a navigation request.
  //
  // Always called on the IO thread and only when the Network Service is
  // enabled.
  virtual std::vector<std::unique_ptr<URLLoaderRequestInterceptor>>
  WillCreateURLLoaderRequestInterceptors(
      content::NavigationUIData* navigation_ui_data,
      int frame_tree_node_id,
      int64_t navigation_id,
      scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner);

  // Callback to handle a request for a URLLoader.
  using URLLoaderRequestHandler = base::OnceCallback<void(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client)>;

  // Allows the embedder to return a callback that is capable of loading a
  // service worker navigation preload request. Similar to
  // |WillCreateURLLoaderRequestInterceptors()|, but only allows for synchronous
  // decisions of whether to handle the preload request with no fallback. As a
  // result of being synchronous, the embedder can decide which, if there are
  // multiple, URLLoader handlers is appropriate. If the embedder returns a null
  // callback, the default behavior will be used to fetch the request.
  virtual URLLoaderRequestHandler
  CreateURLLoaderHandlerForServiceWorkerNavigationPreload(
      int frame_tree_node_id,
      const network::ResourceRequest& resource_request);

  // Called when the NetworkService, accessible through
  // content::GetNetworkService(), is created. Implementations should avoid
  // calling into GetNetworkService() again to avoid re-entrancy if the service
  // fails to start.
  virtual void OnNetworkServiceCreated(
      network::mojom::NetworkService* network_service);

  // Configures the NetworkContextParams (|network_context_params|) and
  // CertVerifierCreationParams (|cert_verifier_creation_params|) for a
  // BrowserContext's StoragePartition. StoragePartition will use the
  // NetworkService to create a new NetworkContext using these params.
  //
  // Embedders wishing to modify the initial configuration of the CertVerifier
  // should edit |cert_verifier_creation_params| rather than
  // |network_context_params->cert_verifier_params|, which will be discarded.
  //
  // By default the |network_context_params| is populated with |user_agent|
  // based on the value returned by GetUserAgent(), and with a fixed legacy
  // "accept-language" header value of "en-us,en".
  // If |in_memory| is true, |relative_partition_path| is still a path that
  // uniquely identifies the storage partition, though nothing should be written
  // to it.
  //
  // If |relative_partition_path| is the empty string, it means this needs to
  // create the default NetworkContext for the BrowserContext.
  virtual void ConfigureNetworkContextParams(
      BrowserContext* context,
      bool in_memory,
      const base::FilePath& relative_partition_path,
      network::mojom::NetworkContextParams* network_context_params,
      cert_verifier::mojom::CertVerifierCreationParams*
          cert_verifier_creation_params);

  // Returns the parent paths that contain all the network service's
  // BrowserContexts' storage. Multiple paths can be returned, e.g. in case the
  // persistent storage location differs from the cache storage location.
  virtual std::vector<base::FilePath> GetNetworkContextsParentDirectory();

  // Called once during initialization of NetworkService to provide constants
  // to NetLog.  (Though it may be called multiples times if NetworkService
  // crashes and needs to be reinitialized).  The return value is merged with
  // |GetNetConstants()| and passed to FileNetLogObserver - see documentation
  // of |FileNetLogObserver::CreateBounded()| for more information.  The
  // convention is to put new constants under a subdict at the key "clientInfo".
  virtual base::Value::Dict GetNetLogConstants();

#if BUILDFLAG(IS_ANDROID)
  // Only used by Android WebView.
  // Returns:
  //   true  - The check was successfully performed without throwing a
  //           Java exception. |*ignore_navigation| is set to the
  //           result of the check in this case.
  //   false - A Java exception was thrown. It is no longer safe to
  //           make JNI calls, because of the uncleared exception.
  //           Callers should return to the message loop as soon as
  //           possible, so that the exception can be rethrown.
  virtual bool ShouldOverrideUrlLoading(int frame_tree_node_id,
                                        bool browser_initiated,
                                        const GURL& gurl,
                                        const std::string& request_method,
                                        bool has_user_gesture,
                                        bool is_redirect,
                                        bool is_outermost_main_frame,
                                        ui::PageTransition transition,
                                        bool* ignore_navigation);

  // Returns true if navigation can synchronously continue if the frame
  // being navigated (and all child frames) do not have beforeunload handlers.
  // Synchronously continuing with navigation can lead to reentrancy in
  // content, which is not supported (triggers CHECKs). Defaults to true. The
  // ability to disable this is provided for Android WebView, which is known to
  // cause reentrancy for such synchronous navigations.
  virtual bool SupportsAvoidUnnecessaryBeforeUnloadCheckSync();
#endif

  // Called on IO or UI thread to determine whether or not to allow load and
  // render MHTML page from http/https URLs.
  virtual bool AllowRenderingMhtmlOverHttp(
      NavigationUIData* navigation_ui_data);

  // Called on IO or UI thread to determine whether or not to allow load and
  // render MHTML page from http/https URLs.
  virtual bool ShouldForceDownloadResource(const GURL& url,
                                           const std::string& mime_type);

  virtual void CreateDeviceInfoService(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::DeviceAPIService> receiver);

  virtual void CreateManagedConfigurationService(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::ManagedConfigurationService>
          receiver);

#if !BUILDFLAG(IS_ANDROID)
  // Allows the embedder to provide an implementation of the Serial API.
  virtual SerialDelegate* GetSerialDelegate();
#endif

  // Allows the embedder to provide an implementation of the WebHID API.
  virtual HidDelegate* GetHidDelegate();

  // Allows the embedder to provide an implementation of the Web Bluetooth API.
  virtual BluetoothDelegate* GetBluetoothDelegate();

  // Allows the embedder to provide an implementation of the WebUSB API.
  virtual UsbDelegate* GetUsbDelegate();

  // Allows the embedder to provide an implementation of the Private Network
  // Device API.
  virtual PrivateNetworkDeviceDelegate* GetPrivateNetworkDeviceDelegate();

  // Allows the embedder to provide an implementation of the Local Font Access
  // API.
  virtual FontAccessDelegate* GetFontAccessDelegate();

#if BUILDFLAG(IS_CHROMEOS)
  // Allows the embedder to provide an implementation of the Web Smart Card API.
  virtual SmartCardDelegate* GetSmartCardDelegate(
      BrowserContext* browser_context);
#endif

  // Attempt to open the Payment Handler window inside its corresponding
  // PaymentRequest UI surface. Returns true if the ContentBrowserClient
  // implementation supports this operation (desktop Chrome) or false otherwise.
  // |callback| is invoked with true if the window opened successfully, false if
  // the attempt failed. Both the render process and frame IDs are also passed
  // to |callback|.
  virtual bool ShowPaymentHandlerWindow(
      content::BrowserContext* browser_context,
      const GURL& url,
      base::OnceCallback<void(bool /* success */,
                              int /* render_process_id */,
                              int /* render_frame_id */)> callback);

  // Called when starting BrowserMainLoop to create a base::ThreadPoolInstance.
  // The embedder may choose to not create a thread pool; in that case
  // ThreadPoolInstance::Get() will be null. Returns true if the
  // ThreadPoolInstance was created.
  // Note: the embedder should *not* start the ThreadPoolInstance for
  // BrowserMainLoop, BrowserMainLoop itself is responsible for that.
  virtual bool CreateThreadPool(base::StringPiece name);

  // Returns an embedder-provided subclass of WebAuthenticationDelegate. This
  // allows the embedder to customize the implementation of the Web
  // Authentication API.
  virtual WebAuthenticationDelegate* GetWebAuthenticationDelegate();

#if !BUILDFLAG(IS_ANDROID)
  // Returns an AuthenticatorRequestClientDelegate subclass instance to provide
  // embedder-specific configuration for a single Web Authentication API request
  // being serviced in a given RenderFrame. The instance is guaranteed to be
  // destroyed before the RenderFrame goes out of scope. The embedder may choose
  // to return nullptr to indicate that the request cannot be serviced right
  // now.
  virtual std::unique_ptr<AuthenticatorRequestClientDelegate>
  GetWebAuthenticationRequestDelegate(RenderFrameHost* render_frame_host);
#endif

  // Get platform ClientCertStore. May return nullptr. Called on the UI thread.
  virtual std::unique_ptr<net::ClientCertStore> CreateClientCertStore(
      BrowserContext* browser_context);

  // Creates a LoginDelegate that asks the user for a username and password.
  // |web_contents| should not be null when CreateLoginDelegate is called.
  // |first_auth_attempt| is needed by AwHttpAuthHandler constructor.
  // |auth_required_callback| is used to transfer auth credentials to
  // URLRequest::SetAuth(). The credentials parameter of the callback
  // is absl::nullopt if the request should be cancelled; otherwise
  // the credentials will be used to respond to the auth challenge.
  // This method is called on the UI thread. The callback must be
  // called on the UI thread as well. If the LoginDelegate is destroyed
  // before the callback, the request has been canceled and the callback
  // should not be called.
  //
  // NOTE: For the Negotiate challenge on ChromeOS the credentials are handled
  // on OS level. In that case CreateLoginDelegate returns nullptr, since the
  // credentials are not passed to the browser and the authentication
  // should be cancelled. (See b/260522530).
  //
  // |auth_required_callback| may not be called reentrantly. If the request may
  // be handled synchronously, CreateLoginDelegate must post the callback to a
  // separate event loop iteration, taking care not to call it if the
  // LoginDelegate is destroyed in the meantime.
  //
  // There is no guarantee that |web_contents| will outlive the
  // LoginDelegate. The LoginDelegate implementation can use WebContentsObserver
  // to be notified when the WebContents is destroyed.
  //
  // TODO(davidben): This should be guaranteed. It isn't due to asynchronous
  // destruction between the network logic and the owning WebContents, but that
  // should be hidden from the embedder. Ideally this method would be on
  // WebContentsDelegate, where the lifetime ordering is more
  // obvious. https://crbug.com/456255
  virtual std::unique_ptr<LoginDelegate> CreateLoginDelegate(
      const net::AuthChallengeInfo& auth_info,
      WebContents* web_contents,
      const GlobalRequestID& request_id,
      bool is_request_for_primary_main_frame,
      const GURL& url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      bool first_auth_attempt,
      LoginAuthRequiredCallback auth_required_callback);

  // Launches the url for the given tab. Returns true if an attempt to handle
  // the url was made, e.g. by launching an app. Note that this does not
  // guarantee that the app successfully handled it.
  //
  // |initiating_origin| is the origin of the last redirecting server (falling
  // back to the request initiator if there were no redirects / if the request
  // goes straight to an external protocol, or null, e.g. in the case of
  // browser-initiated navigations. The initiating origin is intended to help
  // users make security decisions about whether to allow an external
  // application to launch.
  //
  // |initiator_document| refers to the document that initiated the navigation,
  // if it is still available. Use |initiating_origin| instead for security
  // decisions.
  //
  // |out_factory| allows the embedder to continue the navigation, by providing
  // their own URLLoader. If it isn't set, the navigation is canceled. It is
  // canceled either silently when this function returns true, or with an error
  // page otherwise.
  virtual bool HandleExternalProtocol(
      const GURL& url,
      base::RepeatingCallback<WebContents*()> web_contents_getter,
      int frame_tree_node_id,
      NavigationUIData* navigation_data,
      bool is_primary_main_frame,
      bool is_in_fenced_frame_tree,
      network::mojom::WebSandboxFlags sandbox_flags,
      ui::PageTransition page_transition,
      bool has_user_gesture,
      const absl::optional<url::Origin>& initiating_origin,
      RenderFrameHost* initiator_document,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory);

  // Creates an OverlayWindow to be used for video or Picture-in-Picture.
  // This window will house the content shown when in Picture-in-Picture mode.
  // This will return a new OverlayWindow.
  //
  // May return nullptr if embedder does not support this functionality. The
  // default implementation provides nullptr OverlayWindow.
  virtual std::unique_ptr<VideoOverlayWindow>
  CreateWindowForVideoPictureInPicture(
      VideoPictureInPictureWindowController* controller);

  // Registers the watcher to observe updates in RendererPreferences.
  virtual void RegisterRendererPreferenceWatcher(
      BrowserContext* browser_context,
      mojo::PendingRemote<blink::mojom::RendererPreferenceWatcher> watcher);

  // Returns true if it is OK to accept untrusted exchanges, such as expired
  // signed exchanges, and unsigned Web Bundles.
  // The embedder may require --user-data-dir flag and so on to accept it in
  // order to make sure that insecure contents will not persist accidentally.
  virtual bool CanAcceptUntrustedExchangesIfNeeded();

  // Called on every request completion to update the data use when network
  // service is enabled.
  virtual void OnNetworkServiceDataUseUpdate(
      GlobalRenderFrameHostId render_frame_host_id,
      int32_t network_traffic_annotation_id_hash,
      int64_t recv_bytes,
      int64_t sent_bytes);

  // Returns the absolute path to a directory in which sandboxed out-of-process
  // Storage Service instances should be confined. By default this is empty, and
  // the browser cannot create sandboxed Storage Service instances.
  virtual base::FilePath GetSandboxedStorageServiceDataDirectory();

  // Returns true if the audio service should be sandboxed. false otherwise.
  virtual bool ShouldSandboxAudioService();

  // Returns true if the network service should be sandboxed. false otherwise.
  // This is called on the UI thread.
  virtual bool ShouldSandboxNetworkService();

  // Returns true if system DNS resolution should be run outside of the network
  // service.
  virtual bool ShouldRunOutOfProcessSystemDnsResolution();

  // Browser-side API to log blink UseCounters for events that don't occur in
  // the renderer.
  virtual void LogWebFeatureForCurrentPage(
      content::RenderFrameHost* render_frame_host,
      blink::mojom::WebFeature feature) {}

  // Returns a string describing the embedder product name and version,
  // of the form "productname/version", with no other slashes.
  // Used as part of the user agent string.
  virtual std::string GetProduct();

  // Returns the user agent. This can also return the reduced user agent, based
  // on blink::features::kUserAgentReduction. Content may cache this value.
  virtual std::string GetUserAgent();

  // Returns the user agent, allowing for preferences (i.e. enterprise policy).
  // Default to the non-context |GetUserAgent| above.
  virtual std::string GetUserAgentBasedOnPolicy(
      content::BrowserContext* context);

  // Returns user agent metadata. Content may cache this value.
  virtual blink::UserAgentMetadata GetUserAgentMetadata();

  // Returns a 256x256 transparent background image of the product logo, i.e.
  // the browser icon, if available.
  virtual absl::optional<gfx::ImageSkia> GetProductLogo();

  // Returns whether |origin| should be considered a integral component similar
  // to native code, and as such whether its log messages should be recorded.
  virtual bool IsBuiltinComponent(BrowserContext* browser_context,
                                  const url::Origin& origin);

  // Returns whether given |url| has to be blocked. It's used only for renderer
  // debug URLs, as other requests are handled via NavigationThrottlers and
  // blocklist policies are applied there.
  virtual bool ShouldBlockRendererDebugURL(const GURL& url,
                                           BrowserContext* context,
                                           RenderFrameHost* render_frame_host);

  // Returns the default accessibility mode for the given browser context.
  virtual ui::AXMode GetAXModeForBrowserContext(
      BrowserContext* browser_context);

#if BUILDFLAG(IS_ANDROID)
  // Defines the heuristics we can use to enable wide color gamut (WCG).
  enum class WideColorGamutHeuristic {
    kUseDisplay,  // Use WCG if display supports it.
    kUseWindow,   // Use WCG if window is WCG.
    kNone,        // Never use WCG.
  };

  // Returns kNone by default.
  virtual WideColorGamutHeuristic GetWideColorGamutHeuristic();

  // Creates the TtsEnvironmentAndroid. A return value of null results in using
  // a default implementation.
  virtual std::unique_ptr<TtsEnvironmentAndroid> CreateTtsEnvironmentAndroid();

  // If enabled, DialogOverlays will observe the container view for location
  // changes and reposition themselves automatically. Note that this comes with
  // some overhead and should only be enabled if the embedder itself can be
  // moved. Defaults to false.
  virtual bool ShouldObserveContainerViewLocationForDialogOverlays();
#endif

  // Obtains the list of MIME types that are for plugins with external handlers.
  virtual base::flat_set<std::string> GetPluginMimeTypesWithExternalHandlers(
      BrowserContext* browser_context);

  // Possibly augment |download_policy| based on the status of |frame_host| as
  // well as |user_gesture|.
  virtual void AugmentNavigationDownloadPolicy(
      RenderFrameHost* frame_host,
      bool user_gesture,
      blink::NavigationDownloadPolicy* download_policy);

  // Writes the browsing topics for a particular requesting context into the
  // output parameter `topics` and returns whether the access permission is
  // allowed. `context_origin` and `main_frame` will potentially be used for the
  // access permission check, for calculating the topics, and/or for the
  // `BrowsingTopicsPageLoadDataTracker` to track the API usage. If `get_topics`
  // is true, topics calculation result will be stored to `topics`. If `observe`
  // is true, record the observation (i.e. the <calling context site,
  // top level site> pair) to the `BrowsingTopicsSiteDataStorage` database.
  virtual bool HandleTopicsWebApi(
      const url::Origin& context_origin,
      content::RenderFrameHost* main_frame,
      browsing_topics::ApiCallerSource caller_source,
      bool get_topics,
      bool observe,
      std::vector<blink::mojom::EpochTopicPtr>& topics);

  // Returns the number of distinct topics epochs versions for `main_frame`.
  // Must be called when topics are eligible (i.e. `HandleTopicsWebApi` would
  // return true for the same main frame context).
  virtual int NumVersionsInTopicsEpochs(
      content::RenderFrameHost* main_frame) const;

  // Returns whether a site is blocked to use Bluetooth scanning API.
  virtual bool IsBluetoothScanningBlocked(
      content::BrowserContext* browser_context,
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin);

  // Blocks a site to use Bluetooth scanning API.
  virtual void BlockBluetoothScanning(content::BrowserContext* browser_context,
                                      const url::Origin& requesting_origin,
                                      const url::Origin& embedding_origin);

  // Returns via callback:
  //  1. A boolean indicating whether persistent device IDs are allowed.
  //  2. A salt for hashing media device IDs for the given storage key.
  //  Ideally, the salt should be unique per `storage_key` and persistent if
  //  cookie access is allowed for `site_for_cookies`. However the embedder is
  //  free to return a salt that does not satisfy all these properties.
  virtual void GetMediaDeviceIDSalt(
      content::RenderFrameHost* rfh,
      const net::SiteForCookies& site_for_cookies,
      const blink::StorageKey& storage_key,
      base::OnceCallback<void(bool, const std::string&)> callback);

  // Requests an SMS from |origin_list| from a remote device with telephony
  // capabilities, for example the user's mobile phone. Callbacks |callback|
  // with the origins and one-time-code from the SMS upon success or a failure
  // type on error. The returned callback cancels receiving of the response.
  // Calling it will run |callback| if it hasn't been executed yet, otherwise it
  // will be a no-op. Returns a null callback if fetching from a remote device
  // is disabled.
  virtual base::OnceClosure FetchRemoteSms(
      content::WebContents* web_contents,
      const std::vector<url::Origin>& origin_list,
      base::OnceCallback<void(absl::optional<std::vector<url::Origin>>,
                              absl::optional<std::string>,
                              absl::optional<content::SmsFetchFailureType>)>
          callback);

  // Uploads an enterprise legacy tech event to the enterprise management server
  // if the `url` that triggers the event is matched to any enterprise policies
  // that are set by administrators.
  virtual void ReportLegacyTechEvent(
      content::RenderFrameHost* render_frame_host,
      const std::string type,
      const GURL& url,
      const std::string& filename,
      uint64_t line,
      uint64_t column);

  // Check whether paste is allowed. To paste, an implementation may require
  // a `render_frame_host` to have user activation or various permissions.
  // Primary checks should be done in the renderer, to allow for errors to
  // be emitted, but this allows for a security recheck in the browser in
  // case of compromised renderers.
  //
  // Due to potential race conditions, permissions may be disallowed here in
  // uncompromised renderers. For example, a permission may be granted when
  // checked in the renderer, but the permission may be revoked by the time
  // this check starts.
  virtual bool IsClipboardPasteAllowed(
      content::RenderFrameHost* render_frame_host);

  // Determines if a clipboard paste containing |data| of type |data_type| is
  // allowed in this renderer frame.  Possible data types supported for paste
  // are in the ClipboardHostImpl class.  Text based formats will use the
  // data_type ui::ClipboardFormatType::PlainTextType() unless it is known
  // to be of a more specific type, like RTF or HTML, in which case a type
  // such as ui::ClipboardFormatType::RtfType() or
  // ui::ClipboardFormatType::HtmlType() is used.
  //
  // It is also possible for the data type to be
  // ui::ClipboardFormatType::WebCustomDataType() indicating that the paste
  // uses a custom data format.  It is up to the implementation to attempt to
  // understand the type if possible.  It is acceptable to deny pastes of
  // unknown data types.
  //
  // The implementation is expected to show UX to the user if needed.  If
  // shown, the UX should be associated with the specific WebContents.
  //
  // The callback is called, possibly asynchronously, with a status indicating
  // whether the operation is allowed or not.  If the operation is allowed,
  // the callback is passed the data the can be pasted.
  virtual void IsClipboardPasteContentAllowed(
      content::WebContents* web_contents,
      const GURL& url,
      const ui::ClipboardFormatType& data_type,
      ClipboardPasteData content_analyisis_data,
      IsClipboardPasteContentAllowedCallback callback);

  // Returns true if a copy to the clipboard from `url` is allowed by the
  // CopyPreventionSettings policy, false otherwise. The check is only performed
  // if `data_size_in_bytes` is greater or equal than the minimum data size
  // specified in the policy. If the copy is blocked `replacement_data` will be
  // filled with an explainer message meant to be written to the clipboard for
  // the user to see.
  virtual bool IsClipboardCopyAllowed(content::BrowserContext* browser_context,
                                      const GURL& url,
                                      size_t data_size_in_bytes,
                                      std::u16string& replacement_data);

  // Allows the embedder to override normal user activation checks done when
  // entering fullscreen. For example, it is used in layout tests to allow
  // fullscreen when mock screen orientation changes.
  virtual bool CanEnterFullscreenWithoutUserActivation();

#if BUILDFLAG(ENABLE_VR)
  // Allows the embedder to provide mechanisms to integrate with WebXR
  // functionality.
  virtual XrIntegrationClient* GetXrIntegrationClient();
#endif

  // External applications and services may launch the browser in a mode which
  // exposes browser control interfaces via Mojo. Any such interface binding
  // request received from an external client is passed to this method.
  virtual void BindBrowserControlInterface(mojo::ScopedMessagePipeHandle pipe);

  // Returns true when a context (e.g., iframe) whose URL is |url| should
  // inherit the parent COEP value implicitly, similar to "blob:"
  virtual bool ShouldInheritCrossOriginEmbedderPolicyImplicitly(
      const GURL& url);

  // Returns true when a ServiceWorker whose URL is |url| should inherit its
  // PolicyContainer from its creator instead of from its main script
  // response.
  virtual bool ShouldServiceWorkerInheritPolicyContainerFromCreator(
      const GURL& url);

  // Returns whether a context with the given |origin| should be allowed to make
  // insecure private network requests.
  //
  // See the CORS-RFC1918 spec for more details:
  // https://wicg.github.io/cors-rfc1918.
  //
  // |browser_context| must not be nullptr. Caller retains ownership.
  // |origin| is the origin of a navigation ready to commit.
  virtual bool ShouldAllowInsecurePrivateNetworkRequests(
      BrowserContext* browser_context,
      const url::Origin& origin);

  // Whether the JIT should be disabled for the given |browser_context| and
  // |site_url|. Pass an empty GURL for |site_url| to get the default JIT policy
  // for the current |browser_context|.
  // |site_url| should not be resolved to an effective URL before passing to
  // this function.
  virtual bool IsJitDisabledForSite(BrowserContext* browser_context,
                                    const GURL& site_url);

  // Returns the URL-Keyed Metrics service for chrome:ukm.
  virtual ukm::UkmService* GetUkmService();

  // Returns the Origin Trials Settings. If the embedder does not support Origin
  // Trials, the method will return a null
  // blink::mojom::OriginTrialsSettingsPtr.
  virtual blink::mojom::OriginTrialsSettingsPtr GetOriginTrialsSettings();

  // Called when a keepalive request
  // (https://fetch.spec.whatwg.org/#request-keepalive-flag) is requested.
  virtual void OnKeepaliveRequestStarted(BrowserContext* browser_context);

  // Called when a keepalive request finishes either successfully or
  // unsuccessfully.
  virtual void OnKeepaliveRequestFinished();

#if BUILDFLAG(IS_MAC)
  // Sets up the embedder sandbox parameters for the given sandbox type. Returns
  // true if parameters were successfully set up or false if no additional
  // parameters were set up.
  virtual bool SetupEmbedderSandboxParameters(
      sandbox::mojom::Sandbox sandbox_type,
      sandbox::SandboxCompiler* compiler);
#endif  // BUILDFLAG(IS_MAC)

  virtual void GetHyphenationDictionary(
      base::OnceCallback<void(const base::FilePath&)>);

  // Returns true if the embedder has an error page to show for the given http
  // status code.
  virtual bool HasErrorPage(int http_status_code);

  // Creates a controller that intermediates the exchange of ID tokens for the
  // given |web_contents|.
  virtual std::unique_ptr<IdentityRequestDialogController>
  CreateIdentityRequestDialogController(WebContents* web_contents);

  // Creates an mdoc provider to fetch mdocs from native apps.
  virtual std::unique_ptr<MDocProvider> CreateMDocProvider();

  // Returns true if JS dialogs from an iframe with different origin from the
  // main frame should be disallowed.
  virtual bool SuppressDifferentOriginSubframeJSDialogs(
      BrowserContext* browser_context);

  // Allows the embedder to provide an AnchorElementPreconnectDelegate that will
  // be used to make heuristics based preconnects.
  virtual std::unique_ptr<AnchorElementPreconnectDelegate>
  CreateAnchorElementPreconnectDelegate(RenderFrameHost& render_frame_host);

  // Allows the embedder to provide a SpeculationHostDelegate that will be used
  // to process speculation rules provided by the document hosted by
  // `render_frame_host`.
  virtual std::unique_ptr<SpeculationHostDelegate>
  CreateSpeculationHostDelegate(RenderFrameHost& render_frame_host);

  // Allows the embedder to provide a PrefetchServiceDelegate that will be used
  // to make prefetches.
  virtual std::unique_ptr<PrefetchServiceDelegate>
  CreatePrefetchServiceDelegate(BrowserContext* browser_context);

  // Allows the embedder to provide a PrerenderWebContentsDelegate that will be
  // used to start prerendering in a new WebContents (i.e. new tab).
  virtual std::unique_ptr<PrerenderWebContentsDelegate>
  CreatePrerenderWebContentsDelegate();

  // Returns true if find-in-page should be disabled for a given `origin`.
  virtual bool IsFindInPageDisabledForOrigin(const url::Origin& origin);

  // Called on every WebContents creation.
  virtual void OnWebContentsCreated(WebContents* web_contents);

  // Allows overriding the policy of whether to assign documents to origin-keyed
  // agent clusters by default. That is, it controls the behaviour when the
  // Origin-Agent-Cluster header is absent.
  //
  // If the embedder returns true, this prevents the use of origin-keyed agent
  // clusters by default (i.e., when the Origin-Agent-Cluster header is absent).
  // If the embedder returns false, then the decision is based on
  // blink::features::kOriginAgentClusterDefaultEnabled instead.
  virtual bool ShouldDisableOriginAgentClusterDefault(
      BrowserContext* browser_context);

  // Whether a navigation in |browser_context| should preconnect early.
  virtual bool ShouldPreconnectNavigation(BrowserContext* browser_context);

  // Returns true if First-Party Sets is enabled. The value of this method
  // should not change in a single browser session.
  virtual bool IsFirstPartySetsEnabled();

  // Returns true iff the embedder will provide a list of First-Party Sets via
  // content::FirstPartySetsHandler::SetPublicFirstPartySets during startup, at
  // some point. If `IsFirstPartySetsEnabled()` returns false, this method will
  // still be called, but its return value will be ignored.
  //
  // If this method returns false but `IsFirstPartySetsEnabled()` returns true
  // (e.g. in tests), an empty list will be used instead of waiting for the
  // embedder to call content::FirstPartySetsHandler::SetPublicFirstPartySets.
  virtual bool WillProvidePublicFirstPartySets();

  // This returns a dictionary that an embedder can use to pass data from the
  // browser to the renderer for error pages.
  // The |error_code| is the network error as specified in
  // `net/base/net_error_list.h`. Information is returned in a struct. Default
  // implementation returns nullptr.
  virtual mojom::AlternativeErrorPageOverrideInfoPtr
  GetAlternativeErrorPageOverrideInfo(
      const GURL& url,
      content::RenderFrameHost* render_frame_host,
      content::BrowserContext* browser_context,
      int32_t error_code);

  // Handle a new-window/-tab request using an external implementation-defined
  // handler, if appropriate. Returns true iff the request was handled.
  virtual bool OpenExternally(const GURL& url,
                              WindowOpenDisposition disposition);

  // Called when a `SharedStorageWorkletHost` is created for `rfh`.
  virtual void OnSharedStorageWorkletHostCreated(RenderFrameHost* rfh) {}

  // Whether the outermost origin should be sent to the renderer. This is
  // needed if the outermost origin is an extension, but for normal pages
  // we do not want to expose this.
  virtual bool ShouldSendOutermostOriginToRenderer(
      const url::Origin& outermost_origin);

  // Returns true if a given filesystem: `url` navigation is allowed (i.e.
  // originates from a Chrome App). Returns false for all other
  // navigations.
  virtual bool IsFileSystemURLNavigationAllowed(
      content::BrowserContext* browser_context,
      const GURL& url);

  // Called when optionally blockable insecure content is displayed on a secure
  // page (resulting in mixed content).
  virtual void OnDisplayInsecureContent(WebContents* web_contents) {}

#if BUILDFLAG(IS_MAC)
  // Gets the suffix for an embedder-specific helper child process. The
  // |child_flags| is a value greater than
  // ChildProcessHost::CHILD_EMBEDDER_FIRST. The embedder-specific helper app
  // bundle should be placed next to the known //content Mac helpers in the
  // framework bundle.
  virtual std::string GetChildProcessSuffix(int child_flags);
#endif  // BUILDFLAG(IS_MAC)

  // Checks if Isolated Web Apps are enabled, e.g. by feature flag
  // or in any other way.
  virtual bool AreIsolatedWebAppsEnabled(BrowserContext* browser_context);

  // This function can serve to block third-party storage partitioning
  // from being enabled if it returns false. If it returns true, then
  // we fallback on the base feature to determine if partitioning is on.
  virtual bool IsThirdPartyStoragePartitioningAllowed(
      content::BrowserContext* browser_context,
      const url::Origin& top_level_origin);

  // Checks if file or directory pickers from the file system access web API
  // require a user gesture (transient activation). They usually do, but this
  // can be bypassed via admin policy.
  virtual bool IsTransientActivationRequiredForShowFileOrDirectoryPicker(
      WebContents* web_contents);

  // Checks if `origin` should always receive a first-party StorageKey
  // in RenderFrameHostImpl. Currently in Chrome, this is true for all
  // extension origins.
  virtual bool ShouldUseFirstPartyStorageKey(const url::Origin& origin);

  // Allows the embedder to return a delegate for the responsiveness calculator.
  // The default implementation returns nullptr.
  virtual std::unique_ptr<ResponsivenessCalculatorDelegate>
  CreateResponsivenessCalculatorDelegate();

  // Checks if the given BrowserContext can receive cookie changes when it is in
  // BFCache.
  virtual bool CanBackForwardCachedPageReceiveCookieChanges(
      content::BrowserContext& browser_context,
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const absl::optional<url::Origin>& top_frame_origin,
      const net::CookieSettingOverrides overrides);

  // Callback will be called with either an error
  // (!=`FileSystemAccessStatus::kOk`) or a list of cloud file handles as
  // result.
  using GetCloudIdentifiersCallback = base::OnceCallback<void(
      blink::mojom::FileSystemAccessErrorPtr,
      std::vector<blink::mojom::FileSystemAccessCloudIdentifierPtr>)>;

  // Retrieve the identifiers the cloud storage providers use for a given
  // file/directory.
  virtual void GetCloudIdentifiers(
      const storage::FileSystemURL& url,
      FileSystemAccessPermissionContext::HandleType handle_type,
      GetCloudIdentifiersCallback callback);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CONTENT_BROWSER_CLIENT_H_
