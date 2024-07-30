// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/marker_range_mapping_context.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/markers/text_fragment_marker.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class MarkerRangeMappingContextTest : public RenderingTest {
 public:
  MarkerRangeMappingContextTest()
      : RenderingTest(MakeGarbageCollected<EmptyLocalFrameClient>()) {}
};

TEST_F(MarkerRangeMappingContextTest, FullNodeOffsetsCorrect) {
  // Laid out as
  //   a b c d e f g h i
  //   j k l m n o p q r
  //
  // Two fragments:
  //   DOM offsets (9,26), (27,44)
  //   Text offsets (0,17), (0,17)
  SetBodyInnerHTML(R"HTML(
    <div style="width:100px;">
        a b c d e f g h i j k l m n o p q r
    </div>
  )HTML");

  auto* div_node =
      To<HTMLDivElement>(GetDocument().QuerySelector(AtomicString("div")));
  ASSERT_TRUE(div_node->firstChild()->IsTextNode());
  auto* text_node = To<Text>(div_node->firstChild());
  ASSERT_TRUE(text_node);

  const TextOffsetRange fragment_range = {9, 26};
  MarkerRangeMappingContext mapping_context(*text_node, fragment_range);

  TextFragmentMarker* marker_pre =
      MakeGarbageCollected<TextFragmentMarker>(1, 5);  // Before text
  auto offsets = mapping_context.GetTextContentOffsets(*marker_pre);
  ASSERT_FALSE(offsets.has_value());
  offsets.reset();

  TextFragmentMarker* marker_a =
      MakeGarbageCollected<TextFragmentMarker>(7, 10);  // Partially before
  offsets = mapping_context.GetTextContentOffsets(*marker_a);
  ASSERT_TRUE(offsets.has_value());
  ASSERT_EQ(0u, offsets->start);
  ASSERT_EQ(1u, offsets->end);
  offsets.reset();

  TextFragmentMarker* marker_b =
      MakeGarbageCollected<TextFragmentMarker>(11, 12);  // 'b'
  offsets = mapping_context.GetTextContentOffsets(*marker_b);
  ASSERT_TRUE(offsets.has_value());
  ASSERT_EQ(2u, offsets->start);
  ASSERT_EQ(3u, offsets->end);
  offsets.reset();

  TextFragmentMarker* marker_ij = MakeGarbageCollected<TextFragmentMarker>(
      25, 28);  // Overlaps 1st and 2nd line
  offsets = mapping_context.GetTextContentOffsets(*marker_ij);
  ASSERT_TRUE(offsets.has_value());
  ASSERT_EQ(16u, offsets->start);
  ASSERT_EQ(17u, offsets->end);
  offsets.reset();

  TextFragmentMarker* marker_post =
      MakeGarbageCollected<TextFragmentMarker>(30, 35);  // After text
  offsets = mapping_context.GetTextContentOffsets(*marker_post);
  ASSERT_FALSE(offsets.has_value());
  offsets.reset();
}

}  // namespace blink
