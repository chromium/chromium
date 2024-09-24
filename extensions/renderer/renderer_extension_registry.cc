// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/renderer_extension_registry.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/lazy_instance.h"
#include "base/unguessable_token.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/renderer/extensions_renderer_client.h"

namespace extensions {

namespace {

base::LazyInstance<RendererExtensionRegistry>::DestructorAtExit
    g_renderer_extension_registry = LAZY_INSTANCE_INITIALIZER;

}  // namespace

RendererExtensionRegistry::RendererExtensionRegistry() = default;

RendererExtensionRegistry::~RendererExtensionRegistry() = default;

// static
RendererExtensionRegistry* RendererExtensionRegistry::Get() {
  return g_renderer_extension_registry.Pointer();
}

const ExtensionSet* RendererExtensionRegistry::GetMainThreadExtensionSet()
    const {
  // This can only be modified on the RenderThread, because
  // GetMainThreadExtensionSet is inherently thread unsafe.
  // Enforcing single-thread modification at least mitigates this.
  // TODO(annekao): Remove this restriction once GetMainThreadExtensionSet is
  // fixed.
  DCHECK(content::RenderThread::Get());
  base::AutoLock lock(lock_);
  return &extensions_;
}

bool RendererExtensionRegistry::Contains(
    const ExtensionId& extension_id) const {
  base::AutoLock lock(lock_);
  return extensions_.Contains(extension_id);
}

bool RendererExtensionRegistry::Insert(
    const scoped_refptr<const Extension>& extension) {
  DCHECK(content::RenderThread::Get());
  base::AutoLock lock(lock_);

  if (!BackgroundInfo::IsServiceWorkerBased(extension.get())) {
    // Non-SW based extension should never have an activation token.
    CHECK(!base::Contains(worker_activation_tokens_, extension->id()));
    return extensions_.Insert(extension);
  }

  ExtensionsRendererClient* client = ExtensionsRendererClient::Get();

  // SW based extensions should always have an activation token, except for
  // incognito processes for a spanning mode extension. The CHECK() for all
  // other worker based extension is performed in
  // Dispatcher::WillEvaluateServiceWorkerOnWorkerThread(). We can't CHECK() for
  // IsIncognitoProcess() == false here because this may be called on renderer
  // process initialization before the boolean for that has been set.
  bool is_incognito_spanning = client->IsIncognitoProcess() &&
                               IncognitoInfo::IsSpanningMode(extension.get());
  if (is_incognito_spanning) {
    CHECK(!base::Contains(worker_activation_tokens_, extension->id()));
  }

  return extensions_.Insert(extension);
}

bool RendererExtensionRegistry::Remove(const ExtensionId& id) {
  DCHECK(content::RenderThread::Get());
  base::AutoLock lock(lock_);
  return extensions_.Remove(id);
}

ExtensionId RendererExtensionRegistry::GetExtensionOrAppIDByURL(
    const GURL& url) const {
  base::AutoLock lock(lock_);
  return extensions_.GetExtensionOrAppIDByURL(url);
}

const Extension* RendererExtensionRegistry::GetExtensionOrAppByURL(
    const GURL& url,
    bool include_guid) const {
  base::AutoLock lock(lock_);
  return extensions_.GetExtensionOrAppByURL(url, include_guid);
}

const Extension* RendererExtensionRegistry::GetHostedAppByURL(
    const GURL& url) const {
  base::AutoLock lock(lock_);
  return extensions_.GetHostedAppByURL(url);
}

const Extension* RendererExtensionRegistry::GetByID(
    const ExtensionId& id) const {
  base::AutoLock lock(lock_);
  return extensions_.GetByID(id);
}

ExtensionIdSet RendererExtensionRegistry::GetIDs() const {
  base::AutoLock lock(lock_);
  return extensions_.GetIDs();
}

bool RendererExtensionRegistry::ExtensionBindingsAllowed(
    const GURL& url) const {
  base::AutoLock lock(lock_);
  return extensions_.ExtensionBindingsAllowed(url);
}

void RendererExtensionRegistry::SetWorkerActivationToken(
    const scoped_refptr<const Extension>& extension,
    base::UnguessableToken worker_activation_token) {
  DCHECK(content::RenderThread::Get());
  DCHECK(BackgroundInfo::IsServiceWorkerBased(extension.get()));

  base::AutoLock lock(lock_);
  worker_activation_tokens_[extension->id()] =
      std::move(worker_activation_token);
}

std::optional<base::UnguessableToken>
RendererExtensionRegistry::GetWorkerActivationToken(
    const ExtensionId& extension_id) const {
  base::AutoLock lock(lock_);
  auto iter = worker_activation_tokens_.find(extension_id);
  if (iter == worker_activation_tokens_.end()) {
    return std::nullopt;
  }
  return iter->second;
}

}  // namespace extensions
