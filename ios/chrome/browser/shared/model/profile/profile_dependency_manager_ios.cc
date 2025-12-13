// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/profile/profile_dependency_manager_ios.h"

#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"

// static
ProfileDependencyManagerIOS* ProfileDependencyManagerIOS::GetInstance() {
  static base::NoDestructor<ProfileDependencyManagerIOS> kInstance(PassKey{});
  return kInstance.get();
}

ProfileDependencyManagerIOS::ProfileDependencyManagerIOS(PassKey pass_key) {}

ProfileDependencyManagerIOS::~ProfileDependencyManagerIOS() = default;

void ProfileDependencyManagerIOS::RegisterProfilePrefsForServices(
    user_prefs::PrefRegistrySyncable* registry) {
  RegisterPrefsForServices(registry);
}

void ProfileDependencyManagerIOS::CreateProfileServices(ProfileIOS* profile) {
  DoCreateProfileServices(profile, /*is_testing_profile=*/false);
}

void ProfileDependencyManagerIOS::CreateProfileServicesForTest(
    ProfileIOS* profile) {
  DoCreateProfileServices(profile, /*is_testing_profile=*/true);
}

void ProfileDependencyManagerIOS::DestroyProfileServices(ProfileIOS* profile) {
  DestroyContextServices(profile);
}

void ProfileDependencyManagerIOS::MarkProfileLive(ProfileIOS* profile) {
  MarkContextLive(profile);
}

void ProfileDependencyManagerIOS::DoCreateProfileServices(
    ProfileIOS* profile,
    bool is_testing_profile) {
  TRACE_EVENT0("browser",
               "ProfileDependencyManagerIOS::DoCreateProfileServices");
  CreateContextServices(profile, is_testing_profile);
}
