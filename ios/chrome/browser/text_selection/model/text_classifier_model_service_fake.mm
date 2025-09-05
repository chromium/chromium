// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/text_selection/model/text_classifier_model_service_fake.h"

#import <memory>

#import "base/functional/bind.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/text_selection/model/text_classifier_model_service.h"

namespace {

// Returns a new instance of TextClassifierModelServiceFake.
std::unique_ptr<KeyedService> BuildInstance(ProfileIOS* profile) {
  OptimizationGuideService* opt_guide =
      OptimizationGuideServiceFactory::GetForProfile(profile);
  return std::make_unique<TextClassifierModelServiceFake>(opt_guide);
}

}  // anonymous namespace

// static
TextClassifierModelServiceFake::TestingFactory
TextClassifierModelServiceFake::GetTestingFactory() {
  return base::BindOnce(BuildInstance);
}

TextClassifierModelServiceFake::~TextClassifierModelServiceFake() {}

TextClassifierModelServiceFake::TextClassifierModelServiceFake(
    OptimizationGuideService* opt_guide)
    : TextClassifierModelService(opt_guide) {}
