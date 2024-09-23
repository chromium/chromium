// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/screen_time/model/screen_time_history_deleter.h"

#import <ScreenTime/ScreenTime.h>

#import "base/time/time.h"
#import "components/history/core/browser/history_service.h"
#import "net/base/apple/url_conversions.h"

namespace {
// Converts base::Time to NSDate.
NSDate* NSDateFromTime(const base::Time& time) {
  return [NSDate dateWithTimeIntervalSince1970:time.InSecondsFSinceUnixEpoch()];
}
}  // namespace

ScreenTimeHistoryDeleter::ScreenTimeHistoryDeleter(
    history::HistoryService* history_service)
    : history_service_(history_service) {
  DCHECK(history_service_);
  history_service_observation_.Observe(history_service_.get());
  screen_time_history_ = [[STWebHistory alloc] init];
}

ScreenTimeHistoryDeleter::~ScreenTimeHistoryDeleter() = default;

void ScreenTimeHistoryDeleter::Shutdown() {
  if (history_service_) {
    history_service_observation_.Reset();
  }
  history_service_ = nullptr;
  screen_time_history_ = nil;
}

void ScreenTimeHistoryDeleter::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  if (deletion_info.IsAllHistory()) {
    [screen_time_history_ deleteAllHistory];
  } else if (deletion_info.time_range().IsValid()) {
    const history::DeletionTimeRange& range = deletion_info.time_range();
    NSDateInterval* interval =
        [[NSDateInterval alloc] initWithStartDate:NSDateFromTime(range.begin())
                                          endDate:NSDateFromTime(range.end())];
    [screen_time_history_ deleteHistoryDuringInterval:interval];
  } else {
    for (const history::URLRow& row : deletion_info.deleted_rows()) {
      NSURL* url = net::NSURLWithGURL(row.url());
      [screen_time_history_ deleteHistoryForURL:url];
    }
  }
}
