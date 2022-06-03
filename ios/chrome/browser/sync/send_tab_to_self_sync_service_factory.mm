// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/send_tab_to_self_sync_service_factory.h"

#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/sync/device_info_sync_service_factory.h"
#include "ios/chrome/browser/sync/model_type_store_service_factory.h"
#include "ios/chrome/common/channel_info.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using send_tab_to_self::SendTabToSelfSyncService;

std::unique_ptr<KeyedService> BuildSendTabToSelfService(
    web::BrowserState* context) {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);

  syncer::OnceModelTypeStoreFactory store_factory =
      ModelTypeStoreServiceFactory::GetForBrowserState(browser_state)
          ->GetStoreFactory();

  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS);

  syncer::DeviceInfoTracker* device_info_tracker =
      DeviceInfoSyncServiceFactory::GetForBrowserState(browser_state)
          ->GetDeviceInfoTracker();

  return std::make_unique<SendTabToSelfSyncService>(
      GetChannel(), std::move(store_factory), history_service,
      device_info_tracker);
}

// static
SendTabToSelfSyncServiceFactory*
SendTabToSelfSyncServiceFactory::GetInstance() {
  static base::NoDestructor<SendTabToSelfSyncServiceFactory> instance;
  return instance.get();
}

// static
SendTabToSelfSyncService* SendTabToSelfSyncServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<SendTabToSelfSyncService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
BrowserStateKeyedServiceFactory::TestingFactory
SendTabToSelfSyncServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildSendTabToSelfService);
}

SendTabToSelfSyncServiceFactory::SendTabToSelfSyncServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SendTabToSelfSyncService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
  DependsOn(ios::HistoryServiceFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
}

SendTabToSelfSyncServiceFactory::~SendTabToSelfSyncServiceFactory() {}

std::unique_ptr<KeyedService>
SendTabToSelfSyncServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildSendTabToSelfService(context);
}
