// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/text_selection/text_classifier_model_service_fake.h"

#import <memory>

#import "ios/chrome/browser/optimization_guide/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/text_selection/text_classifier_model_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
std::unique_ptr<KeyedService>
TextClassifierModelServiceFake::CreateTextClassifierModelService(
    web::BrowserState* context) {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);
  auto* opt_guide =
      OptimizationGuideServiceFactory::GetForBrowserState(browser_state);
  auto service =
      base::WrapUnique(new TextClassifierModelServiceFake(opt_guide));
  return service;
}

TextClassifierModelServiceFake::~TextClassifierModelServiceFake() {}

TextClassifierModelServiceFake::TextClassifierModelServiceFake(
    OptimizationGuideService* opt_guide)
    : TextClassifierModelService(opt_guide) {}
