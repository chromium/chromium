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
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/optional.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/browser/generated_code_cache_settings.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/content_client.h"
#include "content/public/common/media_stream_request.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/renderer_preference_watcher.mojom.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/socket_permission_request.h"
#include "content/public/common/window_container_type.mojom.h"
#include "media/base/video_codecs.h"
#include "media/cdm/cdm_proxy.h"
#include "media/media_buildflags.h"
#include "media/mojo/interfaces/remoting.mojom.h"
#include "net/base/mime_util.h"
#include "net/cookies/canonical_cookie.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/websocket.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/embedded_service_info.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/service_manager/sandbox/sandbox_type.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom.h"
#include "third_party/blink/public/web/window_features.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

#if (defined(OS_POSIX) && !defined(OS_MACOSX)) || defined(OS_FUCHSIA)
#include "base/posix/global_descriptors.h"
#endif

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
#include "content/public/browser/posix_file_descriptor_info.h"
#endif

class GURL;
using LoginAuthRequiredCallback =
    base::OnceCallback<void(const base::Optional<net::AuthCredentials>&)>;

namespace base {
class CommandLine;
class FilePath;
}

namespace blink {
namespace mojom {
class WebUsbService;
}
}  // namespace blink

namespace device {
class LocationProvider;
}

namespace gfx {
class ImageSkia;
}

namespace media {
class AudioLogFactory;
class AudioManager;
enum class EncryptionMode;
}

namespace mojo {
class ScopedInterfaceEndpointHandle;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace service_manager {
class Identity;
class Service;
struct BindSourceInfo;
}

namespace net {
class AuthChallengeInfo;
class AuthCredentials;
class ClientCertIdentity;
using ClientCertIdentityList = std::vector<std::unique_ptr<ClientCertIdentity>>;
class ClientCertStore;
class CookieStore;
class HttpRequestHeaders;
class NetLog;
class SSLCertRequestInfo;
class SSLInfo;
class URLRequest;
}  // namespace net

namespace network {
namespace mojom {
class NetworkService;
}
struct ResourceRequest;
}  // namespace network

namespace rappor {
class RapporService;
}

namespace sandbox {
class TargetPolicy;
}

namespace ui {
class SelectFilePolicy;
}

namespace url {
class Origin;
}

namespace storage {
class FileSystemBackend;
}

namespace content {

enum class PermissionType;
class AuthenticatorRequestClientDelegate;
class BrowserChildProcessHost;
class BrowserContext;
class BrowserMainParts;
class BrowserPpapiHost;
class BrowserURLHandler;
class ClientCertificateDelegate;
class ControllerPresentationServiceDelegate;
class DevToolsManagerDelegate;
class LoginDelegate;
class MediaObserver;
class NavigationHandle;
class NavigationUIData;
class PlatformNotificationService;
class QuotaPermissionContext;
class ReceiverPresentationServiceDelegate;
class RenderFrameHost;
class RenderProcessHost;
class RenderViewHost;
class ResourceContext;
class ServiceManagerConnection;
class SiteInstance;
class SpeechRecognitionManagerDelegate;
class StoragePartition;
class TracingDelegate;
class URLLoaderRequestInterceptor;
class URLLoaderThrottle;
class VpnServiceProxy;
class WebContents;
class WebContentsViewDelegate;
enum class OriginPolicyErrorReason;
struct MainFunctionParams;
struct OpenURLParams;
struct Referrer;
struct RendererPreferences;
struct WebPreferences;

CONTENT_EXPORT void OverrideOnBindInterface(
    const service_manager::BindSourceInfo& remote_info,
    const std::string& name,
    mojo::ScopedMessagePipeHandle* handle);

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
  virtual ~ContentBrowserClient() {}

  // Allows the embedder to set any number of custom BrowserMainParts
  // implementations for the browser startup code. See comments in
  // browser_main_parts.h.
  virtual BrowserMainParts* CreateBrowserMainParts(
      const MainFunctionParams& parameters);

