// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"

namespace blink {

class NGFragmentItemsTest : public NGLayoutTest {};

// crbug.com/1147357
// DirtyLinesFromNeedsLayout() didn't work well with an orthogonal writing-mode
// root as a child, and it caused a failure of OOF descendants propagation.
TEST_F(NGFragmentItemsTest,
       DirtyLinesFromNeedsLayoutWithOrthogonalWritingMode) {
  SetBodyInnerHTML(R"HTML(
<style>
button {
  font-size: 100px;
}
#span1 {
  position: absolute;
}
code {
  writing-mode: vertical-rl;
}
</style>
<rt id="rt1"><span id="span1"></span></rt>
<button>
<code><ruby id="ruby1"></ruby></code>
b AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
</button>)HTML");
  RunDocumentLifecycle();

  GetDocument().getElementById("ruby1")->appendChild(
      GetDocument().getElementById("rt1"));
  RunDocumentLifecycle();

  EXPECT_TRUE(GetLayoutObjectByElementId("span1")->EverHadLayout());
}

}  // namespace blink
