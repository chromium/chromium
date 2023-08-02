// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/metrics_util.h"

#import "ios/chrome/common/app_group/app_group_metrics.h"

void UpdateUMACountForKey(NSString* key) {
  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
  NSInteger numberOfDisplay = [sharedDefaults integerForKey:key];
  [sharedDefaults setInteger:numberOfDisplay + 1 forKey:key];
}

void UpdateHistogramCount(NSString* histogram, int bucket) {
  NSString* key = app_group::HistogramCountKey(histogram, bucket);

  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
  NSInteger count = [sharedDefaults integerForKey:key];
  [sharedDefaults setInteger:count + 1 forKey:key];
}
