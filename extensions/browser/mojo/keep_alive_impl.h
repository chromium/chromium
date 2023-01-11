// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MOJO_KEEP_ALIVE_IMPL_H_
#define EXTENSIONS_BROWSER_MOJO_KEEP_ALIVE_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/common/mojom/keep_alive.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
}

namespace extensions {
class Extension;
class ProcessManager;

// An RAII mojo service implementation for extension keep alives. This adds a
// keep alive on construction and removes it on destruction.
class KeepAliveImpl : public KeepAlive,
                      public ExtensionRegistryObserver,
                      public ProcessManagerObserver {
 public:
  KeepAliveImpl(const KeepAliveImpl&) = delete;
  KeepAliveImpl& operator=(const KeepAliveImpl&) = delete;

  // Create a keep alive for |extension| running in |context| and connect it to
  // |receiver|. When the receiver closes its pipe, the keep alive ends.
  static void Create(content::BrowserContext* browser_context,
                     const Extension* extension,
                     content::RenderFrameHost* render_frame_host,
                     mojo::PendingReceiver<KeepAlive> receiver);

 private:
  KeepAliveImpl(content::BrowserContext* context,
                const Extension* extension,
                mojo::PendingReceiver<KeepAlive> receiver);
  ~KeepAliveImpl() override;

  // ExtensionRegistryObserver overrides.
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;
  void OnShutdown(ExtensionRegistry* registry) override;

  // ProcessManagerObserver overrides.
  void OnProcessManagerShutdown(ProcessManager* manager) override;

  // Invoked when the mojo connection is disconnected.
  void OnDisconnected();

  raw_ptr<content::BrowserContext> context_;
  raw_ptr<const Extension> extension_;
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};
  base::ScopedObservation<ProcessManager, ProcessManagerObserver>
      process_manager_observation_{this};
  mojo::Receiver<KeepAlive> receiver_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MOJO_KEEP_ALIVE_IMPL_H_
