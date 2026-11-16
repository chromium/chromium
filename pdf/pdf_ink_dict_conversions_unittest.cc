// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_dict_conversions.h"

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace chrome_pdf {

TEST(PdfInkDictConversionsTest, GetColorFromDict) {
  EXPECT_EQ(SK_ColorBLACK, GetColorFromDict(base::test::ParseJsonDict(
                               R"({"color": {"r": 0, "g": 0, "b": 0}})")));
  EXPECT_EQ(SK_ColorWHITE,
            GetColorFromDict(base::test::ParseJsonDict(
                R"({"color": {"r": 255, "g": 255, "b": 255}})")));
  EXPECT_EQ(SkColorSetRGB(18, 164, 175),
            GetColorFromDict(base::test::ParseJsonDict(
                R"({"color": {"r": 18, "g": 164, "b": 175}})")));
}

TEST(PdfInkDictConversionsTest, SkColorToDict) {
  EXPECT_THAT(SkColorToDict(SK_ColorBLACK),
              base::test::IsJson(R"({"r": 0, "g": 0, "b": 0})"));
  EXPECT_THAT(SkColorToDict(SK_ColorWHITE),
              base::test::IsJson(R"({"r": 255, "g": 255, "b": 255})"));
  EXPECT_THAT(SkColorToDict(SkColorSetRGB(18, 164, 175)),
              base::test::IsJson(R"({"r": 18, "g": 164, "b": 175})"));
  EXPECT_THAT(SkColorToDict(SkColorSetARGB(128, 10, 20, 30)),
              base::test::IsJson(R"({"r": 10, "g": 20, "b": 30})"));
}

}  // namespace chrome_pdf
