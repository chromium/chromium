// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/browsing_data_mediator.h"

#import "components/browsing_data/core/browsing_data_utils.h"
#import "components/browsing_data/core/pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/browsing_data_consumer.h"

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
}

- (void)disconnect {
  _prefs = nil;
}

#pragma mark - BrowsingDataMutator

- (void)timeRangeSelected:(browsing_data::TimePeriod)timeRange {
  _prefs->SetInteger(browsing_data::prefs::kDeleteTimePeriod,
                     static_cast<int>(timeRange));
}

@end
