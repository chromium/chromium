// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_TEST_DATA_PROTECTION_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_TEST_DATA_PROTECTION_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

#import "components/enterprise/connectors/core/reporting_constants.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_paging.h"

// Test app interface for enterprise data protection.
@interface DataProtectionAppInterface : NSObject

// Sets `watermarkText` on the current active tab's DataProtectionTabHelper.
+ (void)setWatermarkText:(NSString*)watermarkText;

// Requests a Full Page screenshot PDF representation from the foreground
// active scene's `UIScreenshotServiceDelegate` and returns `YES` if non-empty
// PDF data is generated before timing out.
+ (BOOL)canGenerateFullPageScreenshotPDF;

// Sets the `EnterpriseRealTimeUrlCheckMode` pref on the active profile to
// `mode`.
+ (void)setRealTimeUrlCheckMode:
    (enterprise_connectors::EnterpriseRealTimeUrlCheckMode)mode;

// Clears the `kEnterpriseRealTimeUrlCheckMode` user pref on the original
// profile.
+ (void)clearRealTimeUrlCheckMode;

// Exits the tab grid.
+ (void)exitTabGrid;

// Switches the Tab Grid to the specified `page` without animation. The Tab Grid
// must already be visible.
+ (void)showTabGridPage:(TabGridPage)page;

@end

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_TEST_DATA_PROTECTION_APP_INTERFACE_H_
