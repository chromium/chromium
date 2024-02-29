// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_RENDERER_EXTENSION_REGISTRY_H_
#define EXTENSIONS_RENDERER_RENDERER_EXTENSION_REGISTRY_H_

#include <stddef.h>

#include <optional>

#include "base/synchronization/lock.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"

class GURL;

namespace base {
class UnguessableToken;
}

namespace extensions {

// Thread safe container for all loaded extensions in this process,
// essentially the renderer counterpart to ExtensionRegistry.
class RendererExtensionRegistry {
 public:
  RendererExtensionRegistry();

  RendererExtensionRegistry(const RendererExtensionRegistry&) = delete;
  RendererExtensionRegistry& operator=(const RendererExtensionRegistry&) =
      delete;

  ~RendererExtensionRegistry();

  static RendererExtensionRegistry* Get();

  // Returns the ExtensionSet that underlies this RenderExtensionRegistry.
  //
  // This is not thread-safe and must only be called on the RenderThread, but
  // even so, it's not thread safe because other threads may decide to
  // modify this. Don't persist a reference to this.
  //
  // TODO(annekao): remove or make thread-safe and callback-based.
  const ExtensionSet* GetMainThreadExtensionSet() const;

  // Forwards to the ExtensionSet methods by the same name.
  bool Contains(const ExtensionId& id) const;
  bool Insert(const scoped_refptr<const Extension>& extension);
  bool Remove(const ExtensionId& id);
  ExtensionId GetExtensionOrAppIDByURL(const GURL& url) const;
  const Extension* GetExtensionOrAppByURL(const GURL& url,
                                          bool include_guid = false) const;
  const Extension* GetHostedAppByURL(const GURL& url) const;
  const Extension* GetByID(const ExtensionId& id) const;
  ExtensionIdSet GetIDs() const;
  bool ExtensionBindingsAllowed(const GURL& url) const;

  // Activation token-related methods.
  //
  // Sets the activation token for a Service Worker based |extension|.
  void SetWorkerActivationToken(const scoped_refptr<const Extension>& extension,
                                base::UnguessableToken worker_activation_token);
  // Returns the current activation token for worker based extension with
  // |extension_id|. Returns std::nullopt otherwise.
  std::optional<base::UnguessableToken> GetWorkerActivationToken(
      const ExtensionId& extension_id) const;

 private:
  ExtensionSet extensions_;

  // Maps extension id to activation token, for worker based extensions.
  std::map<ExtensionId, base::UnguessableToken> worker_activation_tokens_;

  mutable base::Lock lock_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_RENDERER_EXTENSION_REGISTRY_H_
