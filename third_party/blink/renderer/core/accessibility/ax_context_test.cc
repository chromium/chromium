// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/accessibility/ax_context.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "ui/accessibility/ax_mode.h"

namespace blink {
namespace test {

class AXContextTest : public RenderingTest {};

TEST_F(AXContextTest, AXContextCreatesAXObjectCache) {
  SetBodyInnerHTML(R"HTML(<p>Hello, world</p>)HTML");

  EXPECT_FALSE(GetDocument().ExistingAXObjectCache());
  auto context =
      std::make_unique<AXContext>(GetDocument(), ui::kAXModeComplete);
  EXPECT_TRUE(GetDocument().ExistingAXObjectCache());
  context.reset();
  EXPECT_FALSE(GetDocument().ExistingAXObjectCache());
}

TEST_F(AXContextTest, AXContextSetsAXMode) {
  SetBodyInnerHTML(R"HTML(<p>Hello, world</p>)HTML");

  constexpr ui::AXMode mode_1 = ui::AXMode::kWebContents;
  constexpr ui::AXMode mode_2 = ui::AXMode::kScreenReader;
  ui::AXMode mode_combined = mode_1;
  mode_combined |= mode_2;

  EXPECT_FALSE(GetDocument().ExistingAXObjectCache());

  // Create a context with mode_1.
  auto context_1 = std::make_unique<AXContext>(GetDocument(), mode_1);
  EXPECT_TRUE(GetDocument().ExistingAXObjectCache());
  EXPECT_EQ(mode_1, GetDocument().ExistingAXObjectCache()->GetAXMode());

  // Create a context with mode_2. The AXObjectCache should now use the
  // logical OR of both modes.
  auto context_2 = std::make_unique<AXContext>(GetDocument(), mode_2);
  EXPECT_TRUE(GetDocument().ExistingAXObjectCache());
  EXPECT_EQ(mode_combined, GetDocument().ExistingAXObjectCache()->GetAXMode());

  // Remove the first context, check that we just get mode_2 active now.
  context_1.reset();
  EXPECT_TRUE(GetDocument().ExistingAXObjectCache());
  EXPECT_EQ(mode_2, GetDocument().ExistingAXObjectCache()->GetAXMode());

  // Remove the second context and the AXObjectCache should go away.
  context_2.reset();
  EXPECT_FALSE(GetDocument().ExistingAXObjectCache());
}

}  // namespace test
}  // namespace blink
