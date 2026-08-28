// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SITE_PERMISSIONS_SITE_PERMISSIONS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SITE_PERMISSIONS_SITE_PERMISSIONS_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/ui/list_model/list_model.h"

// Accessibility identifier for the Site Permissions table view.
extern NSString* const kSitePermissionsTableViewId;

// Accessibility identifier for the Site Permissions search bar.
extern NSString* const kSitePermissionsSearchBarId;

// Accessibility identifier for the Site Permissions empty view.
extern NSString* const kSitePermissionsEmptyViewId;

// Section identifiers for Site Permissions table view.
typedef NS_ENUM(NSInteger, SitePermissionsSectionIdentifier) {
  SectionIdentifierSites = kSectionIdentifierEnumZero,
};

// Item types for Site Permissions table view.
typedef NS_ENUM(NSInteger, SitePermissionsItemType) {
  ItemTypeSite = kItemTypeEnumZero,
};

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_SITE_PERMISSIONS_SITE_PERMISSIONS_CONSTANTS_H_
