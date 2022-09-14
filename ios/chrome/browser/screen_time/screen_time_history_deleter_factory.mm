// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/screen_time/screen_time_history_deleter_factory.h"

#import "base/no_destructor.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/screen_time/screen_time_history_deleter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
ScreenTimeHistoryDeleter* ScreenTimeHistoryDeleterFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<ScreenTimeHistoryDeleter*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
ScreenTimeHistoryDeleterFactory*
ScreenTimeHistoryDeleterFactory::GetInstance() {
  static base::NoDestructor<ScreenTimeHistoryDeleterFactory> instance;
  return instance.get();
}

ScreenTimeHistoryDeleterFactory::ScreenTimeHistoryDeleterFactory()
    : BrowserStateKeyedServiceFactory(
          "ScreenTimeHistoryDeleter",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::HistoryServiceFactory::GetInstance());
}

ScreenTimeHistoryDeleterFactory::~ScreenTimeHistoryDeleterFactory() {}

std::unique_ptr<KeyedService>
ScreenTimeHistoryDeleterFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS);
  return std::make_unique<ScreenTimeHistoryDeleter>(history_service);
}

web::BrowserState* ScreenTimeHistoryDeleterFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

bool ScreenTimeHistoryDeleterFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