  // Allows the embedder to change the default behavior of
  // BrowserThread::PostAfterStartupTask to better match whatever
  // definition of "startup" the embedder has in mind. This may be
  // called on any thread.
  // Note: see related BrowserThread::PostAfterStartupTask.
  virtual void PostAfterStartupTask(
      const base::Location& from_here,
      const scoped_refptr<base::TaskRunner>& task_runner,
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

  // If content creates the WebContentsView implementation, it will ask the
  // embedder to return an (optional) delegate to customize it. The view will
  // own the delegate.
  virtual WebContentsViewDelegate* GetWebContentsViewDelegate(
      WebContents* web_contents);

  // Allow embedder control GPU process launch retry on failure behavior.
  virtual bool AllowGpuLaunchRetryOnIOThread();

  // Notifies that a render process will be created. This is called before
  // the content layer adds its own BrowserMessageFilters, so that the
  // embedder's IPC filters have priority.
  //
  // If the client provides a service request, the content layer will ask the
  // corresponding embedder renderer-side component to bind it to an
  // implementation at the appropriate moment during initialization.
  virtual void RenderProcessWillLaunch(
      RenderProcessHost* host,
      service_manager::mojom::ServiceRequest* service_request) {}

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

  // Returns whether gesture fling events should use the mobile-behavior gesture
  // curve for scrolling.
  virtual bool ShouldUseMobileFlingCurve() const;

  // Returns whether all instances of the specified effective URL should be
  // rendered by the same process, rather than using process-per-site-instance.
  virtual bool ShouldUseProcessPerSite(BrowserContext* browser_context,
                                       const GURL& effective_url);

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
  // and origin lock can be applied to all URLs.
  virtual bool ShouldLockToOrigin(BrowserContext* browser_context,
                                  const GURL& effective_url);

  // Returns the scheme of request initiator that should be ignored by
  // cross-origin read blocking.  nullptr can be returned to indicate that no
  // exceptions should be granted based on initiator's scheme.
  virtual const char* GetInitiatorSchemeBypassingDocumentBlocking();

  // Gives the embedder a chance to log that CORB would have blocked a response
  // if it wasn't for GetInitatorSchemeBypassingDocumentBlocking above.  Called
  // only after all the other CORB checks (potentially including sniffing) have
  // been already run / right before blocking would have otherwise happened (and
  // only for non-empty, non-4xx responses).
  // TODO(lukasza): Remove once we gather enough data.
  virtual void LogInitiatorSchemeBypassingDocumentBlocking(
      const url::Origin& initiator_origin,
      int render_process_id,
      ResourceType resource_type);

  // Called to create a URLLoaderFactory for network requests in the following
  // cases:
  // - The default factory to be used by a frame.  In this case
  //   |request_initiator| is the origin being committed in the frame (or the
  //   last origin committed in the frame).
  // - The initiator-specific factory to be used by a frame.  This happens for
  //   origins covered via
  //   RenderFrameHost::MarkInitiatorAsRequiringSeparateURLLoaderFactory.
  //
  // This method allows the //content embedder to provide a URLLoaderFactory
  // with |request_initiator|-specific properties (e.g. with relaxed
  // Cross-Origin Read Blocking enforcement as needed by some extensions).
  //
  // If the embedder doesn't want to override the URLLoaderFactory for the given
  // |request_initiator|, then it should return an invalid
  // mojo::InterfacePtrInfo.
  virtual network::mojom::URLLoaderFactoryPtrInfo
  CreateURLLoaderFactoryForNetworkRequests(
      RenderProcessHost* process,
      network::mojom::NetworkContext* network_context,
      const url::Origin& request_initiator);

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
  virtual bool LogWebUIUrl(const GURL& web_ui_url) const;

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

  // Returns whether a URL should be allowed to open from a specific context.
  // This also applies in cases where the new URL will open in another process.
  virtual bool ShouldAllowOpenURL(SiteInstance* site_instance, const GURL& url);

  // Returns whether a URL can be displayed within a WebUI for a given
  // BrowserContext. Temporary workaround while crbug.com/768526 is resolved.
  // Note: This is used by an internal Cast implementation of this class.
  virtual bool IsURLAcceptableForWebUI(BrowserContext* browser_context,
                                       const GURL& url);

  // Allows the embedder to override parameters when navigating. Called for both
  // opening new URLs and when transferring URLs across processes.
  virtual void OverrideNavigationParams(SiteInstance* site_instance,
                                        ui::PageTransition* transition,
                                        bool* is_renderer_initiated,
                                        content::Referrer* referrer) {}

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
      BrowserContext* browser_context, const GURL& url);

  // Called when a site instance is first associated with a process.
  virtual void SiteInstanceGotProcess(SiteInstance* site_instance) {}

  // Called from a site instance's destructor.
  virtual void SiteInstanceDeleting(SiteInstance* site_instance) {}

  // Returns true if for the navigation from |current_url| to |new_url|
  // in |site_instance|, a new SiteInstance and BrowsingInstance should be
  // created (even if we are in a process model that doesn't usually swap.)
  // This forces a process swap and severs script connections with existing
  // tabs.
  virtual bool ShouldSwapBrowsingInstancesForNavigation(
      SiteInstance* site_instance,
      const GURL& current_url,
      const GURL& new_url);

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

  // Indicates whether a file path should be accessible via file URL given a
  // request from a browser context which lives within |profile_path|.
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

