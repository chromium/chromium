// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/font_list.h"

#import <Cocoa/Cocoa.h>

#include <utility>

#include "base/strings/sys_string_conversions.h"
#include "base/values.h"

namespace content {

std::unique_ptr<base::ListValue> GetFontList_SlowBlocking() {
  @autoreleasepool {
    std::unique_ptr<base::ListValue> font_list(new base::ListValue);
    NSFontManager* fontManager = [[[NSFontManager alloc] init] autorelease];
    NSMutableDictionary* fonts_dict = [NSMutableDictionary dictionary];
    NSArray* fonts = [fontManager availableFontFamilies];

    for (NSString* family_name in fonts) {
      NSString* localized_family_name =
          [fontManager localizedNameForFamily:family_name face:nil];
      fonts_dict[family_name] = localized_family_name;
    }

    // Sort family names based on localized names.
    NSArray* sortedFonts = [fonts_dict
        keysSortedByValueUsingSelector:@selector(localizedStandardCompare:)];

    for (NSString* family_name in sortedFonts) {
      NSString* localized_family_name = fonts_dict[family_name];
      auto font_item = std::make_unique<base::ListValue>();
      font_item->AppendString(base::SysNSStringToUTF16(family_name));
      font_item->AppendString(base::SysNSStringToUTF16(localized_family_name));
      font_list->Append(std::move(font_item));
    }

    return font_list;
  }
}

}  // namespace content
