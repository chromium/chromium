// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/send_tab_to_self/send_tab_to_self_client_service_factory.h"

#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/send_tab_to_self/send_tab_to_self_client_service_ios.h"
#include "ios/chrome/browser/sync/send_tab_to_self_sync_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace send_tab_to_self {

// static
SendTabToSelfClientServiceFactory*
SendTabToSelfClientServiceFactory::GetInstance() {
  static base::NoDestructor<SendTabToSelfClientServiceFactory> instance;
  return instance.get();
}

// static
SendTabToSelfClientServiceIOS*
SendTabToSelfClientServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<SendTabToSelfClientServiceIOS*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

SendTabToSelfClientServiceFactory::SendTabToSelfClientServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SendTabToSelfClientService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(SendTabToSelfSyncServiceFactory::GetInstance());
}

SendTabToSelfClientServiceFactory::~SendTabToSelfClientServiceFactory() {}

std::unique_ptr<KeyedService>
SendTabToSelfClientServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);

  SendTabToSelfSyncService* sync_service =
      SendTabToSelfSyncServiceFactory::GetForBrowserState(browser_state);

  return std::make_unique<SendTabToSelfClientServiceIOS>(
      browser_state, sync_service->GetSendTabToSelfModel());
}

}  // namespace send_tab_to_self
