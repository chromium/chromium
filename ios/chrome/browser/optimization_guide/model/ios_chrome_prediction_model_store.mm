// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/optimization_guide/model/ios_chrome_prediction_model_store.h"

#import "ios/chrome/browser/optimization_guide/model/optimization_guide_global_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"

namespace optimization_guide {

// TODO:(crbug.com/440098411): Remove this once downstream stops using it.
// static
IOSChromePredictionModelStore* IOSChromePredictionModelStore::GetInstance() {
  return &GetApplicationContext()
              ->GetOptimizationGuideGlobalState()
              ->prediction_model_store();
}

IOSChromePredictionModelStore::IOSChromePredictionModelStore() = default;
IOSChromePredictionModelStore::~IOSChromePredictionModelStore() = default;

PrefService* IOSChromePredictionModelStore::GetLocalState() const {
  return GetApplicationContext()->GetLocalState();
}

}  // namespace optimization_guide
