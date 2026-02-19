// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_DEFAULT_BROWSER_UI_DEFAULT_BROWSER_CONFIG_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_DEFAULT_BROWSER_UI_DEFAULT_BROWSER_CONFIG_H_

#import "ios/chrome/browser/content_suggestions/ui/cells/icon_detail_view.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/icon_detail_view_config.h"

@protocol DefaultBrowserCommands;

// Item containing the configurations for the Default Browser view.
@interface DefaultBrowserConfig
    : IconDetailViewConfig <IconDetailViewTapDelegate>

// Command handler for user actions.
@property(nonatomic, weak) id<DefaultBrowserCommands> defaultBrowserHandler;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_DEFAULT_BROWSER_UI_DEFAULT_BROWSER_CONFIG_H_
