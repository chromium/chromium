// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_pending_image.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

TEST(StylePendingImageTest, IsEqual) {
  using css_test_helpers::ParseValue;
  test::TaskEnvironment task_environment_;
  ScopedNullExecutionContext execution_context;
  auto* document =
      Document::CreateForTest(execution_context.GetExecutionContext());
  const CSSValue* value1 = ParseValue(*document, "<image>", "url('#a')");
  const CSSValue* value2 = ParseValue(*document, "<image>", "url('#a')");
  const CSSValue* value3 = ParseValue(*document, "<image>", "url('#b')");
  ASSERT_TRUE(value1);
  ASSERT_TRUE(value2);
  ASSERT_TRUE(value3);
  EXPECT_EQ(*value1, *value2);
  EXPECT_NE(*value1, *value3);
  auto* pending1 = MakeGarbageCollected<StylePendingImage>(*value1);
  auto* pending2 = MakeGarbageCollected<StylePendingImage>(*value2);
  auto* pending3 = MakeGarbageCollected<StylePendingImage>(*value3);
  EXPECT_EQ(*pending1, *pending2);
  EXPECT_NE(*pending1, *pending3);
}

}  // namespace blink
