// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_OPTIMIZATION_GUIDE_GLOBAL_STATE_H_
#define IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_OPTIMIZATION_GUIDE_GLOBAL_STATE_H_

#import "base/memory/weak_ptr.h"
#import "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#import "components/optimization_guide/core/delivery/prediction_manager.h"
#import "components/optimization_guide/core/delivery/prediction_model_store.h"
#import "components/optimization_guide/core/model_execution/model_broker_state.h"
#import "components/optimization_guide/core/model_execution/on_device_asset_manager.h"
#import "components/optimization_guide/core/optimization_guide_enums.h"
#import "ios/chrome/browser/optimization_guide/model/chrome_profile_download_service_tracker.h"

namespace optimization_guide {

// This holds the ModelBrokerState and other common objects shared between
// profiles. // This is normally owned by the ApplicationContext.
class OptimizationGuideGlobalState final {
 public:
  OptimizationGuideGlobalState();
  ~OptimizationGuideGlobalState();

  PredictionModelStore& prediction_model_store() {
    return prediction_model_store_;
  }

  PredictionManager& prediction_manager() { return prediction_manager_; }

 private:
  OptimizationGuideGlobalState(const OptimizationGuideGlobalState&) = delete;
  OptimizationGuideGlobalState& operator=(const OptimizationGuideGlobalState&) =
      delete;

  PredictionModelStore prediction_model_store_;
  ChromeProfileDownloadServiceTracker profile_download_service_tracker_;
  PredictionManager prediction_manager_;

  base::WeakPtrFactory<OptimizationGuideGlobalState> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_OPTIMIZATION_GUIDE_GLOBAL_STATE_H_
