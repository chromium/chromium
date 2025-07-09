// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/feature_engagement/model/event_exporter.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/event_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/features.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/default_browser/model/features.h"

namespace {

// Returns number of days since the given time.
int DaysSinceTime(base::Time time) {
  return (time - base::Time::UnixEpoch()).InDays();
}

}  // namespace

EventExporter::EventExporter() {}

EventExporter::~EventExporter() = default;

void EventExporter::ExportEvents(ExportEventsCallback callback) {
  std::vector<EventData> events_to_migrate;

  // Migrate the FRE promo event.
  // TODO(crbug.com/382733018): Clean up the default browser promos eligibility
  // tracking migration code.
  if (!FRETimestampMigrationDone()) {
    AddFREPromoEvent(events_to_migrate);
    LogFRETimestampMigrationDone();
  }

  // Migrate promo interest signals
  // TODO(crbug.com/382733018): Clean up the default browser promos eligibility
  // tracking migration code.
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

  // TODO(crbug.com/382733018): Clean up the default browser promos eligibility
  // tracking migration code.
  if (!IsPromoImpressionsMigrationDone()) {
    AddGenericPromoImpressions(events_to_migrate);
    AddTailoredPromoImpressions(events_to_migrate);
    LogPromoImpressionsMigrationDone();
  }

  // Migrate the default browser's non-modal promo events.
  // TODO(crbug.com/391166425): Clean up the non-modal promo migration code.
  if (!IsNonModalPromoMigrationDone()) {
    NSDate* last_interaction = LastTimeUserInteractedWithNonModalPromo();
    if (last_interaction && UserInteractionWithNonModalPromoCount() > 0) {
      const NSInteger count = UserInteractionWithNonModalPromoCount();
      for (NSInteger i = 0; i < count; ++i) {
        events_to_migrate.emplace_back(
            feature_engagement::events::
                kNonModalDefaultBrowserPromoUrlPasteTrigger,
            DaysSinceTime(base::Time::FromNSDate(last_interaction)));
      }
    }
    LogNonModalPromoMigrationDone();
  }

  // Migrate the sign-in fullscreen promo display events.
  // TODO(crbug.com/396111171): Post migration clean up.
  if (IsFullscreenSigninPromoManagerMigrationEnabled() &&
      !signin::IsFullscreenSigninPromoManagerMigrationDone()) {
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    NSInteger count = [defaults integerForKey:kSigninPromoViewDisplayCountKey];
    for (NSInteger i = 0; i < count; ++i) {
      events_to_migrate.emplace_back(
          feature_engagement::events::kIOSSigninFullscreenPromoTrigger,
          DaysSinceTime(base::Time::Now()));
    }
    signin::LogFullscreenSigninPromoManagerMigrationDone();
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(events_to_migrate)));
}

void EventExporter::AddFREPromoEvent(std::vector<EventData>& events) {
  const base::Time time = GetDefaultBrowserFREPromoTimestampIfLast();
  if (time != base::Time::UnixEpoch()) {
    events.emplace_back(feature_engagement::events::kIOSDefaultBrowserFREShown,
                        DaysSinceTime(time));
  }
}

void EventExporter::AddPromoInterestEvents(std::vector<EventData>& events,
                                           DefaultPromoType promo,
                                           const std::string& event_name) {
  for (base::Time time : LoadTimestampsForPromoType(promo)) {
    events.emplace_back(event_name, DaysSinceTime(time));
  }
}

void EventExporter::AddGenericPromoImpressions(std::vector<EventData>& events) {
  const base::Time time = GetGenericDefaultBrowserPromoTimestamp();
  if (time != base::Time::UnixEpoch()) {
    events.emplace_back(
        feature_engagement::events::kGenericDefaultBrowserPromoTrigger,
        DaysSinceTime(time));
  }
}
void EventExporter::AddTailoredPromoImpressions(
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
