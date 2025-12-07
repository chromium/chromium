// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TEXT_SELECTION_MODEL_TEXT_CLASSIFIER_MODEL_SERVICE_FAKE_H_
#define IOS_CHROME_BROWSER_TEXT_SELECTION_MODEL_TEXT_CLASSIFIER_MODEL_SERVICE_FAKE_H_

#include "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"
#include "ios/chrome/browser/text_selection/model/text_classifier_model_service.h"

class OptimizationGuideService;

// Fake implementation of TextClassifierModelService that can be used by tests.
class TextClassifierModelServiceFake : public TextClassifierModelService {
 public:
  using TestingFactory = ProfileKeyedServiceFactoryIOS::TestingFactory;

  static TestingFactory GetTestingFactory();
  ~TextClassifierModelServiceFake() override;

  TextClassifierModelServiceFake(OptimizationGuideService* opt_guide);
};

#endif  // IOS_CHROME_BROWSER_TEXT_SELECTION_MODEL_TEXT_CLASSIFIER_MODEL_SERVICE_FAKE_H_
