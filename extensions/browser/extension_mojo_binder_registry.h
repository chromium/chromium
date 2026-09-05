// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_MOJO_BINDER_REGISTRY_H_
#define EXTENSIONS_BROWSER_EXTENSION_MOJO_BINDER_REGISTRY_H_

#include <memory>

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/pass_key.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
struct ServiceWorkerVersionBaseInfo;
}  // namespace content

namespace extensions {

class Extension;

// An interface for features to register extension-scoped Mojo binders when an
// extension document or service worker connects. Features should implement this
// interface and transfer ownership of the provider instance to the
// `ExtensionMojoBinderRegistry` KeyedService for a given BrowserContext.
class ExtensionMojoBinderProvider {
 public:
  explicit ExtensionMojoBinderProvider(ExtensionId extension_id);
  virtual ~ExtensionMojoBinderProvider();

  // Returns the ID of the component extension supported by this provider.
  const ExtensionId& extension_id() const { return extension_id_; }

  // Returns true if Mojo JS bindings should be enabled in this extension's
  // document contexts. Defaults to false.
  virtual bool IsMojoJsEnabledForFrame() const;

  // Returns true if Mojo JS bindings should be enabled in this extension's
  // service worker contexts. Defaults to false.
  virtual bool IsMojoJsEnabledForServiceWorker() const;

  // Registers Mojo interface binders for document frames belonging to this
  // extension.
  virtual void PopulateFrameBinders(
      mojo::BinderMapWithContext<content::RenderFrameHost*>& binder_map,
      content::RenderFrameHost* render_frame_host,
      const Extension& extension) {}

  // Registers Mojo interface binders for service workers belonging to this
  // extension.
  virtual void PopulateServiceWorkerBinders(
      mojo::BinderMapWithContext<const content::ServiceWorkerVersionBaseInfo&>&
          binder_map,
      content::BrowserContext* browser_context,
      const Extension& extension) {}

 private:
  const ExtensionId extension_id_;
};

// A registry for extension-scoped Mojo interface binder providers. It decouples
// the core extensions layer from individual features by allowing features to
// register binder providers (`ExtensionMojoBinderProvider`) that populate their
// interfaces when extension documents or service workers request connection.
class ExtensionMojoBinderRegistry : public KeyedService {
 public:
  ExtensionMojoBinderRegistry();
  ExtensionMojoBinderRegistry(const ExtensionMojoBinderRegistry&) = delete;
  ExtensionMojoBinderRegistry& operator=(const ExtensionMojoBinderRegistry&) =
      delete;
  ~ExtensionMojoBinderRegistry() override;

  // Registers a provider for extension-scoped Mojo interface binders, taking
  // ownership of the provider instance.
  // See specializations in extension_mojo_binder_registry.cc for approved
  // callers and IPC review requirements.
  template <typename T>
  void RegisterProvider(base::PassKey<T> passkey,
                        std::unique_ptr<ExtensionMojoBinderProvider> provider);

  // Populates the binder map with Mojo binders provided by the registered
  // extension provider for the given document.
  void PopulateFrameBinders(
      mojo::BinderMapWithContext<content::RenderFrameHost*>* binder_map,
      content::RenderFrameHost* render_frame_host,
      const Extension& extension);

  // Populates the binder map with Mojo binders provided by the registered
  // extension provider for the given service worker.
  void PopulateServiceWorkerBinders(
      mojo::BinderMapWithContext<const content::ServiceWorkerVersionBaseInfo&>*
          binder_map,
      content::BrowserContext* browser_context,
      const Extension& extension);

  // Returns true if `extension` is allowed to use MojoJS bindings in document
  // contexts.
  bool IsMojoJsEnabledForFrame(const Extension& extension) const;

  // Returns true if `extension` is allowed to use MojoJS bindings in service
  // worker contexts.
  bool IsMojoJsEnabledForServiceWorker(const Extension& extension) const;

  void ClearProvidersForTesting();

 private:
  void RegisterProviderImpl(
      std::unique_ptr<ExtensionMojoBinderProvider> provider);

  ExtensionMojoBinderProvider* GetProviderIfAllowed(
      const Extension& extension) const;

  SEQUENCE_CHECKER(sequence_checker_);
  absl::flat_hash_map<ExtensionId, std::unique_ptr<ExtensionMojoBinderProvider>>
      providers_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_MOJO_BINDER_REGISTRY_H_
