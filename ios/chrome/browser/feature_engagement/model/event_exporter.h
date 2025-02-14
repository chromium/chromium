// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FEATURE_ENGAGEMENT_MODEL_EVENT_EXPORTER_H_
#define IOS_CHROME_BROWSER_FEATURE_ENGAGEMENT_MODEL_EVENT_EXPORTER_H_

#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/default_browser/model/utils.h"

// A class to export manually-tracked events to the Feature Engagement Tracker.
class EventExporter : public feature_engagement::TrackerEventExporter {
 public:
  EventExporter();
  ~EventExporter() override;

  // feature_engagement::TrackerEventExporter implementation
  void ExportEvents(ExportEventsCallback callback) override;

 private:
  void AddFREPromoEvent(std::vector<EventData>& events);
  void AddPromoInterestEvents(std::vector<EventData>& events,
                              DefaultPromoType promo,
                              const std::string& event_name);
  void AddGenericPromoImpressions(std::vector<EventData>& events);
  void AddTailoredPromoImpressions(std::vector<EventData>& events);
};

#endif  // IOS_CHROME_BROWSER_FEATURE_ENGAGEMENT_MODEL_EVENT_EXPORTER_H_
