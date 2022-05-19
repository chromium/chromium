// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/destination_usage_history.h"

#include "base/json/json_writer.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/prefs/pref_service.h"
#import "ios/chrome/browser/pref_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Converts base::Value::Dict* to NSDictionary.
NSDictionary* NSDictionaryFromDictionaryValue(const base::Value::Dict* value) {
  std::string JSON;
  const bool success = base::JSONWriter::Write(*value, &JSON);
  DCHECK(success) << "Failed to convert base::Value::Dict to JSON";
  NSString* JSONString = @(JSON.c_str());
  NSData* JSONData = [JSONString dataUsingEncoding:NSUTF8StringEncoding];
  NSDictionary* dictionary = base::mac::ObjCCastStrict<NSDictionary>(
      [NSJSONSerialization JSONObjectWithData:JSONData
                                      options:kNilOptions
                                        error:nil]);
  DCHECK(dictionary) << "Failed to convert JSON to NSDictionary";

  return dictionary;
}

}  // namespace

@interface DestinationUsageHistory ()

// Dictionary that tracks the total number of times a given destination was
// clicked in a given day.
@property(nonatomic, readwrite, strong) NSDictionary* history;

@end

@implementation DestinationUsageHistory

- (instancetype)initWithPrefService:(PrefService*)prefService {
  if (self = [super init]) {
    _prefService = prefService;

    if (_prefService) {
      const base::Value::Dict* locallyStoredHistory =
          _prefService
              ->GetDictionary(prefs::kOverflowMenuDestinationUsageHistory)
              ->GetIfDict();

      if (!locallyStoredHistory || locallyStoredHistory->empty()) {
        // User's first time tracking destination usage history, so a new
        // dictionary should be created.
        _history = [[NSDictionary alloc] init];
      } else {
        _history = NSDictionaryFromDictionaryValue(locallyStoredHistory);
      }
    }
  }

  return self;
}

#pragma mark - Public

- (void)trackDestinationClick:(NSString*)destinationName {
  if (self.prefService) {
    base::Value::Dict dict;
    dict.Set("lastClicked", base::SysNSStringToUTF8(destinationName));

    self.prefService->SetDict(prefs::kOverflowMenuDestinationUsageHistory,
                              std::move(dict));
  }
}

@end
