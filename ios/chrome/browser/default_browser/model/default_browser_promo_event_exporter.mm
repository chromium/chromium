// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/default_browser_promo_event_exporter.h"

#import "base/time/time.h"
#import "components/feature_engagement/public/event_constants.h"

namespace {

// Returns number of days since the given time.
int DaysSinceTime(base::Time time) {
  return (time - base::Time::UnixEpoch()).InDays();
}

}  // namespace

DefaultBrowserEventExporter::DefaultBrowserEventExporter() {}

DefaultBrowserEventExporter::~DefaultBrowserEventExporter() = default;

void DefaultBrowserEventExporter::ExportEvents(ExportEventsCallback callback) {
  std::vector<EventData> events_to_migrate;

  // Migrate the FRE promo event.
  if (!FRETimestampMigrationDone()) {
    AddFREPromoEvent(events_to_migrate);
    LogFRETimestampMigrationDone();
  }

  // Migrate promo interest signals
  if (!IsPromoInterestEventMigrationDone()) {
    AddPromoInterestEvents(
        events_to_migrate, DefaultPromoTypeGeneral,
        feature_engagement::events::kGenericDefaultBrowserPromoConditionsMet);
    AddPromoInterestEvents(
        events_to_migrate, DefaultPromoTypeAllTabs,
        feature_engagement::events::kAllTabsPromoConditionsMet);
    AddPromoInterestEvents(
        events_to_migrate, DefaultPromoTypeMadeForIOS,
        feature_engagement::events::kMadeForIOSPromoConditionsMet);
    AddPromoInterestEvents(
        events_to_migrate, DefaultPromoTypeStaySafe,
        feature_engagement::events::kStaySafePromoConditionsMet);
    LogPromoInterestEventMigrationDone();
  }

  if (!IsPromoImpressionsMigrationDone()) {
    AddGenericPromoImpressions(events_to_migrate);
    AddTailoredPromoImpressions(events_to_migrate);
    LogPromoImpressionsMigrationDone();
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(events_to_migrate)));
}

void DefaultBrowserEventExporter::AddFREPromoEvent(
    std::vector<EventData>& events) {
  const base::Time time = GetDefaultBrowserFREPromoTimestampIfLast();
  if (time != base::Time::UnixEpoch()) {
    events.emplace_back(feature_engagement::events::kIOSDefaultBrowserFREShown,
                        DaysSinceTime(time));
  }
}

void DefaultBrowserEventExporter::AddPromoInterestEvents(
    std::vector<EventData>& events,
    DefaultPromoType promo,
    const std::string& event_name) {
  for (base::Time time : LoadTimestampsForPromoType(promo)) {
    events.emplace_back(event_name, DaysSinceTime(time));
  }
}

void DefaultBrowserEventExporter::AddGenericPromoImpressions(
    std::vector<EventData>& events) {
  const base::Time time = GetGenericDefaultBrowserPromoTimestamp();
  if (time != base::Time::UnixEpoch()) {
    events.emplace_back(
        feature_engagement::events::kGenericDefaultBrowserPromoTrigger,
        DaysSinceTime(time));
  }
}
void DefaultBrowserEventExporter::AddTailoredPromoImpressions(
    std::vector<EventData>& events) {
  const base::Time time = GetTailoredDefaultBrowserPromoTimestamp();
  if (time != base::Time::UnixEpoch()) {
    // For tailored promos trigger the group config and all the individual
    // tailored promos.
    events.emplace_back(
        feature_engagement::events::kTailoredDefaultBrowserPromosGroupTrigger,
        DaysSinceTime(time));
    events.emplace_back(feature_engagement::events::kAllTabsPromoTrigger,
                        DaysSinceTime(time));
    events.emplace_back(feature_engagement::events::kMadeForIOSPromoTrigger,
                        DaysSinceTime(time));
    events.emplace_back(feature_engagement::events::kStaySafePromoTrigger,
                        DaysSinceTime(time));
  }
}
