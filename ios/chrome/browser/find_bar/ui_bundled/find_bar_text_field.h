// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIND_BAR_UI_BUNDLED_FIND_BAR_TEXT_FIELD_H_
#define IOS_CHROME_BROWSER_FIND_BAR_UI_BUNDLED_FIND_BAR_TEXT_FIELD_H_

#import <UIKit/UIKit.h>

// FindBarTextField is a textfield that provides a space within its editing rect
// to show an overlay. This space is in the trailing side of the editing rect.
// It is used in Find Bar to show "X of Y" results count overlay next to the
// search term.
@interface FindBarTextField : UITextField

@property(nonatomic, readwrite) CGFloat overlayWidth;

@end

#endif  // IOS_CHROME_BROWSER_FIND_BAR_UI_BUNDLED_FIND_BAR_TEXT_FIELD_H_
