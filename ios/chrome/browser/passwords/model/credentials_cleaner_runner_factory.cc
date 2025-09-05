// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/model/credentials_cleaner_runner_factory.h"

#include "base/no_destructor.h"
#include "components/password_manager/core/browser/credentials_cleaner_runner.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

CredentialsCleanerRunnerFactory::CredentialsCleanerRunnerFactory()
    : ProfileKeyedServiceFactoryIOS("CredentialsCleanerRunner") {}

CredentialsCleanerRunnerFactory::~CredentialsCleanerRunnerFactory() = default;

CredentialsCleanerRunnerFactory*
CredentialsCleanerRunnerFactory::GetInstance() {
  static base::NoDestructor<CredentialsCleanerRunnerFactory> instance;
  return instance.get();
}

password_manager::CredentialsCleanerRunner*
CredentialsCleanerRunnerFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<password_manager::CredentialsCleanerRunner>(
          profile, /*create=*/true);
}

std::unique_ptr<KeyedService>
CredentialsCleanerRunnerFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<password_manager::CredentialsCleanerRunner>();
}
