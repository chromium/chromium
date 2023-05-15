// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSIONS_BROWSER_CLIENT_H_
#define EXTENSIONS_BROWSER_EXTENSIONS_BROWSER_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/bluetooth_chooser.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_prefs_observer.h"
#include "extensions/browser/extensions_browser_api_provider.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_loader.mojom-forward.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

class ExtensionFunctionRegistry;
class PrefService;

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace content {
class BrowserContext;
class RenderFrameHost;
class SiteInstance;
class StoragePartitionConfig;
class WebContents;
}  // namespace content

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace network {
struct ResourceRequest;
namespace mojom {
class NetworkContext;
}
}  // namespace network

namespace update_client {
class UpdateClient;
}  // namespace update_client

namespace url {
class Origin;
}  // namespace url

namespace base {
class CancelableTaskTracker;
}  // namespace base

namespace extensions {

class ComponentExtensionResourceManager;
class Extension;
class ExtensionCache;
class ExtensionError;
class ExtensionHostDelegate;
class ExtensionSet;
class ExtensionSystem;
class ExtensionSystemProvider;
class ExtensionWebContentsObserver;
class KioskDelegate;
class PermissionSet;
class ProcessManagerDelegate;
class ProcessMap;
class RuntimeAPIDelegate;
class ScopedExtensionUpdaterKeepAlive;
class UserScriptListener;

// Interface to allow the extensions module to make browser-process-specific
// queries of the embedder. Should be Set() once in the browser process.
//
// NOTE: Methods that do not require knowledge of browser concepts should be
// added in ExtensionsClient (extensions/common/extensions_client.h) even if
// they are only used in the browser process.
class ExtensionsBrowserClient {
 public:
  ExtensionsBrowserClient();
  ExtensionsBrowserClient(const ExtensionsBrowserClient&) = delete;
  ExtensionsBrowserClient& operator=(const ExtensionsBrowserClient&) = delete;
  virtual ~ExtensionsBrowserClient();

  // Returns the single instance of |this|.
  static ExtensionsBrowserClient* Get();

  // Sets and initializes the single instance.
  static void Set(ExtensionsBrowserClient* client);

  // Registers all extension functions.
  void RegisterExtensionFunctions(ExtensionFunctionRegistry* registry);

  // Adds a new API provider to the client.
  void AddAPIProvider(std::unique_ptr<ExtensionsBrowserAPIProvider> provider);

  /////////////////////////////////////////////////////////////////////////////
  // Virtual Methods

  // Returns true if the embedder has started shutting down.
  virtual bool IsShuttingDown() = 0;

  // Returns true if extensions have been disabled (e.g. via a command-line flag
  // or preference).
  virtual bool AreExtensionsDisabled(const base::CommandLine& command_line,
                                     content::BrowserContext* context) = 0;

  // Returns true if the |context| is known to the embedder.
  virtual bool IsValidContext(content::BrowserContext* context) = 0;

  // Returns true if the BrowserContexts could be considered equivalent, for
  // example, if one is an off-the-record context owned by the other.
  virtual bool IsSameContext(content::BrowserContext* first,
                             content::BrowserContext* second) = 0;

  // Returns true if |context| has an off-the-record context associated with it.
  virtual bool HasOffTheRecordContext(content::BrowserContext* context) = 0;

  // Returns the off-the-record context associated with |context|. If |context|
  // is already off-the-record, returns |context|.
  // WARNING: This may create a new off-the-record context. To avoid creating
  // another context, check HasOffTheRecordContext() first.
  virtual content::BrowserContext* GetOffTheRecordContext(
      content::BrowserContext* context) = 0;

  // Returns the original "recording" context. This method returns |context| if
  // |context| is not incognito.
  virtual content::BrowserContext* GetOriginalContext(
      content::BrowserContext* context) = 0;

  // The below methods include parameters to enforce the value given to Regular
  // Profile type for Guest/System Profile types. Guest and System Profiles are
  // defaulted to return no profile (which leads to no service constructed in
  // terms of KeyedServices).
  //
  // Returns the Original Profile for Regular Profile and redirects Incognito
  // to the Original Profile.
  // Force values to have the same behavior for Guest and System Profile.
  virtual content::BrowserContext* GetRedirectedContextInIncognito(
      content::BrowserContext* context,
      bool force_guest_profile,
      bool force_system_profile) = 0;
  // Returns Profile for Regular and Incognito.
  // Force values to have the same behavior for Guest and System Profile.
  virtual content::BrowserContext* GetContextForRegularAndIncognito(
      content::BrowserContext* context,
      bool force_guest_profile,
      bool force_system_profile) = 0;
  // Returns Profile only for Original Regular profile.
  // Force values to have the same behavior for Guest and System Profile.
  virtual content::BrowserContext* GetRegularProfile(
      content::BrowserContext* context,
      bool force_guest_profile,
      bool force_system_profile) = 0;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Returns a user id hash from |context| or an empty string if no hash could
  // be extracted.
  virtual std::string GetUserIdHashFromContext(
      content::BrowserContext* context) = 0;
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Returns if a browser |context| belongs to the main profile or not.
  virtual bool IsFromMainProfile(content::BrowserContext* context) = 0;
#endif

