// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UTIL_UNICODE_UTIL_H_
#define IOS_CHROME_BROWSER_UI_UTIL_UNICODE_UTIL_H_

#import <UIKit/UIKit.h>

namespace unicode_util {

// Returns true if `c` is a character with R or AL bidirectionality as specified
// by RFC 3454.
bool IsCharRTL(unichar c);

// Returns true if `c` is a character with L bidirectionality as specified by
// RFC 3454.
bool IsCharLTR(unichar c);

// Returns the writing direction for `string` based on rules P2 and P3 of the
// Unicode BiDi Algorithm.  Returns NSWritingDirectionNatural for strings that
// don't contain any bidirectionality information.
NSWritingDirection UnicodeWritingDirectionForString(NSString* string);

// Converts `string`'s characters to their unicode encodings (i.e. "\u" followed
// by 4 hex digits).  This is useful for creating string constants for RTL
// languages without inducing RTL text layout when viewed in a text editor.
NSString* GetEscapedUnicodeStringForString(NSString* string);

}  // namespace unicode_util

#endif  // IOS_CHROME_BROWSER_UI_UTIL_UNICODE_UTIL_H_
