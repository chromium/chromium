// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_proxying_webtransport_shutdown_notifier_factory.h"

#include "extensions/browser/extension_registry_factory.h"

namespace extensions {

// static
WebRequestProxyingWebTransportShutdownNotifierFactory*
WebRequestProxyingWebTransportShutdownNotifierFactory::GetInstance() {
  static base::NoDestructor<
      WebRequestProxyingWebTransportShutdownNotifierFactory>
      instance;
  return instance.get();
}

WebRequestProxyingWebTransportShutdownNotifierFactory::
    WebRequestProxyingWebTransportShutdownNotifierFactory()
    : BrowserContextKeyedServiceShutdownNotifierFactory(
          "WebRequestProxyingWebTransport") {
  DependsOn(ExtensionRegistryFactory::GetInstance());
}

}  // namespace extensions