  // Returns true if |context| corresponds to a guest session.
  virtual bool IsGuestSession(content::BrowserContext* context) const = 0;

  // Returns true if |extension_id| can run in an incognito window.
  virtual bool IsExtensionIncognitoEnabled(
      const std::string& extension_id,
      content::BrowserContext* context) const = 0;

  // Returns true if |extension| can see events and data from another
  // sub-profile (incognito to original profile, or vice versa).
  virtual bool CanExtensionCrossIncognito(
      const extensions::Extension* extension,
      content::BrowserContext* context) const = 0;

  // Return the resource relative path and id for the given request.
  virtual base::FilePath GetBundleResourcePath(
      const network::ResourceRequest& request,
      const base::FilePath& extension_resources_path,
      int* resource_id) const = 0;

  // Creates and starts a URLLoader to load an extension resource from the
  // embedder's resource bundle (.pak) files. Used for component extensions.
  virtual void LoadResourceFromResourceBundle(
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      const base::FilePath& resource_relative_path,
      int resource_id,
      scoped_refptr<net::HttpResponseHeaders> headers,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client) = 0;

  // Returns true if the embedder wants to allow a chrome-extension:// resource
  // request coming from renderer A to access a resource in an extension running
  // in renderer B. For example, Chrome overrides this to provide support for
  // webview and dev tools. May be called on either the UI or IO thread.
  virtual bool AllowCrossRendererResourceLoad(
      const network::ResourceRequest& request,
      network::mojom::RequestDestination destination,
      ui::PageTransition page_transition,
      int child_id,
      bool is_incognito,
      const Extension* extension,
      const ExtensionSet& extensions,
      const ProcessMap& process_map) = 0;

  // Returns the PrefService associated with |context|.
  virtual PrefService* GetPrefServiceForContext(
      content::BrowserContext* context) = 0;

  // Populates a list of ExtensionPrefs observers to be attached to each
  // BrowserContext's ExtensionPrefs upon construction. These observers
  // are not owned by ExtensionPrefs.
  virtual void GetEarlyExtensionPrefsObservers(
      content::BrowserContext* context,
      std::vector<EarlyExtensionPrefsObserver*>* observers) const = 0;

  // Returns the ProcessManagerDelegate shared across all BrowserContexts. May
  // return NULL in tests or for simple embedders.
  virtual ProcessManagerDelegate* GetProcessManagerDelegate() const = 0;

  // Creates a new ExtensionHostDelegate instance.
  virtual std::unique_ptr<ExtensionHostDelegate>
  CreateExtensionHostDelegate() = 0;

  // Returns true if the client version has updated since the last run. Called
  // once each time the extensions system is loaded per browser_context. The
  // implementation may wish to use the BrowserContext to record the current
  // version for later comparison.
  virtual bool DidVersionUpdate(content::BrowserContext* context) = 0;

  // Permits an external protocol handler to be launched. See
  // ExternalProtocolHandler::PermitLaunchUrl() in Chrome.
  virtual void PermitExternalProtocolHandler() = 0;

  // Return true if the device is enrolled in Demo Mode.
  virtual bool IsInDemoMode() = 0;

  // Return true if |app_id| matches the screensaver and the device is enrolled
  // in Demo Mode.
  virtual bool IsScreensaverInDemoMode(const std::string& app_id) = 0;

  // Return true if the system is run in forced app mode.
  virtual bool IsRunningInForcedAppMode() = 0;

  // Returns whether the system is run in forced app mode for app with the
  // provided extension ID.
  virtual bool IsAppModeForcedForApp(const ExtensionId& id) = 0;

  // Return true if the user is logged in as a public session.
  virtual bool IsLoggedInAsPublicAccount() = 0;

  // Returns the factory that provides an ExtensionSystem to be returned from
  // ExtensionSystem::Get.
  virtual ExtensionSystemProvider* GetExtensionSystemFactory() = 0;

  // Registers additional interfaces to a binder map for a browser interface
  // broker.
  virtual void RegisterBrowserInterfaceBindersForFrame(
      mojo::BinderMapWithContext<content::RenderFrameHost*>* binder_map,
      content::RenderFrameHost* render_frame_host,
      const Extension* extension) const = 0;

