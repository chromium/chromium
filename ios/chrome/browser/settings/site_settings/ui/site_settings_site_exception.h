// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_UI_SITE_SETTINGS_SITE_EXCEPTION_H_
#define IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_UI_SITE_SETTINGS_SITE_EXCEPTION_H_

#import <Foundation/Foundation.h>

#import "components/content_settings/core/common/content_settings_pattern.h"

@class CrURL;

// Model representing a site exception entry in the category detail list.
@interface SiteSettingsSiteException : NSObject

// The raw origin/pattern string (e.g., "https://example.com:443").
@property(nonatomic, copy) NSString* origin;

// The formatted host/domain title displayed in the cell.
@property(nonatomic, copy) NSString* formattedTitle;

// The CrURL representation of the origin, used for favicon fetching.
@property(nonatomic, strong) CrURL* URL;

// The content settings primary pattern for this exception.
@property(nonatomic, assign) ContentSettingsPattern primaryPattern;

// The content settings secondary pattern for this exception.
@property(nonatomic, assign) ContentSettingsPattern secondaryPattern;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_UI_SITE_SETTINGS_SITE_EXCEPTION_H_
