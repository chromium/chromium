// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/text_selection/model/text_classifier_model_service_fake.h"

#import <memory>

#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/text_selection/model/text_classifier_model_service.h"

// static
std::unique_ptr<KeyedService>
TextClassifierModelServiceFake::CreateTextClassifierModelService(
    web::BrowserState* context) {
  ProfileIOS* profile = ProfileIOS::FromBrowserState(context);
  OptimizationGuideService* opt_guide =
      OptimizationGuideServiceFactory::GetForProfile(profile);
  std::unique_ptr<TextClassifierModelService> service =
      base::WrapUnique(new TextClassifierModelServiceFake(opt_guide));
  return service;
}

TextClassifierModelServiceFake::~TextClassifierModelServiceFake() {}

TextClassifierModelServiceFake::TextClassifierModelServiceFake(
    OptimizationGuideService* opt_guide)
    : TextClassifierModelService(opt_guide) {}
