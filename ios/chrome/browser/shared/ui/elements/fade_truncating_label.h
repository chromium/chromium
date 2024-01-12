// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_FADE_TRUNCATING_LABEL_H_
#define IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_FADE_TRUNCATING_LABEL_H_

#import <UIKit/UIKit.h>

/// A label which applies a fade-to-background color gradient to the trailing
/// end of the string if it is too large to fit the available area. It  uses the
/// attributedText property of UILabel to implement the fading. If
/// `numberOfLines` different than 1, the `lineBreakMode` is ignored.
@interface FadeTruncatingLabel : UILabel

/// Whether the text being displayed should be treated as a URL.
@property(nonatomic, assign) BOOL displayAsURL;

/// Line spacing used when drawing multiple lines.
@property(nonatomic, assign) CGFloat lineSpacing;

/// Aligns text to left or right depending on its direction (RTL/LTR). Setting
/// to NO reset `textAlignment` to NSTextAlignmentNatural. Defaults to NO.
@property(nonatomic, assign) BOOL textAlignmentFollowsTextDirection;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_FADE_TRUNCATING_LABEL_H_
