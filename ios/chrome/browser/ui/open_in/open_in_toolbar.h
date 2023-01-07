// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_TOOLBAR_H_
#define IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_TOOLBAR_H_

#import <UIKit/UIKit.h>

@interface OpenInToolbar : UIView

// Init with the given local target and action.
- (instancetype)initWithTarget:(id)target
                        action:(SEL)action NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)aRect NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_TOOLBAR_H_
