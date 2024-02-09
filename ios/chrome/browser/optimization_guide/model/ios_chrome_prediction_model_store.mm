// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/optimization_guide/model/ios_chrome_prediction_model_store.h"

#import "ios/chrome/browser/shared/model/application_context/application_context.h"

namespace optimization_guide {

// static
IOSChromePredictionModelStore* IOSChromePredictionModelStore::GetInstance() {
  static base::NoDestructor<IOSChromePredictionModelStore> model_store;
  return model_store.get();
}

IOSChromePredictionModelStore::IOSChromePredictionModelStore() = default;
IOSChromePredictionModelStore::~IOSChromePredictionModelStore() = default;

PrefService* IOSChromePredictionModelStore::GetLocalState() const {
  return GetApplicationContext()->GetLocalState();
}

}  // namespace optimization_guide
