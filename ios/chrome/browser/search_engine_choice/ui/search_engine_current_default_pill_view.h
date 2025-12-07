// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_UI_SEARCH_ENGINE_CURRENT_DEFAULT_PILL_VIEW_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_UI_SEARCH_ENGINE_CURRENT_DEFAULT_PILL_VIEW_H_

#import <UIKit/UIKit.h>

// View to display the 'Current default' pill.
@interface SearchEngineCurrentDefaultPillView : UIView

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_UI_SEARCH_ENGINE_CURRENT_DEFAULT_PILL_VIEW_H_
