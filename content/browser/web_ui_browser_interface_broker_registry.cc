// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_ui_browser_interface_broker_registry.h"

#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

namespace content {

WebUIBrowserInterfaceBrokerRegistry::WebUIBrowserInterfaceBrokerRegistry() =
    default;
WebUIBrowserInterfaceBrokerRegistry::~WebUIBrowserInterfaceBrokerRegistry() =
    default;

// This registry maintains a mapping from WebUI to its MojoJS interface broker
// initializer, i.e. callbacks that populate an interface broker's binder map
// with interfaces exposed to MojoJS. If such a mapping exists, we instantiate
// the broker in ReadyToCommitNavigation, enable MojoJS bindings for this
// frame, and ask renderer to use it to handle Mojo.bindInterface calls.
WebUIBrowserInterfaceBrokerRegistry&
WebUIBrowserInterfaceBrokerRegistry::GetTrustedRegistry() {
  static base::NoDestructor<
      std::unique_ptr<WebUIBrowserInterfaceBrokerRegistry>>
      trusted_registry([&] {
        auto trusted = std::make_unique<WebUIBrowserInterfaceBrokerRegistry>();
        GetContentClient()->browser()->RegisterTrustedWebUIInterfaceBrokers(
            *trusted);
        return trusted;
      }());
  return **trusted_registry;
}

WebUIBrowserInterfaceBrokerRegistry&
WebUIBrowserInterfaceBrokerRegistry::GetUntrustedRegistry() {
  static base::NoDestructor<
      std::unique_ptr<WebUIBrowserInterfaceBrokerRegistry>>
      untrusted_registry([&] {
        auto untrusted =
            std::make_unique<WebUIBrowserInterfaceBrokerRegistry>();
        GetContentClient()->browser()->RegisterUntrustedWebUIInterfaceBrokers(
            *untrusted);
        return untrusted;
      }());
  return **untrusted_registry;
}

std::unique_ptr<PerWebUIBrowserInterfaceBroker>
WebUIBrowserInterfaceBrokerRegistry::CreateInterfaceBroker(
    WebUIController& controller) {
  auto iter = binder_initializers_.find(controller.GetType());
  if (iter == binder_initializers_.end()) {
    return nullptr;
  }

  std::vector<BinderInitializer> binder_initializers =
      global_binder_initializers_;
  std::ranges::copy(iter->second, std::back_inserter(binder_initializers));

  return std::make_unique<PerWebUIBrowserInterfaceBroker>(controller,
                                                          binder_initializers);
}

}  // namespace content
