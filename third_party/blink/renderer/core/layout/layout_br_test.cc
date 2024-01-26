// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_br.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class LayoutBRTest : public RenderingTest {};

TEST_F(LayoutBRTest, TextMethods) {
  SetBodyInnerHTML("<br id=target style='-webkit-text-security:disc'>");
  const auto* br = DynamicTo<LayoutBR>(GetLayoutObjectByElementId("target"));

  EXPECT_TRUE(br->OriginalText().IsNull());
  EXPECT_EQ(String("\n"), br->TransformedText());
  EXPECT_EQ(String("\n"), br->PlainText());

  EXPECT_EQ(1u, br->ResolvedTextLength());
}

}  // namespace blink
