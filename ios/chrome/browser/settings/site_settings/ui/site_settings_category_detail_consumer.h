// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_UI_SITE_SETTINGS_CATEGORY_DETAIL_CONSUMER_H_
#define IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_UI_SITE_SETTINGS_CATEGORY_DETAIL_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "components/content_settings/core/common/content_settings.h"

@class SiteSettingsSiteException;

// Consumer protocol for the site settings category detail screen.
@protocol SiteSettingsCategoryDetailConsumer <NSObject>

// Updates the selected default setting (CONTENT_SETTING_ASK or
// CONTENT_SETTING_BLOCK).
- (void)setDefaultSetting:(ContentSetting)setting;

// Updates the site exceptions for allowed and not allowed sections.
- (void)setAllowedSites:(NSArray<SiteSettingsSiteException*>*)allowedSites
        notAllowedSites:(NSArray<SiteSettingsSiteException*>*)notAllowedSites;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_UI_SITE_SETTINGS_CATEGORY_DETAIL_CONSUMER_H_
