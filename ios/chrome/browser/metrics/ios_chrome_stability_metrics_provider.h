// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_IOS_CHROME_STABILITY_METRICS_PROVIDER_H_
#define IOS_CHROME_BROWSER_METRICS_IOS_CHROME_STABILITY_METRICS_PROVIDER_H_

#include "base/macros.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/stability_metrics_helper.h"
#include "ios/web/public/deprecated/global_web_state_observer.h"

class PrefService;

namespace web {
class NavigationContext;
}  // namespace web

// IOSChromeStabilityMetricsProvider gathers and logs Chrome-specific stability-
// related metrics.
class IOSChromeStabilityMetricsProvider : public metrics::MetricsProvider,
                                          public web::GlobalWebStateObserver {
 public:
  // Buckets for the histogram that counts events relevant for counting page
  // loads. These events are mutually exclusive.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class PageLoadCountNavigationType {
    // A chrome:// URL navigation. This is not counted for page load.
    CHROME_URL_NAVIGATION = 0,
    // A same-document web (i.e. not chrome:// URL) navigation. This is not
    // counted for page load.
    SAME_DOCUMENT_WEB_NAVIGATION = 1,
    // A navigation that is not SAME_DOCUMENT_WEB or CHROME_URL. It is counted
    // as a page load.
    PAGE_LOAD_NAVIGATION = 2,

    // OBSOLETE VALUES. DO NOT REUSE.
    OBSOLETE_LOADING_STARTED = 3,

    COUNT
  };

  explicit IOSChromeStabilityMetricsProvider(PrefService* local_state);
  ~IOSChromeStabilityMetricsProvider() override;

  // metrics::MetricsDataProvider:
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;
  void ProvideStabilityMetrics(
      metrics::SystemProfileProto* system_profile_proto) override;
  void ClearSavedStabilityMetrics() override;

  // web::GlobalWebStateObserver:
  void WebStateDidStartLoading(web::WebState* web_state) override;
  void WebStateDidStartNavigation(
      web::WebState* web_state,
      web::NavigationContext* navigation_context) override;
  void RenderProcessGone(web::WebState* web_state) override;

  // Records a renderer process crash.
  void LogRendererCrash();

  static const char kPageLoadCountLoadingStartedMetric[];
  static const char kPageLoadCountMetric[];

 private:
  metrics::StabilityMetricsHelper helper_;

  // True if recording is currently enabled.
  bool recording_enabled_;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeStabilityMetricsProvider);
};

#endif  // IOS_CHROME_BROWSER_METRICS_IOS_CHROME_STABILITY_METRICS_PROVIDER_H_
