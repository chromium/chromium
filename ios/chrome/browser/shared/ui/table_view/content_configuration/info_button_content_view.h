// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_INFO_BUTTON_CONTENT_VIEW_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_INFO_BUTTON_CONTENT_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/chrome_content_view.h"

@class InfoButtonContentConfiguration;

// A view that displays a single info button, leading-aligned.
@interface InfoButtonContentView : UIView <ChromeContentView>

// Returns the view, configured with `configuration`.
- (instancetype)initWithConfiguration:
    (InfoButtonContentConfiguration*)configuration;

// Returns the info button, to be used in testing only.
- (UIButton*)infoButtonForTesting;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CONTENT_CONFIGURATION_INFO_BUTTON_CONTENT_VIEW_H_
