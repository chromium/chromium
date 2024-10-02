// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_TEST_EXTENSIONS_BROWSER_CLIENT_H_
#define EXTENSIONS_BROWSER_TEST_EXTENSIONS_BROWSER_CLIENT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/update_client/update_client.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/updater/extension_cache.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/fetch_api.mojom.h"

namespace extensions {
class KioskDelegate;

// A simplified ExtensionsBrowserClient for a single normal browser context and
// an optional incognito browser context associated with it. A test that uses
// this class should call ExtensionsBrowserClient::Set() with its instance.
class TestExtensionsBrowserClient : public ExtensionsBrowserClient {
 public:
  // If provided, |main_context| must not be an incognito context.
  explicit TestExtensionsBrowserClient(content::BrowserContext* main_context);
  // Alternate constructor allowing |main_context_| to be set later.
  TestExtensionsBrowserClient();
  TestExtensionsBrowserClient(const TestExtensionsBrowserClient&) = delete;
  TestExtensionsBrowserClient& operator=(const TestExtensionsBrowserClient&) =
      delete;
  ~TestExtensionsBrowserClient() override;

  void set_process_manager_delegate(ProcessManagerDelegate* delegate) {
    process_manager_delegate_ = delegate;
  }
  void set_extension_system_factory(ExtensionSystemProvider* factory) {
    extension_system_factory_ = factory;
  }
  void set_pref_service(PrefService* pref_service) {
    pref_service_ = pref_service;
  }
  void set_extension_cache(std::unique_ptr<ExtensionCache> extension_cache) {
    extension_cache_ = std::move(extension_cache);
  }

  void set_lock_screen_context(content::BrowserContext* context) {
    lock_screen_context_ = context;
  }

  // Sets a factory to respond to calls of the CreateUpdateClient method.
  void SetUpdateClientFactory(
      base::RepeatingCallback<update_client::UpdateClient*(void)> factory);

  // Sets the main browser context. Only call if a BrowserContext was not
  // already provided. |main_context| must not be an incognito context.
  void SetMainContext(content::BrowserContext* main_context);

  // Associates an incognito context with |main_context_|.
  void SetIncognitoContext(content::BrowserContext* incognito_context);

  // ExtensionsBrowserClient overrides:
  bool IsShuttingDown() override;
  bool AreExtensionsDisabled(const base::CommandLine& command_line,
                             content::BrowserContext* context) override;
  bool IsValidContext(void* context) override;
  bool IsSameContext(content::BrowserContext* first,
                     content::BrowserContext* second) override;
  bool HasOffTheRecordContext(content::BrowserContext* context) override;
  content::BrowserContext* GetOffTheRecordContext(
      content::BrowserContext* context) override;
  content::BrowserContext* GetOriginalContext(
      content::BrowserContext* context) override;

  content::BrowserContext* GetContextRedirectedToOriginal(
      content::BrowserContext* context,
      bool force_guest_profile) override;
  content::BrowserContext* GetContextOwnInstance(
      content::BrowserContext* context,
      bool force_guest_profile) override;
  content::BrowserContext* GetContextForOriginalOnly(
      content::BrowserContext* context,
      bool force_guest_profile) override;
  bool AreExtensionsDisabledForContext(
      content::BrowserContext* context) override;

#if BUILDFLAG(IS_CHROMEOS)
  std::string GetUserIdHashFromContext(
      content::BrowserContext* context) override;
#endif
  bool IsGuestSession(content::BrowserContext* context) const override;
  bool IsExtensionIncognitoEnabled(
      const ExtensionId& extension_id,
      content::BrowserContext* context) const override;
  bool CanExtensionCrossIncognito(
      const extensions::Extension* extension,
      content::BrowserContext* context) const override;
  base::FilePath GetBundleResourcePath(
      const network::ResourceRequest& request,
      const base::FilePath& extension_resources_path,
      int* resource_id) const override;
  void LoadResourceFromResourceBundle(
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      const base::FilePath& resource_relative_path,
      int resource_id,
      scoped_refptr<net::HttpResponseHeaders> headers,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client) override;

