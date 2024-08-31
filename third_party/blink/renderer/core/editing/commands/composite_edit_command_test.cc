// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/composite_edit_command.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/keywords.h"

namespace blink {

namespace {

class SampleCommand final : public CompositeEditCommand {
 public:
  SampleCommand(Document&);

  void InsertNodeBefore(Node*,
                        Node* ref_child,
                        EditingState*,
                        ShouldAssumeContentIsAlwaysEditable =
                            kDoNotAssumeContentIsAlwaysEditable);
  void InsertNodeAfter(Node*, Node*, EditingState*);

  void MoveParagraphContentsToNewBlockIfNecessary(const Position&,
                                                  EditingState*);
  void MoveParagraphs(const VisiblePosition& start_of_paragraph_to_move,
                      const VisiblePosition& end_of_paragraph_to_move,
                      const VisiblePosition& destination,
                      EditingState* editing_state);

  // CompositeEditCommand member implementations
  void DoApply(EditingState*) final {}
  InputEvent::InputType GetInputType() const final {
    return InputEvent::InputType::kNone;
  }
};

SampleCommand::SampleCommand(Document& document)
    : CompositeEditCommand(document) {}

void SampleCommand::InsertNodeBefore(
    Node* insert_child,
    Node* ref_child,
    EditingState* editing_state,
    ShouldAssumeContentIsAlwaysEditable
        should_assume_content_is_always_editable) {
  CompositeEditCommand::InsertNodeBefore(
      insert_child, ref_child, editing_state,
      should_assume_content_is_always_editable);
}

void SampleCommand::InsertNodeAfter(Node* insert_child,
                                    Node* ref_child,
                                    EditingState* editing_state) {
  CompositeEditCommand::InsertNodeAfter(insert_child, ref_child, editing_state);
}

void SampleCommand::MoveParagraphContentsToNewBlockIfNecessary(
    const Position& position,
    EditingState* editing_state) {
  CompositeEditCommand::MoveParagraphContentsToNewBlockIfNecessary(
      position, editing_state);
}

void SampleCommand::MoveParagraphs(
    const VisiblePosition& start_of_paragraph_to_move,
    const VisiblePosition& end_of_paragraph_to_move,
    const VisiblePosition& destination,
    EditingState* editing_state) {
  CompositeEditCommand::MoveParagraphs(start_of_paragraph_to_move,
                                       end_of_paragraph_to_move, destination,
                                       editing_state);
}

}  // namespace

class CompositeEditCommandTest : public EditingTestBase {};

TEST_F(CompositeEditCommandTest, insertNodeBefore) {
  SetBodyContent("<div contenteditable><b></b></div>");
  SampleCommand& sample = *MakeGarbageCollected<SampleCommand>(GetDocument());
  Node* insert_child = GetDocument().createTextNode("foo");
  Element* ref_child = GetDocument().QuerySelector(AtomicString("b"));
  Element* div = GetDocument().QuerySelector(AtomicString("div"));

  EditingState editing_state;
  sample.InsertNodeBefore(insert_child, ref_child, &editing_state);
  EXPECT_FALSE(editing_state.IsAborted());
  EXPECT_EQ("foo<b></b>", div->innerHTML());
}

TEST_F(CompositeEditCommandTest, insertNodeBeforeInUneditable) {
  SetBodyContent("<div><b></b></div>");
  SampleCommand& sample = *MakeGarbageCollected<SampleCommand>(GetDocument());
  Node* insert_child = GetDocument().createTextNode("foo");
  Element* ref_child = GetDocument().QuerySelector(AtomicString("b"));

  EditingState editing_state;
  sample.InsertNodeBefore(insert_child, ref_child, &editing_state);
  EXPECT_TRUE(editing_state.IsAborted());
}

TEST_F(CompositeEditCommandTest, insertNodeBeforeDisconnectedNode) {
  SetBodyContent("<div><b></b></div>");
  SampleCommand& sample = *MakeGarbageCollected<SampleCommand>(GetDocument());
  Node* insert_child = GetDocument().createTextNode("foo");
  Element* ref_child = GetDocument().QuerySelector(AtomicString("b"));
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  div->remove();

  EditingState editing_state;
  sample.InsertNodeBefore(insert_child, ref_child, &editing_state);
  EXPECT_FALSE(editing_state.IsAborted());
  EXPECT_EQ("<b></b>", div->innerHTML())
      << "InsertNodeBeforeCommand does nothing for disconnected node";
}

TEST_F(CompositeEditCommandTest, insertNodeBeforeWithDirtyLayoutTree) {
  SetBodyContent("<div><b></b></div>");
  SampleCommand& sample = *MakeGarbageCollected<SampleCommand>(GetDocument());
  Node* insert_child = GetDocument().createTextNode("foo");
  Element* ref_child = GetDocument().QuerySelector(AtomicString("b"));
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  div->setAttribute(html_names::kContenteditableAttr, keywords::kTrue);

  EditingState editing_state;
  sample.InsertNodeBefore(insert_child, ref_child, &editing_state);
  EXPECT_FALSE(editing_state.IsAborted());
  EXPECT_EQ("foo<b></b>", div->innerHTML());
}

TEST_F(CompositeEditCommandTest,
       MoveParagraphContentsToNewBlockWithNonEditableStyle) {
  SetBodyContent(
      "<style>div{-webkit-user-modify:read-only;user-select:none;}</style>"
      "foo");
  SampleCommand& sample = *MakeGarbageCollected<SampleCommand>(GetDocument());
  Element* body = GetDocument().body();
  Node* text = body->lastChild();
  body->setAttribute(html_names::kContenteditableAttr, keywords::kTrue);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  EditingState editing_state;
  sample.MoveParagraphContentsToNewBlockIfNecessary(Position(text, 0),
                                                    &editing_state);
  EXPECT_TRUE(editing_state.IsAborted());
  EXPECT_EQ(
      "<div><br></div>"
      "<style>div{-webkit-user-modify:read-only;user-select:none;}</style>"
      "foo",
      body->innerHTML());
}

TEST_F(CompositeEditCommandTest,
       MoveParagraphContentsToNewBlockWithUAShadowDOM1) {
  SetBodyContent("<object contenteditable><input></object>");
  base::RunLoop().RunUntilIdle();

  SampleCommand& sample = *MakeGarbageCollected<SampleCommand>(GetDocument());
  Element* input = GetDocument().QuerySelector(AtomicString("input"));
  Position pos = Position::BeforeNode(*input);
  EditingState editing_state;

  // Should not crash
  sample.MoveParagraphContentsToNewBlockIfNecessary(pos, &editing_state);
  EXPECT_FALSE(editing_state.IsAborted());
  EXPECT_EQ("<object contenteditable=\"\"><div><input></div></object>",
            GetDocument().body()->innerHTML());
}

TEST_F(CompositeEditCommandTest,
       MoveParagraphContentsToNewBlockWithUAShadowDOM2) {
  GetDocument().setDesignMode("on");
  SetBodyContent("<span></span><button><meter></meter></button>");

  SampleCommand& sample = *MakeGarbageCollected<SampleCommand>(GetDocument());
  Element* button = GetDocument().QuerySelector(AtomicString("button"));
  Position pos = Position(button, 0);
  EditingState editing_state;

  // Should not crash
  sample.MoveParagraphContentsToNewBlockIfNecessary(pos, &editing_state);
  EXPECT_FALSE(editing_state.IsAborted());
  EXPECT_EQ("<div></div><span></span><button><meter></meter></button>",
            GetDocument().body()->innerHTML());
}

TEST_F(CompositeEditCommandTest,
       MoveParagraphContentsToNewBlockWithButtonAndBr) {
  GetDocument().setDesignMode("on");
  InsertStyleElement("br { content: 'x'; }");
  SetBodyContent("<button><br></button>");

  SampleCommand& sample = *MakeGarbageCollected<SampleCommand>(GetDocument());
  Element* button = GetDocument().QuerySelector(AtomicString("button"));
  Position pos = Position(button, 0);
  EditingState editing_state;

  // Should not crash
  sample.MoveParagraphContentsToNewBlockIfNecessary(pos, &editing_state);
  EXPECT_FALSE(editing_state.IsAborted());
  EXPECT_EQ("<button><div><br></div><br></button>",
            GetDocument().body()->innerHTML());
}

TEST_F(CompositeEditCommandTest, InsertNodeOnDisconnectedParent) {
  SetBodyContent("<p><b></b></p>");
  SampleCommand& sample = *MakeGarbageCollected<SampleCommand>(GetDocument());
  Node* insert_child = GetDocument().QuerySelector(AtomicString("b"));
  Element* ref_child = GetDocument().QuerySelector(AtomicString("p"));
  ref_child->remove();
  EditingState editing_state_before;
  // editing state should abort here.
  sample.InsertNodeBefore(insert_child, ref_child, &editing_state_before);
  EXPECT_TRUE(editing_state_before.IsAborted());

  EditingState editing_state_after;
  // editing state should abort here.
  sample.InsertNodeAfter(insert_child, ref_child, &editing_state_after);
  EXPECT_TRUE(editing_state_after.IsAborted());
}

TEST_F(CompositeEditCommandTest, MoveParagraphsWithBr) {
  SetBodyContent("<ol><li><span><br></span></li></ol><br>");

  EditingState editing_state;
  SampleCommand& sample = *MakeGarbageCollected<SampleCommand>(GetDocument());
  Element* li = GetDocument().QuerySelector(AtomicString("li"));
  Element* br1 = GetDocument().QuerySelector(AtomicString("ol br"));
  Element* br2 = GetDocument().QuerySelector(AtomicString("ol + br"));
  br1->setTextContent("x");
  UpdateAllLifecyclePhasesForTest();

  // The start precedes the end, but when using MostFor/BackwardCaretPosition
  // to constrain the range, the resulting end would precede the start.
  const VisiblePosition& start = VisiblePosition::FirstPositionInNode(*li);
  const VisiblePosition& end = VisiblePosition::LastPositionInNode(*li);
  const VisiblePosition& destination = VisiblePosition::BeforeNode(*br2);
  EXPECT_EQ(start.DeepEquivalent(), Position::BeforeNode(*br1));
  EXPECT_EQ(end.DeepEquivalent(), Position(br1, 0));
  EXPECT_EQ(destination.DeepEquivalent(), Position::BeforeNode(*br2));
  EXPECT_LT(start.DeepEquivalent(), end.DeepEquivalent());
  EXPECT_GT(MostForwardCaretPosition(start.DeepEquivalent()),
            MostBackwardCaretPosition(end.DeepEquivalent()));

  // Should not crash
  sample.MoveParagraphs(start, end, destination, &editing_state);
  EXPECT_FALSE(editing_state.IsAborted());
  EXPECT_EQ("<ol><li><span><br></span></li></ol><br>",
            GetDocument().body()->innerHTML());
}

TEST_F(CompositeEditCommandTest, MoveParagraphsWithInlineBlocks) {
  InsertStyleElement("span {display: inline-block; width: 0; height: 10px}");
  SetBodyContent("<div><span></span><span></span>&#x20;</div><br>");

  EditingState editing_state;
  SampleCommand& sample = *MakeGarbageCollected<SampleCommand>(GetDocument());
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  Element* span1 = GetDocument().QuerySelector(AtomicString("span"));
  Element* span2 = GetDocument().QuerySelector(AtomicString("span + span"));
  Element* br = GetDocument().QuerySelector(AtomicString("br"));

  // The start precedes the end, but when using MostFor/BackwardCaretPosition
  // to constrain the range, the resulting end would precede the start.
  const VisiblePosition& start = VisiblePosition::FirstPositionInNode(*div);
  const VisiblePosition& end = VisiblePosition::LastPositionInNode(*div);
  const VisiblePosition& destination = VisiblePosition::BeforeNode(*br);
  EXPECT_EQ(start.DeepEquivalent(), Position::BeforeNode(*span1));
  EXPECT_EQ(end.DeepEquivalent(), Position::BeforeNode(*span2));
  EXPECT_EQ(destination.DeepEquivalent(), Position::BeforeNode(*br));
  EXPECT_LT(start.DeepEquivalent(), end.DeepEquivalent());
  EXPECT_GT(MostForwardCaretPosition(start.DeepEquivalent()),
            MostBackwardCaretPosition(end.DeepEquivalent()));

  // Should not crash
  sample.MoveParagraphs(start, end, destination, &editing_state);
  EXPECT_FALSE(editing_state.IsAborted());
  EXPECT_EQ("<div><span></span><span></span> </div><br>",
            GetDocument().body()->innerHTML());
}

TEST_F(CompositeEditCommandTest, MoveParagraphsWithTableAndCaption) {
  Document& document = GetDocument();
  document.setDesignMode("on");
  InsertStyleElement(
      "table { writing-mode: vertical-lr; }"
      "caption { appearance: radio; }");
  SetBodyInnerHTML("<table><caption><div><br></div><input></caption></table>");

  EditingState editing_state;
  SampleCommand& sample = *MakeGarbageCollected<SampleCommand>(GetDocument());
  Element* br = document.QuerySelector(AtomicString("br"));
  Element* input = document.QuerySelector(AtomicString("input"));

  const VisiblePosition& start = VisiblePosition::FirstPositionInNode(*input);
  const VisiblePosition& end = VisiblePosition::AfterNode(*input);
  const VisiblePosition& destination = VisiblePosition::BeforeNode(*br);
  EXPECT_EQ(start.DeepEquivalent(), Position::BeforeNode(*input));
  EXPECT_EQ(end.DeepEquivalent(), Position::AfterNode(*input));
  EXPECT_EQ(destination.DeepEquivalent(), Position::BeforeNode(*br));

  // Should not crash. See http://crbug.com/1310613
  sample.MoveParagraphs(start, end, destination, &editing_state);
  EXPECT_FALSE(editing_state.IsAborted());
  EXPECT_EQ("<table><caption><div><input></div></caption></table>",
            GetDocument().body()->innerHTML());
}

TEST_F(CompositeEditCommandTest,
       MoveParagraphContentsToNewBlockWithNullVisiblePosition1) {
  EditingState editing_state;
  Document& document = GetDocument();
  Element* body = document.body();
  document.setDesignMode("on");
  SetBodyInnerHTML("<div contenteditable=false><br></div>");
  SampleCommand& sample = *MakeGarbageCollected<SampleCommand>(GetDocument());

  // Should not crash. See http://crbug.com/1351899
  sample.MoveParagraphContentsToNewBlockIfNecessary(Position(body, 0),
                                                    &editing_state);
  EXPECT_TRUE(editing_state.IsAborted());
  EXPECT_EQ("<div contenteditable=\"false\"><br></div>", body->innerHTML());
}

TEST_F(CompositeEditCommandTest,
       MoveParagraphContentsToNewBlockWithNullVisiblePosition2) {
  EditingState editing_state;
  Document& document = GetDocument();
  Element* body = document.body();
  document.setDesignMode("on");
  InsertStyleElement("div, input {-webkit-user-modify: read-only}");
  SetBodyInnerHTML("<input>");
  SampleCommand& sample = *MakeGarbageCollected<SampleCommand>(GetDocument());

  // Should not crash. See http://crbug.com/1351899
  sample.MoveParagraphContentsToNewBlockIfNecessary(Position(body, 0),
                                                    &editing_state);
  EXPECT_TRUE(editing_state.IsAborted());
  EXPECT_EQ("<div><br></div><input>", body->innerHTML());
}

}  // namespace blink
