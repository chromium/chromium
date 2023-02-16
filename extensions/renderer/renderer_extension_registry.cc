// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/renderer_extension_registry.h"

#include "base/check.h"
#include "base/lazy_instance.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/manifest_handlers/background_info.h"

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

size_t RendererExtensionRegistry::size() const {
  base::AutoLock lock(lock_);
  return extensions_.size();
}

bool RendererExtensionRegistry::is_empty() const {
  base::AutoLock lock(lock_);
  return extensions_.is_empty();
}

bool RendererExtensionRegistry::Contains(
    const std::string& extension_id) const {
  base::AutoLock lock(lock_);
  return extensions_.Contains(extension_id);
}

bool RendererExtensionRegistry::Insert(
    const scoped_refptr<const Extension>& extension) {
  DCHECK(content::RenderThread::Get());
  base::AutoLock lock(lock_);
  return extensions_.Insert(extension);
}

bool RendererExtensionRegistry::Remove(const std::string& id) {
  DCHECK(content::RenderThread::Get());
  base::AutoLock lock(lock_);
  return extensions_.Remove(id);
}

std::string RendererExtensionRegistry::GetExtensionOrAppIDByURL(
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
    const std::string& id) const {
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

void RendererExtensionRegistry::SetWorkerActivationSequence(
    const scoped_refptr<const Extension>& extension,
    ActivationSequence worker_activation_sequence) {
  DCHECK(content::RenderThread::Get());
  DCHECK(Contains(extension->id()));
  DCHECK(BackgroundInfo::IsServiceWorkerBased(extension.get()));

  base::AutoLock lock(lock_);
  worker_activation_sequences_[extension->id()] = worker_activation_sequence;
}

absl::optional<ActivationSequence>
RendererExtensionRegistry::GetWorkerActivationSequence(
    const ExtensionId& extension_id) const {
  base::AutoLock lock(lock_);
  auto iter = worker_activation_sequences_.find(extension_id);
  if (iter == worker_activation_sequences_.end())
    return absl::nullopt;
  return iter->second;
}

}  // namespace extensions