  // Creates a RuntimeAPIDelegate responsible for handling extensions
  // management-related events such as update and installation on behalf of the
  // core runtime API implementation.
  virtual std::unique_ptr<RuntimeAPIDelegate> CreateRuntimeAPIDelegate(
      content::BrowserContext* context) const = 0;

  // Returns the manager of resource bundles used in extensions. Returns NULL if
  // the manager doesn't exist.
  virtual const ComponentExtensionResourceManager*
  GetComponentExtensionResourceManager() = 0;

  // Propagate a event to all the renderers in every browser context. The
  // implementation must be safe to call from any thread.
  virtual void BroadcastEventToRenderers(
      events::HistogramValue histogram_value,
      const std::string& event_name,
      base::Value::List args,
      bool dispatch_to_off_the_record_profiles) = 0;

  // Gets the single ExtensionCache instance shared across the browser process.
  virtual ExtensionCache* GetExtensionCache() = 0;

  // Indicates whether extension update checks should be allowed.
  virtual bool IsBackgroundUpdateAllowed() = 0;

  // Indicates whether an extension update which specifies its minimum browser
  // version as |min_version| can be installed by the client. Not all extensions
  // embedders share the same versioning model, so interpretation of the string
  // is left up to the embedder.
  virtual bool IsMinBrowserVersionSupported(const std::string& min_version) = 0;

  // Embedders can override this function to handle extension errors.
  virtual void ReportError(content::BrowserContext* context,
                           std::unique_ptr<ExtensionError> error);

  // Returns the ExtensionWebContentsObserver for the given |web_contents|.
  virtual ExtensionWebContentsObserver* GetExtensionWebContentsObserver(
      content::WebContents* web_contents) = 0;

  // Cleans up browser-side state associated with a WebView that is being
  // destroyed.
  virtual void CleanUpWebView(content::BrowserContext* browser_context,
                              int embedder_process_id,
                              int view_instance_id) {}

  // Clears the back-forward cache for all active tabs across all browser
  // contexts.
  virtual void ClearBackForwardCache() {}

  // Attaches the task manager extension tag to |web_contents|, if needed based
  // on |view_type|, so that its corresponding task shows up in the task
  // manager.
  virtual void AttachExtensionTaskManagerTag(content::WebContents* web_contents,
                                             mojom::ViewType view_type) {}

  // Returns a new UpdateClient.
  virtual scoped_refptr<update_client::UpdateClient> CreateUpdateClient(
      content::BrowserContext* context);

  // Returns a new ScopedExtensionUpdaterKeepAlive, or nullptr if the embedder
  // does not support keeping the context alive while the updater is running.
  virtual std::unique_ptr<ScopedExtensionUpdaterKeepAlive>
  CreateUpdaterKeepAlive(content::BrowserContext* context);

  // Returns true if activity logging is enabled for the given |context|.
  virtual bool IsActivityLoggingEnabled(content::BrowserContext* context);

  // Retrives the embedder's notion of tab and window ID for a given
  // WebContents. May return -1 for either or both values if the embedder does
  // not implement any such concepts. This is used to support the WebRequest API
  // exposing such numbers to callers.
  virtual void GetTabAndWindowIdForWebContents(
      content::WebContents* web_contents,
      int* tab_id,
      int* window_id);

  // Returns a delegate that provides kiosk mode functionality.
  virtual KioskDelegate* GetKioskDelegate() = 0;

  // Whether the browser context is associated with Chrome OS lock screen.
  virtual bool IsLockScreenContext(content::BrowserContext* context) = 0;

  // Returns the locale used by the application.
  virtual std::string GetApplicationLocale() = 0;

  // Returns whether |extension_id| is currently enabled.
  // This will only return a valid answer for installed extensions (regardless
  // of whether it is currently loaded or not) under the provided |context|.
  // Loaded extensions return true if they are currently loaded or terminated.
  // Unloaded extensions will return true if they are not blocked, disabled,
  // blocklisted or uninstalled (for external extensions). The default return
  // value of this function is false.
  virtual bool IsExtensionEnabled(const std::string& extension_id,
                                  content::BrowserContext* context) const;

  // http://crbug.com/829412
  // Renderers with WebUI bindings shouldn't make http(s) requests for security
  // reasons (e.g. to avoid malicious responses being able to run code in
  // priviliged renderers). Fix these webui's to make requests through C++
  // code instead.
  virtual bool IsWebUIAllowedToMakeNetworkRequests(const url::Origin& origin);

  virtual network::mojom::NetworkContext* GetSystemNetworkContext();

  virtual UserScriptListener* GetUserScriptListener();

  // Called when all initial script loads from extensions have been completed
  // for the given BrowserContext.
  virtual void SignalContentScriptsLoaded(content::BrowserContext* context);

  // Returns the user agent used by the content module.
  virtual std::string GetUserAgent() const;

