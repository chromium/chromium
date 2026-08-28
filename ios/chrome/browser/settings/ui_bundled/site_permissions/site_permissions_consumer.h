// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SITE_PERMISSIONS_SITE_PERMISSIONS_CONSUMER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SITE_PERMISSIONS_SITE_PERMISSIONS_CONSUMER_H_

#import <Foundation/Foundation.h>

@class SitePermissionsSiteItem;

// Consumer protocol for the Site Permissions list screen.
@protocol SitePermissionsConsumer <NSObject>

// Updates the consumer with the list of configured sites.
- (void)setSitePermissionsSiteItems:(NSArray<SitePermissionsSiteItem*>*)items;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SITE_PERMISSIONS_SITE_PERMISSIONS_CONSUMER_H_
