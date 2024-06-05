// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/page_image/model/page_image_service_factory.h"

#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/page_image_service/image_service.h"
#import "components/page_image_service/image_service_impl.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_scheme_classifier_impl.h"
#import "ios/chrome/browser/autocomplete/model/remote_suggestions_service_factory.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

// static
page_image_service::ImageService* PageImageServiceFactory::GetForBrowserState(
    ChromeBrowserState* state) {
  return static_cast<page_image_service::ImageService*>(
      GetInstance()->GetServiceForBrowserState(state, true));
}

PageImageServiceFactory::PageImageServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "PageImageService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::TemplateURLServiceFactory::GetInstance());
  DependsOn(RemoteSuggestionsServiceFactory::GetInstance());
  DependsOn(OptimizationGuideServiceFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());
}

// static
PageImageServiceFactory* PageImageServiceFactory::GetInstance() {
  static base::NoDestructor<PageImageServiceFactory> instance;
  return instance.get();
}

PageImageServiceFactory::~PageImageServiceFactory() {}

std::unique_ptr<KeyedService> PageImageServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);

  TemplateURLService* template_url_service =
      ios::TemplateURLServiceFactory::GetForBrowserState(browser_state);
  RemoteSuggestionsService* remote_suggestions_service =
      RemoteSuggestionsServiceFactory::GetForBrowserState(browser_state, true);
  OptimizationGuideService* optimization_guide_service =
      OptimizationGuideServiceFactory::GetForBrowserState(browser_state);
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForBrowserState(browser_state);
  std::unique_ptr<AutocompleteSchemeClassifier> autocomplete_scheme_classifier =
      std::make_unique<AutocompleteSchemeClassifierImpl>();

  return std::make_unique<page_image_service::ImageServiceImpl>(
      template_url_service, remote_suggestions_service,
      optimization_guide_service, sync_service,
      std::move(autocomplete_scheme_classifier));
}
