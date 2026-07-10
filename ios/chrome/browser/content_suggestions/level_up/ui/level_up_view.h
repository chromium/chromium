// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_LEVEL_UP_UI_LEVEL_UP_VIEW_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_LEVEL_UP_UI_LEVEL_UP_VIEW_H_

#import <UIKit/UIKit.h>

@class LevelUpConfig;

// A dedicated view for the Level Up card in the NTP Magic Stack.
@interface LevelUpView : UIView

- (instancetype)initWithConfig:(LevelUpConfig*)config;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_LEVEL_UP_UI_LEVEL_UP_VIEW_H_
