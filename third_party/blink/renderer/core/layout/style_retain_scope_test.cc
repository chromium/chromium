// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/style_retain_scope.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

namespace blink {

TEST(StyleRetainScopeTest, Current) {
  EXPECT_EQ(StyleRetainScope::Current(), nullptr);
  {
    StyleRetainScope scope;
    EXPECT_EQ(StyleRetainScope::Current(), &scope);
    {
      StyleRetainScope scope2;
      EXPECT_EQ(StyleRetainScope::Current(), &scope2);
    }
    EXPECT_EQ(StyleRetainScope::Current(), &scope);
  }
  EXPECT_EQ(StyleRetainScope::Current(), nullptr);
}

TEST(StyleRetainScopeTest, Retain) {
  scoped_refptr<const ComputedStyle> style = ComputedStyle::Create();
  EXPECT_TRUE(style->HasOneRef());
  {
    StyleRetainScope scope;
    scope.Retain(*style);

    EXPECT_FALSE(style->HasOneRef());
    EXPECT_TRUE(style->HasAtLeastOneRef());
  }
  EXPECT_TRUE(style->HasOneRef());
}

}  // namespace blink
