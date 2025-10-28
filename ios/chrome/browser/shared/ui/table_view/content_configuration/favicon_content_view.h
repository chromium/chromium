// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_FAVICON_CONTENT_VIEW_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_FAVICON_CONTENT_VIEW_H_

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/chrome_content_view.h"

@class FaviconContentConfiguration;

// A content view for a favicon.
@interface FaviconContentView : UIView <ChromeContentView>

- (instancetype)initWithConfiguration:
    (FaviconContentConfiguration*)configuration NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_FAVICON_CONTENT_VIEW_H_
