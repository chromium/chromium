// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/NSString+Chromium.h"

@implementation NSString (Chromium)

+ (instancetype)cr_fromString:(std::string_view)utf8 {
  return base::SysUTF8ToNSString(utf8);
}

+ (instancetype)cr_fromString16:(std::u16string_view)utf16 {
  return base::SysUTF16ToNSString(utf16);
}

- (std::string)cr_UTF8String {
  return base::SysNSStringToUTF8(self);
}

- (std::u16string)cr_UTF16String {
  return base::SysNSStringToUTF16(self);
}

@end
