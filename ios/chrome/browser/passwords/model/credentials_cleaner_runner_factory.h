// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_CREDENTIALS_CLEANER_RUNNER_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_CREDENTIALS_CLEANER_RUNNER_FACTORY_H_

#include "components/password_manager/core/browser/credentials_cleaner_runner.h"
#include "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Creates instances of CredentialsCleanerRunner per Profile.
class CredentialsCleanerRunnerFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          CredentialsCleanerRunnerFactory,
          password_manager::CredentialsCleanerRunner> {
 public:
  explicit CredentialsCleanerRunnerFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_CREDENTIALS_CLEANER_RUNNER_FACTORY_H_
