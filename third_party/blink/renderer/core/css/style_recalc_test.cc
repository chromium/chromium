// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_recalc.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class StyleRecalcTest : public PageTestBase {};

TEST_F(StyleRecalcTest, SuppressRecalc) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .foo { color: green; }
    </style>
    <div id=element></div>
  )HTML");

  Element* element = GetDocument().getElementById("element");
  ASSERT_TRUE(element);
  element->classList().Add("foo");

  EXPECT_TRUE(StyleRecalcChange().ShouldRecalcStyleFor(*element));
  EXPECT_FALSE(
      StyleRecalcChange().SuppressRecalc().ShouldRecalcStyleFor(*element));
  // The flag should be lost when ForChildren is called.
  EXPECT_TRUE(StyleRecalcChange()
                  .SuppressRecalc()
                  .ForChildren(*element)
                  .ShouldRecalcStyleFor(*element));
}

}  // namespace blink
