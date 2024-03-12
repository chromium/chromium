// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_DEFAULT_BROWSER_PROMO_EVENT_EXPORTER_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_DEFAULT_BROWSER_PROMO_EVENT_EXPORTER_H_

#import "components/feature_engagement/public/tracker.h"

// A class to export saved default browser promo impressions and eligibility
// events to the Feature Engagement Tracker.
class DefaultBrowserEventExporter
    : public feature_engagement::TrackerEventExporter {
 public:
  DefaultBrowserEventExporter();
  ~DefaultBrowserEventExporter() override;

  // feature_engagement::TrackerEventExporter implementation
  void ExportEvents(ExportEventsCallback callback) override;
};

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_DEFAULT_BROWSER_PROMO_EVENT_EXPORTER_H_
