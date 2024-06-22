// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_RENDERER_STARTUP_HELPER_H_
#define EXTENSIONS_BROWSER_RENDERER_STARTUP_HELPER_H_

#include <map>
#include <set>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/browser/render_process_host_observer.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/renderer.mojom.h"
#include "extensions/common/mojom/renderer_host.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace content {
class BrowserContext;
class RenderProcessHost;
}  // namespace content

namespace extensions {
class Extension;

// Informs renderers about extensions-related data (loaded extensions, available
// functions, etc.) when they start. Sends this information to both extension
// and non-extension renderers, as the non-extension renderers may have content
// scripts. Lives on the UI thread. Shared between incognito and non-incognito
// browser contexts. Also handles sending the loaded, unloaded, and activated
// extension messages, since these can *only* be sent once the process is
// initialized.
// TODO(devlin): "StartupHelper" is no longer sufficient to describe the entire
// behavior of this class.
class RendererStartupHelper : public KeyedService,
                              public content::RenderProcessHostCreationObserver,
                              public content::RenderProcessHostObserver,
                              public mojom::RendererHost {
 public:
  // This class sends messages to all renderers started for |browser_context|.
  explicit RendererStartupHelper(content::BrowserContext* browser_context);

  RendererStartupHelper(const RendererStartupHelper&) = delete;
  RendererStartupHelper& operator=(const RendererStartupHelper&) = delete;

  ~RendererStartupHelper() override;

  // content::RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(
      content::RenderProcessHost* process_host) override;

  // content::RenderProcessHostObserver:
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // mojom::RendererHost:
  void AddAPIActionToActivityLog(const ExtensionId& extension_id,
                                 const std::string& call_name,
                                 base::Value::List args,
                                 const std::string& extra) override;
  void AddEventToActivityLog(const ExtensionId& extension_id,
                             const std::string& call_name,
                             base::Value::List args,
                             const std::string& extra) override;
  void AddDOMActionToActivityLog(const ExtensionId& extension_id,
                                 const std::string& call_name,
                                 base::Value::List args,
                                 const GURL& url,
                                 const std::u16string& url_title,
                                 int32_t call_type) override;
  void GetMessageBundle(const ExtensionId& extension_id,
                        GetMessageBundleCallback callback) override;

  // Sends a message to the specified |process| activating the given extension
  // once the process is initialized. OnExtensionLoaded should have already been
  // called for the extension.
  void ActivateExtensionInProcess(const Extension& extension,
                                  content::RenderProcessHost* process);

  // Sends a message to all initialized processes to [un]load the given
  // extension. We have explicit calls for these (rather than using an
  // ExtensionRegistryObserver) because this needs to happen before other
  // initialization which might rely on the renderers being notified.
  void OnExtensionUnloaded(const Extension& extension);
  void OnExtensionLoaded(const Extension& extension);

  // Sends a message to all renderers to update the developer mode.
  void OnDeveloperModeChanged(bool in_developer_mode);

  // Sets properties for the user script world of the given `world_id` for
  // the given `extension` in all applicable renderers.
  void SetUserScriptWorldProperties(const Extension& extension,
                                    std::optional<std::string> world_id,
                                    std::optional<std::string> csp,
                                    bool enable_messaging);

  // Notifies renderers to clear any properties for the user script world
  // associated with the given `extension` and `world_id`.
  void ClearUserScriptWorldProperties(
      const Extension& extension,
      const std::optional<std::string>& world_id);

  // Returns mojom::Renderer* corresponding to |process|. This would return
  // nullptr when it's called before |process| is inserted to
  // |process_mojo_map_| or after it's deleted. Note that the callers should
  // pass a valid content::RenderProcessHost*.
  mojom::Renderer* GetRenderer(content::RenderProcessHost* process);

  static void BindForRenderer(
      int process_id,
      mojo::PendingAssociatedReceiver<mojom::RendererHost> receiver);

 protected:
  // Provide ability for tests to override.
  virtual mojo::PendingAssociatedRemote<mojom::Renderer> BindNewRendererRemote(
      content::RenderProcessHost* process);

 private:
  friend class RendererStartupHelperTest;
  friend class RendererStartupHelperInterceptor;

  // Initializes the specified process, informing it of system state and loaded
  // extensions.
  void InitializeProcess(content::RenderProcessHost* process);

  // Untracks the given process.
  void UntrackProcess(content::RenderProcessHost* process);

  // Return the browser context associated with the renderer.
  content::BrowserContext* GetRendererBrowserContext();

  // TODO(hferreiro): can be nullptr as in ChromeExtensionMessageFilter?
  raw_ptr<content::BrowserContext> browser_context_;  // Not owned.

  // Tracks the set of loaded extensions and the processes they are loaded in.
  std::map<ExtensionId,
           std::set<raw_ptr<content::RenderProcessHost, SetExperimental>>>
      extension_process_map_;

  // The set of ids for extensions that are active in a process that has not
  // been initialized. The activation message will be sent the process is
  // initialized.
  std::map<content::RenderProcessHost*, std::set<ExtensionId>>
      pending_active_extensions_;

  // A map of render processes to mojo remotes. Being in this
  // map means that have had the initial batch of IPC messages
  // sent, including the set of loaded extensions. Further messages that
  // activate, load, or unload extensions should not be sent until after this
  // happens.
  std::map<content::RenderProcessHost*, mojo::AssociatedRemote<mojom::Renderer>>
      process_mojo_map_;

  // Associate each renderer with the RenderProcessHost id.
  mojo::AssociatedReceiverSet<mojom::RendererHost, int> receivers_;
};

// Factory for RendererStartupHelpers. Declared here because this header is
// rarely included and it's probably cheaper to put it here than to make the
// compiler generate another object file.
class RendererStartupHelperFactory : public BrowserContextKeyedServiceFactory {
 public:
  RendererStartupHelperFactory(const RendererStartupHelperFactory&) = delete;
  RendererStartupHelperFactory& operator=(const RendererStartupHelperFactory&) =
      delete;

  static RendererStartupHelper* GetForBrowserContext(
      content::BrowserContext* context);
  static RendererStartupHelperFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<RendererStartupHelperFactory>;

  RendererStartupHelperFactory();
  ~RendererStartupHelperFactory() override;

  // BrowserContextKeyedServiceFactory implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_RENDERER_STARTUP_HELPER_H_
