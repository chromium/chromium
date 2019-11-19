// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ELEMENTS_FADE_TRUNCATING_LABEL_H_
#define IOS_CHROME_BROWSER_UI_ELEMENTS_FADE_TRUNCATING_LABEL_H_

#import <UIKit/UIKit.h>

typedef enum {
  FadeTruncatingTail = 0x1,
  FadeTruncatingHead = 0x2,
  FadeTruncatingHeadAndTail = FadeTruncatingHead | FadeTruncatingTail
} FadeTruncatingMode;

// A label which applies a fade-to-background color gradient to one or both ends
// of the string if it is too large to fit the available area. It  uses the
// attributedText property of UILabel to implement the fading.
@interface FadeTruncatingLabel : UILabel

// Which side(s) to truncate.
@property(nonatomic, assign) FadeTruncatingMode truncateMode;

// Whether the text being displayed should be treated as a URL.
@property(nonatomic, assign) BOOL displayAsURL;

@end

#endif  // IOS_CHROME_BROWSER_UI_ELEMENTS_FADE_TRUNCATING_LABEL_H_
