// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_sharing/model/device_sharing_manager_factory.h"

#import "ios/chrome/browser/device_sharing/model/device_sharing_manager.h"
#import "ios/chrome/browser/device_sharing/model/device_sharing_manager_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace {
std::unique_ptr<KeyedService> BuildDeviceSharingManager(
    web::BrowserState* context) {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  return std::make_unique<DeviceSharingManagerImpl>(profile);
}
}  // namespace

// static
DeviceSharingManager* DeviceSharingManagerFactory::GetForProfile(
    ProfileIOS* profile) {
  return GetInstance()->GetServiceForProfileAs<DeviceSharingManager>(
      profile, /*create=*/true);
}

// static
DeviceSharingManagerFactory* DeviceSharingManagerFactory::GetInstance() {
  static base::NoDestructor<DeviceSharingManagerFactory> instance;
  return instance.get();
}

// static
BrowserStateKeyedServiceFactory::TestingFactory
DeviceSharingManagerFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildDeviceSharingManager);
}

DeviceSharingManagerFactory::DeviceSharingManagerFactory()
    : ProfileKeyedServiceFactoryIOS("DeviceSharingManager",
                                    ProfileSelection::kRedirectedInIncognito) {}

std::unique_ptr<KeyedService>
DeviceSharingManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildDeviceSharingManager(context);
}
