// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/parcel_tracking_item.h"

#import "base/time/time.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "url/gurl.h"

@implementation ParcelTrackingItem

#pragma mark - MagicStackModule

- (ContentSuggestionsModuleType)type {
  return ContentSuggestionsModuleType::kParcelTracking;
}

@end
