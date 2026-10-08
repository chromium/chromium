// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_TYPED_REFCOUNTED_PROFILE_KEYED_SERVICE_FACTORY_IOS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_TYPED_REFCOUNTED_PROFILE_KEYED_SERVICE_FACTORY_IOS_H_

#include <concepts>
#include <utility>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/traits_bag.h"
#include "base/types/pass_key.h"
#include "components/keyed_service/core/service_access_type.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_traits.h"
#include "ios/chrome/browser/shared/model/profile/refcounted_profile_keyed_service_factory_ios.h"

// LINT.IfChange(TypedRefcountedProfileKeyedServiceFactoryIOS)
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
// the factory constructor (see RefcountedProfileKeyedServiceFactoryIOS) and
// the method `BuildServiceInstanceFor(ProfileIOS*)`.
//
// Usage example:
//
// class RefcountedFooService : public RefcountedKeyedService { ... };
// class RefcountedFooServiceFactory final
//     : public RefcountedTypedProfileKeyedServiceFactoryIOS<
//           RefcountedFooServiceFactory, RefcountedFooService> {
//  public:
//   RefcountedFooServiceFactory(PassKey key);
//       : RefcountedTypedProfileKeyedServiceFactoryIOS(
//           std::move(key), "RefcountedFooService", ...) {}
//
//  private:
//   scoped_refptr<RefcountKeyedService>
//   BuildServiceInstanceFor(ProfileIOS* profile) final {
//     return base::MakeRefCounted<RefcountedFooService>(...);
//   }
// };
//
// Note that the constructor needs to be public and accept a single parameter
// of type `PassKey` (only TypedRefcountedProfileKeyedServiceFactoryIOS<...>
// can create such a parameter). This constructor can only be called from
// GetInstance().
template <typename Factory, typename Service, typename... FactoryTraits>
  requires std::convertible_to<Service*, RefcountedKeyedService*>
class TypedRefcountedProfileKeyedServiceFactoryIOS
    : public RefcountedProfileKeyedServiceFactoryIOS {
 public:
  using PassKey = base::PassKey<
      TypedRefcountedProfileKeyedServiceFactoryIOS<Factory,
                                                   Service,
                                                   FactoryTraits...>>;

  // Whether `UseServiceAccess` is in `AccessTraits...`
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
  static scoped_refptr<Service> GetForProfile(ProfileIOS* profile)
    requires(!GetForProfileUseServiceAccess)
  {
    CHECK(profile);
    return GetInstance()->template GetServiceForProfileAs<Service>(
        profile,
        /*create=*/true);
  }

  static scoped_refptr<Service> GetForProfile(ProfileIOS* profile,
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
  static scoped_refptr<Service> GetForProfileIfExists(ProfileIOS* profile)
    requires(!GetForProfileUseServiceAccess)
  {
    CHECK(profile);
    return GetInstance()->template GetServiceForProfileAs<Service>(
        profile,
        /*create=*/false);
  }

  static scoped_refptr<Service> GetForProfileIfExists(
      ProfileIOS* profile,
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
  TypedRefcountedProfileKeyedServiceFactoryIOS(PassKey pass_key,
                                               const char* name,
                                               Traits... traits)
      : RefcountedProfileKeyedServiceFactoryIOS(
            name,
            std::forward<Traits>(traits)...) {}
};
// LINT.ThenChange(//ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h:TypedProfileKeyedServiceFactoryIOS)

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_TYPED_REFCOUNTED_PROFILE_KEYED_SERVICE_FACTORY_IOS_H_
