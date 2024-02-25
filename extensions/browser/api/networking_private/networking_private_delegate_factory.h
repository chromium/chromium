// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_DELEGATE_FACTORY_H_
#define EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_DELEGATE_FACTORY_H_

#include <memory>

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "extensions/browser/api/networking_private/networking_private_delegate.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

// Factory for creating NetworkingPrivateDelegate instances as a keyed service.
// NetworkingPrivateDelegate supports the networkingPrivate API.
class NetworkingPrivateDelegateFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // There needs to be a way to allow the application (e.g. Chrome) to provide
  // additional delegates to the API (in src/extensions). Since this factory is
  // already a singleton, it provides a good place to hold these delegate
  // factories. See NetworkingPrivateDelegate for the delegate declarations.

  class UIDelegateFactory {
   public:
    UIDelegateFactory();

    UIDelegateFactory(const UIDelegateFactory&) = delete;
    UIDelegateFactory& operator=(const UIDelegateFactory&) = delete;

    virtual ~UIDelegateFactory();

    virtual std::unique_ptr<NetworkingPrivateDelegate::UIDelegate>
    CreateDelegate() = 0;
  };

  NetworkingPrivateDelegateFactory(const NetworkingPrivateDelegateFactory&) =
      delete;
  NetworkingPrivateDelegateFactory& operator=(
      const NetworkingPrivateDelegateFactory&) = delete;

  // Provide optional factories for creating delegate instances.
  void SetUIDelegateFactory(std::unique_ptr<UIDelegateFactory> factory);

  static NetworkingPrivateDelegate* GetForBrowserContext(
      content::BrowserContext* browser_context);
  static NetworkingPrivateDelegateFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<NetworkingPrivateDelegateFactory>;

  NetworkingPrivateDelegateFactory();
  ~NetworkingPrivateDelegateFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* browser_context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  std::unique_ptr<UIDelegateFactory> ui_factory_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_DELEGATE_FACTORY_H_
