// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/ukm_source_id.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/util/type_safety/strong_alias.h"
#include "build/build_config.h"
#include "components/download/public/common/quarantine_connection.h"
#include "content/common/content_export.h"
#include "content/public/browser/allow_service_worker_result.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/browser/generated_code_cache_settings.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/common/page_visibility_state.h"
#include "content/public/common/window_container_type.mojom-forward.h"
#include "device/vr/buildflags/buildflags.h"
#include "media/base/video_codecs.h"
#include "media/mojo/mojom/media_service.mojom-forward.h"
#include "media/mojo/mojom/remoting.mojom-forward.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom-forward.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "services/network/public/mojom/websocket.mojom-forward.h"
#include "storage/browser/file_system/file_system_context.h"
#include "third_party/blink/public/common/loader/previews_state.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

#if (defined(OS_POSIX) && !defined(OS_MAC)) || defined(OS_FUCHSIA)
#include "base/posix/global_descriptors.h"
#endif

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
#include "content/public/browser/posix_file_descriptor_info.h"
#endif

namespace net {
class AuthCredentials;
class SiteForCookies;
}  // namespace net

class GURL;
using LoginAuthRequiredCallback =
    base::OnceCallback<void(const base::Optional<net::AuthCredentials>&)>;

namespace base {
class CommandLine;
class DictionaryValue;
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace blink {
namespace mojom {
class BadgeService;
class RendererPreferences;
class RendererPreferenceWatcher;
class WebUsbService;
class WindowFeatures;
enum class WebFeature : int32_t;
}  // namespace mojom
namespace web_pref {
struct WebPreferences;
}  // namespace web_pref
class AssociatedInterfaceRegistry;
class URLLoaderThrottle;
}  // namespace blink

namespace device {
class LocationProvider;
}  // namespace device

namespace media {
class AudioLogFactory;
class AudioManager;
enum class EncryptionScheme;
}  // namespace media

namespace mojo {
template <typename>
class BinderMapWithContext;
}  // namespace mojo

namespace network {
enum class OriginPolicyState;
class SharedURLLoaderFactory;
namespace mojom {
class TrustedHeaderClient;
}  // namespace mojom
}  // namespace network

namespace service_manager {
class Identity;
struct Manifest;
class Service;

template <typename...>
class BinderRegistryWithArgs;
using BinderRegistry = BinderRegistryWithArgs<>;

namespace mojom {
class Service;
}  // namespace mojom
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
class SeatbeltExecClient;
class TargetPolicy;
namespace policy {
enum class SandboxType;
}  // namespace policy
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
enum class PermissionType;
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
class FeatureObserverClient;
class HidDelegate;
class LoginDelegate;
class MediaObserver;
class NavigationHandle;
class NavigationThrottle;
class NavigationUIData;
class OverlayWindow;
class PictureInPictureWindowController;
class PlatformNotificationService;
class QuotaPermissionContext;
class ReceiverPresentationServiceDelegate;
class RenderFrameHost;
class RenderProcessHost;
class RenderViewHost;
class ResourceContext;
class SerialDelegate;
class SiteInstance;
class SpeechRecognitionManagerDelegate;
class StoragePartition;
class TracingDelegate;
class TtsPlatform;
class URLLoaderRequestInterceptor;
class VpnServiceProxy;
class WebContents;
class WebContentsViewDelegate;
class XrIntegrationClient;
struct GlobalFrameRoutingId;
struct GlobalRequestID;
struct MainFunctionParams;
struct NavigationDownloadPolicy;
struct OpenURLParams;
struct PepperPluginInfo;
struct Referrer;
struct SocketPermissionRequest;

#if defined(OS_ANDROID)
class TtsEnvironmentAndroid;
#endif

#if defined(OS_CHROMEOS)
class TtsControllerDelegate;
#endif

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
  // Callback used with IsClipboardPasteAllowed() method.
  using ClipboardPasteAllowed =
      util::StrongAlias<class ClipboardPasteAllowedTag, bool>;
  using IsClipboardPasteAllowedCallback =
      base::OnceCallback<void(ClipboardPasteAllowed)>;

  virtual ~ContentBrowserClient() {}

  // Allows the embedder to set any number of custom BrowserMainParts
  // implementations for the browser startup code. See comments in
  // browser_main_parts.h.
  virtual std::unique_ptr<BrowserMainParts> CreateBrowserMainParts(
      const MainFunctionParams& parameters);

