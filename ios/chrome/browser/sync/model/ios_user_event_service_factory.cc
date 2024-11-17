// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sync/model/ios_user_event_service_factory.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_store_service.h"
#include "components/sync/service/sync_service.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_user_events/no_op_user_event_service.h"
#include "components/sync_user_events/user_event_service_impl.h"
#include "components/sync_user_events/user_event_sync_bridge.h"
#include "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/sync/model/data_type_store_service_factory.h"
#include "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/web/public/browser_state.h"

// static
syncer::UserEventService* IOSUserEventServiceFactory::GetForProfile(
    ProfileIOS* profile) {
  return static_cast<syncer::UserEventService*>(
      GetInstance()->GetServiceForBrowserState(profile, true));
}

// static
IOSUserEventServiceFactory* IOSUserEventServiceFactory::GetInstance() {
  static base::NoDestructor<IOSUserEventServiceFactory> instance;
  return instance.get();
}

IOSUserEventServiceFactory::IOSUserEventServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "UserEventService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
  DependsOn(SessionSyncServiceFactory::GetInstance());
}

IOSUserEventServiceFactory::~IOSUserEventServiceFactory() {}

std::unique_ptr<KeyedService>
IOSUserEventServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  if (context->IsOffTheRecord()) {
    return std::make_unique<syncer::NoOpUserEventService>();
  }

  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  syncer::OnceDataTypeStoreFactory store_factory =
      DataTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory();
  auto bridge = std::make_unique<syncer::UserEventSyncBridge>(
      std::move(store_factory),
      std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
          syncer::USER_EVENTS, /*dump_stack=*/base::BindRepeating(
              &syncer::ReportUnrecoverableError, ::GetChannel())),
      SessionSyncServiceFactory::GetForProfile(profile)->GetGlobalIdMapper());
  return std::make_unique<syncer::UserEventServiceImpl>(std::move(bridge));
}

web::BrowserState* IOSUserEventServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}
