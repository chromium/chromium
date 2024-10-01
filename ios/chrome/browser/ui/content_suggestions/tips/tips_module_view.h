// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TIPS_TIPS_MODULE_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TIPS_TIPS_MODULE_VIEW_H_

#import <UIKit/UIKit.h>

@class TipsModuleState;

// A view displaying the Tips module in the Magic Stack.
@interface TipsModuleView : UIView

// Initializes the `TipsModuleView` with `state`.
- (instancetype)initWithState:(TipsModuleState*)state;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TIPS_TIPS_MODULE_VIEW_H_
