// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/profile/refcounted_profile_keyed_service_factory_ios.h"

#include "base/check.h"
#include "ios/chrome/browser/shared/model/profile/profile_dependency_manager_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_utils.h"

namespace {

// Wraps `testing_factory` as a KeyedServiceFactory::TestingFactory.
base::OnceCallback<scoped_refptr<RefcountedKeyedService>(void*)> WrapFactory(
    RefcountedProfileKeyedServiceFactoryIOS::TestingFactory testing_factory) {
  if (!testing_factory) {
    return {};
  }

  return base::BindOnce(
      [](RefcountedProfileKeyedServiceFactoryIOS::TestingFactory
             testing_factory,
         void* context) -> scoped_refptr<RefcountedKeyedService> {
        return std::move(testing_factory)
            .Run(static_cast<ProfileIOS*>(context));
      },
      std::move(testing_factory));
}

}  // namespace

void RefcountedProfileKeyedServiceFactoryIOS::SetTestingFactory(
    PassKey pass_key,
    ProfileIOS* profile,
    TestingFactory testing_factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RefcountedKeyedServiceFactory::SetTestingFactory(
      profile, WrapFactory(std::move(testing_factory)));
}

void RefcountedProfileKeyedServiceFactoryIOS::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Nothing to do.
}

scoped_refptr<RefcountedKeyedService>
RefcountedProfileKeyedServiceFactoryIOS::GetServiceForProfile(
    ProfileIOS* profile,
    bool create) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetServiceForContext(profile, create);
}

void RefcountedProfileKeyedServiceFactoryIOS::DependsOn(
    ProfileKeyedServiceFactoryIOS* other) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RefcountedKeyedServiceFactory::DependsOn(other);
}

void RefcountedProfileKeyedServiceFactoryIOS::DependsOn(
    RefcountedProfileKeyedServiceFactoryIOS* other) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RefcountedKeyedServiceFactory::DependsOn(other);
}

void* RefcountedProfileKeyedServiceFactoryIOS::GetContextToUse(
    void* context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AssertContextWasntDestroyed(context);
  return GetContextToUseForKeyedServiceFactory(
      static_cast<ProfileIOS*>(context), profile_selection_);
}

bool RefcountedProfileKeyedServiceFactoryIOS::ServiceIsCreatedWithContext()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return service_creation_ == ServiceCreation::kCreateWithProfile;
}

bool RefcountedProfileKeyedServiceFactoryIOS::ServiceIsNULLWhileTesting()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return testing_creation_ == TestingCreation::kNoServiceForTests;
}

void RefcountedProfileKeyedServiceFactoryIOS::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RegisterProfilePrefs(registry);
}

scoped_refptr<RefcountedKeyedService>
RefcountedProfileKeyedServiceFactoryIOS::BuildServiceInstanceFor(
    void* context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return BuildServiceInstanceFor(static_cast<ProfileIOS*>(context));
}

RefcountedProfileKeyedServiceFactoryIOS::
    RefcountedProfileKeyedServiceFactoryIOS(const char* name,
                                            ProfileSelection profile_selection,
                                            ServiceCreation service_creation,
                                            TestingCreation testing_creation,
                                            base::trait_helpers::NotATraitTag)
    : RefcountedKeyedServiceFactory(name,
                                    ProfileDependencyManagerIOS::GetInstance(),
                                    BROWSER_STATE),
      profile_selection_(profile_selection),
      service_creation_(service_creation),
      testing_creation_(testing_creation) {
  // If the KeyedService are created automatically with the profile, then it
  // should not be created during unit tests otherwise it is not possible to
  // use a TestProfileIOS without this service (and then the tests cannot be
  // independent from the service).
  //
  // If this assertion fails, you need to either remove kCreateWithProfile
  // (it may not be needed for the service), or add kNoServiceForTests and
  // inject a test factory in tests that need the service to exist.
  CHECK(service_creation != ServiceCreation::kCreateWithProfile ||
        testing_creation_ == TestingCreation::kNoServiceForTests);
}
