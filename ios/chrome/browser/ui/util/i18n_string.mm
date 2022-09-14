// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/util/i18n_string.h"

#import <string>

#import "base/i18n/rtl.h"
#import "base/strings/sys_string_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* AdjustStringForLocaleDirection(NSString* text) {
  std::u16string converted_text = base::SysNSStringToUTF16(text);
  bool has_changed =
      base::i18n::AdjustStringForLocaleDirection(&converted_text);
  if (has_changed) {
    return base::SysUTF16ToNSString(converted_text);
  }
  return [text copy];
}