  // Allows the content embedder to adjust the command line arguments for
  // a utility process started to run a service. This is called on a background
  // thread.
  virtual void AdjustUtilityServiceProcessCommandLine(
      const service_manager::Identity& identity,
      base::CommandLine* command_line) {}

  // Returns the locale used by the application.
  // This is called on the UI and IO threads.
  virtual std::string GetApplicationLocale();

  // Returns the languages used in the Accept-Languages HTTP header.
  // (Not called GetAcceptLanguages so it doesn't clash with win32).
  virtual std::string GetAcceptLangs(BrowserContext* context);

  // Returns the default favicon.  The callee doesn't own the given bitmap.
  virtual const gfx::ImageSkia* GetDefaultFavicon();

  // Returns the fully qualified path to the log file name, or an empty path.
  // This function is used by the sandbox to allow write access to the log.
  virtual base::FilePath GetLoggingFileName(
      const base::CommandLine& command_line);

  // Allow the embedder to control if an AppCache can be used for the given url.
  // This is called on the IO thread.
  virtual bool AllowAppCache(const GURL& manifest_url,
                             const GURL& first_party,
                             ResourceContext* context);

  // Allow the embedder to control if a Service Worker can be associated
  // with the given scope.
  // A null |wc_getter| callback indicates this is for starting a service
  // worker, which is not necessarily associated with a particular tab.
  // This is called on the IO thread.
  virtual bool AllowServiceWorker(
      const GURL& scope,
      const GURL& first_party,
      ResourceContext* context,
      base::RepeatingCallback<WebContents*()> wc_getter);

  // Allow the embedder to control if a Shared Worker can be connected from a
  // given tab.
  // This is called on the UI thread.
  virtual bool AllowSharedWorker(const GURL& worker_url,
                                 const GURL& main_frame_url,
                                 const std::string& name,
                                 const url::Origin& constructor_origin,
                                 BrowserContext* context,
                                 int render_process_id,
                                 int render_frame_id);

  virtual bool IsDataSaverEnabled(BrowserContext* context);

  // Updates the given prefs for Service Worker and Shared Worker. The prefs
  // are to be sent to the renderer process when a worker is created. Note that
  // We don't use this method for Dedicated Workers as they inherit preferences
  // from their closest ancestor frame.
  virtual void UpdateRendererPreferencesForWorker(
      BrowserContext* browser_context,
      RendererPreferences* out_prefs);

  // Allow the embedder to return additional headers that should be sent when
  // fetching |url| as well as add extra load flags.
  virtual void NavigationRequestStarted(
      int frame_tree_node_id,
      const GURL& url,
      std::unique_ptr<net::HttpRequestHeaders>* extra_headers,
      int* extra_load_flags) {}

  // Allow the embedder to modify headers for a redirect. If non-nullopt,
  // |*modified_request_headers| are applied to the request headers after
  // updating them for the redirect.
  virtual void NavigationRequestRedirected(
      int frame_tree_node_id,
      const GURL& url,
      base::Optional<net::HttpRequestHeaders>* modified_request_headers) {}

  // Allow the embedder to control if the given cookie can be read.
  // This is called on the IO thread.
  virtual bool AllowGetCookie(const GURL& url,
                              const GURL& first_party,
                              const net::CookieList& cookie_list,
                              ResourceContext* context,
                              int render_process_id,
                              int render_frame_id);

  // Allow the embedder to control if the given cookie can be set.
  // This is called on the IO thread.
  virtual bool AllowSetCookie(const GURL& url,
                              const GURL& first_party,
                              const net::CanonicalCookie& cookie,
                              ResourceContext* context,
                              int render_process_id,
                              int render_frame_id);

  // Notifies the embedder that an attempt has been made to read the cookies in
  // |cookie_list|.
  virtual void OnCookiesRead(int process_id,
                             int routing_id,
                             const GURL& url,
                             const GURL& first_party_url,
                             const net::CookieList& cookie_list,
                             bool blocked_by_policy);

  // Notifies the embedder that an attempt has been made to set |cookie|.
  virtual void OnCookieChange(int process_id,
                              int routing_id,
                              const GURL& url,
                              const GURL& first_party_url,
                              const net::CanonicalCookie& cookie,
                              bool blocked_by_policy);

  // Allow the embedder to control if access to file system by a shared worker
  // is allowed.
  // This is called on the IO thread.
  virtual void AllowWorkerFileSystem(
      const GURL& url,
      ResourceContext* context,
      const std::vector<GlobalFrameRoutingId>& render_frames,
      base::Callback<void(bool)> callback);

