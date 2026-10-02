// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_BRICK_CONFIGURATIONS_TITLE_BRICK_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SHARED_UI_BRICK_CONFIGURATIONS_TITLE_BRICK_CONFIGURATION_H_

#import <UIKit/UIKit.h>

// Configuration object for the `TitleBrick` brick.
@interface TitleBrickConfiguration : NSObject

// The title string to display.
@property(nonatomic, copy) NSString* title;

// The subtitle string to display.
@property(nonatomic, copy) NSString* subtitle;

// The text style for the title label, default is `UIFontTextStyleTitle1`.
@property(nonatomic, copy) UIFontTextStyle titleStyle;

// The text style for the subtitle label, default is `UIFontTextStyleBody`.
@property(nonatomic, copy) UIFontTextStyle subtitleStyle;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_BRICK_CONFIGURATIONS_TITLE_BRICK_CONFIGURATION_H_
