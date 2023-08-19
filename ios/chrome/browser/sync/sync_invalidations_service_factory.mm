// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/sync_invalidations_service_factory.h"

#import "base/no_destructor.h"
#import "components/gcm_driver/gcm_profile_service.h"
#import "components/gcm_driver/instance_id/instance_id_profile_service.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/sync/invalidations/sync_invalidations_service_impl.h"
#import "ios/chrome/browser/gcm/instance_id/ios_chrome_instance_id_profile_service_factory.h"
#import "ios/chrome/browser/gcm/ios_chrome_gcm_profile_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

// static
syncer::SyncInvalidationsService*
SyncInvalidationsServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<syncer::SyncInvalidationsService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, /*create=*/true));
}

// static
SyncInvalidationsServiceFactory*
SyncInvalidationsServiceFactory::GetInstance() {
  static base::NoDestructor<SyncInvalidationsServiceFactory> instance;
  return instance.get();
}

SyncInvalidationsServiceFactory::SyncInvalidationsServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SyncInvalidationsService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(IOSChromeGCMProfileServiceFactory::GetInstance());
  DependsOn(IOSChromeInstanceIDProfileServiceFactory::GetInstance());
}

SyncInvalidationsServiceFactory::~SyncInvalidationsServiceFactory() = default;

std::unique_ptr<KeyedService>
SyncInvalidationsServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);

  gcm::GCMDriver* gcm_driver =
      IOSChromeGCMProfileServiceFactory::GetForBrowserState(browser_state)
          ->driver();
  instance_id::InstanceIDDriver* instance_id_driver =
      IOSChromeInstanceIDProfileServiceFactory::GetForBrowserState(
          browser_state)
          ->driver();
  return std::make_unique<syncer::SyncInvalidationsServiceImpl>(
      gcm_driver, instance_id_driver);
}