  // Allow the embedder to control if access to IndexedDB by a shared worker
  // is allowed.
  // This is called on the IO thread.
  virtual bool AllowWorkerIndexedDB(
      const GURL& url,
      const base::string16& name,
      ResourceContext* context,
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

  // Allow the embedder to override the cookie store for a particular URL.
  // Returns nullptr to indicate the regular cookie store should be used.
  // This is called on the IO thread.
  virtual net::CookieStore* OverrideCookieStoreForURL(const GURL& url,
                                                      ResourceContext* context);

#if defined(OS_CHROMEOS)
  // Notification that a trust anchor was used by the given user.
  virtual void OnUsedTrustAnchor(const std::string& username_hash) {}
#endif

  // Allows the embedder to override the LocationProvider implementation.
  // Return nullptr to indicate the default one for the platform should be
  // created.
  virtual std::unique_ptr<device::LocationProvider>
  OverrideSystemLocationProvider();

  // Returns a SharedURLLoaderFactory attached to the system network context.
  // Must be called on the UI thread. The default implementation returns
  // nullptr.
  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetSystemSharedURLLoaderFactory();

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

  // Allows the embedder to provide a storage parititon configuration for a
  // site. A storage partition configuration includes a domain of the embedder's
  // choice, an optional name within that domain, and whether the partition is
  // in-memory only.
  //
  // If |can_be_default| is false, the caller is telling the embedder that the
  // |site| is known to not be in the default partition. This is useful in
  // some shutdown situations where the bookkeeping logic that maps sites to
  // their partition configuration are no longer valid.
  //
  // The |partition_domain| is [a-z]* UTF-8 string, specifying the domain in
  // which partitions live (similar to namespace). Within a domain, partitions
  // can be uniquely identified by the combination of |partition_name| and
  // |in_memory| values. When a partition is not to be persisted, the
  // |in_memory| value must be set to true.
  virtual void GetStoragePartitionConfigForSite(
      BrowserContext* browser_context,
      const GURL& site,
      bool can_be_default,
      std::string* partition_domain,
      std::string* partition_name,
      bool* in_memory);

  // Create and return a new quota permission context.
  virtual QuotaPermissionContext* CreateQuotaPermissionContext();

  // Allows the embedder to provide settings that determine the amount
  // of disk space that may be used by content facing storage apis like
  // IndexedDatabase and ServiceWorker::CacheStorage and others.
  virtual void GetQuotaSettings(
      content::BrowserContext* context,
      content::StoragePartition* partition,
      storage::OptionalQuotaSettingsCallback callback);

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
      ResourceType resource_type,
      bool strict_enforcement,
      bool expired_previous_decision,
      const base::Callback<void(CertificateRequestResultType)>& callback);

  // Selects a SSL client certificate and returns it to the |delegate|. Note:
  // |delegate| may be called synchronously or asynchronously.
  //
  // TODO(davidben): Move this hook to WebContentsDelegate.
  virtual void SelectClientCertificate(
      WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      net::ClientCertIdentityList client_certs,
      std::unique_ptr<ClientCertificateDelegate> delegate);

  // Returns a class to get notifications about media event. The embedder can
  // return nullptr if they're not interested.
  virtual MediaObserver* GetMediaObserver();

  // Returns the platform notification service, capable of displaying Web
  // Notifications to the user. The embedder can return a nullptr if they don't
  // support this functionality. May be called from any thread.
  virtual PlatformNotificationService* GetPlatformNotificationService();

  // Returns true if the given page is allowed to open a window of the given
  // type. If true is returned, |no_javascript_access| will indicate whether
  // the window that is created should be scriptable/in the same process.
  // This is called on the UI thread.
  virtual bool CanCreateWindow(
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
      bool* no_javascript_access);

  // Notifies the embedder that the ResourceDispatcherHost has been created.
  // This is when it can optionally add a delegate.
  virtual void ResourceDispatcherHostCreated() {}

  // Allows the embedder to return a delegate for the SpeechRecognitionManager.
  // The delegate will be owned by the manager. It's valid to return nullptr.
  virtual SpeechRecognitionManagerDelegate*
      CreateSpeechRecognitionManagerDelegate();

  // Getter for the net logging object. This can be called on any thread.
  virtual net::NetLog* GetNetLog();

  // Called by WebContents to override the WebKit preferences that are used by
  // the renderer. The content layer will add its own settings, and then it's up
  // to the embedder to update it if it wants.
  virtual void OverrideWebkitPrefs(RenderViewHost* render_view_host,
                                   WebPreferences* prefs) {}

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

  // Notification that a pepper plugin has just been spawned. This allows the
  // embedder to add filters onto the host to implement interfaces.
  // This is called on the IO thread.
  virtual void DidCreatePpapiPlugin(BrowserPpapiHost* browser_host) {}

