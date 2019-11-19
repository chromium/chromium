// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/split_text_node_command.h"

#include "third_party/blink/renderer/core/editing/commands/editing_state.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/plain_text_range.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"

namespace blink {

class SplitTextNodeCommandTest : public EditingTestBase {};

TEST_F(SplitTextNodeCommandTest, splitInMarkerInterior) {
  SetBodyContent("<div contenteditable>test1 test2 test3</div>");

  auto* div = To<ContainerNode>(GetDocument().body()->firstChild());

  EphemeralRange range = PlainTextRange(0, 5).CreateRange(*div);
  GetDocument().Markers().AddTextMatchMarker(
      range, TextMatchMarker::MatchStatus::kInactive);

  range = PlainTextRange(6, 11).CreateRange(*div);
  GetDocument().Markers().AddTextMatchMarker(
      range, TextMatchMarker::MatchStatus::kInactive);

  range = PlainTextRange(12, 17).CreateRange(*div);
  GetDocument().Markers().AddTextMatchMarker(
      range, TextMatchMarker::MatchStatus::kInactive);

  SimpleEditCommand* command = MakeGarbageCollected<SplitTextNodeCommand>(
      To<Text>(GetDocument().body()->firstChild()->firstChild()), 8);

  EditingState editingState;
  command->DoApply(&editingState);

  const Text& text1 = To<Text>(*div->firstChild());
  const Text& text2 = To<Text>(*text1.nextSibling());

  // The first marker should end up in text1, the second marker should be
  // truncated and end up text1, the third marker should end up in text2
  // and its offset shifted to remain on the same piece of text

  EXPECT_EQ(2u, GetDocument().Markers().MarkersFor(text1).size());

  EXPECT_EQ(0u, GetDocument().Markers().MarkersFor(text1)[0]->StartOffset());
  EXPECT_EQ(5u, GetDocument().Markers().MarkersFor(text1)[0]->EndOffset());

  EXPECT_EQ(6u, GetDocument().Markers().MarkersFor(text1)[1]->StartOffset());
  EXPECT_EQ(7u, GetDocument().Markers().MarkersFor(text1)[1]->EndOffset());

  EXPECT_EQ(1u, GetDocument().Markers().MarkersFor(text2).size());

  EXPECT_EQ(4u, GetDocument().Markers().MarkersFor(text2)[0]->StartOffset());
  EXPECT_EQ(9u, GetDocument().Markers().MarkersFor(text2)[0]->EndOffset());

  // Test undo
  command->DoUnapply();

  const Text& text = To<Text>(*div->firstChild());

  EXPECT_EQ(3u, GetDocument().Markers().MarkersFor(text).size());

  EXPECT_EQ(0u, GetDocument().Markers().MarkersFor(text)[0]->StartOffset());
  EXPECT_EQ(5u, GetDocument().Markers().MarkersFor(text)[0]->EndOffset());

  // TODO(rlanday): the truncated marker that spanned node boundaries is not
  // restored properly
  EXPECT_EQ(6u, GetDocument().Markers().MarkersFor(text)[1]->StartOffset());
  EXPECT_EQ(7u, GetDocument().Markers().MarkersFor(text)[1]->EndOffset());

  EXPECT_EQ(12u, GetDocument().Markers().MarkersFor(text)[2]->StartOffset());
  EXPECT_EQ(17u, GetDocument().Markers().MarkersFor(text)[2]->EndOffset());

  // Test redo
  command->DoReapply();

  const Text& text3 = To<Text>(*div->firstChild());
  const Text& text4 = To<Text>(*text3.nextSibling());

  EXPECT_EQ(2u, GetDocument().Markers().MarkersFor(text3).size());

  EXPECT_EQ(0u, GetDocument().Markers().MarkersFor(text3)[0]->StartOffset());
  EXPECT_EQ(5u, GetDocument().Markers().MarkersFor(text3)[0]->EndOffset());

  EXPECT_EQ(6u, GetDocument().Markers().MarkersFor(text3)[1]->StartOffset());
  EXPECT_EQ(7u, GetDocument().Markers().MarkersFor(text3)[1]->EndOffset());

  EXPECT_EQ(1u, GetDocument().Markers().MarkersFor(text4).size());

  EXPECT_EQ(4u, GetDocument().Markers().MarkersFor(text4)[0]->StartOffset());
  EXPECT_EQ(9u, GetDocument().Markers().MarkersFor(text4)[0]->EndOffset());
}

}  // namespace blink
