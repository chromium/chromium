// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/ios_chrome_stability_metrics_provider.h"

#import "base/feature_list.h"
#import "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ios/web/common/features.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "url/gurl.h"

// Name of the UMA enum histogram that counts DidStartNavigation events by type.
const char IOSChromeStabilityMetricsProvider::kPageLoadCountMetric[] =
    "IOS.PageLoadCount.Counts";
// Name of the UMA enum history that counts DidStartLoading events.
const char
    IOSChromeStabilityMetricsProvider::kPageLoadCountLoadingStartedMetric[] =
        "IOS.PageLoadCount.LoadingStarted";

IOSChromeStabilityMetricsProvider::IOSChromeStabilityMetricsProvider(
    PrefService* local_state)
    : helper_(local_state), recording_enabled_(false) {}

IOSChromeStabilityMetricsProvider::~IOSChromeStabilityMetricsProvider() {}

void IOSChromeStabilityMetricsProvider::OnRecordingEnabled() {
  recording_enabled_ = true;
}

void IOSChromeStabilityMetricsProvider::OnRecordingDisabled() {
  recording_enabled_ = false;
}

void IOSChromeStabilityMetricsProvider::LogRendererCrash() {
  if (!recording_enabled_)
    return;

  helper_.LogRendererCrash();
}

void IOSChromeStabilityMetricsProvider::WebStateDidStartLoading(
    web::WebState* web_state) {
  if (!recording_enabled_)
    return;

  UMA_HISTOGRAM_BOOLEAN(kPageLoadCountLoadingStartedMetric, true);
}

void IOSChromeStabilityMetricsProvider::WebStateDidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!recording_enabled_)
    return;

  PageLoadCountNavigationType type =
      PageLoadCountNavigationType::PAGE_LOAD_NAVIGATION;
  if (navigation_context->GetUrl().SchemeIs(kChromeUIScheme)) {
    type = PageLoadCountNavigationType::CHROME_URL_NAVIGATION;
  } else if (navigation_context->IsSameDocument()) {
    type = PageLoadCountNavigationType::SAME_DOCUMENT_WEB_NAVIGATION;
  } else {
    helper_.LogLoadStarted();
  }
  UMA_HISTOGRAM_ENUMERATION(kPageLoadCountMetric, type,
                            PageLoadCountNavigationType::COUNT);
}

void IOSChromeStabilityMetricsProvider::RenderProcessGone(
    web::WebState* web_state) {
  if (!recording_enabled_)
    return;
  LogRendererCrash();
  // TODO(crbug.com/41297697): web_state->GetLastCommittedURL() is likely the
  // URL that caused a renderer crash and can be logged here.
}
