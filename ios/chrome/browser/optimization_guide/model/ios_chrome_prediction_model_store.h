// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_IOS_CHROME_PREDICTION_MODEL_STORE_H_
#define IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_IOS_CHROME_PREDICTION_MODEL_STORE_H_

#import "base/no_destructor.h"
#import "components/optimization_guide/core/prediction_model_store.h"

namespace optimization_guide {

class IOSChromePredictionModelStore : public PredictionModelStore {
 public:
  // Returns the singleton model store.
  static IOSChromePredictionModelStore* GetInstance();

  IOSChromePredictionModelStore();
  ~IOSChromePredictionModelStore() override;

  IOSChromePredictionModelStore(const IOSChromePredictionModelStore&) = delete;
  IOSChromePredictionModelStore& operator=(
      const IOSChromePredictionModelStore&) = delete;

  // optimization_guide::PredictionModelStore:
  PrefService* GetLocalState() const override;

 private:
  friend base::NoDestructor<IOSChromePredictionModelStore>;
};

}  // namespace optimization_guide

#endif  // IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_IOS_CHROME_PREDICTION_MODEL_STORE_H_
