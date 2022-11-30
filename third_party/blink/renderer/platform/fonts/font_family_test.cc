// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_family.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

FontFamily* CreateAndAppendFamily(FontFamily& parent,
                                  const char* family_name,
                                  FontFamily::Type family_type) {
  scoped_refptr<SharedFontFamily> family = SharedFontFamily::Create();
  family->SetFamily(family_name, family_type);
  parent.AppendFamily(family);
  return family.get();
}

}  // namespace

TEST(FontFamilyTest, CountNames) {
  {
    FontFamily family;
    EXPECT_EQ(1u, family.CountNames());
  }
  {
    FontFamily family;
    family.SetFamily("A", FontFamily::Type::kFamilyName);
    CreateAndAppendFamily(family, "B", FontFamily::Type::kFamilyName);
    EXPECT_EQ(2u, family.CountNames());
  }
  {
    FontFamily family;
    family.SetFamily("A", FontFamily::Type::kFamilyName);
    FontFamily* b_family =
        CreateAndAppendFamily(family, "B", FontFamily::Type::kFamilyName);
    CreateAndAppendFamily(*b_family, "C", FontFamily::Type::kFamilyName);
    EXPECT_EQ(3u, family.CountNames());
  }
}

TEST(FontFamilyTest, ToString) {
  {
    FontFamily family;
    EXPECT_EQ("", family.ToString());
  }
  {
    FontFamily family;
    family.SetFamily("A", FontFamily::Type::kFamilyName);
    CreateAndAppendFamily(family, "B", FontFamily::Type::kFamilyName);
    EXPECT_EQ("A, B", family.ToString());
  }
  {
    FontFamily family;
    family.SetFamily("A", FontFamily::Type::kFamilyName);
    FontFamily* b_family =
        CreateAndAppendFamily(family, "B", FontFamily::Type::kFamilyName);
    CreateAndAppendFamily(*b_family, "C", FontFamily::Type::kFamilyName);
    EXPECT_EQ("A, B, C", family.ToString());
  }
}

}  // namespace blink
