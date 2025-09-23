// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAGE_INFO_TRACKING_PROTECTION_UI_PAGE_INFO_TRACKING_PROTECTION_INFO_H_
#define IOS_CHROME_BROWSER_PAGE_INFO_TRACKING_PROTECTION_UI_PAGE_INFO_TRACKING_PROTECTION_INFO_H_

// Object that stores tracking protection information.
@interface PageInfoTrackingProtectionInfo : NSObject

// Determines whether tracking protection is currently enabled or disabled.
@property(nonatomic, assign) BOOL hasTrackingProtectionException;

// Determines whether the tracking protection section should be shown for the
// current site.
@property(nonatomic, assign) BOOL shouldShowTrackingProtectionUI;

@end

#endif  // IOS_CHROME_BROWSER_PAGE_INFO_TRACKING_PROTECTION_UI_PAGE_INFO_TRACKING_PROTECTION_INFO_H_
