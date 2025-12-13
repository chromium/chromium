// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_FAVICON_CONTENT_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_FAVICON_CONTENT_CONFIGURATION_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/chrome_content_configuration.h"

@class FaviconAttributes;

// A content configuration for a favicon.
@interface FaviconContentConfiguration : NSObject <ChromeContentConfiguration>

// The updates to properties must be reflected in the copy method.
// LINT.IfChange(Copy)

// The favicon attributes to be displayed.
@property(nonatomic, strong) FaviconAttributes* faviconAttributes;

// The badge image to be displayed.
@property(nonatomic, strong) UIImage* badgeImage;

// The accessibility identifier of the badge, to be used in tests.
@property(nonatomic, copy) NSString* badgeAccessibilityID;

// LINT.ThenChange(favicon_content_configuration.mm:Copy)

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_FAVICON_CONTENT_CONFIGURATION_H_
