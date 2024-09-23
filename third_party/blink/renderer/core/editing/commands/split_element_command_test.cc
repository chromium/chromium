// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/split_element_command.h"

#include "third_party/blink/renderer/core/editing/commands/editing_state.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"

namespace blink {

class SplitElementCommandTest : public EditingTestBase {};

// Tests that SplitElementCommand works. It splits an element at the
// passed child.
TEST_F(SplitElementCommandTest, Basic) {
  const char* body_content =
      "<div contenteditable><blockquote>a<br>b<br></blockquote></div>";
  SetBodyContent(body_content);
  auto* div = To<ContainerNode>(GetDocument().body()->firstChild());
  Node* blockquote = div->firstChild();
  Node* at_child = blockquote->childNodes()->item(2);

  // <blockquote> has 4 children.
  EXPECT_EQ(4u, blockquote->CountChildren());

  // Execute SplitElementCommand with <blockquote> at the 2nd text, 'b'.
  // Before the command execution,
  //  DIV (editable)
  //    BLOCKQUOTE (editable)
  //      #text "a"
  //      BR (editable)
  //      #text "b"  <- This is `at_child`.
  //      BR (editable)
  SimpleEditCommand* command = MakeGarbageCollected<SplitElementCommand>(
      To<Element>(blockquote), at_child);
  EditingState editingState;
  command->DoApply(&editingState);

  // After the command execution,
  //  DIV (editable)
  //    BLOCKQUOTE (editable)
  //      #text "a"
  //      BR (editable)
  //    BLOCKQUOTE (editable)
  //      #text "b"
  //      BR (editable)

  Node* firstChildAfterSplit = div->firstChild();
  // Ensure that it creates additional <blockquote>.
  EXPECT_NE(blockquote, firstChildAfterSplit);
  EXPECT_EQ(2u, div->CountChildren());
  EXPECT_EQ(2u, firstChildAfterSplit->CountChildren());
  EXPECT_EQ(2u, blockquote->CountChildren());

  // Test undo
  command->DoUnapply();
  EXPECT_EQ(1u, div->CountChildren());
  blockquote = div->firstChild();
  EXPECT_EQ(4u, blockquote->CountChildren());

  // Test redo
  command->DoReapply();
  EXPECT_EQ(2u, div->CountChildren());
  Node* firstChildAfterRedo = div->firstChild();
  EXPECT_NE(blockquote, firstChildAfterRedo);
  EXPECT_EQ(2u, firstChildAfterRedo->CountChildren());
  EXPECT_EQ(2u, blockquote->CountChildren());
}

// Tests that SplitElementCommand doesn't insert a cloned element
// when it doesn't have any children.
TEST_F(SplitElementCommandTest, NotCloneElementWithoutChildren) {
  const char* body_content =
      "<div contenteditable><blockquote>a<br>b<br></blockquote></div>";
  SetBodyContent(body_content);
  auto* div = To<ContainerNode>(GetDocument().body()->firstChild());
  Node* blockquote = div->firstChild();
  Node* at_child = blockquote->firstChild();

  // <blockquote> has 4 children.
  EXPECT_EQ(4u, blockquote->CountChildren());

  // Execute SplitElementCommand with <blockquote> at the first child.
  // Before the command execution,
  //  DIV (editable)
  //    BLOCKQUOTE (editable)
  //      #text "a" <- This is `at_child`.
  //      BR (editable)
  //      #text "b"
  //      BR (editable)
  SimpleEditCommand* command = MakeGarbageCollected<SplitElementCommand>(
      To<Element>(blockquote), at_child);
  EditingState editingState;
  command->DoApply(&editingState);

  // After the command execution, the tree is not changed since it doesn't have
  // anything to split.
  //  DIV (editable)
  //    BLOCKQUOTE (editable)
  //      #text "a"
  //      BR (editable)
  //      #text "b"
  //      BR (editable)

  Node* firstChildAfterSplit = div->firstChild();
  // Ensure that it doesn't create additional <blockquote>.
  EXPECT_EQ(blockquote, firstChildAfterSplit);
  EXPECT_EQ(1u, div->CountChildren());
  EXPECT_EQ(4u, firstChildAfterSplit->CountChildren());

  // Test undo
  command->DoUnapply();
  EXPECT_EQ(1u, div->CountChildren());
  blockquote = div->firstChild();
  EXPECT_EQ(4u, blockquote->CountChildren());

  // Test redo
  command->DoReapply();
  EXPECT_EQ(1u, div->CountChildren());
  Node* firstChildAfterRedo = div->firstChild();
  EXPECT_EQ(blockquote, firstChildAfterRedo);
  EXPECT_EQ(4u, firstChildAfterRedo->CountChildren());
}

}  // namespace blink
