// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_TEST_EXTENSIONS_BROWSER_CLIENT_H_
#define EXTENSIONS_BROWSER_TEST_EXTENSIONS_BROWSER_CLIENT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "components/update_client/update_client.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/updater/extension_cache.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

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
  void set_extension_cache(std::unique_ptr<ExtensionCache> extension_cache) {
    extension_cache_ = std::move(extension_cache);
  }

  void set_lock_screen_context(content::BrowserContext* context) {
    lock_screen_context_ = context;
  }

  // Sets a factory to respond to calls of the CreateUpdateClient method.
  void SetUpdateClientFactory(
      const base::Callback<update_client::UpdateClient*(void)>& factory);

  // Sets the main browser context. Only call if a BrowserContext was not
  // already provided. |main_context| must not be an incognito context.
  void SetMainContext(content::BrowserContext* main_context);

  // Associates an incognito context with |main_context_|.
  void SetIncognitoContext(content::BrowserContext* incognito_context);

  // ExtensionsBrowserClient overrides:
  bool IsShuttingDown() override;
  bool AreExtensionsDisabled(const base::CommandLine& command_line,
                             content::BrowserContext* context) override;
  bool IsValidContext(content::BrowserContext* context) override;
  bool IsSameContext(content::BrowserContext* first,
                     content::BrowserContext* second) override;
  bool HasOffTheRecordContext(content::BrowserContext* context) override;
  content::BrowserContext* GetOffTheRecordContext(
      content::BrowserContext* context) override;
  content::BrowserContext* GetOriginalContext(
      content::BrowserContext* context) override;
#if defined(OS_CHROMEOS)
  std::string GetUserIdHashFromContext(
      content::BrowserContext* context) override;
#endif
  bool IsGuestSession(content::BrowserContext* context) const override;
  bool IsExtensionIncognitoEnabled(
      const std::string& extension_id,
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
      const std::string& content_security_policy,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      bool send_cors_header) override;

  bool AllowCrossRendererResourceLoad(const GURL& url,
                                      blink::mojom::ResourceType resource_type,
                                      ui::PageTransition page_transition,
                                      int child_id,
                                      bool is_incognito,
                                      const Extension* extension,
                                      const ExtensionSet& extensions,
                                      const ProcessMap& process_map) override;
  PrefService* GetPrefServiceForContext(
      content::BrowserContext* context) override;
  void GetEarlyExtensionPrefsObservers(
      content::BrowserContext* context,
      std::vector<EarlyExtensionPrefsObserver*>* observers) const override;
  ProcessManagerDelegate* GetProcessManagerDelegate() const override;
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
      std::unique_ptr<base::ListValue> args,
      bool dispatch_to_off_the_record_profiles) override;
  ExtensionCache* GetExtensionCache() override;
  bool IsBackgroundUpdateAllowed() override;
  bool IsMinBrowserVersionSupported(const std::string& min_version) override;
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

 private:
  // Not owned.
  content::BrowserContext* main_context_ = nullptr;
  // Not owned.
  content::BrowserContext* incognito_context_ = nullptr;
  // Not owned.
  content::BrowserContext* lock_screen_context_ = nullptr;

  // Not owned.
  ProcessManagerDelegate* process_manager_delegate_ = nullptr;

  // Not owned.
  ExtensionSystemProvider* extension_system_factory_ = nullptr;

  std::unique_ptr<ExtensionCache> extension_cache_;

  base::Callback<update_client::UpdateClient*(void)> update_client_factory_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_TEST_EXTENSIONS_BROWSER_CLIENT_H_
