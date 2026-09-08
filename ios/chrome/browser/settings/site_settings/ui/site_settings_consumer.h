// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_UI_SITE_SETTINGS_CONSUMER_H_
#define IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_UI_SITE_SETTINGS_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "components/content_settings/core/common/content_settings.h"
#import "components/content_settings/core/common/content_settings_types.h"

// Consumer protocol for the Site Settings root screen.
@protocol SiteSettingsConsumer <NSObject>

// Sets whether the location category is supported on this platform/build.
- (void)setLocationCategoryEnabled:(BOOL)enabled;

// Sets the default content setting for a category.
- (void)setDefaultSetting:(ContentSetting)setting
                  forType:(ContentSettingsType)type;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_UI_SITE_SETTINGS_CONSUMER_H_