  // Gets the host for an external out-of-process plugin.
  virtual BrowserPpapiHost* GetExternalBrowserPpapiHost(
      int plugin_child_id);

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

  // |schemes| is a return value parameter that gets a whitelist of schemes that
  // should bypass the Is Privileged Context check.
  // See http://www.w3.org/TR/powerful-features/#settings-privileged
  virtual void GetSchemesBypassingSecureContextCheckWhitelist(
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

  // Generate a Service user-id for the supplied browser context. Defaults to
  // returning a random GUID.
  virtual std::string GetServiceUserIdForBrowserContext(
      BrowserContext* browser_context);

  // Allows to register browser interfaces exposed through the
  // RenderProcessHost. Note that interface factory callbacks added to
  // |registry| will by default be run immediately on the IO thread, unless a
  // task runner is provided.
  virtual void ExposeInterfacesToRenderer(
      service_manager::BinderRegistry* registry,
      blink::AssociatedInterfaceRegistry* associated_registry,
      RenderProcessHost* render_process_host) {}

  // Called when RenderFrameHostImpl connects to the Media service. Expose
  // interfaces to the service using |registry|.
  virtual void ExposeInterfacesToMediaService(
      service_manager::BinderRegistry* registry,
      RenderFrameHost* render_frame_host) {}

  // Content was unable to bind a request for this interface, so the embedder
  // should try.
  virtual void BindInterfaceRequestFromFrame(
      RenderFrameHost* render_frame_host,
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle interface_pipe) {}

  // Content was unable to bind a request for this associated interface, so the
  // embedder should try. Returns true if the |handle| was actually taken and
  // bound; false otherwise.
  virtual bool BindAssociatedInterfaceRequestFromFrame(
      RenderFrameHost* render_frame_host,
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle* handle);

  // Content was unable to bind a request for this interface, so the embedder
  // should try. This is called for interface requests from dedicated, shared
  // and service workers.
  virtual void BindInterfaceRequestFromWorker(
      RenderProcessHost* render_process_host,
      const url::Origin& origin,
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle interface_pipe) {}

  // (Currently called only from GPUProcessHost, move somewhere more central).
  // Called when a request to bind |interface_name| on |interface_pipe| is
  // received from |source_info.identity|. If the request is bound,
  // |interface_pipe| will become invalid (taken by the client).
  virtual void BindInterfaceRequest(
      const service_manager::BindSourceInfo& source_info,
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) {}

  using StaticServiceMap =
      std::map<std::string, service_manager::EmbeddedServiceInfo>;

  // Registers services to be loaded in the browser process by the Service
  // Manager. |connection| is the ServiceManagerConnection service are
  // registered with.
  virtual void RegisterInProcessServices(StaticServiceMap* services,
                                         ServiceManagerConnection* connection) {
  }

  virtual void OverrideOnBindInterface(
      const service_manager::BindSourceInfo& remote_info,
      const std::string& name,
      mojo::ScopedMessagePipeHandle* handle) {}

  using ProcessNameCallback = base::RepeatingCallback<base::string16()>;

  struct CONTENT_EXPORT OutOfProcessServiceInfo {
    OutOfProcessServiceInfo();
    OutOfProcessServiceInfo(const ProcessNameCallback& process_name_callback);
    OutOfProcessServiceInfo(const ProcessNameCallback& process_name_callback,
                            const std::string& process_group);
    ~OutOfProcessServiceInfo();

    // The callback function to get the display name of the service process
    // launched for the service.
    ProcessNameCallback process_name_callback;

    // If provided, a string which groups this service into a process shared
    // by other services using the same string.
    base::Optional<std::string> process_group;
  };

  using OutOfProcessServiceMap = std::map<std::string, OutOfProcessServiceInfo>;

  // Registers services to be loaded out of the browser process in an
  // utility process. The value of each map entry should be a process name,
  // to use for the service's host process when launched.
  virtual void RegisterOutOfProcessServices(OutOfProcessServiceMap* services) {}

  // Allows the embedder to terminate the browser if a specific service instance
  // quits or crashes.
  virtual bool ShouldTerminateOnServiceQuit(
      const service_manager::Identity& id);

  // Allow the embedder to provide a dictionary loaded from a JSON file
  // resembling a service manifest whose capabilities section will be merged
  // with content's own for |name|. Additional entries will be appended to their
  // respective sections.
  virtual std::unique_ptr<base::Value> GetServiceManifestOverlay(
      base::StringPiece name);

  struct ServiceManifestInfo {
    // The name of the service.
    std::string name;

    // The resource ID of the manifest.
    int resource_id;
  };

  // Allows the embedder to provide extra service manifests to be registered
  // with the service manager context.
  virtual std::vector<ServiceManifestInfo> GetExtraServiceManifests();

  // Allows the embedder to have a list of services started after the
  // in-process Service Manager has been initialized.
  virtual std::vector<service_manager::Identity> GetStartupServices();

  // Allows to override the visibility state of a RenderFrameHost.
  // |visibility_state| should not be null. It will only be set if needed.
  virtual void OverridePageVisibilityState(
      RenderFrameHost* render_frame_host,
      blink::mojom::PageVisibilityState* visibility_state) {}

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
  // another WebContents. For example, from a Worker. |callback| will be
  // invoked with the appropriate WebContents* when available.
  virtual void OpenURL(BrowserContext* browser_context,
                       const OpenURLParams& params,
                       const base::Callback<void(WebContents*)>& callback);

  // Allows the embedder to record |metric| for a specific |url|.
  virtual void RecordURLMetric(const std::string& metric, const GURL& url) {}

  // Allows the embedder to map URLs to strings, intended to be used as suffixes
  // for metric names. For example, the embedder can map
  // "my-special-site-with-a-complicated-name.example.com/and-complicated-path"
  // to the string "MySpecialSite", which will cause some UMA involving that URL
  // to be logged as "UmaName.MySpecialSite".
  virtual std::string GetMetricSuffixForURL(const GURL& url);

  // Allows the embedder to register one or more NavigationThrottles for the
  // navigation indicated by |navigation_handle|.  A NavigationThrottle is used
  // to control the flow of a navigation on the UI thread. The embedder is
  // guaranteed that the throttles will be executed in the order they were
  // provided.
  virtual std::vector<std::unique_ptr<NavigationThrottle>>
  CreateThrottlesForNavigation(NavigationHandle* navigation_handle);

  // PlzNavigate
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
  // (CDM) and a CdmProxy associated with |key_system|. The CDM supports all
  // protocols in |cdm_proxy_protocols|, but only one CdmProxy protocol will be
  // supported by the CdmProxy on the system, for which the capabilities will
  // be returned.
  virtual void GetHardwareSecureDecryptionCaps(
      const std::string& key_system,
      const base::flat_set<media::CdmProxy::Protocol>& cdm_proxy_protocols,
      base::flat_set<media::VideoCodec>* video_codecs,
      base::flat_set<media::EncryptionMode>* encryption_schemes);

  // Populates |mappings| with all files that need to be mapped before launching
  // a child process.
#if (defined(OS_POSIX) && !defined(OS_MACOSX)) || defined(OS_FUCHSIA)
  virtual void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::PosixFileDescriptorInfo* mappings) {}
#endif  // defined(OS_POSIX) && !defined(OS_MACOSX) || defined(OS_FUCHSIA)

#if defined(OS_WIN)
  // This is called on the PROCESS_LAUNCHER thread before the renderer process
  // is launched. It gives the embedder a chance to add loosen the sandbox
  // policy.
  virtual bool PreSpawnRenderer(sandbox::TargetPolicy* policy);

