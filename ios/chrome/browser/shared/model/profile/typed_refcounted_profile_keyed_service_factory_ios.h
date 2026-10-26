// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_TYPED_REFCOUNTED_PROFILE_KEYED_SERVICE_FACTORY_IOS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_TYPED_REFCOUNTED_PROFILE_KEYED_SERVICE_FACTORY_IOS_H_

#include <concepts>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/traits_bag.h"
#include "base/types/pass_key.h"
#include "components/keyed_service/core/service_access_type.h"
#include "ios/chrome/browser/shared/model/profile/internal/refcounted_profile_keyed_service_factory_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_access.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_traits.h"

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
template <typename Factory, typename Service, typename... AccessTraits>
  requires std::convertible_to<Service*, RefcountedKeyedService*>
class TypedRefcountedProfileKeyedServiceFactoryIOS
    : public RefcountedProfileKeyedServiceFactoryIOS {
 public:
  using PassKey = base::PassKey<
      TypedRefcountedProfileKeyedServiceFactoryIOS<Factory,
                                                   Service,
                                                   AccessTraits...>>;

  // Returns the instance of `Service` associated with `profile` attempting
  // to create one if no value is cached yet. May return null if the factory
  // decides to not create a service for the `profile` (e.g. during tests,
  // for incognito profile, ...).
  //
  // If `UseServiceAccess` is in `FactoryTraits...` then the method takes an
  // extra parameter `access_type`. If its value is IMPLICIT_ACCESS, then
  // the method will first check whether the profile is off-the-record, and
  // if true, return null unconditionally.
  //
  // It is recommended to cache the value returned by this method (in fact the
  // KeyedService infrastructure will cache the mapping between `profile` and
  // the value returned by this method). If there is no cached value, it will
  // created by calling BuildServiceInstanceFor(...) and the result cached for
  // the whole lifetime of the ProfileIOS object.
  //
  // Clients are allowed to omit calling the method and instead store the value
  // returned (as long as they stop using it is destroyed, i.e. when ProfileIOS
  // is destroyed).
  //
  // This means that KeyedService factories must not check transient conditions,
  // that depend on a particular state of the ProfileIOS or of attached services
  // (e.g. whether the profile is managed, or sync is enabled) when creating the
  // service in BuildInstanceFor(...). Instead should return a service instance,
  // and this service should adapt its behavior to match the condition (i.e. if
  // the service should only work when the profile is managed, then it can fail
  // all request if the profile is currently not managed).
  static scoped_refptr<Service> GetForProfile(ProfileIOS* profile)
    requires(sizeof...(AccessTraits) == 0)
  {
    if (!profile) {
      return nullptr;
    }

    return GetForProfile(*profile);
  }

  static scoped_refptr<Service> GetForProfile(ProfileIOS& profile)
    requires(sizeof...(AccessTraits) == 0)
  {
    return GetInstance()->template GetServiceForProfileAs<Service>(
        profile,
        /*create=*/true);
  }

  // If AccessTraits is non-empty, then GetForProfile(ProfileIOS*) is replaced
  // by this variant that accept a ServiceAccessType in addition to the profile.
  //
  // If called with EXPLICIT_ACCESS, then the function unconditionally access
  // the service from the cache, creating it if missing. If IMPLICIT_ACCESS,
  // the AccessTrait classes are used to confirm whether the service can be
  // accessed for this profile (e.g. check that the profile is not incognito).
  //
  // As the AccessTrait classes may test "transient" state of the profile, the
  // value returned must not be cached if using IMPLICIT_ACCESS as access_type.
  static scoped_refptr<Service> GetForProfile(ProfileIOS* profile,
                                              ServiceAccessType access_type)
    requires(sizeof...(AccessTraits) != 0)
  {
    if (!profile) {
      return nullptr;
    }

    return GetForProfile(*profile, access_type);
  }

  static scoped_refptr<Service> GetForProfile(ProfileIOS& profile,
                                              ServiceAccessType access_type)
    requires(sizeof...(AccessTraits) != 0)
  {
    if (access_type == ServiceAccessType::IMPLICIT_ACCESS) {
      if (ServiceAccessChecker<AccessTraits...>::Reject(profile)) {
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
    requires(sizeof...(AccessTraits) == 0)
  {
    if (!profile) {
      return nullptr;
    }

    return GetForProfileIfExists(*profile);
  }

  static scoped_refptr<Service> GetForProfileIfExists(ProfileIOS& profile)
    requires(sizeof...(AccessTraits) == 0)
  {
    return GetInstance()->template GetServiceForProfileAs<Service>(
        profile,
        /*create=*/false);
  }

  // If AccessTraits is non-empty, then GetForProfileIfExists(ProfileIOS*) is
  // replaced by this variant that accept a ServiceAccessType in addition to
  // the profile.
  //
  // If called with EXPLICIT_ACCESS, then the function unconditionally access
  // the service from the cache, creating it if missing. If IMPLICIT_ACCESS,
  // the AccessTrait classes are used to confirm whether the service can be
  // accessed for this profile (e.g. check that the profile is not incognito).
  static scoped_refptr<Service> GetForProfileIfExists(
      ProfileIOS* profile,
      ServiceAccessType access_type)
    requires(sizeof...(AccessTraits) != 0)
  {
    if (!profile) {
      return nullptr;
    }

    return GetForProfileIfExists(*profile, access_type);
  }

  static scoped_refptr<Service> GetForProfileIfExists(
      ProfileIOS& profile,
      ServiceAccessType access_type)
    requires(sizeof...(AccessTraits) != 0)
  {
    if (access_type == ServiceAccessType::IMPLICIT_ACCESS) {
      if (ServiceAccessChecker<AccessTraits...>::Reject(profile)) {
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
