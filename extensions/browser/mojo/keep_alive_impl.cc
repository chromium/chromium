// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mojo/keep_alive_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "content/public/browser/browser_context.h"

namespace extensions {

// static
void KeepAliveImpl::Create(content::BrowserContext* context,
                           const Extension* extension,
                           content::RenderFrameHost* render_frame_host,
                           mojo::PendingReceiver<KeepAlive> receiver) {
  // Owns itself.
  new KeepAliveImpl(context, extension, std::move(receiver));
}

KeepAliveImpl::KeepAliveImpl(content::BrowserContext* context,
                             const Extension* extension,
                             mojo::PendingReceiver<KeepAlive> receiver)
    : context_(context),
      extension_(extension),
      receiver_(this, std::move(receiver)) {
  ProcessManager::Get(context_)->IncrementLazyKeepaliveCount(
      extension_, Activity::MOJO, std::string());
  receiver_.set_disconnect_handler(
      base::BindOnce(&KeepAliveImpl::OnDisconnected, base::Unretained(this)));
  extension_registry_observation_.Observe(ExtensionRegistry::Get(context_));
  process_manager_observation_.Observe(ProcessManager::Get(context_));
}

KeepAliveImpl::~KeepAliveImpl() = default;

void KeepAliveImpl::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  if (browser_context == context_ && extension == extension_) {
    delete this;
  }
}

void KeepAliveImpl::OnShutdown(ExtensionRegistry* registry) {
  delete this;
}

void KeepAliveImpl::OnDisconnected() {
  ProcessManager::Get(context_)->DecrementLazyKeepaliveCount(
      extension_, Activity::MOJO, std::string());
  delete this;
}

void KeepAliveImpl::OnProcessManagerShutdown(ProcessManager* manager) {
  delete this;
}

}  // namespace extensions
