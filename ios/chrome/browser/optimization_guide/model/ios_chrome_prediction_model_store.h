// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_IOS_CHROME_PREDICTION_MODEL_STORE_H_
#define IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_IOS_CHROME_PREDICTION_MODEL_STORE_H_

#import "components/optimization_guide/core/delivery/prediction_model_store.h"

namespace optimization_guide {

class IOSChromePredictionModelStore : public PredictionModelStore {
 public:
  IOSChromePredictionModelStore();
  ~IOSChromePredictionModelStore() override;

  IOSChromePredictionModelStore(const IOSChromePredictionModelStore&) = delete;
  IOSChromePredictionModelStore& operator=(
      const IOSChromePredictionModelStore&) = delete;

  // optimization_guide::PredictionModelStore:
  PrefService* GetLocalState() const override;
};

}  // namespace optimization_guide

#endif  // IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_IOS_CHROME_PREDICTION_MODEL_STORE_H_
