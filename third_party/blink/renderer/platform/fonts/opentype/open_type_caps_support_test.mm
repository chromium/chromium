// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/opentype/open_type_caps_support.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/mac_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace blink {

void ensureHasNativeSmallCaps(const String& font_family_name) {
  sk_sp<SkTypeface> test_typeface =
      SkTypeface::MakeFromName(font_family_name.Utf8().c_str(), SkFontStyle());
  FontPlatformData font_platform_data(test_typeface, font_family_name.Utf8(),
                                      16, false, false);
  // System font names are magical. The family name of the system font in the
  // test below yields ".AppleSystemUIFont", which seems to be a generic role
  // name, because when it's actually instantiated with SkTypeface it ends up as
  // ".SF NS Text" or ".SF NS", depending on the system version. So if we try to
  // instantiate a magical system font, at least ensure that the resulting font
  // is magical as well.
  if (font_family_name[0] == '.')
    ASSERT_TRUE(font_platform_data.FontFamilyName()[0] == '.');
  else
    ASSERT_EQ(font_platform_data.FontFamilyName(), font_family_name);

  OpenTypeCapsSupport caps_support(font_platform_data.GetHarfBuzzFace(),
                                   FontDescription::FontVariantCaps::kSmallCaps,
                                   HB_SCRIPT_LATIN);
  // If caps_support.NeedsRunCaseSplitting() is true, this means that synthetic
  // upper-casing / lower-casing is required and the run needs to be segmented
  // by upper-case, lower-case properties. If it is false, it means that the
  // font feature can be used and no synthetic case-changing is needed.
  ASSERT_FALSE(caps_support.NeedsRunCaseSplitting());
}

TEST(OpenTypeCapsSupportTest, SmallCapsForMacAATFonts) {
  // The AAT fonts for testing are only available on macOS 10.13.
  if (!base::mac::IsAtLeastOS10_13())
    return;

  Vector<String> test_fonts = {
      [[NSFont systemFontOfSize:12] familyName],  // has OpenType small-caps
      "Apple Chancery",  // has old-style (feature id 3,"Letter Case")
                         // small-caps
      "Baskerville"};    // has new-style (feature id 38, "Upper Case")
                         // small-case.
  for (const auto& test_font : test_fonts)
    ensureHasNativeSmallCaps(test_font);
}

}  // namespace blink
