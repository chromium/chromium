// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_RENDERER_STARTUP_HELPER_H_
#define EXTENSIONS_BROWSER_RENDERER_STARTUP_HELPER_H_

#include <map>
#include <set>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/browser/render_process_host_observer.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
class RenderProcessHost;
}

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
                              public content::RenderProcessHostObserver {
 public:
  // This class sends messages to all renderers started for |browser_context|.
  explicit RendererStartupHelper(content::BrowserContext* browser_context);
  ~RendererStartupHelper() override;

  // content::RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(
      content::RenderProcessHost* process_host) override;

  // content::RenderProcessHostObserver:
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

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

 private:
  friend class RendererStartupHelperTest;

  // Initializes the specified process, informing it of system state and loaded
  // extensions.
  void InitializeProcess(content::RenderProcessHost* process);

  // Untracks the given process.
  void UntrackProcess(content::RenderProcessHost* process);

  content::BrowserContext* browser_context_;  // Not owned.

  // Tracks the set of loaded extensions and the processes they are loaded in.
  std::map<ExtensionId, std::set<content::RenderProcessHost*>>
      extension_process_map_;

  // The set of render processes that have had the initial batch of IPC messages
  // sent, including the set of loaded extensions. Further messages that
  // activate, load, or unload extensions should not be sent until after this
  // happens.
  std::set<content::RenderProcessHost*> initialized_processes_;

  // The set of ids for extensions that are active in a process that has not
  // been initialized. The activation message will be sent the process is
  // initialized.
  std::map<content::RenderProcessHost*, std::set<ExtensionId>>
      pending_active_extensions_;

  DISALLOW_COPY_AND_ASSIGN(RendererStartupHelper);
};

// Factory for RendererStartupHelpers. Declared here because this header is
// rarely included and it's probably cheaper to put it here than to make the
// compiler generate another object file.
class RendererStartupHelperFactory : public BrowserContextKeyedServiceFactory {
 public:
  static RendererStartupHelper* GetForBrowserContext(
      content::BrowserContext* context);
  static RendererStartupHelperFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<RendererStartupHelperFactory>;

  RendererStartupHelperFactory();
  ~RendererStartupHelperFactory() override;

  // BrowserContextKeyedServiceFactory implementation:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;

  DISALLOW_COPY_AND_ASSIGN(RendererStartupHelperFactory);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_RENDERER_STARTUP_HELPER_H_
