// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/promos_manager/promos_manager_factory.h"

#import "base/no_destructor.h"
#import "base/time/default_clock.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/promos_manager/features.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/promos_manager/promos_manager_event_exporter_factory.h"
#import "ios/chrome/browser/promos_manager/promos_manager_impl.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

// static
PromosManager* PromosManagerFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<PromosManager*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
PromosManagerFactory* PromosManagerFactory::GetInstance() {
  static base::NoDestructor<PromosManagerFactory> instance;
  return instance.get();
}

PromosManagerFactory::PromosManagerFactory()
    : BrowserStateKeyedServiceFactory(
          "PromosManagerFactory",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(feature_engagement::TrackerFactory::GetInstance());
  DependsOn(PromosManagerEventExporterFactory::GetInstance());
}

PromosManagerFactory::~PromosManagerFactory() = default;

std::unique_ptr<KeyedService> PromosManagerFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  PromosManagerEventExporter* event_exporter =
      ShouldPromosManagerUseFET()
          ? PromosManagerEventExporterFactory::GetForBrowserState(browser_state)
          : nullptr;
  auto promos_manager = std::make_unique<PromosManagerImpl>(
      GetApplicationContext()->GetLocalState(),
      base::DefaultClock::GetInstance(),
      feature_engagement::TrackerFactory::GetForBrowserState(browser_state),
      event_exporter);
  promos_manager->Init();
  return promos_manager;
}

web::BrowserState* PromosManagerFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