  // Allows the embedder to change the default behavior of
  // BrowserThread::PostAfterStartupTask to better match whatever
  // definition of "startup" the embedder has in mind. This may be
  // called on any thread.
  // Note: see related BrowserThread::PostAfterStartupTask.
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
  // embedder to return an (optional) delegate to customize it. The view will
  // own the delegate.
  virtual WebContentsViewDelegate* GetWebContentsViewDelegate(
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
  // |is_main_frame| is true if the navigation will take place in a main frame.
  virtual bool ShouldCompareEffectiveURLsForSiteInstanceSelection(
      BrowserContext* browser_context,
      content::SiteInstance* candidate_site_instance,
      bool is_main_frame,
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
  // isolation is enabled for this URL, and is a bug workaround.
  //
  // TODO(nick): Remove this function once https://crbug.com/160576 is fixed,
  // and ProcessLock can be applied to all URLs.
  virtual bool ShouldLockProcess(BrowserContext* browser_context,
                                 const GURL& effective_url);

  // Returns a boolean indicating whether the WebUI |scheme| requires its
  // process to be locked to the WebUI origin.
  // Note: This method can be called from multiple threads. It is not safe to
  // assume it runs only on the UI thread.
  virtual bool DoesWebUISchemeRequireProcessLock(base::StringPiece scheme);

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

  // Returns a list additional WebUI schemes, if any.  These additional schemes
  // act as aliases to the chrome: scheme.  The additional schemes may or may
  // not serve specific WebUI pages depending on the particular URLDataSource
  // and its override of URLDataSource::ShouldServiceRequest.
  virtual void GetAdditionalWebUISchemes(
      std::vector<std::string>* additional_schemes) {}

  // Returns a list of additional schemes allowed for view-source.  Defaults to
  // the list of WebUI schemes returned by GetAdditionalWebUISchemes.
  virtual void GetAdditionalViewSourceSchemes(
      std::vector<std::string>* additional_schemes);

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

  // Returns whether the given process is allowed to commit |url|.  This is a
  // more conservative check than IsSuitableHost, since it is used after a
  // navigation has committed to ensure that the process did not exceed its
  // authority.
  // This is called on the UI thread.
  virtual bool CanCommitURL(RenderProcessHost* process_host, const GURL& url);

  // Allows the embedder to override parameters when navigating. Called for both
  // opening new URLs and when transferring URLs across processes.
  // |web_contents| is the WebContents the navigation will occur in, which is
  // not necessarily the WebContents the navigation was initiated from. For
  // example, a popup results in a new WebContents. In some situations
  // |web_contents| is null. This generally only occurs when code outside of
  // content triggers this function, such as restore.
  // WARNING: |web_contents| is temporary, and will be removed. See
  // https://crbug.com/1141501.
  virtual void OverrideNavigationParams(
      WebContents* web_contents,
      SiteInstance* site_instance,
      ui::PageTransition* transition,
      bool* is_renderer_initiated,
      content::Referrer* referrer,
      base::Optional<url::Origin>* initiator_origin) {}

  // Temporary hack to determine whether to skip OOPIFs on the new tab page.
  // TODO(creis): Remove when https://crbug.com/566091 is fixed.
  virtual bool ShouldStayInParentProcessForNTP(
      const GURL& url,
      SiteInstance* parent_site_instance);

  // Returns whether a new view for a given |site_url| can be launched in a
  // given |process_host|.
  virtual bool IsSuitableHost(RenderProcessHost* process_host,
                              const GURL& site_url);

  // Returns whether a new view for a new site instance can be added to a
  // given |process_host|.
  virtual bool MayReuseHost(RenderProcessHost* process_host);

  // Returns whether a new process should be created or an existing one should
  // be reused based on the URL we want to load. This should return false,
  // unless there is a good reason otherwise.
  virtual bool ShouldTryToUseExistingProcessHost(
      BrowserContext* browser_context,
      const GURL& url);

  // Returns whether or not subframes of |main_frame| should try to
  // aggressively reuse existing processes, even when below process limit.
  // This gets called when navigating a subframe to a URL that requires a
  // dedicated process and defaults to true, which minimizes the process count.
  // The embedder can choose to override this if there is a reason to avoid the
  // reuse.
  virtual bool ShouldSubframesTryToReuseExistingProcess(
      RenderFrameHost* main_frame);

  // Called when a site instance is first associated with a process.
  virtual void SiteInstanceGotProcess(SiteInstance* site_instance) {}

  // Called from a site instance's destructor.
  virtual void SiteInstanceDeleting(SiteInstance* site_instance) {}

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

  // Returns true if the passed in URL should be assigned as the site of the
  // current SiteInstance, if it does not yet have a site.
  virtual bool ShouldAssignSiteForURL(const GURL& url);

  // Allows the embedder to programmatically provide some origins that should be
  // opted into --isolate-origins mode of Site Isolation.
  virtual std::vector<url::Origin> GetOriginsRequiringDedicatedProcess();

  // Allows the embedder to programmatically control whether the
  // --site-per-process mode of Site Isolation should be used.
  //
  // Note that for correctness, the same value should be consistently returned.
  // See also https://crbug.com/825369
  virtual bool ShouldEnableStrictSiteIsolation();

  // Allows the embedder to programmatically control whether Site Isolation
  // should be disabled.
  //
  // Note that for correctness, the same value should be consistently returned.
  virtual bool ShouldDisableSiteIsolation();

  // Retrieves names of any additional site isolation modes from the embedder.
  virtual std::vector<std::string> GetAdditionalSiteIsolationModes();

  // Called when a new dynamic isolated origin was added in |context|, and the
  // origin desires to be persisted across restarts, to give the embedder an
  // opportunity to save this isolated origin to disk.
  virtual void PersistIsolatedOrigin(BrowserContext* context,
                                     const url::Origin& origin) {}

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

  // Returns the languages used in the Accept-Languages HTTP header.
  // (Not called GetAcceptLanguages so it doesn't clash with win32).
  virtual std::string GetAcceptLangs(BrowserContext* context);

  // Returns the default favicon.
  virtual gfx::ImageSkia GetDefaultFavicon();

  // Returns the fully qualified path to the log file name, or an empty path.
  // This function is used by the sandbox to allow write access to the log.
  virtual base::FilePath GetLoggingFileName(
      const base::CommandLine& command_line);

  // Allow the embedder to control if an AppCache can be used for the given url.
  // This is called on the UI thread.
  virtual bool AllowAppCache(
      const GURL& manifest_url,
      const GURL& site_for_cookies,
      const base::Optional<url::Origin>& top_frame_origin,
      BrowserContext* context);

  // Allows the embedder to control if a service worker is allowed at the given
  // |scope| and can be accessed from |site_for_cookies| and |top_frame_origin|.
  // |site_for_cookies| is used to determine whether the request is done in a
  // third-party context. |top_frame_origin| is used to check if any
  // content_setting affects this request. Only calls that are made within the
  // context of a tab can provide a proper |top_frame_origin|, otherwise the
  // scope of the service worker is used.
  // This function is called whenever an attempt is made to create or access the
  // persistent state of the registration, or to start the service worker.
  //
  // If non-empty, |script_url| is the script of the service worker that is
  // attempted to be registered or started. If it's empty, an attempt is being
  // made to access the registration but there is no specific service worker in
  // the registration being acted on.
  //
  // This is called on the IO thread.
  virtual AllowServiceWorkerResult AllowServiceWorkerOnIO(
      const GURL& scope,
      const GURL& site_for_cookies,
      const base::Optional<url::Origin>& top_frame_origin,
      const GURL& script_url,
      ResourceContext* context);
  // Same but for the UI thread.
  virtual AllowServiceWorkerResult AllowServiceWorkerOnUI(
      const GURL& scope,
      const GURL& site_for_cookies,
      const base::Optional<url::Origin>& top_frame_origin,
      const GURL& script_url,
      BrowserContext* context);

  // Allow the embedder to control if a Shared Worker can be connected from a
  // given tab.
  // This is called on the UI thread.
  virtual bool AllowSharedWorker(
      const GURL& worker_url,
      const GURL& site_for_cookies,
      const base::Optional<url::Origin>& top_frame_origin,
      const std::string& name,
      const url::Origin& constructor_origin,
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
      blink::mojom::RendererPreferences* out_prefs);

  // Allow the embedder to control if access to file system by a shared worker
  // is allowed.
  virtual void AllowWorkerFileSystem(
      const GURL& url,
      BrowserContext* browser_context,
      const std::vector<GlobalFrameRoutingId>& render_frames,
      base::OnceCallback<void(bool)> callback);

  // Allow the embedder to control if access to IndexedDB by a shared worker
  // is allowed.
  virtual bool AllowWorkerIndexedDB(
      const GURL& url,
      BrowserContext* browser_context,
      const std::vector<GlobalFrameRoutingId>& render_frames);

  // Allow the embedder to control if access to Web Locks by a shared worker
  // is allowed.
  virtual bool AllowWorkerWebLocks(
      const GURL& url,
      BrowserContext* browser_context,
      const std::vector<GlobalFrameRoutingId>& render_frames);

  // Allow the embedder to control if access to CacheStorage by a shared worker
  // is allowed.
  virtual bool AllowWorkerCacheStorage(
      const GURL& url,
      BrowserContext* browser_context,
      const std::vector<GlobalFrameRoutingId>& render_frames);

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

  // Allows the embedder to control the conversion measurement API.
  // This gates the following behaviors:
  // - Impression registration
  // - Conversion registration
  // - Conversion reports
  virtual bool AllowConversionMeasurement(
      content::BrowserContext* browser_context);

#if defined(OS_CHROMEOS)
  // Notification that a trust anchor was used by the given user.
  virtual void OnTrustAnchorUsed(BrowserContext* browser_context) {}
#endif

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

#if defined(OS_ANDROID)
  // Allows an embedder to decide whether to use the GmsCoreLocationProvider.
  virtual bool ShouldUseGmsCoreGeolocationProvider();
#endif

  // Allow the embedder to specify a string version of the storage partition
  // config with a site.
  virtual std::string GetStoragePartitionIdForSite(
      BrowserContext* browser_context,
      const GURL& site);

  // Allows the embedder to provide a validation check for |partition_id|s.
  // This domain of valid entries should match the range of outputs for
  // GetStoragePartitionIdForChildProcess().
  virtual bool IsValidStoragePartitionId(BrowserContext* browser_context,
                                         const std::string& partition_id);

  // Allows the embedder to provide a storage partition configuration for a
  // site. A storage partition configuration includes a domain of the embedder's
  // choice, an optional name within that domain, and whether the partition is
  // in-memory only.
  virtual StoragePartitionConfig GetStoragePartitionConfigForSite(
      BrowserContext* browser_context,
      const GURL& site);

  // Create and return a new quota permission context.
  virtual scoped_refptr<QuotaPermissionContext> CreateQuotaPermissionContext();

  // Allows the embedder to provide settings that determine if generated code
  // can be cached and the amount of disk space used for caching generated code.
  virtual GeneratedCodeCacheSettings GetGeneratedCodeCacheSettings(
      BrowserContext* context);

  // Informs the embedder that a certificate error has occured.  If
  // |overridable| is true and if |strict_enforcement| is false, the user
  // can ignore the error and continue. The embedder can call the callback
  // asynchronously.
  virtual void AllowCertificateError(
      WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      bool is_main_frame_request,
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
  //
  // TODO(davidben): Move this hook to WebContentsDelegate.
  virtual base::OnceClosure SelectClientCertificate(
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

  // Returns the platform notification service, capable of displaying Web
  // Notifications to the user. The embedder can return a nullptr if they don't
  // support this functionality. Must be called on the UI thread.
  // TODO(knollr): move this to the BrowserContext.
  virtual PlatformNotificationService* GetPlatformNotificationService(
      BrowserContext* browser_context);

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

#if defined(OS_CHROMEOS)
  // Allows the embedder to return a delegate for the TtsController.
  virtual TtsControllerDelegate* GetTtsControllerDelegate();
#endif

  // Allows the embedder to return a TTS platform implementation.
  virtual TtsPlatform* GetTtsPlatform();

  // Called by WebContents to override the WebKit preferences that are used by
  // the renderer. The content layer will add its own settings, and then it's up
  // to the embedder to update it if it wants.
  virtual void OverrideWebkitPrefs(RenderViewHost* render_view_host,
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

  // Returns the path to the font lookup table cache directory in which - on
  // Windows 7 & 8 - we cache font name meta information to perform @font-face {
  // src: local() } lookups.
  virtual base::FilePath GetFontLookupTableCacheDir();

  // Returns the path to the browser shader disk cache root.
  virtual base::FilePath GetShaderDiskCacheDirectory();

  // Returns the path to the shader disk cache root for shaders generated by
  // skia.
  virtual base::FilePath GetGrShaderDiskCacheDirectory();

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

  // Creates a new DevToolsManagerDelegate. The caller owns the returned value.
  // It's valid to return nullptr.
  virtual DevToolsManagerDelegate* GetDevToolsManagerDelegate();

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

  // Content was unable to bind a receiver for this associated interface, so the
  // embedder should try. Returns true if the |handle| was actually taken and
  // bound; false otherwise.
  virtual bool BindAssociatedReceiverFromFrame(
      RenderFrameHost* render_frame_host,
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle* handle);

  // TODO(https://crbug.com/1045214): Generalize ContentBrowserClient support
  // for service worker-scoped binders.
  //
  // Binds a remote ServiceWorkerGlobalScope to a badge service.  After
  // receiving a badge update from a ServiceWorkerGlobalScope, the badge
  // service must update the badge for each app under |service_worker_scope|.
  virtual void BindBadgeServiceReceiverFromServiceWorker(
      RenderProcessHost* service_worker_process_host,
      const GURL& service_worker_scope,
      mojo::PendingReceiver<blink::mojom::BadgeService> receiver) {}

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

  // Called just before the Service Manager is initialized.
  virtual void WillStartServiceManager() {}

  // Handles a service instance request for a new service instance with identity
  // |identity|. If the client knows how to run the named service, it should
  // bind |*receiver| accordingly, in the browser process.
  //
  // Note that this runs on the main thread, so if a service may need to start
  // and run on the IO thread while the main thread is blocking on something,
  // the service request should instead be handled by
  // |RunServiceInstanceOnIOThread| below.
  virtual void RunServiceInstance(
      const service_manager::Identity& identity,
      mojo::PendingReceiver<service_manager::mojom::Service>* receiver);

  // Allows the embedder to terminate the browser if a specific service instance
  // quits or crashes.
  virtual bool ShouldTerminateOnServiceQuit(
      const service_manager::Identity& id);

  // Allows the embedder to amend service manifests for existing services.
  // Specifically, the sets of exposed and required capabilities, interface
  // filter capabilities (deprecated), packaged services, and preloaded files
  // will be taken from the returned Manifest and appended to those of the
  // existing Manifest for the service named |name|.
  //
  // If no overlay is provided for the service, this returns |base::nullopt|.
  virtual base::Optional<service_manager::Manifest> GetServiceManifestOverlay(
      base::StringPiece name);

  // Allows the embedder to provide extra service manifests to be registered
  // with the service manager context.
  virtual std::vector<service_manager::Manifest> GetExtraServiceManifests();

  // Allows the embedder to have a list of services started after the
  // in-process Service Manager has been initialized.
  virtual std::vector<std::string> GetStartupServices();

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
  // provided.
  virtual std::vector<std::unique_ptr<NavigationThrottle>>
  CreateThrottlesForNavigation(NavigationHandle* navigation_handle);

  // Called at the start of the navigation to get opaque data the embedder
  // wants to see passed to the corresponding URLRequest on the IO thread.
  virtual std::unique_ptr<NavigationUIData> GetNavigationUIData(
      NavigationHandle* navigation_handle);

  // Allows the embedder to provide its own AudioManager implementation.
  // If this function returns nullptr, a default platform implementation
  // will be used.
  virtual std::unique_ptr<media::AudioManager> CreateAudioManager(
      media::AudioLogFactory* audio_log_factory);

  // Returns true if (and only if) CreateAudioManager() is implemented and
  // returns a non-null value.
  virtual bool OverridesAudioManager();

  // Gets supported hardware secure |video_codecs| and |encryption_schemes| for
  // the purpose of decrypting encrypted media using a Content Decryption Module
  // (CDM) associated with |key_system|.
  virtual void GetHardwareSecureDecryptionCaps(
      const std::string& key_system,
      base::flat_set<media::VideoCodec>* video_codecs,
      base::flat_set<media::EncryptionScheme>* encryption_schemes);

  // Populates |mappings| with all files that need to be mapped before launching
  // a child process.
#if (defined(OS_POSIX) && !defined(OS_MAC)) || defined(OS_FUCHSIA)
  virtual void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::PosixFileDescriptorInfo* mappings) {}
#endif  // defined(OS_POSIX) && !defined(OS_MAC) || defined(OS_FUCHSIA)

#if defined(OS_WIN)
  // Defines flags that can be passed to PreSpawnRenderer.
  enum RendererSpawnFlags {
    NONE = 0,
    RENDERER_CODE_INTEGRITY = 1 << 0,
  };

  // This is called on the PROCESS_LAUNCHER thread before the renderer process
  // is launched. It gives the embedder a chance to add loosen the sandbox
  // policy.
  virtual bool PreSpawnRenderer(sandbox::TargetPolicy* policy,
                                RendererSpawnFlags flags);

  // Returns the AppContainer SID for the specified sandboxed process type, or
  // empty string if this sandboxed process type does not support living inside
  // an AppContainer. Called on PROCESS_LAUNCHER thread.
  virtual base::string16 GetAppContainerSidForSandboxType(
      sandbox::policy::SandboxType sandbox_type);

  // Returns whether renderer code integrity is enabled.
  // This is called on the UI thread.
  virtual bool IsRendererCodeIntegrityEnabled();
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

  // Allows the embedder to register per-scheme URLLoaderFactory implementations
  // to handle navigation URL requests for schemes not handled by the Network
  // Service.
  //
  // Note that a RenderFrameHost or RenderProcessHost aren't passed in because
  // these can change during a navigation (e.g. depending on redirects).
  //
  // |ukm_source_id| can be used to record UKM events associated with the
  // navigation.
  //
  // TODO(lukasza): https://crbug.com/1106995: Remove
  // NonNetworkURLLoaderFactoryDeprecatedMap type alias (and parameters in
  // methods below that use this type).  This type encourages incorrect lifetime
  // of factories (the factories and their clones need to be fully owned by
  // their receivers).
  using NonNetworkURLLoaderFactoryDeprecatedMap =
      std::map<std::string, std::unique_ptr<network::mojom::URLLoaderFactory>>;
  using NonNetworkURLLoaderFactoryMap =
      std::map<std::string,
               mojo::PendingRemote<network::mojom::URLLoaderFactory>>;
  virtual void RegisterNonNetworkNavigationURLLoaderFactories(
      int frame_tree_node_id,
      base::UkmSourceId ukm_source_id,
      NonNetworkURLLoaderFactoryDeprecatedMap* uniquely_owned_factories,
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
  // Service. Only called for service worker update check when
  // ServiceWorkerImportedScriptUpdateCheck is enabled.
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
  //
  // TODO(lukasza): https://crbug.com/1106995: Deprecate and remove the
  // |uniquely_owned_factories| parameter - it results in incorrect factory
  // lifetimes.
  virtual void RegisterNonNetworkSubresourceURLLoaderFactories(
      int render_process_id,
      int render_frame_id,
      NonNetworkURLLoaderFactoryDeprecatedMap* uniquely_owned_factories,
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
  // Always called on the UI thread.
  virtual bool WillCreateURLLoaderFactory(
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
      network::mojom::URLLoaderFactoryOverridePtr* factory_override);

  // Returns true when the embedder wants to intercept a websocket connection.
  virtual bool WillInterceptWebSocket(RenderFrameHost* frame);

  // Returns the WebSocket creation options.
  virtual uint32_t GetWebSocketOptions(RenderFrameHost* frame);

  using WebSocketFactory = base::OnceCallback<void(
      const GURL& /* url */,
      std::vector<network::mojom::HttpHeaderPtr> /* additional_headers */,
      mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>,
      mojo::PendingRemote<network::mojom::AuthenticationHandler>,
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
      const base::Optional<std::string>& user_agent,
      mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
          handshake_client);

  // Allows the embedder to intercept or replace the mojo objects used for
  // preference-following access to cookies. This is primarily used for objects
  // vended to renderer processes for limited, origin-locked (to |origin|),
  // access to script-accessible cookies from JavaScript, so returned objects
  // should treat their inputs as untrusted.  |site_for_cookies| represents
  // which domains the cookie manager should consider to be first-party, for
  // purposes of SameSite cookies and any third-party cookie blocking the
  // embedder may implement (if |site_for_cookies| is empty, no domains are
  // first-party). |top_frame_origin| represents the domain for top-level frame,
  // and can be used to look up preferences that are dependent on that.
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
      const net::SiteForCookies& site_for_cookies,
      const url::Origin& top_frame_origin,
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
      const scoped_refptr<network::SharedURLLoaderFactory>&
          network_loader_factory);

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
  // If the CertVerifierService is enabled, the CertVerifierCreationParams will
  // be used to create a new CertVerifierService, which will be passed to the
  // network service in NetworkContextParams. Otherwise, the
  // CertVerifierCreationParams will be placed in the NetworkContextParams and
  // sent directly to the NetworkService for in-process CertVerifier creation.
  //
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
      network::mojom::CertVerifierCreationParams*
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
  virtual base::DictionaryValue GetNetLogConstants();

#if defined(OS_ANDROID)
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
                                        bool is_main_frame,
                                        ui::PageTransition transition,
                                        bool* ignore_navigation);
#endif

  // Called on IO or UI thread to determine whether or not to allow load and
  // render MHTML page from http/https URLs.
  virtual bool AllowRenderingMhtmlOverHttp(
      NavigationUIData* navigation_ui_data);

  // Called on IO or UI thread to determine whether or not to allow load and
  // render MHTML page from http/https URLs.
  virtual bool ShouldForceDownloadResource(const GURL& url,
                                           const std::string& mime_type);

  virtual void CreateWebUsbService(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::WebUsbService> receiver);

#if !defined(OS_ANDROID)
  // Allows the embedder to provide an implementation of the Serial API.
  virtual SerialDelegate* GetSerialDelegate();
#endif

  // Allows the embedder to provide an implementation of the WebHID API.
  virtual HidDelegate* GetHidDelegate();

  // Allows the embedder to provide an implementation of the Web Bluetooth API.
  virtual BluetoothDelegate* GetBluetoothDelegate();

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

  // Returns whether a base::ThreadPoolInstance should be created when
  // BrowserMainLoop starts.
  // If false, a thread pool has been created by the embedder, and
  // BrowserMainLoop should skip creating a second one.
  // Note: the embedder should *not* start the ThreadPoolInstance for
  // BrowserMainLoop, BrowserMainLoop itself is responsible for that.
  virtual bool ShouldCreateThreadPool();

  // Returns an AuthenticatorRequestClientDelegate subclass instance to provide
  // embedder-specific configuration for a single Web Authentication API request
  // being serviced in a given RenderFrame. The instance is guaranteed to be
  // destroyed before the RenderFrame goes out of scope. The embedder may choose
  // to return nullptr to indicate that the request cannot be serviced right
  // now.
#if !defined(OS_ANDROID)
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
  // is base::nullopt if the request should be cancelled; otherwise
  // the credentials will be used to respond to the auth challenge.
  // This method is called on the UI thread. The callback must be
  // called on the UI thread as well. If the LoginDelegate is destroyed
  // before the callback, the request has been canceled and the callback
  // should not be called.
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
      bool is_request_for_main_frame,
      const GURL& url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      bool first_auth_attempt,
      LoginAuthRequiredCallback auth_required_callback);

  // Launches the url for the given tab. Returns true if an attempt to handle
  // the url was made, e.g. by launching an app. Note that this does not
  // guarantee that the app successfully handled it.
  // If this is a navigation request, then |child_id| will be
  // ChildProcessHost::kInvalidUniqueID and |navigation_ui_data| will valid.
  // Otherwise child_id will be the process id and |navigation_ui_data| will be
  // nullptr.
  //
  // |initiating_origin| is the origin that initiated the navigation to the
  // external protocol, and may be null, e.g. in the case of browser-initiated
  // navigations. The initiating origin is intended to help users make security
  // decisions about whether to allow an external application to launch.
  virtual bool HandleExternalProtocol(
      const GURL& url,
      base::OnceCallback<WebContents*()> web_contents_getter,
      int child_id,
      NavigationUIData* navigation_data,
      bool is_main_frame,
      ui::PageTransition page_transition,
      bool has_user_gesture,
      const base::Optional<url::Origin>& initiating_origin,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory);

  // Creates an OverlayWindow to be used for Picture-in-Picture. This window
  // will house the content shown when in Picture-in-Picture mode. This will
  // return a new OverlayWindow.
  // May return nullptr if embedder does not support this functionality. The
  // default implementation provides nullptr OverlayWindow.
  virtual std::unique_ptr<OverlayWindow> CreateWindowForPictureInPicture(
      PictureInPictureWindowController* controller);

  // Registers the watcher to observe updates in RendererPreferences.
  virtual void RegisterRendererPreferenceWatcher(
      BrowserContext* browser_context,
      mojo::PendingRemote<blink::mojom::RendererPreferenceWatcher> watcher);

  // Returns the HTML content of the error page for Origin Policy related
  // errors.
  virtual base::Optional<std::string> GetOriginPolicyErrorPage(
      network::OriginPolicyState error_reason,
      content::NavigationHandle* navigation_handle);

  // Returns true if it is OK to accept untrusted exchanges, such as expired
  // signed exchanges, and unsigned Web Bundles.
  // The embedder may require --user-data-dir flag and so on to accept it in
  // order to make sure that insecure contents will not persist accidentally.
  virtual bool CanAcceptUntrustedExchangesIfNeeded();

  // Called on every request completion to update the data use when network
  // service is enabled.
  virtual void OnNetworkServiceDataUseUpdate(
      int32_t network_traffic_annotation_id_hash,
      int64_t recv_bytes,
      int64_t sent_bytes);

  // Returns the path to a root directory to which sandboxed out-of-process
  // Storage Service instances should be confined. By default this is empty,
  // and the browser cannot create sandboxed Storage Service instances.
  virtual base::FilePath GetSandboxedStorageServiceDataDirectory();

  // Returns true if the audio service should be sandboxed. false otherwise.
  virtual bool ShouldSandboxAudioService();

  // Asks the embedder for the PreviewsState which says which previews should
  // be enabled for the given navigation. The PreviewsState is a bitmask of
  // potentially several Previews optimizations. |initial_state| is used to
  // keep sub-frame navigation state consistent with main frame state.
  // |current_navigation_url| is the URL that is currently being navigated to,
  // and can differ from GetURL() in |navigation_handle| on redirects.
  virtual blink::PreviewsState DetermineAllowedPreviews(
      blink::PreviewsState initial_state,
      content::NavigationHandle* navigation_handle,
      const GURL& current_navigation_url);

  // Asks the embedder for the preview state that should be committed to the
  // renderer. |initial_state| was pre-determined by |DetermineAllowedPreviews|.
  // |navigation_handle| is the corresponding navigation object.
  // |response_headers| are the response headers related to this navigation.
  virtual blink::PreviewsState DetermineCommittedPreviews(
      blink::PreviewsState initial_state,
      content::NavigationHandle* navigation_handle,
      const net::HttpResponseHeaders* response_headers);

  // Browser-side API to log blink UseCounters for events that don't occur in
  // the renderer.
  virtual void LogWebFeatureForCurrentPage(
      content::RenderFrameHost* render_frame_host,
      blink::mojom::WebFeature feature) {}

  // Returns a string describing the embedder product name and version,
  // of the form "productname/version", with no other slashes.
  // Used as part of the user agent string.
  virtual std::string GetProduct();

  // Returns the user agent.  Content may cache this value.
  virtual std::string GetUserAgent();

  // Returns user agent metadata. Content may cache this value.
  virtual blink::UserAgentMetadata GetUserAgentMetadata();

  // Returns a 256x256 transparent background image of the product logo, i.e.
  // the browser icon, if available.
  virtual base::Optional<gfx::ImageSkia> GetProductLogo();

  // Returns whether |origin| should be considered a integral component similar
  // to native code, and as such whether its log messages should be recorded.
  virtual bool IsBuiltinComponent(BrowserContext* browser_context,
                                  const url::Origin& origin);

  // Returns whether given |url| has to be blocked. It's used only for renderer
  // debug URLs, as other requests are handled via NavigationThrottlers and
  // blocklist policies are applied there.
  virtual bool ShouldBlockRendererDebugURL(const GURL& url,
                                           BrowserContext* context);

  // Returns the default accessibility mode for the given browser context.
  virtual ui::AXMode GetAXModeForBrowserContext(
      BrowserContext* browser_context);

#if defined(OS_ANDROID)
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
#endif

  // Obtains the list of MIME types that are for plugins with external handlers.
  virtual base::flat_set<std::string> GetPluginMimeTypesWithExternalHandlers(
      BrowserContext* browser_context);

  // Possibly augment |download_policy| based on the status of |frame_host| as
  // well as |user_gesture|.
  virtual void AugmentNavigationDownloadPolicy(
      const WebContents* web_contents,
      const RenderFrameHost* frame_host,
      bool user_gesture,
      NavigationDownloadPolicy* download_policy);

  // Returns the interest cohort associated with the |browser_context|.
  virtual std::string GetInterestCohortForJsApi(
      content::BrowserContext* browser_context,
      const url::Origin& requesting_origin,
      const net::SiteForCookies& site_for_cookies);

  // Returns whether a site is blocked to use Bluetooth scanning API.
  virtual bool IsBluetoothScanningBlocked(
      content::BrowserContext* browser_context,
      const url::Origin& requesting_origin,
      const url::Origin& embedding_origin);

  // Blocks a site to use Bluetooth scanning API.
  virtual void BlockBluetoothScanning(content::BrowserContext* browser_context,
                                      const url::Origin& requesting_origin,
                                      const url::Origin& embedding_origin);

  // Returns true if the extra ICU data file is available and should be used to
  // initialize ICU.
  virtual bool ShouldLoadExtraIcuDataFile();

  // Returns true if the site is allowed to use persistent media device IDs.
  virtual bool ArePersistentMediaDeviceIDsAllowed(
      content::BrowserContext* browser_context,
      const GURL& scope,
      const GURL& site_for_cookies,
      const base::Optional<url::Origin>& top_frame_origin);

  // Requests an SMS from |origin| from a remote device with telephony
  // capabilities, for example the user's mobile phone. Callbacks |callback|
  // with the contents of the SMS upon success or an empty response on error.
  virtual void FetchRemoteSms(
      content::BrowserContext* browser_context,
      const url::Origin& origin,
      base::OnceCallback<void(base::Optional<std::string>)> callback);

  // Determines if a clipboard paste using |data| of type |data_type| is allowed
  // in this renderer frame.  Possible data types supported for paste can be
  // seen in the ClipboardHostImpl class.  Text based formats will use the
  // data_type ui::ClipboardFormatType::GetPlainTextType() unless it is known
  // to be of a more specific type, like RTF or HTML, in which case a type
  // such as ui::ClipboardFormatType::GetRtfType() or
  // ui::ClipboardFormatType::GetHtmlType() is used.
  //
  // It is also possible for the data type to be
  // ui::ClipboardFormatType::GetWebCustomDataType() indicating that the paste
  // uses a custom data format.  It is up to the implementation to attempt to
  // understand the type if possible.  It is acceptable to deny pastes of
  // unknown data types.
  //
  // The implementation is expected to show UX to the user if needed.  If
  // shown, the UX should be associated with the specific WebContents.
  //
  // The callback is called, possibly asynchronously, with a status indicating
  // whether the operation is allowed or not.
  virtual void IsClipboardPasteAllowed(
      content::WebContents* web_contents,
      const GURL& url,
      const ui::ClipboardFormatType& data_type,
      const std::string& data,
      IsClipboardPasteAllowedCallback callback);

  // Allows the embedder to override normal user activation checks done when
  // entering fullscreen. For example, it is used in layout tests to allow
  // fullscreen when mock screen orientation changes.
  virtual bool CanEnterFullscreenWithoutUserActivation();

#if BUILDFLAG(ENABLE_PLUGINS)
  // Returns true if |embedder_origin| is allowed to embed a plugin described by
  // |plugin_info|.  This method allows restricting some internal plugins (like
  // Chrome's PDF plugin) to specific origins.
  virtual bool ShouldAllowPluginCreation(const url::Origin& embedder_origin,
                                         const PepperPluginInfo& plugin_info);
#endif

#if BUILDFLAG(ENABLE_VR)
  // Allows the embedder to provide mechanisms to integrate with WebXR
  // functionality.
  virtual XrIntegrationClient* GetXrIntegrationClient();
#endif

  virtual bool IsOriginTrialRequiredForAppCache(
      content::BrowserContext* browser_text);

  // External applications and services may launch the browser in a mode which
  // exposes browser control interfaces via Mojo. Any such interface binding
  // request received from an external client is passed to this method.
  virtual void BindBrowserControlInterface(mojo::ScopedMessagePipeHandle pipe);

  // Returns true when a context (e.g., iframe) whose URL is |url| should
  // inherit the parent COEP value implicitly, similar to "blob:"
  virtual bool ShouldInheritCrossOriginEmbedderPolicyImplicitly(
      const GURL& url);

  // Returns whether a context whose URL is |url| should be allowed to make
  // insecure private network requests.
  //
  // See the CORS-RFC1918 spec for more details:
  // https://wicg.github.io/cors-rfc1918.
  //
  // |browser_context| must not be nullptr. Caller retains ownership.
  // |url| is the URL of a navigation ready to commit.
  virtual bool ShouldAllowInsecurePrivateNetworkRequests(
      BrowserContext* browser_context,
      const GURL& url);

  // Returns the URL-Keyed Metrics service for chrome:ukm.
  virtual ukm::UkmService* GetUkmService();

#if defined(OS_MAC)
  // Sets up the embedder sandbox parameters for the given sandbox type. Returns
  // true if parameters were successfully set up or false if no additional
  // parameters were set up.
  virtual bool SetupEmbedderSandboxParameters(
      sandbox::policy::SandboxType sandbox_type,
      sandbox::SeatbeltExecClient* client);
#endif  // defined(OS_MAC)
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CONTENT_BROWSER_CLIENT_H_
