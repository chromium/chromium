// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SITE_PERMISSIONS_SITE_PERMISSIONS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SITE_PERMISSIONS_SITE_PERMISSIONS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"

class FaviconLoader;
class HostContentSettingsMap;
@protocol SitePermissionsConsumer;

// Mediator for the Site Permissions screen. Queries HostContentSettingsMap for
// configured origins, observes updates, and supplies formatted items to the
// consumer.
@interface SitePermissionsMediator : NSObject <TableViewFaviconDataSource>

// The consumer receiving site permission items. Setting the consumer triggers
// an initial load.
@property(nonatomic, weak) id<SitePermissionsConsumer> consumer;

// Designated initializer.
- (instancetype)initWithHostContentSettingsMap:
                    (HostContentSettingsMap*)settingsMap
                                 faviconLoader:(FaviconLoader*)faviconLoader
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects observations and clears references.
- (void)disconnect;

// Loads site permissions from HostContentSettingsMap and updates the consumer.
- (void)loadSitePermissions;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SITE_PERMISSIONS_SITE_PERMISSIONS_MEDIATOR_H_
