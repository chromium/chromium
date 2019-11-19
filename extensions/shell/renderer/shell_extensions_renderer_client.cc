// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/renderer/shell_extensions_renderer_client.h"

#include "content/public/renderer/render_thread.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/dispatcher_delegate.h"

namespace extensions {

ShellExtensionsRendererClient::ShellExtensionsRendererClient()
    : dispatcher_(std::make_unique<Dispatcher>(
          std::make_unique<DispatcherDelegate>())) {
  dispatcher_->OnRenderThreadStarted(content::RenderThread::Get());
}

ShellExtensionsRendererClient::~ShellExtensionsRendererClient() {
}

bool ShellExtensionsRendererClient::IsIncognitoProcess() const {
  // app_shell doesn't support off-the-record contexts.
  return false;
}

int ShellExtensionsRendererClient::GetLowestIsolatedWorldId() const {
  // app_shell doesn't need to reserve world IDs for anything other than
  // extensions, so we always return 1. Note that 0 is reserved for the global
  // world.
  return 1;
}

Dispatcher* ShellExtensionsRendererClient::GetDispatcher() {
  return dispatcher_.get();
}

bool ShellExtensionsRendererClient::ExtensionAPIEnabledForServiceWorkerScript(
    const GURL& scope,
    const GURL& script_url) const {
  return false;
}

}  // namespace extensions
