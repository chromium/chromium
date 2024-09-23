// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_REFCOUNTED_PROFILE_KEYED_SERVICE_FACTORY_IOS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PROFILE_REFCOUNTED_PROFILE_KEYED_SERVICE_FACTORY_IOS_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_refptr.h"
#include "base/traits_bag.h"
#include "components/keyed_service/ios/refcounted_browser_state_keyed_service_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_traits.h"

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
    : public RefcountedBrowserStateKeyedServiceFactory {
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

 protected:
  // Final implementation of BrowserStateKeyedServiceFactory:
  web::BrowserState* GetBrowserStateToUse(web::BrowserState* ctx) const final;
  bool ServiceIsCreatedWithBrowserState() const final;
  bool ServiceIsNULLWhileTesting() const final;

  // Helper that casts the value returned by GetKeyedServiceForProfile() to the
  // sub-class T of KeyedService.
  template <typename T>
    requires std::convertible_to<T*, RefcountedKeyedService*>
  scoped_refptr<T> GetServiceForProfileAs(ProfileIOS* profile, bool create) {
    return base::WrapRefCounted(
        static_cast<T*>(GetServiceForProfile(profile, create).get()));
  }

 private:
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
