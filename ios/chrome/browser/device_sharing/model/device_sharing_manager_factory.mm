// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_sharing/model/device_sharing_manager_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/device_sharing/model/device_sharing_manager.h"
#import "ios/chrome/browser/device_sharing/model/device_sharing_manager_impl.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
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
  return static_cast<DeviceSharingManager*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
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
    : BrowserStateKeyedServiceFactory(
          "DeviceSharingManager",
          BrowserStateDependencyManager::GetInstance()) {}

std::unique_ptr<KeyedService>
DeviceSharingManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildDeviceSharingManager(context);
}

web::BrowserState* DeviceSharingManagerFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  // Incognito browser states use same service as regular browser states.
  return GetBrowserStateRedirectedInIncognito(context);
}
