// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/model/ios_password_requirements_service_factory.h"

#include "base/no_destructor.h"
#include "components/password_manager/core/browser/password_requirements_service.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// static
password_manager::PasswordRequirementsService*
IOSPasswordRequirementsServiceFactory::GetForProfile(ProfileIOS* profile) {
  return GetInstance()
      ->GetServiceForProfileAs<password_manager::PasswordRequirementsService>(
          profile, /*create=*/true);
}

// static
IOSPasswordRequirementsServiceFactory*
IOSPasswordRequirementsServiceFactory::GetInstance() {
  static base::NoDestructor<IOSPasswordRequirementsServiceFactory> instance;
  return instance.get();
}

IOSPasswordRequirementsServiceFactory::IOSPasswordRequirementsServiceFactory()
    : ProfileKeyedServiceFactoryIOS("PasswordRequirementsServiceFactory",
                                    TestingCreation::kNoServiceForTests) {}

IOSPasswordRequirementsServiceFactory::
    ~IOSPasswordRequirementsServiceFactory() {}

std::unique_ptr<KeyedService>
IOSPasswordRequirementsServiceFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return password_manager::CreatePasswordRequirementsService(
      profile->GetSharedURLLoaderFactory());
}
