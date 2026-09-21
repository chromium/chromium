// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_COORDINATOR_SITE_SETTINGS_CATEGORY_DETAIL_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_COORDINATOR_SITE_SETTINGS_CATEGORY_DETAIL_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "components/content_settings/core/common/content_settings_types.h"
#import "ios/chrome/browser/settings/site_settings/ui/site_settings_category_detail_mutator.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"

class FaviconLoader;
class HostContentSettingsMap;
@protocol SiteSettingsCategoryDetailConsumer;

// Mediator for the Site Settings Category Detail screen.
@interface SiteSettingsCategoryDetailMediator
    : NSObject <SiteSettingsCategoryDetailMutator, TableViewFaviconDataSource>

// Consumer receiving default setting and site exception items.
@property(nonatomic, weak) id<SiteSettingsCategoryDetailConsumer> consumer;

// Designated initializer.
- (instancetype)initWithHostContentSettingsMap:
                    (HostContentSettingsMap*)settingsMap
                                 faviconLoader:(FaviconLoader*)faviconLoader
                           contentSettingsType:(ContentSettingsType)type
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects observations and clears references.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_COORDINATOR_SITE_SETTINGS_CATEGORY_DETAIL_MEDIATOR_H_
