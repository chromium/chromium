// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_MODEL_WEB_PAYMENTS_METRICS_RECORDER_H_
#define IOS_CHROME_BROWSER_PAYMENTS_MODEL_WEB_PAYMENTS_METRICS_RECORDER_H_

#import <stddef.h>

#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/web_state_user_data.h"

// WebPaymentsMetricsRecorder observes changes in the WebState to measure web
// payments flows.
//
// In order to help the payments industry understand and improve the user
// experience of payment flows in Chrome, this class records basic anonymized
// metrics (for clients which are opted into metrics collection). Only technical
// information for such payment flows is ever recorded, never any personal
// information or transaction details.
//
// Currently we record such metrics for:
//
// - 3D-Secure payment challenges (see ShouldAllowRequest).
class WebPaymentsMetricsRecorder
    : public web::WebStatePolicyDecider,
      public web::WebStateUserData<WebPaymentsMetricsRecorder> {
 public:
  WebPaymentsMetricsRecorder(const WebPaymentsMetricsRecorder&) = delete;
  WebPaymentsMetricsRecorder& operator=(const WebPaymentsMetricsRecorder&) =
      delete;

  ~WebPaymentsMetricsRecorder() override;

 private:
  friend class web::WebStateUserData<WebPaymentsMetricsRecorder>;

  explicit WebPaymentsMetricsRecorder(web::WebState* web_state);

  // web::WebStatePolicyDecider:
  // Only used to listen to navigations on subframes; it never cancels
  // navigations. Inheriting from WebStateObserver instead is insufficient
  // because it only listens to main frames.
  void ShouldAllowRequest(NSURLRequest* request,
                          RequestInfo request_info,
                          PolicyDecisionCallback callback) override;

  // The hash of the most recent form data, or 0 if there is none. WebKit
  // reports a form submission again for each HTTP 307 or 308 redirect, with the
  // same form data, so this is used to avoid recording the same 3DS message
  // twice.
  size_t last_form_data_hash_ = 0;
};

#endif  // IOS_CHROME_BROWSER_PAYMENTS_MODEL_WEB_PAYMENTS_METRICS_RECORDER_H_