  bool AllowCrossRendererResourceLoad(
      const network::ResourceRequest& request,
      network::mojom::RequestDestination destination,
      ui::PageTransition page_transition,
      int child_id,
      bool is_incognito,
      const Extension* extension,
      const ExtensionSet& extensions,
      const ProcessMap& process_map,
      const GURL& upstream_url) override;
  PrefService* GetPrefServiceForContext(
      content::BrowserContext* context) override;
  void GetEarlyExtensionPrefsObservers(
      content::BrowserContext* context,
      std::vector<EarlyExtensionPrefsObserver*>* observers) const override;
  ProcessManagerDelegate* GetProcessManagerDelegate() const override;
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
  GetControlledFrameEmbedderURLLoader(
      const url::Origin& app_origin,
      content::FrameTreeNodeId frame_tree_node_id,
      content::BrowserContext* browser_context) override;
  std::unique_ptr<ExtensionHostDelegate> CreateExtensionHostDelegate() override;
  bool DidVersionUpdate(content::BrowserContext* context) override;
  void PermitExternalProtocolHandler() override;
  bool IsInDemoMode() override;
  bool IsScreensaverInDemoMode(const std::string& app_id) override;
  bool IsRunningInForcedAppMode() override;
  bool IsAppModeForcedForApp(const ExtensionId& extension_id) override;
  bool IsLoggedInAsPublicAccount() override;
  ExtensionSystemProvider* GetExtensionSystemFactory() override;
  void RegisterBrowserInterfaceBindersForFrame(
      mojo::BinderMapWithContext<content::RenderFrameHost*>* binder_map,
      content::RenderFrameHost* render_frame_host,
      const Extension* extension) const override;
  std::unique_ptr<RuntimeAPIDelegate> CreateRuntimeAPIDelegate(
      content::BrowserContext* context) const override;
  const ComponentExtensionResourceManager*
  GetComponentExtensionResourceManager() override;
  void BroadcastEventToRenderers(
      events::HistogramValue histogram_value,
      const std::string& event_name,
      base::Value::List args,
      bool dispatch_to_off_the_record_profiles) override;
  ExtensionCache* GetExtensionCache() override;
  bool IsBackgroundUpdateAllowed() override;
  bool IsMinBrowserVersionSupported(const std::string& min_version) override;
  void CreateExtensionWebContentsObserver(
      content::WebContents* web_contents) override;
  ExtensionWebContentsObserver* GetExtensionWebContentsObserver(
      content::WebContents* web_contents) override;
  KioskDelegate* GetKioskDelegate() override;
  scoped_refptr<update_client::UpdateClient> CreateUpdateClient(
      content::BrowserContext* context) override;
  bool IsLockScreenContext(content::BrowserContext* context) override;
  std::string GetApplicationLocale() override;

  ExtensionSystemProvider* extension_system_factory() {
    return extension_system_factory_;
  }

  void set_pref_service_for_context(content::BrowserContext* context,
                                    PrefService* pref_service) {
    set_pref_service_for_context_[context] = pref_service;
  }

 private:
  // Not owned.
  raw_ptr<content::BrowserContext> main_context_ = nullptr;
  // Not owned.
  raw_ptr<content::BrowserContext> incognito_context_ = nullptr;
  // Not owned.
  raw_ptr<content::BrowserContext> lock_screen_context_ = nullptr;

  // Not owned.
  raw_ptr<ProcessManagerDelegate> process_manager_delegate_ = nullptr;

  // Not owned.
  raw_ptr<ExtensionSystemProvider> extension_system_factory_ = nullptr;

  // Not owned.
  raw_ptr<PrefService> pref_service_ = nullptr;

  // Not owned.
  std::map<content::BrowserContext*, raw_ptr<PrefService>>
      set_pref_service_for_context_;

  std::unique_ptr<ExtensionCache> extension_cache_;

  base::RepeatingCallback<update_client::UpdateClient*(void)>
      update_client_factory_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_TEST_EXTENSIONS_BROWSER_CLIENT_H_
