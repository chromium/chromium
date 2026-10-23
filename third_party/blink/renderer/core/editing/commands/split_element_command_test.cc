// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/split_element_command.h"

#include "third_party/blink/renderer/core/dom/id_target_observer.h"
#include "third_party/blink/renderer/core/dom/id_target_observer_registry.h"
#include "third_party/blink/renderer/core/editing/commands/editing_state.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/wtf/text/format.h"

namespace blink {

class SplitElementCommandTest : public EditingTestBase {};

namespace {

// Appends attributes to `element` while an id change is being dispatched,
// forcing its attribute storage to reallocate. This stands in for what
// production does on this path: the id notification updates style and layout,
// which synchronizes the source element's lazy style attribute and appends to
// the same storage. Either way, any reference into that storage held by the
// code dispatching the id change is invalidated.
class AttributeReallocatingIdObserver final : public IdTargetObserver {
 public:
  AttributeReallocatingIdObserver(IdTargetObserverRegistry& registry,
                                  const AtomicString& id,
                                  Element* element)
      : IdTargetObserver(registry, id), element_(element) {}

  void IdTargetChanged() override {
    // Enough appends to grow the attribute vector past its current capacity at
    // least once, which frees the buffer it was using.
    for (unsigned i = 0; i < 8; ++i) {
      element_->setAttribute(
          QualifiedName(g_null_atom, AtomicString(Format("{}pad", i)),
                        g_null_atom),
          AtomicString("1"));
    }
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(element_);
    IdTargetObserver::Trace(visitor);
  }

 private:
  Member<Element> element_;
};

}  // namespace

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

// Undoing a split restores the original element's id onto the element that the
// split took it from. Dispatching that id change can run code that reallocates
// the cloned element's attribute storage, so the undo must not hold a reference
// into that storage while it does so. Regression test for crbug.com/572546688.
//
// Only meaningful under ASan: without the fix this reads freed memory, which an
// unsanitized build will not notice.
TEST_F(SplitElementCommandTest, UndoRestoresIdWhileAttributesAreReallocated) {
  // The padding attributes put the element over AttributeVector's inline
  // capacity of 4, so that its attributes live in a heap buffer that appending
  // more attributes can reallocate.
  SetBodyContent(
      "<div contenteditable><blockquote id=split a=1 b=2 c=3 d=4 e=5>"
      "a<br>b<br></blockquote></div>");
  auto* div = To<ContainerNode>(GetDocument().body()->firstChild());
  auto* blockquote = To<Element>(div->firstChild());
  Node* at_child = blockquote->childNodes()->item(2);

  SimpleEditCommand* command =
      MakeGarbageCollected<SplitElementCommand>(blockquote, at_child);
  EditingState editing_state;
  command->DoApply(&editing_state);

  // The split moved the id onto a clone inserted before `blockquote`.
  auto* clone = To<Element>(div->firstChild());
  ASSERT_NE(clone, blockquote);
  ASSERT_EQ(AtomicString("split"),
            clone->FastGetAttribute(html_names::kIdAttr));
  ASSERT_TRUE(blockquote->FastGetAttribute(html_names::kIdAttr).IsNull());

  // Move the clone off the shared, immutable attribute storage it was cloned
  // with and onto a mutable, heap-allocated attribute vector.
  clone->setAttribute(
      QualifiedName(g_null_atom, AtomicString("f"), g_null_atom),
      AtomicString("6"));

  // Give `blockquote` an id of its own, so that restoring the original id
  // replaces a value rather than adding one.
  blockquote->setAttribute(html_names::kIdAttr, AtomicString("other"));

  // Restoring the id notifies this observer, which reallocates the clone's
  // attributes while the undo is still reading the id out of them.
  MakeGarbageCollected<AttributeReallocatingIdObserver>(
      GetDocument().EnsureIdTargetObserverRegistry(), AtomicString("split"),
      clone);

  command->DoUnapply();

  EXPECT_EQ(AtomicString("split"),
            blockquote->FastGetAttribute(html_names::kIdAttr));
  EXPECT_EQ(1u, div->CountChildren());
}

}  // namespace blink
