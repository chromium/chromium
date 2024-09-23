// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_family.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(FontFamilyTest, ToString) {
  {
    FontFamily family;
    EXPECT_EQ("", family.ToString());
  }
  {
    scoped_refptr<SharedFontFamily> b = SharedFontFamily::Create(
        AtomicString("B"), FontFamily::Type::kFamilyName);
    FontFamily family(AtomicString("A"), FontFamily::Type::kFamilyName,
                      std::move(b));
    EXPECT_EQ("A, B", family.ToString());
  }
  {
    scoped_refptr<SharedFontFamily> c = SharedFontFamily::Create(
        AtomicString("C"), FontFamily::Type::kFamilyName);
    scoped_refptr<SharedFontFamily> b = SharedFontFamily::Create(
        AtomicString("B"), FontFamily::Type::kFamilyName, std::move(c));
    FontFamily family(AtomicString("A"), FontFamily::Type::kFamilyName,
                      std::move(b));
    EXPECT_EQ("A, B, C", family.ToString());
  }
}

}  // namespace blink
