// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABS_BACKGROUND_TAB_ANIMATION_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TABS_BACKGROUND_TAB_ANIMATION_VIEW_H_

#import <UIKit/UIKit.h>

// View containing a link image and a shadow. Used to notify the user that a new
// tab has been opened in background.
@interface BackgroundTabAnimationView : UIView

- (instancetype)initWithFrame:(CGRect)frame
                    incognito:(BOOL)incognito NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Starts an Open In New Tab animation in |parentView|, from |originPoint| with
// a |completion| block. The named layout guide for the TabGrid button should be
// accessible from |parentView|. |originPoint| should be in window coordinates.
- (void)animateFrom:(CGPoint)originPoint
    toTabGridButtonWithCompletion:(void (^)())completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABS_BACKGROUND_TAB_ANIMATION_VIEW_H_
