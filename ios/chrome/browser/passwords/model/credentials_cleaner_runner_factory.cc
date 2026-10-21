// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/model/credentials_cleaner_runner_factory.h"

#include <utility>

#include "components/password_manager/core/browser/credentials_cleaner_runner.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

CredentialsCleanerRunnerFactory::CredentialsCleanerRunnerFactory(PassKey key)
    : TypedProfileKeyedServiceFactoryIOS(std::move(key),
                                         "CredentialsCleanerRunner") {}

std::unique_ptr<KeyedService>
CredentialsCleanerRunnerFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<password_manager::CredentialsCleanerRunner>();
}
