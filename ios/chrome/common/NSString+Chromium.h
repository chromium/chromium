// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_NSSTRING_CHROMIUM_H_
#define IOS_CHROME_COMMON_NSSTRING_CHROMIUM_H_

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"

/// NSString extension for converting strings to and from Chromium strings used
/// elsewhere. Equivalent to sys_string_conversions, but the syntax is
/// Swift-compatible and more ObjC-friendly.
@interface NSString (Chromium)

/// Use to convert std::string and string_view to NSString.
+ (instancetype)cr_fromString:(std::string_view)utf8;

/// Use to convert std::u16string and u16string_view to NSString.
+ (instancetype)cr_fromString16:(std::u16string_view)utf16;

/// Returns self converted to std::string. Equivalent to UTF8String.
@property(nonatomic, readonly) std::string cr_UTF8String;

/// Returns self converted to std::u16string. Equivalent to UTF16String.
@property(nonatomic, readonly) std::u16string cr_UTF16String;

@end

#endif  // IOS_CHROME_COMMON_NSSTRING_CHROMIUM_H_
