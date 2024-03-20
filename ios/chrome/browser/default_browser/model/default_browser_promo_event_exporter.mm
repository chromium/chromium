// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/default_browser_promo_event_exporter.h"

#import "base/time/time.h"
#import "components/feature_engagement/public/event_constants.h"
#import "ios/chrome/browser/default_browser/model/utils.h"

DefaultBrowserEventExporter::DefaultBrowserEventExporter() {}

DefaultBrowserEventExporter::~DefaultBrowserEventExporter() = default;

void DefaultBrowserEventExporter::ExportEvents(ExportEventsCallback callback) {
  std::vector<EventData> events_to_migrate;

  if (!FRETimestampMigrationDone()) {
    const base::Time time = GetDefaultBrowserFREPromoTimestampIfLast();
    if (time != base::Time::UnixEpoch()) {
      events_to_migrate.emplace_back(
          feature_engagement::events::kIOSDefaultBrowserFREShown,
          (time - base::Time::UnixEpoch()).InDays());
    }
    LogFRETimestampMigrationDone();
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(events_to_migrate)));
}
