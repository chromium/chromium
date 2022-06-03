// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/unicode_util.h"


#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace unicode_util {
namespace {
// Character ranges for characters with R or AL bidirectionality.
// http://www.ietf.org/rfc/rfc3454.txt
const NSUInteger kRTLRangeCount = 34;
unichar kRTLUnicodeRanges[kRTLRangeCount][2] = {
    {0x05BE, 0x05BE}, {0x05C0, 0x05C0}, {0x05C3, 0x05C3}, {0x05D0, 0x05EA},
    {0x05F0, 0x05F4}, {0x061B, 0x061B}, {0x061F, 0x061F}, {0x0621, 0x063A},
    {0x0640, 0x064A}, {0x066D, 0x066F}, {0x0671, 0x06D5}, {0x06DD, 0x06DD},
    {0x06E5, 0x06E6}, {0x06FA, 0x06FE}, {0x0700, 0x070D}, {0x0710, 0x0710},
    {0x0712, 0x072C}, {0x0780, 0x07A5}, {0x07B1, 0x07B1}, {0x200F, 0x200F},
    {0xFB1D, 0xFB1D}, {0xFB1F, 0xFB28}, {0xFB2A, 0xFB36}, {0xFB38, 0xFB3C},
    {0xFB3E, 0xFB3E}, {0xFB40, 0xFB41}, {0xFB43, 0xFB44}, {0xFB46, 0xFBB1},
    {0xFBD3, 0xFD3D}, {0xFD50, 0xFD8F}, {0xFD92, 0xFDC7}, {0xFDF0, 0xFDFC},
    {0xFE70, 0xFE74}, {0xFE76, 0xFEFC}};
// Character ranges for characters with L bidirectionality.
// http://www.ietf.org/rfc/rfc3454.txt
const NSUInteger kLTRRangeCount = 17;
unichar kLTRUnicodeRanges[kLTRRangeCount][2] = {
    {0x0041, 0x005A}, {0x0061, 0x007A}, {0x00AA, 0x00AA}, {0x00B5, 0x00B5},
    {0x00BA, 0x00BA}, {0x00C0, 0x00D6}, {0x00D8, 0x00F6}, {0x00F8, 0x0220},
    {0x0222, 0x0233}, {0x0250, 0x02AD}, {0x02B0, 0x02B8}, {0x02BB, 0x02C1},
    {0x02D0, 0x02D1}, {0x02E0, 0x02E4}, {0x02EE, 0x02EE}, {0x037A, 0x037A},
    {0x0386, 0x0386}};

// Returns the character set created from the unicode value ranges in
// |kRTLUnicodeRanges|.
NSCharacterSet* GetRTLCharSet() {
  static NSCharacterSet* g_rtl_charset = nil;
  static dispatch_once_t rtl_once_token;
  dispatch_once(&rtl_once_token, ^{
    NSMutableCharacterSet* rtl_charset = [[NSMutableCharacterSet alloc] init];
    for (NSUInteger range_idx = 0; range_idx < kRTLRangeCount; ++range_idx) {
      unichar range_begin = kRTLUnicodeRanges[range_idx][0];
      unichar range_end = kRTLUnicodeRanges[range_idx][1];
      NSRange rtl_range = NSMakeRange(range_begin, range_end + 1 - range_begin);
      [rtl_charset addCharactersInRange:rtl_range];
    }
    g_rtl_charset = rtl_charset;
  });
  return g_rtl_charset;
}

// Returns the character set created from the unicode value ranges in
// |kLTRUnicodeRanges|.
NSCharacterSet* GetLTRCharSet() {
  static NSCharacterSet* g_ltr_charset = nil;
  static dispatch_once_t ltr_once_token;
  dispatch_once(&ltr_once_token, ^{
    NSMutableCharacterSet* ltr_charset = [[NSMutableCharacterSet alloc] init];
    for (NSUInteger range_idx = 0; range_idx < kLTRRangeCount; ++range_idx) {
      unichar range_begin = kLTRUnicodeRanges[range_idx][0];
      unichar range_end = kLTRUnicodeRanges[range_idx][1];
      NSRange ltr_range = NSMakeRange(range_begin, range_end + 1 - range_begin);
      [ltr_charset addCharactersInRange:ltr_range];
    }
    g_ltr_charset = ltr_charset;
  });
  return g_ltr_charset;
}
}  // namespace

bool IsCharRTL(unichar c) {
  return [GetRTLCharSet() characterIsMember:c];
}

bool IsCharLTR(unichar c) {
  return [GetLTRCharSet() characterIsMember:c];
}

NSWritingDirection UnicodeWritingDirectionForString(NSString* string) {
  for (NSUInteger char_idx = 0; char_idx < string.length; ++char_idx) {
    unichar c = [string characterAtIndex:char_idx];
    if (IsCharRTL(c))
      return NSWritingDirectionRightToLeft;
    if (IsCharLTR(c))
      return NSWritingDirectionLeftToRight;
  }
  return NSWritingDirectionNatural;
}

NSString* GetEscapedUnicodeStringForString(NSString* string) {
  NSMutableString* unicode_string = [NSMutableString string];
  for (NSUInteger i = 0; i < string.length; ++i) {
    unichar c = [string characterAtIndex:i];
    // unichars are 16-bit unsigned integers, and when printed out using the %x
    // string format, it only uses the minimum number of digits necessary to
    // express the character value.  However, constructing NSString literals
    // using Unicode character codes requires that each character have 4 hex
    // digits following the "\u".  |zero_prefix| is constructed such that it
    // ensures each character to have 4 digits.
    NSString* zero_prefix = @"";
    if (c < 0x0010)
      zero_prefix = @"000";
    else if (c < 0x0100)
      zero_prefix = @"00";
    else if (c < 0x1000)
      zero_prefix = @"0";
    [unicode_string appendFormat:@"\\u%@%x", zero_prefix, c];
  }
  return unicode_string;
}

}  // namespace unicode_util
