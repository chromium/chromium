// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/profile/internal/profile_keyed_service_factory_ios.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/notreached.h"
#include "ios/chrome/browser/shared/model/profile/internal/profile_dependency_manager_ios.h"
#include "ios/chrome/browser/shared/model/profile/internal/profile_keyed_service_utils.h"
#include "ios/chrome/browser/shared/model/profile/internal/refcounted_profile_keyed_service_factory_ios.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

// LINT.IfChange(ProfileKeyedServiceFactoryIOS)

namespace {

// Converts a `testing_factory` into a KeyedServiceTestingFactoryIOS.
KeyedServiceTestingFactoryIOSByRef IntoByRef(
    KeyedServiceTestingFactoryIOSByPtr testing_factory) {
  if (!testing_factory) {
    return {};
  }

  return base::BindOnce(
      [](KeyedServiceTestingFactoryIOSByPtr testing_factory,
         ProfileIOS& profile) {
        return std::move(testing_factory).Run(&profile);
      },
      std::move(testing_factory));
}

// Wraps `testing_factory` as a KeyedServiceFactory::TestingFactory.
base::OnceCallback<std::unique_ptr<KeyedService>(void*)> WrapFactory(
    KeyedServiceTestingFactoryIOSByRef testing_factory) {
  if (!testing_factory) {
    return {};
  }

  return base::BindOnce(
      [](KeyedServiceTestingFactoryIOSByRef testing_factory,
         void* context) -> std::unique_ptr<KeyedService> {
        return std::move(testing_factory)
            .Run(CHECK_DEREF(static_cast<ProfileIOS*>(context)));
      },
      std::move(testing_factory));
}

}  // namespace

void ProfileKeyedServiceFactoryIOS::SetTestingFactory(
    PassKey pass_key,
    ProfileIOS& profile,
    TestingFactoryByPtr testing_factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  KeyedServiceFactory::SetTestingFactory(
      &profile, WrapFactory(IntoByRef(std::move(testing_factory))));
}

void ProfileKeyedServiceFactoryIOS::SetTestingFactory(
    PassKey pass_key,
    ProfileIOS& profile,
    TestingFactoryByRef testing_factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  KeyedServiceFactory::SetTestingFactory(
      &profile, WrapFactory(std::move(testing_factory)));
}

void ProfileKeyedServiceFactoryIOS::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Nothing to do.
}

KeyedService* ProfileKeyedServiceFactoryIOS::GetServiceForProfile(
    ProfileIOS& profile,
    bool create) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetServiceForContext(&profile, create);
}

void ProfileKeyedServiceFactoryIOS::DependsOn(
    ProfileKeyedServiceFactoryIOS* other) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  KeyedServiceFactory::DependsOn(other);
}

void ProfileKeyedServiceFactoryIOS::DependsOn(
    RefcountedProfileKeyedServiceFactoryIOS* other) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  KeyedServiceFactory::DependsOn(other);
}

void* ProfileKeyedServiceFactoryIOS::GetContextToUse(void* context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AssertContextWasntDestroyed(context);
  return GetContextToUseForKeyedServiceFactory(
      static_cast<ProfileIOS*>(context), profile_selection_);
}

bool ProfileKeyedServiceFactoryIOS::ServiceIsCreatedWithContext() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return service_creation_ == ServiceCreation::kCreateWithProfile;
}

bool ProfileKeyedServiceFactoryIOS::ServiceIsNULLWhileTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return testing_creation_ == TestingCreation::kNoServiceForTests;
}

void ProfileKeyedServiceFactoryIOS::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RegisterProfilePrefs(registry);
}

std::unique_ptr<KeyedService>
ProfileKeyedServiceFactoryIOS::BuildServiceInstanceFor(void* context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return BuildServiceInstanceFor(
      CHECK_DEREF(static_cast<ProfileIOS*>(context)));
}

// TODO(crbug.com/576182342): Remove those two implementation once all
// sub-classes have been converted to override the overload accepting
// a ProfileIOS& instead of a ProfileIOS*.
std::unique_ptr<KeyedService>
ProfileKeyedServiceFactoryIOS::BuildServiceInstanceFor(
    ProfileIOS& profile) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return BuildServiceInstanceFor(&profile);
}

std::unique_ptr<KeyedService>
ProfileKeyedServiceFactoryIOS::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  NOTREACHED();
}

ProfileKeyedServiceFactoryIOS::ProfileKeyedServiceFactoryIOS(
    const char* name,
    ProfileSelection profile_selection,
    ServiceCreation service_creation,
    TestingCreation testing_creation,
    base::trait_helpers::NotATraitTag)
    : KeyedServiceFactory(name,
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

// LINT.ThenChange(//ios/chrome/browser/shared/model/profile/internal/refcounted_profile_keyed_service_factory_ios.cc:RefcountedProfileKeyedServiceFactoryIOS)