  // Returns the AppContainer SID for the specified sandboxed process type, or
  // empty string if this sandboxed process type does not support living inside
  // an AppContainer.
  virtual base::string16 GetAppContainerSidForSandboxType(
      int sandbox_type) const;
#endif

  // Binds a new media remoter service to |request|, if supported by the
  // embedder, for the |source| that lives in the render frame represented
  // by |render_frame_host|. This may be called multiple times if there is more
  // than one source candidate in the same render frame.
  virtual void CreateMediaRemoter(RenderFrameHost* render_frame_host,
                                  media::mojom::RemotingSourcePtr source,
                                  media::mojom::RemoterRequest request) {}

  // Returns the RapporService from the browser process.
  virtual ::rappor::RapporService* GetRapporService();

  // Provides parameters for initializing the global task scheduler. Default
  // params are used if this returns nullptr.
  virtual std::unique_ptr<base::TaskScheduler::InitParams>
  GetTaskSchedulerInitParams();

  // Allows the embedder to register one or more URLLoaderThrottles for a
  // navigation request.
  // This is called both when the network service is enabled and disabled.
  // This is called on the IO thread.
  virtual std::vector<std::unique_ptr<URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      ResourceContext* resource_context,
      const base::RepeatingCallback<WebContents*()>& wc_getter,
      NavigationUIData* navigation_ui_data,
      int frame_tree_node_id);

  // Allows the embedder to register per-scheme URLLoaderFactory implementations
  // to handle navigation URL requests for schemes not handled by the Network
  // Service. Only called when the Network Service is enabled.
  // Note that a RenderFrameHost or RenderProcessHost aren't passed in because
  // these can change during a navigation (e.g. depending on redirects).
  using NonNetworkURLLoaderFactoryMap =
      std::map<std::string, std::unique_ptr<network::mojom::URLLoaderFactory>>;
  virtual void RegisterNonNetworkNavigationURLLoaderFactories(
      int frame_tree_node_id,
      NonNetworkURLLoaderFactoryMap* factories);

