// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_KEYED_SERVICE_FACTORY_IOS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_PROFILE_KEYED_SERVICE_FACTORY_IOS_H_

#include "base/compiler_specific.h"
#include "base/traits_bag.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_traits.h"

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
class ProfileKeyedServiceFactoryIOS : public BrowserStateKeyedServiceFactory {
 public:
  // List of traits that are valid for the constructor.
  struct ValidTraits {
    ValidTraits(ProfileSelection);
    ValidTraits(ServiceCreation);
    ValidTraits(TestingCreation);
  };

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

 protected:
  // Final implementation of BrowserStateKeyedServiceFactory:
  web::BrowserState* GetBrowserStateToUse(web::BrowserState* ctx) const final;
  bool ServiceIsCreatedWithBrowserState() const final;
  bool ServiceIsNULLWhileTesting() const final;

  // Helper that casts the value returned by GetKeyedServiceForProfile() to the
  // sub-class T of KeyedService.
  template <typename T>
    requires std::convertible_to<T*, KeyedService*>
  T* GetServiceForProfileAs(ProfileIOS* profile, bool create) {
    return static_cast<T*>(GetServiceForProfile(profile, create));
  }

 private:
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
