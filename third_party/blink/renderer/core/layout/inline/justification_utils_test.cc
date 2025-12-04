// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/justification_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class JustificationUtilsTest : public RenderingTest {};

TEST_F(JustificationUtilsTest, SetupItemJustificationOpportunityCrash) {
  SetBodyInnerHTML(R"HTML(
<style>
.CLASS2 {
  float: right;
  display: block;
  text-align: justify;
}
.CLASS6 {
  padding: 8%;
}
</style>
<ruby class="CLASS2">
<rt class="CLASS6">a</rt>
<svg></svg>
</ruby>
)HTML");
  // Pass if no crashes. We had a null dereference in InlineItem::Style().
}

}  // namespace blink