  // Returns whether |scheme| should bypass extension-specific navigation checks
  // (e.g. whether the |scheme| is allowed to initiate navigations to extension
  // resources that are not declared as web accessible).
  virtual bool ShouldSchemeBypassNavigationChecks(
      const std::string& scheme) const;

  // Gets and sets the last save (download) path for a given context.
  virtual base::FilePath GetSaveFilePath(content::BrowserContext* context);
  virtual void SetLastSaveFilePath(content::BrowserContext* context,
                                   const base::FilePath& path);

  // Returns true if the |extension_id| requires its own isolated storage
  // partition.
  virtual bool HasIsolatedStorage(const std::string& extension_id,
                                  content::BrowserContext* context);

  // Returns whether screenshot of |web_contents| is restricted due to Data Leak
  // Protection policy.
  virtual bool IsScreenshotRestricted(content::WebContents* web_contents) const;

  // Returns true if the given |tab_id| exists.
  virtual bool IsValidTabId(content::BrowserContext* context, int tab_id) const;

  // Returns true if chrome extension telemetry service is enabled.
  virtual bool IsExtensionTelemetryServiceEnabled(
      content::BrowserContext* context) const;

  // TODO(anunoy): This is a temporary implementation of notifying the
  // extension telemetry service of the tabs.executeScript API invocation
  // while its usefulness is evaluated.
  virtual void NotifyExtensionApiTabExecuteScript(
      content::BrowserContext* context,
      const ExtensionId& extension_id,
      const std::string& code) const;

  // Notifies the extension telemetry service when declarativeNetRequest API
  // rules are added.
  virtual void NotifyExtensionApiDeclarativeNetRequest(
      content::BrowserContext* context,
      const ExtensionId& extension_id,
      const std::vector<api::declarative_net_request::Rule>& rules) const;

  // TODO(zackhan): This is a temporary implementation of notifying the
  // extension telemetry service when there are web requests initiated from
  // chrome extensions. Its usefulness will be evaluated.
  virtual void NotifyExtensionRemoteHostContacted(
      content::BrowserContext* context,
      const ExtensionId& extension_id,
      const GURL& url) const;

  // Return true if the USB device is allowed by policy.
  virtual bool IsUsbDeviceAllowedByPolicy(content::BrowserContext* context,
                                          const ExtensionId& extension_id,
                                          int vendor_id,
                                          int product_id) const;

  // Populate callback with the asynchronously retrieved cached favicon image.
  virtual void GetFavicon(
      content::BrowserContext* browser_context,
      const Extension* extension,
      const GURL& url,
      base::CancelableTaskTracker* tracker,
      base::OnceCallback<void(
          scoped_refptr<base::RefCountedMemory> bitmap_data)> callback) const;

  // Returns all BrowserContexts related to the given extension. For an
  // extension limited to a signal context, this will be a vector of the single
  // passed-in context. For extensions allowed to run in incognito contexts
  // associated with `browser_context`, this will include all those contexts.
  // Note: It may not be appropriate to treat these the same depending on
  // whether the extension runs in "split" or "spanning" mode.
  virtual std::vector<content::BrowserContext*> GetRelatedContextsForExtension(
      content::BrowserContext* browser_context,
      const Extension& extension) const;

  // Adds any hosts that should be automatically considered "granted" if
  // requested to `granted_permissions`.
  virtual void AddAdditionalAllowedHosts(
      const PermissionSet& desired_permissions,
      PermissionSet* granted_permissions) const;

  virtual void AddAPIActionToActivityLog(
      content::BrowserContext* browser_context,
      const ExtensionId& extension_id,
      const std::string& call_name,
      base::Value::List args,
      const std::string& extra);
  virtual void AddEventToActivityLog(content::BrowserContext* context,
                                     const ExtensionId& extension_id,
                                     const std::string& call_name,
                                     base::Value::List args,
                                     const std::string& extra);
  virtual void AddDOMActionToActivityLog(
      content::BrowserContext* browser_context,
      const ExtensionId& extension_id,
      const std::string& call_name,
      base::Value::List args,
      const GURL& url,
      const std::u16string& url_title,
      int call_type);

  // Returns the StoragePartitionConfig that should be used for a <webview> or
  // <controlledframe> with the given |partition_name| that is owned by a frame
  // within |owner_site_instance|.
  virtual content::StoragePartitionConfig GetWebViewStoragePartitionConfig(
      content::BrowserContext* browser_context,
      content::SiteInstance* owner_site_instance,
      const std::string& partition_name,
      bool in_memory);

  // Creates password reuse detection manager when new extension web contents
  // are created.
  virtual void CreatePasswordReuseDetectionManager(
      content::WebContents* web_contents) const;

 private:
  std::vector<std::unique_ptr<ExtensionsBrowserAPIProvider>> providers_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSIONS_BROWSER_CLIENT_H_
