// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_TYPED_PROFILE_KEYED_SERVICE_FACTORY_IOS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_TYPED_PROFILE_KEYED_SERVICE_FACTORY_IOS_H_

#include <concepts>
#include <utility>

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/parameter_pack.h"
#include "base/types/pass_key.h"
#include "components/keyed_service/core/service_access_type.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_traits.h"

// LINT.IfChange(TypedProfileKeyedServiceFactoryIOS)
//
// Helper class that takes care of the boilerplate for writing the `Factory`
// owning and associating instances of `Service` to ProfileIOS instances.
//
// This class requires the `Service` to be fully declared (the header must be
// included, a forward-declaration is not enough).
//
// This class automates the definition of the methods to access the `Service`
// instance associated with a ProfileIOS, and the definition of the method to
// access the factory singleton. In general sub-classes only need to define
// the factory constructor (see ProfileKeyedServiceFactoryIOS) and the method
// `BuildServiceInstanceFor(ProfileIOS*)`.
//
// Usage example:
//
// class FooService : public KeyedService { ... };
// class FooServiceFactory final
//     : public TypedProfileKeyedServiceFactoryIOS<
//           FooServiceFactory, FooService> {
//  public:
//   FooServiceFactory(PassKey key)
//       : TypedProfileKeyedServiceFactoryIOS(
//           std::move(key), "FooService", ...) {}
//
//  private:
//   std::unique_ptr<KeyedService>
//   BuildServiceInstanceFor(ProfileIOS* profile) final {
//     return std::make_unique<FooService>(...);
//   }
// };
//
// Note that the constructor needs to be public and accept a single parameter
// of type `PassKey` (only TypedProfileKeyedServiceFactoryIOS<...> can create
// such a parameter). This constructor can only be called from GetInstance().
template <typename Factory, typename Service, typename... FactoryTraits>
  requires std::convertible_to<Service*, KeyedService*>
class TypedProfileKeyedServiceFactoryIOS
    : public ProfileKeyedServiceFactoryIOS {
 public:
  using PassKey = base::PassKey<
      TypedProfileKeyedServiceFactoryIOS<Factory, Service, FactoryTraits...>>;

  // Whether `UseServiceAccess` is in `FactoryTraits...`
  static constexpr bool GetForProfileUseServiceAccess =
      base::trait_helpers::HasTypeInVariadicPack<UseServiceAccess,
                                                 FactoryTraits...>;

  // Returns the instance of `Service` associated with `profile` attempting
  // to create one if no value is cached yet. May return null if the factory
  // decides to not create a service for the `profile` (e.g. during tests,
  // for incognito profile, ...).
  //
  // If `UseServiceAccess` is in `FactoryTraits...` then the method takes an
  // extra parameter `access_type`. If its value is IMPLICIT_ACCESS, then
  // the method will first check whether the profile is off-the-record, and
  // if true, return null unconditionally.
  static Service* GetForProfile(ProfileIOS* profile)
    requires(!GetForProfileUseServiceAccess)
  {
    CHECK(profile);
    return GetInstance()->template GetServiceForProfileAs<Service>(
        profile,
        /*create=*/true);
  }

  static Service* GetForProfile(ProfileIOS* profile,
                                ServiceAccessType access_type)
    requires(GetForProfileUseServiceAccess)
  {
    CHECK(profile);
    if (access_type == ServiceAccessType::IMPLICIT_ACCESS) {
      if (profile->IsOffTheRecord()) {
        return nullptr;
      }
    }

    return GetInstance()->template GetServiceForProfileAs<Service>(
        profile,
        /*create=*/true);
  }

  // Returns the instance of `Service` associated with `profile` or null
  // if no value is cached by the factory.
  static Service* GetForProfileIfExists(ProfileIOS* profile)
    requires(!GetForProfileUseServiceAccess)
  {
    CHECK(profile);
    return GetInstance()->template GetServiceForProfileAs<Service>(
        profile,
        /*create=*/false);
  }

  static Service* GetForProfileIfExists(ProfileIOS* profile,
                                        ServiceAccessType access_type)
    requires(GetForProfileUseServiceAccess)
  {
    CHECK(profile);
    if (access_type == ServiceAccessType::IMPLICIT_ACCESS) {
      if (profile->IsOffTheRecord()) {
        return nullptr;
      }
    }

    return GetInstance()->template GetServiceForProfileAs<Service>(
        profile,
        /*create=*/false);
  }

  // Returns the singleton instance of `Factory`.
  static Factory* GetInstance() {
    static base::NoDestructor<Factory> kInstance(PassKey{});
    return kInstance.get();
  }

 protected:
  template <typename... Traits>
  TypedProfileKeyedServiceFactoryIOS(PassKey pass_key,
                                     const char* name,
                                     Traits... traits)
      : ProfileKeyedServiceFactoryIOS(name, std::forward<Traits>(traits)...) {}
};
// LINT.ThenChange(//ios/chrome/browser/shared/model/profile/typed_refcounted_profile_keyed_service_factory_ios.h:TypedRefcountedProfileKeyedServiceFactoryIOS)

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_TYPED_PROFILE_KEYED_SERVICE_FACTORY_IOS_H_
