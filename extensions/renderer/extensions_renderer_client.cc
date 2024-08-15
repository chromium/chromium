// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/extensions_renderer_client.h"

#include <memory>
#include <ostream>

#include "base/check.h"
#include "extensions/renderer/dispatcher.h"

namespace extensions {

namespace {

ExtensionsRendererClient* g_client = nullptr;

}  // namespace

ExtensionsRendererClient::ExtensionsRendererClient() = default;
ExtensionsRendererClient::~ExtensionsRendererClient() = default;

ExtensionsRendererClient* ExtensionsRendererClient::Get() {
  CHECK(g_client);
  return g_client;
}

void ExtensionsRendererClient::Set(ExtensionsRendererClient* client) {
  g_client = client;
}

void ExtensionsRendererClient::AddAPIProvider(
    std::unique_ptr<ExtensionsRendererAPIProvider> api_provider) {
  CHECK(!dispatcher())
      << "API providers must be added before the Dispatcher is instantiated.";
  api_providers_.push_back(std::move(api_provider));
}

void ExtensionsRendererClient::SetDispatcherForTesting(
    std::unique_ptr<Dispatcher> dispatcher) {
  dispatcher_ = std::move(dispatcher);
}

void ExtensionsRendererClient::CreateDispatcher() {
  CHECK(!dispatcher_);
  dispatcher_ = std::make_unique<Dispatcher>(std::move(api_providers_));
}

}  // namespace extensions
