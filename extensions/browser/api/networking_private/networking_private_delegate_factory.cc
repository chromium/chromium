// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/networking_private/networking_private_delegate_factory.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extensions_browser_client.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "extensions/browser/api/networking_private/networking_private_chromeos.h"
#elif BUILDFLAG(IS_LINUX)
#include "extensions/browser/api/networking_private/networking_private_linux.h"
#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "components/wifi/wifi_service.h"
#include "extensions/browser/api/networking_private/networking_private_service_client.h"
#endif

namespace extensions {

using content::BrowserContext;

NetworkingPrivateDelegateFactory::UIDelegateFactory::UIDelegateFactory() =
    default;

NetworkingPrivateDelegateFactory::UIDelegateFactory::~UIDelegateFactory() =
    default;

// static
NetworkingPrivateDelegate*
NetworkingPrivateDelegateFactory::GetForBrowserContext(
    BrowserContext* browser_context) {
  return static_cast<NetworkingPrivateDelegate*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
NetworkingPrivateDelegateFactory*
NetworkingPrivateDelegateFactory::GetInstance() {
  return base::Singleton<NetworkingPrivateDelegateFactory>::get();
}

NetworkingPrivateDelegateFactory::NetworkingPrivateDelegateFactory()
    : BrowserContextKeyedServiceFactory(
          "NetworkingPrivateDelegate",
          BrowserContextDependencyManager::GetInstance()) {}

NetworkingPrivateDelegateFactory::~NetworkingPrivateDelegateFactory() = default;

void NetworkingPrivateDelegateFactory::SetUIDelegateFactory(
    std::unique_ptr<UIDelegateFactory> factory) {
  ui_factory_ = std::move(factory);
}

std::unique_ptr<KeyedService>
NetworkingPrivateDelegateFactory::BuildServiceInstanceForBrowserContext(
    BrowserContext* browser_context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<NetworkingPrivateDelegate> delegate;
#if BUILDFLAG(IS_CHROMEOS)
  delegate = std::make_unique<NetworkingPrivateChromeOS>(browser_context);
#elif BUILDFLAG(IS_LINUX)
  delegate = std::make_unique<NetworkingPrivateLinux>();
#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  std::unique_ptr<wifi::WiFiService> wifi_service(wifi::WiFiService::Create());
  delegate =
      std::make_unique<NetworkingPrivateServiceClient>(std::move(wifi_service));
#else
  NOTREACHED_IN_MIGRATION();
  delegate = nullptr;
#endif

  if (ui_factory_) {
    delegate->set_ui_delegate(ui_factory_->CreateDelegate());
  }

  return delegate;
}

BrowserContext* NetworkingPrivateDelegateFactory::GetBrowserContextToUse(
    BrowserContext* context) const {
  return ExtensionsBrowserClient::Get()->GetContextRedirectedToOriginal(
      context, /*force_guest_profile=*/true);
}

}  // namespace extensions
