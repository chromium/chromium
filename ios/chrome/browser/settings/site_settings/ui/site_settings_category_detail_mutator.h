// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_UI_SITE_SETTINGS_CATEGORY_DETAIL_MUTATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_UI_SITE_SETTINGS_CATEGORY_DETAIL_MUTATOR_H_

#import <Foundation/Foundation.h>

#import "components/content_settings/core/common/content_settings.h"

@class SiteSettingsSiteException;

// Mutator protocol to handle user actions in the category detail screen.
@protocol SiteSettingsCategoryDetailMutator <NSObject>

// Updates the default content setting for this category.
- (void)setDefaultSetting:(ContentSetting)setting;

// Updates the permission setting for the specified site exception.
- (void)setSetting:(ContentSetting)setting
           forSite:(SiteSettingsSiteException*)site;

// Removes the permission exception for the specified site.
- (void)deleteSettingForSite:(SiteSettingsSiteException*)site;

// Removes the permission exceptions for all specified `sites`.
- (void)deleteSettingsForSites:(NSArray<SiteSettingsSiteException*>*)sites;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_UI_SITE_SETTINGS_CATEGORY_DETAIL_MUTATOR_H_
