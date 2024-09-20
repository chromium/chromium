// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TEXT_SELECTION_MODEL_TEXT_CLASSIFIER_MODEL_SERVICE_FAKE_H_
#define IOS_CHROME_BROWSER_TEXT_SELECTION_MODEL_TEXT_CLASSIFIER_MODEL_SERVICE_FAKE_H_

#include <memory>

#import "ios/chrome/browser/text_selection/model/text_classifier_model_service.h"

namespace web {
class BrowserState;
}  // namespace web
class OptimizationGuideService;

// Fake implementation of TextClassifierModelService that can be used by tests.
class TextClassifierModelServiceFake : public TextClassifierModelService {
 public:
  static std::unique_ptr<KeyedService> CreateTextClassifierModelService(
      web::BrowserState* context);
  ~TextClassifierModelServiceFake() override;

 private:
  TextClassifierModelServiceFake(OptimizationGuideService* opt_guide);
};

#endif  // IOS_CHROME_BROWSER_TEXT_SELECTION_MODEL_TEXT_CLASSIFIER_MODEL_SERVICE_FAKE_H_
