// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/cssom/css_unparsed_value.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(CSSUnparsedValueTest, FromCustomPropertyDeclarationWithCSSWideKeyword) {
  const auto* initial_value =
      MakeGarbageCollected<CSSCustomPropertyDeclaration>("--var",
                                                         CSSValueID::kInitial);
  const auto* unparsed_value = CSSUnparsedValue::FromCSSValue(*initial_value);
  ASSERT_NE(nullptr, unparsed_value);
  ASSERT_EQ(1U, unparsed_value->length());

  const auto& item =
      unparsed_value->AnonymousIndexedGetter(0, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(item.IsString());
  EXPECT_EQ("initial", item.GetAsString());
}

}  // namespace blink
