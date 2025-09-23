// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAGE_INFO_TRACKING_PROTECTION_UI_PAGE_INFO_TRACKING_PROTECTION_CONSUMER_H_
#define IOS_CHROME_BROWSER_PAGE_INFO_TRACKING_PROTECTION_UI_PAGE_INFO_TRACKING_PROTECTION_CONSUMER_H_

#import "ios/chrome/browser/page_info/tracking_protection/ui/page_info_tracking_protection_info.h"

// Defines APIs for the frontend ViewController to load and update the UI.
@protocol PageInfoTrackingProtectionConsumer <NSObject>

// Propagates the changes in TrackingProtection state to the ViewController.
- (void)setTrackingProtectionInfo:(PageInfoTrackingProtectionInfo*)info;

@end

#endif  // IOS_CHROME_BROWSER_PAGE_INFO_TRACKING_PROTECTION_UI_PAGE_INFO_TRACKING_PROTECTION_CONSUMER_H_
