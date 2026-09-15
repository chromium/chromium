// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_SAAS_USAGE_SAAS_USAGE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_SAAS_USAGE_SAAS_USAGE_TAB_HELPER_H_

#import "base/memory/raw_ref.h"
#import "base/scoped_observation.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace enterprise_reporting {

class SaasUsageReportingController;

// Observes WebState navigations and reports them to
// SaasUsageReportingController.
class SaasUsageTabHelper : public web::WebStateObserver,
                           public web::WebStateUserData<SaasUsageTabHelper> {
 public:
  SaasUsageTabHelper(const SaasUsageTabHelper&) = delete;
  SaasUsageTabHelper& operator=(const SaasUsageTabHelper&) = delete;

  ~SaasUsageTabHelper() override;

  // web::WebStateObserver:
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  friend class web::WebStateUserData<SaasUsageTabHelper>;

  SaasUsageTabHelper(web::WebState* web_state,
                     SaasUsageReportingController* controller);

  base::ScopedObservation<web::WebState, web::WebStateObserver> observation_{
      this};
  const raw_ref<SaasUsageReportingController> controller_;
};

}  // namespace enterprise_reporting

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_REPORTING_SAAS_USAGE_SAAS_USAGE_TAB_HELPER_H_
