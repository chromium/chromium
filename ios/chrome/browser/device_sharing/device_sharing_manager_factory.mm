// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_sharing/device_sharing_manager_factory.h"

#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/device_sharing/device_sharing_manager.h"
#import "ios/chrome/browser/device_sharing/device_sharing_manager_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
std::unique_ptr<KeyedService> BuildDeviceSharingManager(
    web::BrowserState* context) {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<DeviceSharingManagerImpl>(browser_state);
}
}

// static
DeviceSharingManager* DeviceSharingManagerFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<DeviceSharingManager*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
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