  // Allows the embedder to register per-scheme URLLoaderFactory implementations
  // to handle subresource URL requests for schemes not handled by the Network
  // Service. This function can also be used to make a factory for other
  // non-subresource requests, such as for the service worker script when
  // starting a service worker. In that case, the frame id will be
  // MSG_ROUTING_NONE.
  virtual void RegisterNonNetworkSubresourceURLLoaderFactories(
      int render_process_id,
      int render_frame_id,
      NonNetworkURLLoaderFactoryMap* factories);

  // Allows the embedder to intercept URLLoaderFactory interfaces used for
  // navigation or being brokered on behalf of a renderer fetching subresources.
  //
  // |is_navigation| is true when it's a request used for navigation.
  //
  // |request_initiator| indicates which origin will be the initiator of
  // requests that will use the URLLoaderFactory (see also
  // |network::ResourceRequest::requests|).  |request_initiator| is set when
  // it's a request for a renderer fetching subresources. It's not set when
  // creating a factory for navigation requests, because navigation requests are
  // made on behalf of the browser, rather than on behalf of any particular
  // origin.
  //
  // |*factory_request| is always valid upon entry and MUST be valid upon
  // return. The embedder may swap out the value of |*factory_request| for its
  // own, in which case it must return |true| to indicate that it's proxying
  // requests for the URLLoaderFactory. Otherwise |*factory_request| is left
  // unmodified and this must return |false|.
  //
  // |bypass_redirect_checks| will be set to true when the embedder will be
  // handling redirect security checks.
  //
  // Always called on the UI thread and only when the Network Service is
  // enabled.
  virtual bool WillCreateURLLoaderFactory(
      BrowserContext* browser_context,
      RenderFrameHost* frame,
      bool is_navigation,
      const url::Origin& request_initiator,
      network::mojom::URLLoaderFactoryRequest* factory_request,
      bool* bypass_redirect_checks);

  // Allows the embedder to intercept a WebSocket connection. |*request|
  // is always valid upon entry and MUST be valid upon return. The embedder
  // may swap out the value of |*request| for its own.
  //
  // Always called on the UI thread and only when the Network Service is
  // enabled.
  virtual void WillCreateWebSocket(
      RenderFrameHost* frame,
      network::mojom::WebSocketRequest* request,
      network::mojom::AuthenticationHandlerPtr* authentication_handler);

  // Allows the embedder to returns a list of request interceptors that can
  // intercept a navigation request.
  //
  // Always called on the IO thread and only when the Network Service is
  // enabled.
  virtual std::vector<std::unique_ptr<URLLoaderRequestInterceptor>>
  WillCreateURLLoaderRequestInterceptors(NavigationUIData* navigation_ui_data,
                                         int frame_tree_node_id);

  // Called when the NetworkService, accessible through
  // content::GetNetworkService(), is created. Implementations should avoid
  // calling into GetNetworkService() again to avoid re-entrancy if the service
  // fails to start.
  virtual void OnNetworkServiceCreated(
      network::mojom::NetworkService* network_service);

  // Creates a NetworkContext for a BrowserContext's StoragePartition. If the
  // network service is enabled, it must return a NetworkContext using the
  // network service. If the network service is disabled, the embedder may
  // return a NetworkContext, or it may return nullptr, in which case the
  // StoragePartition will create one wrapping the URLRequestContext obtained
  // from the BrowserContext.
  //
  // Called before the corresonding BrowserContext::CreateRequestContext method
  // is called.
  //
  // If |in_memory| is true, |relative_partition_path| is still a path that
  // uniquely identifies the storage partition, though nothing should be written
  // to it.
  //
  // If |relative_partition_path| is the empty string, it means this needs to
  // create the default NetworkContext for the BrowserContext.
  virtual network::mojom::NetworkContextPtr CreateNetworkContext(
      BrowserContext* context,
      bool in_memory,
      const base::FilePath& relative_partition_path);

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
      mojo::InterfaceRequest<blink::mojom::WebUsbService> request);

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

  // Returns whether a base::TaskScheduler should be created when
  // BrowserMainLoop starts.
  // If false, a task scheduler has been created by the embedder, and
  // BrowserMainLoop should skip creating a second one.
  // Note: the embedder should *not* start the TaskScheduler for
  // BrowserMainLoop, BrowserMainLoop itself is responsible for that.
  virtual bool ShouldCreateTaskScheduler();

  // Returns an AuthenticatorRequestClientDelegate subclass instance to provide
  // embedder-specific configuration for a single Web Authentication API request
  // being serviced in a given RenderFrame. The instance is guaranteed to be
  // destroyed before the RenderFrame goes out of scope. The embedder may choose
  // to return nullptr to indicate that the request cannot be serviced right
  // now.
  virtual std::unique_ptr<AuthenticatorRequestClientDelegate>
  GetWebAuthenticationRequestDelegate(RenderFrameHost* render_frame_host);

