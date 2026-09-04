// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/proxy/model/proxy_service_controller_factory.h"

#import "base/no_destructor.h"
#import "ios/chrome/browser/enterprise/proxy/model/proxy_service_controller.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// static
ProxyServiceController* ProxyServiceControllerFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<ProxyServiceController>(
      profile, /*create=*/true);
}

// static
ProxyServiceControllerFactory* ProxyServiceControllerFactory::GetInstance() {
  static base::NoDestructor<ProxyServiceControllerFactory> instance;
  return instance.get();
}

ProxyServiceControllerFactory::ProxyServiceControllerFactory()
    : ProfileKeyedServiceFactoryIOS("ProxyServiceController",
                                    ProfileSelection::kNoInstanceInIncognito) {}

ProxyServiceControllerFactory::~ProxyServiceControllerFactory() = default;

std::unique_ptr<KeyedService>
ProxyServiceControllerFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  return std::make_unique<ProxyServiceController>();
}
