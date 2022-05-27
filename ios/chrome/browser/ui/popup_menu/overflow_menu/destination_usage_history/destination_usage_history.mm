// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/destination_usage_history/destination_usage_history.h"

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The number of days since the Unix epoch; one day, in this context, runs from
// UTC midnight to UTC midnight.
int TodaysDay() {
  return (base::Time::Now() - base::Time::UnixEpoch()).InDays();
}

// Creates a path of the form "<day>.<destination>", where "." indexes
// into the next DictionaryValue down.
const std::string DottedPath(int day, overflow_menu::Destination destination) {
  std::string destination_name =
      overflow_menu::StringNameForDestination(destination);

  return base::NumberToString(day) + "." + destination_name;
}

}  // namespace

@implementation DestinationUsageHistory

- (instancetype)initWithPrefService:(PrefService*)prefService {
  if (self = [super init]) {
    _prefService = prefService;
  }

  return self;
}

#pragma mark - Public

// Track click for |destination| and associate it with TodaysDay().
- (void)trackDestinationClick:(overflow_menu::Destination)destination {
  DCHECK(_prefService);
  // Exit early if there's no pref service; this is not expected to happen.
  if (!_prefService)
    return;

  const base::Value* pref =
      _prefService->GetDictionary(prefs::kOverflowMenuDestinationUsageHistory);
  const base::Value::Dict* history = pref->GetIfDict();
  const std::string path = DottedPath(TodaysDay(), destination);

  absl::optional<int> prevNumClicks = history->FindIntByDottedPath(path);
  int numClicks = prevNumClicks.value_or(0) + 1;

  DictionaryPrefUpdate update(_prefService,
                              prefs::kOverflowMenuDestinationUsageHistory);
  update->SetIntPath(path, numClicks);
}

@end