#if defined(OS_MACOSX)
  // Returns whether WebAuthn supports the built-in Touch ID platform
  // authenticator. If true, the embedder must supply a configuration in
  // |AuthenticatorRequestClientDelegate::GetTouchIdAuthenticatorConfig|.
  virtual bool IsWebAuthenticationTouchIdAuthenticatorSupported();
#endif

  // Get platform ClientCertStore. May return nullptr.
  virtual std::unique_ptr<net::ClientCertStore> CreateClientCertStore(
      ResourceContext* resource_context);

  // Creates a LoginDelegate that asks the user for a username and password.
  // Caller owns the returned pointer.
  // |first_auth_attempt| is needed by AwHttpAuthHandler constructor.
  // |auth_required_callback| is used to transfer auth credentials to
  // URLRequest::SetAuth(). The credentials parameter of the callback
  // is base::nullopt if the request should be cancelled; otherwise
  // the credentials will be used to respond to the auth challenge. This
  // callback should be called on the IO thread task runner.
  virtual scoped_refptr<LoginDelegate> CreateLoginDelegate(
      net::AuthChallengeInfo* auth_info,
      content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
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
  virtual bool HandleExternalProtocol(
      const GURL& url,
      ResourceRequestInfo::WebContentsGetter web_contents_getter,
      int child_id,
      NavigationUIData* navigation_data,
      bool is_main_frame,
      ui::PageTransition page_transition,
      bool has_user_gesture);

  // Creates an OverlayWindow to be used for Picture-in-Picture. This window
  // will house the content shown when in Picture-in-Picture mode. This will
  // return a new OverlayWindow.
  // May return nullptr if embedder does not support this functionality. The
  // default implementation provides nullptr OverlayWindow.
  virtual std::unique_ptr<OverlayWindow> CreateWindowForPictureInPicture(
      PictureInPictureWindowController* controller);

  // Returns true if it is safe to redirect to |url|, otherwise returns false.
  // This is called on the IO thread.
  virtual bool IsSafeRedirectTarget(const GURL& url, ResourceContext* context);

  // Registers the watcher to observe updates in RendererPreferences. The
  // watchers are for shared workers and service workers.
  virtual void RegisterRendererPreferenceWatcherForWorkers(
      BrowserContext* browser_context,
      mojom::RendererPreferenceWatcherPtr watcher);

  // Returns the HTML content of the error page for Origin Policy related
  // errors.
  virtual base::Optional<std::string> GetOriginPolicyErrorPage(
      OriginPolicyErrorReason error_reason,
      const url::Origin& origin,
      const GURL& url);

  // Returns true if it is OK to ignore errors for certificates specified by the
  // --ignore-certificate-errors-spki-list command line flag. The embedder may
  // perform additional checks, such as requiring --user-data-dir flag too to
  // make sure that insecure contents will not persist accidentally.
  virtual bool CanIgnoreCertificateErrorIfNeeded();

  // Called on every request completion to update the data use when network
  // service is enabled.
  virtual void OnNetworkServiceDataUseUpdate(
      int32_t network_traffic_annotation_id_hash,
      int64_t recv_bytes,
      int64_t sent_bytes);

  // Asks the embedder for the PreviewsState which says which previews should
  // be enabled for the given navigation. The PreviewsState is a bitmask of
  // potentially several Previews optimizations. It is only called for requests
  // with an unspecified Previews state.  If previews_to_allow is set to
  // anything other than PREVIEWS_UNSPECIFIED, it is taken as a limit on
  // available preview states.
  virtual content::PreviewsState DetermineAllowedPreviews(
      content::PreviewsState initial_state,
      content::NavigationHandle* navigation_handle);

  // Asks the embedder for the preview state that should be committed to the
  // renderer. |initial_state| was pre-determined by |DetermineAllowedPreviews|.
  // |navigation_handle| is the corresponding navigation object.
  // |response_headers| are the response headers related to this navigation.
  virtual content::PreviewsState DetermineCommittedPreviews(
      content::PreviewsState initial_state,
      content::NavigationHandle* navigation_handle,
      const net::HttpResponseHeaders* response_headers);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CONTENT_BROWSER_CLIENT_H_
