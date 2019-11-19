// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class LayoutCountTest : public RenderingTest {};

TEST_F(LayoutCountTest, SimpleBlockLayoutIsOnePass) {
  ScopedTrackLayoutPassesPerBlockForTest track_layout_passes_per_block(true);
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      " <div id='block' style='height:1000px'>Item</div>");

  auto* block = To<LayoutBlockFlow>(
      GetDocument().getElementById("block")->GetLayoutObject());
  ASSERT_EQ(block->GetLayoutPassCountForTesting(), 1);
}

}  // namespace blink
