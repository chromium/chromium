// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_TOOLBAR_H_
#define IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_TOOLBAR_H_

#import <UIKit/UIKit.h>

@interface OpenInToolbar : UIView

- (instancetype)initWithTarget:(id)target action:(SEL)action;

// Updates the constraint managing the bottom margin height using NamedGuides.
- (void)updateBottomMarginHeight;

@end

#endif  // IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_TOOLBAR_H_
