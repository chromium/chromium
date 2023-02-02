// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UTIL_ATTRIBUTED_STRING_UTIL_H_
#define IOS_CHROME_BROWSER_UI_UTIL_ATTRIBUTED_STRING_UTIL_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

/// Add attributes to a copy of `attributedString`.
NSAttributedString* AttributedStringCopyWithAttributes(
    NSAttributedString* attributedString,
    NSLineBreakMode lineBreakMode,
    NSTextAlignment textAlignment,
    BOOL forceLeftToRight);

/// Returns the maximum number of lines used by `attributedString` when drawing
/// with limited `width`.
NSInteger NumberOfLinesOfAttributedString(NSAttributedString* attributedString,
                                          CGFloat limitedWidth);

#endif  // IOS_CHROME_BROWSER_UI_UTIL_ATTRIBUTED_STRING_UTIL_H_
