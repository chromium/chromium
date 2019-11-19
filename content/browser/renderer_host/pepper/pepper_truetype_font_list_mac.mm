// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_truetype_font_list.h"

#import <Cocoa/Cocoa.h>

#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "ppapi/c/dev/ppb_truetype_font_dev.h"
#include "ppapi/proxy/serialized_structs.h"

namespace content {

namespace {

// Table to map AppKit weights to Pepper ones.
const PP_TrueTypeFontWeight_Dev kPepperFontWeights[] = {
    PP_TRUETYPEFONTWEIGHT_THIN,         // 0 is the minimum AppKit weight.
    PP_TRUETYPEFONTWEIGHT_ULTRALIGHT,
    PP_TRUETYPEFONTWEIGHT_ULTRALIGHT,
    PP_TRUETYPEFONTWEIGHT_LIGHT,
    PP_TRUETYPEFONTWEIGHT_LIGHT,
    PP_TRUETYPEFONTWEIGHT_NORMAL,       // 5 is a 'normal' AppKit weight.
    PP_TRUETYPEFONTWEIGHT_MEDIUM,
    PP_TRUETYPEFONTWEIGHT_MEDIUM,
    PP_TRUETYPEFONTWEIGHT_SEMIBOLD,
    PP_TRUETYPEFONTWEIGHT_BOLD,         // 9 is a 'bold' AppKit weight.
    PP_TRUETYPEFONTWEIGHT_ULTRABOLD,
    PP_TRUETYPEFONTWEIGHT_HEAVY,
};
const NSInteger kPepperFontWeightsLength = base::size(kPepperFontWeights);

}  // namespace

void GetFontFamilies_SlowBlocking(std::vector<std::string>* font_families) {
  @autoreleasepool {
    NSFontManager* fontManager = [[[NSFontManager alloc] init] autorelease];
    NSArray* fonts = [fontManager availableFontFamilies];
    font_families->reserve([fonts count]);
    for (NSString* family_name in fonts)
      font_families->push_back(base::SysNSStringToUTF8(family_name));
  }
}

void GetFontsInFamily_SlowBlocking(
    const std::string& family,
    std::vector<ppapi::proxy::SerializedTrueTypeFontDesc>* fonts_in_family) {
  @autoreleasepool {
    NSFontManager* fontManager = [[[NSFontManager alloc] init] autorelease];
    NSString* ns_family = base::SysUTF8ToNSString(family);
    NSArray* ns_fonts_in_family =
        [fontManager availableMembersOfFontFamily:ns_family];

    for (NSArray* font_info in ns_fonts_in_family) {
      ppapi::proxy::SerializedTrueTypeFontDesc desc;
      desc.family = family;
      NSInteger font_weight = [[font_info objectAtIndex:2] intValue];
      font_weight = std::max(static_cast<NSInteger>(0), font_weight);
      font_weight = std::min(kPepperFontWeightsLength - 1, font_weight);
      desc.weight = kPepperFontWeights[font_weight];

      NSFontTraitMask font_traits =
          [[font_info objectAtIndex:3] unsignedIntValue];
      desc.style = PP_TRUETYPEFONTSTYLE_NORMAL;
      if (font_traits & NSItalicFontMask)
        desc.style = PP_TRUETYPEFONTSTYLE_ITALIC;

      desc.width = PP_TRUETYPEFONTWIDTH_NORMAL;
      if (font_traits & NSCondensedFontMask)
        desc.width = PP_TRUETYPEFONTWIDTH_CONDENSED;
      else if (font_traits & NSExpandedFontMask)
        desc.width = PP_TRUETYPEFONTWIDTH_EXPANDED;

      // Mac doesn't support requesting non-default character sets.
      desc.charset = PP_TRUETYPEFONTCHARSET_DEFAULT;

      fonts_in_family->push_back(desc);
    }
  }
}

}  // namespace content
