// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_REFCOUNTED_PROFILE_KEYED_SERVICE_FACTORY_IOS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_REFCOUNTED_PROFILE_KEYED_SERVICE_FACTORY_IOS_H_

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/traits_bag.h"
#include "base/types/pass_key.h"
#include "components/keyed_service/core/refcounted_keyed_service_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_traits.h"

class ProfileIOS;
class ProfileKeyedServiceFactoryIOS;
class TestProfileIOS;

// RefcountedProfileKeyedServiceFactoryIOS provides a ProfileIOS-specific
// interface forKeyedServiceFactory under //ios/chrome/browser.
//
// It automates the registration with the DependencyManager, simplifies the
// selection of the correct ProfileIOS to use when passed an off-the-record
// ProfileIOS, simplifies the declaration that the service instance should
// not be created during testing or that the service should be created with
// the ProfileIOS.
//
// - Example of a factory that use the regular ProfileIOS in incognito:
//
// class ExampleRefcountedKeyedServiceFactory
//     : public RefcountedProfileKeyedServiceFactoryIOS {
//  private:
//   ExampleRefcountedKeyedServiceFactory()
//       : RefcountedProfileKeyedServiceFactoryIOS(
//             "ExampleRefcountedKeyedService",
//             ProfileSelection::kRedirectedInIncognito) {}
// };
//
// - Example of a factory creating the service with profile unless during tests.
//
// class ExampleRefcountedKeyedServiceFactory
//     : public RefcountedProfileKeyedServiceFactoryIOS {
//  private:
//   ExampleRefcountedKeyedServiceFactory()
//       : RefcountedProfileKeyedServiceFactoryIOS(
//             "ExampleRefcountedKeyedService",
//             ServiceCreation::kCreateWithProfile,
//             TestingCreation::kNoServiceForTests) {}
// };
//
// Any change to this class should also be reflected on
// ProfileKeyedServiceFactoryIOS.
class RefcountedProfileKeyedServiceFactoryIOS
    : public RefcountedKeyedServiceFactory {
 public:
  // List of traits that are valid for the constructor.
  struct ValidTraits {
    ValidTraits(ProfileSelection);
    ValidTraits(ServiceCreation);
    ValidTraits(TestingCreation);
  };

  // For SetTestingFactory(...).
  using PassKey = base::PassKey<TestProfileIOS>;

  // A callback that returns the instance of a KeyedService for a given
  // ProfileIOS instance. This is used for testing where the test wants
  // to create a specific test double for a service.
  using TestingFactory =
      base::OnceCallback<scoped_refptr<RefcountedKeyedService>(
          ProfileIOS* profile)>;

  // Constructor accepts zero or more traits.
  template <typename... Traits>
    requires base::trait_helpers::AreValidTraits<ValidTraits, Traits...>
  NOINLINE RefcountedProfileKeyedServiceFactoryIOS(const char* name,
                                                   Traits... traits)
      : RefcountedProfileKeyedServiceFactoryIOS(
            name,
            base::trait_helpers::GetEnum<ProfileSelection,
                                         ProfileSelection::kDefault>(traits...),
            base::trait_helpers::GetEnum<ServiceCreation,
                                         ServiceCreation::kDefault>(traits...),
            base::trait_helpers::GetEnum<TestingCreation,
                                         TestingCreation::kDefault>(traits...),
            base::trait_helpers::NotATraitTag()) {}

  // Associates `testing_factory` with `profile` so that `testing_factory` is
  // used to create the KeyedService when requested.  `testing_factory` can be
  // empty to signal that KeyedService should be null. Multiple calls to
  // SetTestingFactory() are allowed; previous services will be shut down.
  void SetTestingFactory(PassKey pass_key,
                         ProfileIOS* profile,
                         TestingFactory testing_factory);

 protected:
  // Helper that casts the value returned by GetKeyedServiceForProfile() to the
  // sub-class T of KeyedService.
  template <typename T>
    requires std::convertible_to<T*, RefcountedKeyedService*>
  scoped_refptr<T> GetServiceForProfileAs(ProfileIOS* profile, bool create) {
    return base::WrapRefCounted(
        static_cast<T*>(GetServiceForProfile(profile, create).get()));
  }

  // Registers any user preferences on this service. This should be overridden
  // by any service that wants to register profile-specific preferences.
  virtual void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Creates a new instance of the service for `profile`.
  virtual scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const = 0;

  // The main public interface for declaring dependencies between services
  // created by factories.
  void DependsOn(ProfileKeyedServiceFactoryIOS* other);
  void DependsOn(RefcountedProfileKeyedServiceFactoryIOS* other);

 private:
  // RefcountedKeyedServiceFactory implementation:
  void* GetContextToUse(void* context) const final;
  bool ServiceIsCreatedWithContext() const final;
  bool ServiceIsNULLWhileTesting() const final;
  void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry) final;
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      void* context) const final;

  // Common implementation that maps `profile` to some service object. Deals
  // with incognito and testing profiles according to constructor traits. If
  // `create` is true, the service will be create using
  // BuildServiceInstanceFor() if it doesn't already exists.
  scoped_refptr<RefcountedKeyedService> GetServiceForProfile(
      ProfileIOS* profile,
      bool create);

  // The template constructor has to be in the header but it delegates to this
  // constructor to initialize all other members out-of-line
  RefcountedProfileKeyedServiceFactoryIOS(const char* name,
                                          ProfileSelection profile_selection,
                                          ServiceCreation service_creation,
                                          TestingCreation testing_creation,
                                          base::trait_helpers::NotATraitTag);

  // Policies for this factory.
  const ProfileSelection profile_selection_;
  const ServiceCreation service_creation_;
  const TestingCreation testing_creation_;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_REFCOUNTED_PROFILE_KEYED_SERVICE_FACTORY_IOS_H_
