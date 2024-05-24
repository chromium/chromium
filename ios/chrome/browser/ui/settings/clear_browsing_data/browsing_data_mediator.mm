// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/browsing_data_mediator.h"

#import "components/browsing_data/core/browsing_data_utils.h"
#import "components/browsing_data/core/pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/browsing_data_consumer.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation BrowsingDataMediator {
  raw_ptr<PrefService> _prefs;
}

- (instancetype)initWithPrefs:(PrefService*)prefs {
  if (self = [super init]) {
    _prefs = prefs;
  }
  return self;
}

- (void)setConsumer:(id<BrowsingDataConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  [_consumer
      setTimeRange:static_cast<browsing_data::TimePeriod>(_prefs->GetInteger(
                       browsing_data::prefs::kDeleteTimePeriod))];
  [self dispatchBrowsingDataSummary];
}

- (void)disconnect {
  _prefs = nil;
}

#pragma mark - BrowsingDataMutator

- (void)timeRangeSelected:(browsing_data::TimePeriod)timeRange {
  _prefs->SetInteger(browsing_data::prefs::kDeleteTimePeriod,
                     static_cast<int>(timeRange));

  // TODO(crbug.com/341097601): Call `dispatchBrowsingDataSummary` to update the
  // browsing data summary with the updated counters when the time range is
  // changed.
}

#pragma mark - Private

// Dispatches the updated browsing data summary to the ViewController.
- (void)dispatchBrowsingDataSummary {
  NSMutableArray<NSString*>* summaryItems = [[NSMutableArray alloc] init];

  if (_prefs->GetBoolean(browsing_data::prefs::kDeleteBrowsingHistory)) {
    // TODO(crbug.com/341097601): Use the actual number of sites that could be
    // deleted for the selected time frame.
    [summaryItems addObject:l10n_util::GetPluralNSStringF(
                                IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SITES, 2)];
  }

  if (_prefs->GetBoolean(browsing_data::prefs::kDeleteCookies)) {
    [summaryItems
        addObject:l10n_util::GetNSString(
                      IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SITE_DATA)];
  }

  if (_prefs->GetBoolean(browsing_data::prefs::kDeleteCache)) {
    [summaryItems
        addObject:l10n_util::GetNSString(
                      IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_CACHED_FILES)];
  }

  if (_prefs->GetBoolean(browsing_data::prefs::kDeletePasswords)) {
    // TODO(crbug.com/341097601): Use the actual number of passwords that could
    // be deleted for the selected time frame.
    [summaryItems
        addObject:l10n_util::GetPluralNSStringF(
                      IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_PASSWORDS, 1)];
  }

  if (_prefs->GetBoolean(browsing_data::prefs::kDeleteFormData)) {
    // TODO(crbug.com/341097601): Use the actual number of passwords that could
    // be deleted for the selected time frame.
    [summaryItems
        addObject:l10n_util::GetPluralNSStringF(
                      IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_AUTOFILL_DATA, 5)];
  }

  // TODO(crbug.com/342185075): Check if the comma is translated correctly for
  // right to left languages, e.g. arabic.
  [_consumer setBrowsingDataSummary:
                 [summaryItems
                     componentsJoinedByString:
                         l10n_util::GetNSString(
                             IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SEPARATOR)]];
}

@end
