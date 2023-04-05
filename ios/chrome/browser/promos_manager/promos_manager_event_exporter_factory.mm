// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/promos_manager/promos_manager_event_exporter_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "base/no_destructor.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/promos_manager/promos_manager_event_exporter.h"

// static
PromosManagerEventExporter*
PromosManagerEventExporterFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<PromosManagerEventExporter*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
PromosManagerEventExporterFactory*
PromosManagerEventExporterFactory::GetInstance() {
  static base::NoDestructor<PromosManagerEventExporterFactory> instance;
  return instance.get();
}

PromosManagerEventExporterFactory::PromosManagerEventExporterFactory()
    : BrowserStateKeyedServiceFactory(
          "PromosManagerEventExporterFactory",
          BrowserStateDependencyManager::GetInstance()) {}

PromosManagerEventExporterFactory::~PromosManagerEventExporterFactory() =
    default;

std::unique_ptr<KeyedService>
PromosManagerEventExporterFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  auto promos_manager = std::make_unique<PromosManagerEventExporter>(
      GetApplicationContext()->GetLocalState());
  return promos_manager;
}

web::BrowserState* PromosManagerEventExporterFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
