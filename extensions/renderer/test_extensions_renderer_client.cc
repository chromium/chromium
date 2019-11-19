// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/test_extensions_renderer_client.h"

namespace extensions {

TestExtensionsRendererClient::TestExtensionsRendererClient() {
  ExtensionsRendererClient::Set(this);
}

TestExtensionsRendererClient::~TestExtensionsRendererClient() {
  ExtensionsRendererClient::Set(nullptr);
}

bool TestExtensionsRendererClient::IsIncognitoProcess() const {
  return false;
}

int TestExtensionsRendererClient::GetLowestIsolatedWorldId() const {
  // Note that 0 is reserved for the global world.
  return 1;
}

Dispatcher* TestExtensionsRendererClient::GetDispatcher() {
  return nullptr;
}

bool TestExtensionsRendererClient::ExtensionAPIEnabledForServiceWorkerScript(
    const GURL& scope,
    const GURL& script_url) const {
  return false;
}

}  // namespace extensions
