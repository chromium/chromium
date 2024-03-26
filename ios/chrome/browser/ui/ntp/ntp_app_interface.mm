// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/ntp_app_interface.h"

#import "ios/chrome/browser/ui/ntp/metrics/home_metrics.h"

@implementation NTPAppInterface

+ (void)recordModuleFreshnessSignalForType:
    (ContentSuggestionsModuleType)module_type {
  RecordModuleFreshnessSignal(module_type);
}

@end
