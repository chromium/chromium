// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ssl/captive_portal_metrics_tab_helper.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "components/captive_portal/captive_portal_detector.h"
#import "ios/chrome/browser/ssl/captive_portal_detector_tab_helper.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Result of captive portal network check after a request takes longer than
// |kCaptivePortalTestDelayInMilliseconds|.
const char kCaptivePortalCausingTimeoutHistogram[] =
    "CaptivePortal.Session.TimeoutDetectionResult";

// Result of captive portal network check after a request fails with
// NSURLErrorSecureConnectionFailed.
const char kCaptivePortalSecureConnectionFailedHistogram[] =
    "CaptivePortal.Session.SecureConnectionFailed";

// Time in milliseconds of still ongoing request before testing if the user is
// behind a captive portal.
const int64_t kCaptivePortalTestDelayInMilliseconds = 8000;

CaptivePortalMetricsTabHelper::CaptivePortalMetricsTabHelper(
    web::WebState* web_state)
    : web_state_(web_state) {
  web_state_->AddObserver(this);
}

CaptivePortalMetricsTabHelper::~CaptivePortalMetricsTabHelper() = default;

void CaptivePortalMetricsTabHelper::TimerTriggered() {
  TestForCaptivePortal(
      base::BindOnce(&HandleTimeoutCaptivePortalDetectionResult));
}

void CaptivePortalMetricsTabHelper::TestForCaptivePortal(
    captive_portal::CaptivePortalDetector::DetectionCallback callback) {
  CaptivePortalDetectorTabHelper* tab_helper =
      CaptivePortalDetectorTabHelper::FromWebState(web_state_);
  // TODO(crbug.com/760873): replace test with DCHECK when this method is only
  // called on WebStates attached to tabs.
  if (tab_helper) {
    tab_helper->detector()->DetectCaptivePortal(
        GURL(captive_portal::CaptivePortalDetector::kDefaultURL),
        std::move(callback), NO_TRAFFIC_ANNOTATION_YET);
  }
}

// static
CaptivePortalStatus
CaptivePortalMetricsTabHelper::CaptivePortalStatusFromDetectionResult(
    const captive_portal::CaptivePortalDetector::Results& results) {
  CaptivePortalStatus status;
  switch (results.result) {
    case captive_portal::RESULT_INTERNET_CONNECTED:
      status = CaptivePortalStatus::ONLINE;
      break;
    case captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL:
      status = CaptivePortalStatus::PORTAL;
      break;
    default:
      status = CaptivePortalStatus::UNKNOWN;
      break;
  }
  return status;
}

// static
void CaptivePortalMetricsTabHelper::
    HandleSecureConnectionFailedCaptivePortalDetectionResult(
        const captive_portal::CaptivePortalDetector::Results& results) {
  CaptivePortalStatus status =
      CaptivePortalMetricsTabHelper::CaptivePortalStatusFromDetectionResult(
          results);
  UMA_HISTOGRAM_ENUMERATION(kCaptivePortalSecureConnectionFailedHistogram,
                            static_cast<int>(status),
                            static_cast<int>(CaptivePortalStatus::COUNT));
}

// static
void CaptivePortalMetricsTabHelper::HandleTimeoutCaptivePortalDetectionResult(
    const captive_portal::CaptivePortalDetector::Results& results) {
  CaptivePortalStatus status =
      CaptivePortalMetricsTabHelper::CaptivePortalStatusFromDetectionResult(
          results);
  UMA_HISTOGRAM_ENUMERATION(kCaptivePortalCausingTimeoutHistogram,
                            static_cast<int>(status),
                            static_cast<int>(CaptivePortalStatus::COUNT));
}

// WebStateObserver
void CaptivePortalMetricsTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  timer_.Stop();

  timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kCaptivePortalTestDelayInMilliseconds),
      this, &CaptivePortalMetricsTabHelper::TimerTriggered);
}

void CaptivePortalMetricsTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  timer_.Stop();

  NSError* error = navigation_context->GetError();
  if ([error.domain isEqualToString:NSURLErrorDomain] &&
      error.code == NSURLErrorSecureConnectionFailed) {
    TestForCaptivePortal(
        base::Bind(&HandleSecureConnectionFailedCaptivePortalDetectionResult));
  } else if ([error.domain isEqualToString:NSURLErrorDomain] &&
             error.code == NSURLErrorTimedOut) {
    TestForCaptivePortal(
        base::Bind(&HandleTimeoutCaptivePortalDetectionResult));
  }
}

void CaptivePortalMetricsTabHelper::WebStateDestroyed(
    web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);

  timer_.Stop();

  // Ensure the captive portal detection is canceled if it never completed.
  CaptivePortalDetectorTabHelper* tab_helper =
      CaptivePortalDetectorTabHelper::FromWebState(web_state_);
  // TODO(crbug.com/760873): replace test with DCHECK when this method is only
  // called on WebStates attached to tabs.
  if (tab_helper) {
    tab_helper->detector()->Cancel();
  }

  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

WEB_STATE_USER_DATA_KEY_IMPL(CaptivePortalMetricsTabHelper)
