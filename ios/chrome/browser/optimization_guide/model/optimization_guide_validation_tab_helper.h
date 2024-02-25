// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_OPTIMIZATION_GUIDE_VALIDATION_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_OPTIMIZATION_GUIDE_VALIDATION_TAB_HELPER_H_

#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "ios/web/public/web_state_observer.h"
#include "ios/web/public/web_state_user_data.h"

// A tab helper that acts as a consumer to the optimization guide service.
class OptimizationGuideValidationTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<OptimizationGuideValidationTabHelper> {
 public:
  ~OptimizationGuideValidationTabHelper() override;

  OptimizationGuideValidationTabHelper(
      const OptimizationGuideValidationTabHelper&) = delete;
  OptimizationGuideValidationTabHelper& operator=(
      const OptimizationGuideValidationTabHelper&) = delete;

 private:
  friend class web::WebStateUserData<OptimizationGuideValidationTabHelper>;

  explicit OptimizationGuideValidationTabHelper(web::WebState* web_state);

  // WebStateObserver implementation:
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // Callback invoked when the decision for validate metadata fetch type is
  // received from the optimization guide.
  void OnMetadataFetchValidationDecisionReceived(
      const GURL& url,
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  WEB_STATE_USER_DATA_KEY_DECL();

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<OptimizationGuideValidationTabHelper> weak_factory_{
      this};
};

#endif  // IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_OPTIMIZATION_GUIDE_VALIDATION_TAB_HELPER_H_
