// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/extensions_renderer_client.h"

#include "base/check.h"

namespace extensions {

namespace {

ExtensionsRendererClient* g_client = NULL;

}  // namespace

ExtensionsRendererClient* ExtensionsRendererClient::Get() {
  CHECK(g_client);
  return g_client;
}

void ExtensionsRendererClient::Set(ExtensionsRendererClient* client) {
  g_client = client;
}

}  // namespace extensions
