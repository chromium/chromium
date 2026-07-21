// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_MOJO_BINDER_REGISTRY_H_
#define EXTENSIONS_BROWSER_EXTENSION_MOJO_BINDER_REGISTRY_H_

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
struct ServiceWorkerVersionBaseInfo;
}  // namespace content

namespace extensions {

class Extension;

// A wrapper around `mojo::BinderMapWithContext` that applies an allowlist
// filter before registering an interface binder for an extension.
template <typename Context>
class ExtensionBinderMap {
 public:
  using AllowlistFilter =
      base::RepeatingCallback<bool(const Extension*, std::string_view)>;

  ExtensionBinderMap(mojo::BinderMapWithContext<Context>* binder_map,
                     const Extension* extension,
                     AllowlistFilter filter)
      : binder_map_(binder_map),
        extension_(extension),
        filter_(std::move(filter)) {}

  template <typename Interface>
  void Add(
      base::RepeatingCallback<void(Context, mojo::PendingReceiver<Interface>)>
          binder) {
    if (filter_.Run(extension_, Interface::Name_)) {
      binder_map_->template Add<Interface>(std::move(binder));
    } else {
      DLOG(ERROR) << "Rejected attempt to register Mojo interface binder '"
                  << Interface::Name_ << "' for component extension '"
                  << (extension_ ? extension_->id() : "null") << "'.";
    }
  }

  template <typename Interface>
  void Add(void (*binder)(Context, mojo::PendingReceiver<Interface>)) {
    if (filter_.Run(extension_, Interface::Name_)) {
      binder_map_->template Add<Interface>(binder);
    } else {
      DLOG(ERROR) << "Rejected attempt to register Mojo interface binder '"
                  << Interface::Name_ << "' for component extension '"
                  << (extension_ ? extension_->id() : "null") << "'.";
    }
  }

 private:
  raw_ptr<mojo::BinderMapWithContext<Context>> binder_map_;
  raw_ptr<const Extension> extension_;
  AllowlistFilter filter_;
};

// An interface for features to register extension-scoped Mojo binders when an
// extension document or service worker connects. Features should implement this
// interface and transfer ownership of the provider instance to the
// `ExtensionMojoBinderRegistry` singleton during startup (typically inside
// the constructors of profile keyed service factories).
class ExtensionMojoBinderProvider {
 public:
  virtual ~ExtensionMojoBinderProvider() = default;

  // Returns the ID of the component extension supported by this provider.
  virtual ExtensionId GetExtensionId() const = 0;

  virtual void PopulateFrameBinders(
      ExtensionBinderMap<content::RenderFrameHost*>& binder_map,
      content::RenderFrameHost* render_frame_host,
      const Extension* extension) {}

  virtual void PopulateServiceWorkerBinders(
      ExtensionBinderMap<const content::ServiceWorkerVersionBaseInfo&>&
          binder_map,
      content::BrowserContext* browser_context,
      const Extension* extension) {}
};

// A registry for extension-scoped Mojo interface binder providers. It decouples
// the core extensions layer from individual features by allowing features to
// register binder providers (`ExtensionMojoBinderProvider`) that populate their
// interfaces when extension documents or service workers request connection.
class ExtensionMojoBinderRegistry {
 public:
  ExtensionMojoBinderRegistry(const ExtensionMojoBinderRegistry&) = delete;
  ExtensionMojoBinderRegistry& operator=(const ExtensionMojoBinderRegistry&) =
      delete;

  static ExtensionMojoBinderRegistry* GetInstance();

  // Registers a provider for extension-scoped Mojo interface binders, taking
  // ownership of the provider instance.
  void RegisterProvider(std::unique_ptr<ExtensionMojoBinderProvider> provider);

  // Populates registered frame binders allowed for the extension into the map.
  void PopulateFrameBinders(
      mojo::BinderMapWithContext<content::RenderFrameHost*>* binder_map,
      content::RenderFrameHost* render_frame_host,
      const Extension* extension);

  // Populates registered service worker binders allowed for the extension into
  // the map.
  void PopulateServiceWorkerBinders(
      mojo::BinderMapWithContext<const content::ServiceWorkerVersionBaseInfo&>*
          binder_map,
      content::BrowserContext* browser_context,
      const Extension* extension);

  bool IsAllowedInterfaceForExtension(const Extension* extension,
                                      std::string_view interface_name) const;

  void SetBypassAllowlistForTesting(bool bypass);
  void ClearProvidersForTesting();

 private:
  friend class base::NoDestructor<ExtensionMojoBinderRegistry>;

  ExtensionMojoBinderRegistry();
  ~ExtensionMojoBinderRegistry();

  base::flat_map<ExtensionId, std::unique_ptr<ExtensionMojoBinderProvider>>
      providers_;
  bool bypass_allowlist_for_testing_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_MOJO_BINDER_REGISTRY_H_
