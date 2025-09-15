// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_KEYED_SERVICE_FACTORY_IOS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_KEYED_SERVICE_FACTORY_IOS_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/traits_bag.h"
#include "base/types/pass_key.h"
#include "components/keyed_service/core/keyed_service_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_traits.h"

class ProfileIOS;
class RefcountedProfileKeyedServiceFactoryIOS;
class TestProfileIOS;

// ProfileKeyedServiceFactoryIOS provides a ProfileIOS-specific interface for
// KeyedServiceFactory under //ios/chrome/browser.
//
// It automates the registration with the DependencyManager, simplifies the
// selection of the correct ProfileIOS to use when passed an off-the-record
// ProfileIOS, simplifies the declaration that the service instance should
// not be created during testing or that the service should be created with
// the ProfileIOS.
//
// - Example of a factory that use the regular ProfileIOS in incognito:
//
// class ExampleKeyedServiceFactory : public ProfileKeyedServiceFactoryIOS {
//  private:
//   ExampleKeyedServiceFactory()
//       : ProfileKeyedServiceFactoryIOS(
//             "ExampleKeyedService",
//             ProfileSelection::kRedirectedInIncognito) {}
// };
//
// - Example of a factory creating the service with profile unless during tests.
//
// class ExampleKeyedServiceFactory : public ProfileKeyedServiceFactoryIOS {
//  private:
//   ExampleKeyedServiceFactory()
//       : ProfileKeyedServiceFactoryIOS(
//             "ExampleKeyedService",
//             ServiceCreation::kCreateWithProfile,
//             TestingCreation::kNoServiceForTests) {}
// };
//
// Any change to this class should also be reflected on
// RefcountedProfileKeyedServiceFactoryIOS.
class ProfileKeyedServiceFactoryIOS : public KeyedServiceFactory {
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
      base::OnceCallback<std::unique_ptr<KeyedService>(ProfileIOS* profile)>;

  // Constructor accepts zero or more traits.
  template <typename... Traits>
    requires base::trait_helpers::AreValidTraits<ValidTraits, Traits...>
  NOINLINE ProfileKeyedServiceFactoryIOS(const char* name, Traits... traits)
      : ProfileKeyedServiceFactoryIOS(
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
    requires std::convertible_to<T*, KeyedService*>
  T* GetServiceForProfileAs(ProfileIOS* profile, bool create) {
    return static_cast<T*>(GetServiceForProfile(profile, create));
  }

  // Registers any user preferences on this service. This should be overridden
  // by any service that wants to register profile-specific preferences.
  virtual void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Creates a new instance of the service for `profile`.
  virtual std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const = 0;

  // The main public interface for declaring dependencies between services
  // created by factories.
  void DependsOn(ProfileKeyedServiceFactoryIOS* other);
  void DependsOn(RefcountedProfileKeyedServiceFactoryIOS* other);

 private:
  // KeyedServiceFactory implementation:
  void* GetContextToUse(void* context) const final;
  bool ServiceIsCreatedWithContext() const final;
  bool ServiceIsNULLWhileTesting() const final;
  void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry) final;
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      void* context) const final;

  // Common implementation that maps `profile` to some service object. Deals
  // with incognito and testing profiles according to constructor traits. If
  // `create` is true, the service will be create using
  // BuildServiceInstanceFor() if it doesn't already exists.
  KeyedService* GetServiceForProfile(ProfileIOS* profile, bool create);

  // The template constructor has to be in the header but it delegates to this
  // constructor to initialize all other members out-of-line
  ProfileKeyedServiceFactoryIOS(const char* name,
                                ProfileSelection profile_selection,
                                ServiceCreation service_creation,
                                TestingCreation testing_creation,
                                base::trait_helpers::NotATraitTag);

  // Policies for this factory.
  const ProfileSelection profile_selection_;
  const ServiceCreation service_creation_;
  const TestingCreation testing_creation_;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_KEYED_SERVICE_FACTORY_IOS_H_
