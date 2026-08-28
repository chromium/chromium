// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SITE_PERMISSIONS_SITE_PERMISSIONS_SITE_ITEM_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SITE_PERMISSIONS_SITE_PERMISSIONS_SITE_ITEM_H_

#import <Foundation/Foundation.h>

@class CrURL;

// Model item representing a site/origin entry in the Site Permissions list.
@interface SitePermissionsSiteItem : NSObject

// The raw origin string (e.g. "https://example.com:443" or
// "https://google.com").
@property(nonatomic, copy) NSString* origin;

// The formatted host/domain title displayed in the cell.
@property(nonatomic, copy) NSString* formattedTitle;

// The CrURL representation of the origin, used for favicon fetching and URL
// display.
@property(nonatomic, strong) CrURL* URL;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SITE_PERMISSIONS_SITE_PERMISSIONS_SITE_ITEM_H_
