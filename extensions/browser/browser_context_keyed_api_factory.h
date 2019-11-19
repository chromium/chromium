// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_BROWSER_CONTEXT_KEYED_API_FACTORY_H_
#define EXTENSIONS_BROWSER_BROWSER_CONTEXT_KEYED_API_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

template <typename T>
class BrowserContextKeyedAPIFactory;

// Instantiations of BrowserContextKeyedAPIFactory should use this base class
// and also define a static const char* service_name() function (used in the
// BrowserContextKeyedServiceFactory constructor). These fields should
// be accessible to the BrowserContextKeyedAPIFactory for the service.
class BrowserContextKeyedAPI : public KeyedService {
 protected:
  // Defaults for flags that control BrowserContextKeyedAPIFactory behavior.
  // These can be overridden by subclasses to change that behavior.
  // See BrowserContextKeyedServiceFactory for usage.

  // These flags affect what instance is returned when Get() is called
  // on an incognito profile. By default, it returns NULL. If
  // kServiceRedirectedInIncognito is true, it returns the instance for the
  // corresponding regular profile. If kServiceHasOwnInstanceInIncognito
  // is true, it returns a separate instance.
  static const bool kServiceRedirectedInIncognito = false;
  static const bool kServiceHasOwnInstanceInIncognito = false;

  // If set to false, don't start the service at BrowserContext creation time.
  // (The default differs from the BrowserContextKeyedServiceFactory default,
  // because historically, BrowserContextKeyedAPIs often do tasks at startup.)
  static const bool kServiceIsCreatedWithBrowserContext = true;

  // If set to true, GetForProfile returns NULL for TestingBrowserContexts.
  static const bool kServiceIsNULLWhileTesting = false;

  // Users of this factory template must define a GetFactoryInstance()
  // and manage their own instances (using LazyInstance), because those cannot
  // be included in more than one translation unit (and thus cannot be
  // initialized in a header file).
  //
  // In the header file, declare GetFactoryInstance(), e.g.:
  //   class HistoryAPI {
  //   ...
  //    public:
  //     static BrowserContextKeyedAPIFactory<HistoryAPI>* GetFactoryInstance();
  //   };
  //
  // In the cc file, provide the implementation, e.g.:
  //   static base::LazyInstance<BrowserContextKeyedAPIFactory<HistoryAPI>>::
  //      DestructorAtExit g_factory = LAZY_INSTANCE_INITIALIZER;
  //
  //   // static
  //   BrowserContextKeyedAPIFactory<HistoryAPI>*
  //   HistoryAPI::GetFactoryInstance() {
  //     return g_factory.Pointer();
  //   }
};

// Declare dependencies on other factories.
// By default, ExtensionSystemFactory is the only dependency; however,
// specializations can override this. Declare your specialization in
// your header file after the BrowserContextKeyedAPI class definition.
// Declare this struct in the header file. The implementation may optionally
// be placed in your .cc file.
// This method should be used instead of
// BrowserContextKeyedAPIFactory<T>::DeclareFactoryDependencies() because it
// permits partial specialization, as in the case of ApiResourceManager<T>.
//
//   template <>
//   struct BrowserContextFactoryDependencies<MyService> {
//     static void DeclareFactoryDependencies(
//         BrowserContextKeyedAPIFactory<ApiResourceManager<T>>* factory) {
//       factory->DependsOn(
//           ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
//       factory->DependsOn(ProfileSyncServiceFactory::GetInstance());
//       ...
//     }
//   };
template <typename T>
struct BrowserContextFactoryDependencies {
  static void DeclareFactoryDependencies(
      BrowserContextKeyedAPIFactory<T>* factory) {
    factory->DependsOn(
        ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  }
};

// A template for factories for KeyedServices that manage extension APIs. T is
// a KeyedService that uses this factory template instead of its own separate
// factory definition to manage its per-profile instances.
template <typename T>
class BrowserContextKeyedAPIFactory : public BrowserContextKeyedServiceFactory {
 public:
  static T* Get(content::BrowserContext* context) {
    return static_cast<T*>(
        T::GetFactoryInstance()->GetServiceForBrowserContext(context, true));
  }

  static T* GetIfExists(content::BrowserContext* context) {
    return static_cast<T*>(
        T::GetFactoryInstance()->GetServiceForBrowserContext(context, false));
  }

  // Declares dependencies on other factories.
  // Deprecated. Use BrowserContextFactoryDependencies<> to declare
  // dependencies instead, as that form allows for partial specializations like
  // in the case of ApiResourceManager<T>.
  void DeclareFactoryDependencies() {
    BrowserContextFactoryDependencies<T>::DeclareFactoryDependencies(this);
  }

  BrowserContextKeyedAPIFactory()
      : BrowserContextKeyedServiceFactory(
            T::service_name(),
            BrowserContextDependencyManager::GetInstance()) {
    DeclareFactoryDependencies();
  }

  ~BrowserContextKeyedAPIFactory() override {}

 private:
  friend struct BrowserContextFactoryDependencies<T>;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    return new T(context);
  }

  // BrowserContextKeyedServiceFactory implementation.
  // These can be effectively overridden with template specializations.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    if (T::kServiceRedirectedInIncognito)
      return ExtensionsBrowserClient::Get()->GetOriginalContext(context);

    if (T::kServiceHasOwnInstanceInIncognito)
      return context;

    return BrowserContextKeyedServiceFactory::GetBrowserContextToUse(context);
  }

  bool ServiceIsCreatedWithBrowserContext() const override {
    return T::kServiceIsCreatedWithBrowserContext;
  }

  bool ServiceIsNULLWhileTesting() const override {
    return T::kServiceIsNULLWhileTesting;
  }

  DISALLOW_COPY_AND_ASSIGN(BrowserContextKeyedAPIFactory);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_BROWSER_CONTEXT_KEYED_API_FACTORY_H_
