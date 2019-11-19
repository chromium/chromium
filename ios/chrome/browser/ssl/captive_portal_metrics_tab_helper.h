// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SSL_CAPTIVE_PORTAL_METRICS_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_SSL_CAPTIVE_PORTAL_METRICS_TAB_HELPER_H_

#include "base/macros.h"
#include "base/timer/timer.h"
#include "components/captive_portal/captive_portal_detector.h"
#include "ios/chrome/browser/ssl/captive_portal_metrics.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebState;
}

// Logs the result of a captive portal detector when users enter error states
// which could be caused by being on a Captive Portal network. These metrics
// will be used to identify how many users incorrectly see these error cases
// because they are on captive portal networks without a network connection. In
// the future, these users should be shown the captive portal login interstitial
// if these metrics show that enough users are affected.
// 1. When a -1200 error occurs.
// 2. If a request is taking a long time to complete.
class CaptivePortalMetricsTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<CaptivePortalMetricsTabHelper> {
 public:
  ~CaptivePortalMetricsTabHelper() override;

 private:
  explicit CaptivePortalMetricsTabHelper(web::WebState* web_state);
  friend class web::WebStateUserData<CaptivePortalMetricsTabHelper>;

  // Tests if network access is currently blocked by a captive portal.
  void TestForCaptivePortal(
      captive_portal::CaptivePortalDetector::DetectionCallback callback);

  // Tests for a captive portal and logs the resulting metric.
  void TimerTriggered();

  // Returns the associated CaptivePortalStatus value for logging to UMA metrics
  // based on detection |results|.
  static CaptivePortalStatus CaptivePortalStatusFromDetectionResult(
      const captive_portal::CaptivePortalDetector::Results& results);

  // Logs the |results| of the captive portal detector to the UMA metric for a
  // failed secure connection.
  static void HandleSecureConnectionFailedCaptivePortalDetectionResult(
      const captive_portal::CaptivePortalDetector::Results& results);

  // Logs the |results| of the captive portal detector to the UMA metric for a
  // potential request timeout.
  static void HandleTimeoutCaptivePortalDetectionResult(
      const captive_portal::CaptivePortalDetector::Results& results);

  // web::WebStateObserver implementation.
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // A timer to test for a captive portal blocking an ongoing request.
  base::OneShotTimer timer_;
  // The web state associated with this tab helper.
  web::WebState* web_state_;

  WEB_STATE_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(CaptivePortalMetricsTabHelper);
};

#endif  // IOS_CHROME_BROWSER_SSL_CAPTIVE_PORTAL_METRICS_TAB_HELPER_H_
