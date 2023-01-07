// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_ui_browser_interface_broker_registry.h"

#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

namespace content {

WebUIBrowserInterfaceBrokerRegistry::WebUIBrowserInterfaceBrokerRegistry() {
  GetContentClient()->browser()->RegisterWebUIInterfaceBrokers(*this);
}

WebUIBrowserInterfaceBrokerRegistry::~WebUIBrowserInterfaceBrokerRegistry() =
    default;

std::unique_ptr<PerWebUIBrowserInterfaceBroker>
WebUIBrowserInterfaceBrokerRegistry::CreateInterfaceBroker(
    WebUIController& controller) {
  auto iter = binder_initializers_.find(controller.GetType());
  if (iter == binder_initializers_.end())
    return nullptr;

  return std::make_unique<PerWebUIBrowserInterfaceBroker>(controller,
                                                          iter->second);
}

}  // namespace content