// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/frame_selection.h"

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_range.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_caret.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/selection_controller.h"
#include "third_party/blink/renderer/core/editing/selection_modifier.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/page/context_menu_controller.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/testing/fake_display_item_client.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace blink {

using testing::IsNull;

class FrameSelectionTest : public EditingTestBase {
 public:
  FrameSelectionTest()
      : root_paint_property_client_(
            MakeGarbageCollected<FakeDisplayItemClient>("root")),
        root_paint_chunk_id_(root_paint_property_client_->Id(),
                             DisplayItem::kUninitializedType) {}
  Persistent<FakeDisplayItemClient> root_paint_property_client_;
  PaintChunk::Id root_paint_chunk_id_;

 protected:
  VisibleSelection VisibleSelectionInDOMTree() const {
    return Selection().ComputeVisibleSelectionInDOMTree();
  }
  VisibleSelectionInFlatTree GetVisibleSelectionInFlatTree() const {
    return Selection().ComputeVisibleSelectionInFlatTree();
  }

  Text* AppendTextNode(const String& data);

  PositionWithAffinity CaretPosition() const {
    return Selection().frame_caret_->CaretPosition();
  }

  Page& GetPage() const { return GetDummyPageHolder().GetPage(); }

  // Returns if a word is is selected.
  bool SelectWordAroundPosition(const Position&);

  // Returns whether the selection was accomplished.
  bool SelectWordAroundCaret();

  // Returns whether the selection was accomplished.
  bool SelectSentenceAroundCaret();

  // Places the caret on the |text| at |selection_index|.
  void ResetAndPlaceCaret(Text* text, size_t selection_index) {
    ASSERT_LE(selection_index,
              static_cast<size_t>(std::numeric_limits<int>::max()));
    Selection().SetSelection(
        SelectionInDOMTree::Builder()
            .Collapse(Position(text, static_cast<int>(selection_index)))
            .Build(),
        SetSelectionOptions());
  }

  // Returns whether a context menu is being displayed.
  bool HasContextMenu() {
    return GetDocument()
        .GetPage()
        ->GetContextMenuController()
        .ContextMenuNodeForFrame(GetDocument().GetFrame());
  }

  void MoveRangeSelectionInternal(const Position& base,
                                  const Position& extent,
                                  TextGranularity granularity) {
    Selection().MoveRangeSelectionInternal(
        SelectionInDOMTree::Builder().SetBaseAndExtent(base, extent).Build(),
        granularity);
  }

 private:
  Persistent<Text> text_node_;
};

Text* FrameSelectionTest::AppendTextNode(const String& data) {
  Text* text = GetDocument().createTextNode(data);
  GetDocument().body()->AppendChild(text);
  return text;
}

bool FrameSelectionTest::SelectWordAroundPosition(const Position& position) {
  Selection().SetSelection(
      SelectionInDOMTree::Builder().Collapse(position).Build(),
      SetSelectionOptions());
  return Selection().SelectWordAroundCaret();
}

bool FrameSelectionTest::SelectWordAroundCaret() {
  return Selection().SelectAroundCaret(TextGranularity::kWord,
                                       HandleVisibility::kNotVisible,
                                       ContextMenuVisibility::kNotVisible);
}

bool FrameSelectionTest::SelectSentenceAroundCaret() {
  return Selection().SelectAroundCaret(TextGranularity::kSentence,
                                       HandleVisibility::kNotVisible,
                                       ContextMenuVisibility::kNotVisible);
}

TEST_F(FrameSelectionTest, FirstEphemeralRangeOf) {
  SetBodyContent("<div id=sample>0123456789</div>abc");
  Element* const sample = GetDocument().getElementById(AtomicString("sample"));
  Node* const text = sample->firstChild();
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .SetBaseAndExtent(EphemeralRange(
                                   Position(text, 3), Position(text, 6)))
                               .Build(),
                           SetSelectionOptions());
  sample->setAttribute(html_names::kStyleAttr, AtomicString("display:none"));
  // Move |VisibleSelection| before "abc".
  UpdateAllLifecyclePhasesForTest();
  const EphemeralRange& range =
      FirstEphemeralRangeOf(Selection().ComputeVisibleSelectionInDOMTree());
  EXPECT_EQ(Position(sample->nextSibling(), 0), range.StartPosition())
      << "firstRange() should return current selection value";
  EXPECT_EQ(Position(sample->nextSibling(), 0), range.EndPosition());
}

TEST_F(FrameSelectionTest, SetValidSelection) {
  Text* text = AppendTextNode("Hello, World!");
  UpdateAllLifecyclePhasesForTest();
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 0), Position(text, 5))
          .Build(),
      SetSelectionOptions());
  EXPECT_FALSE(Selection().ComputeVisibleSelectionInDOMTree().IsNone());
}

#define EXPECT_EQ_SELECTED_TEXT(text) \
  EXPECT_EQ(text, Selection().SelectedText().Utf8())

TEST_F(FrameSelectionTest, SelectWordAroundCaret) {
  // "Foo Bar  Baz,"
  Text* text = AppendTextNode("Foo Bar&nbsp;&nbsp;Baz,");
  UpdateAllLifecyclePhasesForTest();

  // "Fo|o Bar  Baz,"
  EXPECT_TRUE(SelectWordAroundPosition(Position(text, 2)));
  EXPECT_EQ_SELECTED_TEXT("Foo");
  // "Foo| Bar  Baz,"
  EXPECT_TRUE(SelectWordAroundPosition(Position(text, 3)));
  EXPECT_EQ_SELECTED_TEXT("Foo");
  // "Foo Bar | Baz,"
  EXPECT_FALSE(SelectWordAroundPosition(Position(text, 13)));
  // "Foo Bar  Baz|,"
  EXPECT_TRUE(SelectWordAroundPosition(Position(text, 22)));
  EXPECT_EQ_SELECTED_TEXT("Baz");
}

// crbug.com/657996
TEST_F(FrameSelectionTest, SelectWordAroundCaret2) {
  SetBodyContent(
      "<p style='width:70px; font-size:14px'>foo bar<em>+</em> baz</p>");
  // "foo bar
  //  b|az"
  Node* const baz = GetDocument().body()->firstChild()->lastChild();
  EXPECT_TRUE(SelectWordAroundPosition(Position(baz, 2)));
  EXPECT_EQ_SELECTED_TEXT("baz");
}

TEST_F(FrameSelectionTest, SelectAroundCaret_Word) {
  Text* text = AppendTextNode("This is a sentence.");
  UpdateAllLifecyclePhasesForTest();

  // Beginning of text: |This is a sentence.
  ResetAndPlaceCaret(text, strlen(""));
  EXPECT_TRUE(SelectWordAroundCaret());
  EXPECT_EQ_SELECTED_TEXT("This");

  // Beginning of a word: This |is a sentence.
  ResetAndPlaceCaret(text, strlen("This "));
  EXPECT_TRUE(SelectWordAroundCaret());
  EXPECT_EQ_SELECTED_TEXT("is");

  // Somewhere in a word: This is a s|entence.
  ResetAndPlaceCaret(text, strlen("This is a s"));
  EXPECT_TRUE(SelectWordAroundCaret());
  EXPECT_EQ_SELECTED_TEXT("sentence");

  // End a word: This| is a sentence.
  ResetAndPlaceCaret(text, strlen("This"));
  EXPECT_TRUE(SelectWordAroundCaret());
  EXPECT_EQ_SELECTED_TEXT("This");

  // End a word with punctuation: This is a sentence|.
  ResetAndPlaceCaret(text, strlen("This is a sentence"));
  EXPECT_TRUE(SelectWordAroundCaret());
  EXPECT_EQ_SELECTED_TEXT("sentence");

  // End a word after punctuation: This is a sentence.|
  ResetAndPlaceCaret(text, strlen("This is a sentence."));
  EXPECT_FALSE(SelectWordAroundCaret());
  EXPECT_EQ_SELECTED_TEXT("");

  // Beginning of a symbol: Some emojis |😀 🍀.
  text = AppendTextNode(String::FromUTF8("Some emojis 😀 🍀."));
  UpdateAllLifecyclePhasesForTest();
  ResetAndPlaceCaret(text, String::FromUTF8("Some emojis ").length());
  EXPECT_TRUE(SelectWordAroundCaret());
  EXPECT_EQ_SELECTED_TEXT(" 😀");

  // End of a symbol: Some emojis 😀| 🍀.
  ResetAndPlaceCaret(text, String::FromUTF8("Some emojis 😀").length());
  EXPECT_TRUE(SelectWordAroundCaret());
  EXPECT_EQ_SELECTED_TEXT("😀");
}

TEST_F(FrameSelectionTest, SelectAroundCaret_Sentence) {
  Text* text = AppendTextNode(
      "This is the first sentence. This is the second sentence. This is the "
      "last sentence.");
  UpdateAllLifecyclePhasesForTest();

  // This is the first sentence. Th|is is the second sentence. This is the last
  // sentence.
  ResetAndPlaceCaret(text, strlen("This is the first sentence. Th"));
  EXPECT_TRUE(SelectSentenceAroundCaret());
  EXPECT_EQ_SELECTED_TEXT("This is the second sentence.");

  // This is the first sentence|. This is the second sentence. This is the last
  // sentence.
  ResetAndPlaceCaret(text, strlen("This is the first sentence"));
  EXPECT_TRUE(SelectSentenceAroundCaret());
  EXPECT_EQ_SELECTED_TEXT("This is the first sentence.");

  // This is the first sentence.| This is the second sentence. This is the last
  // sentence.
  ResetAndPlaceCaret(text, strlen("This is the first sentence."));
  EXPECT_TRUE(SelectSentenceAroundCaret());
  EXPECT_EQ_SELECTED_TEXT(
      "This is the first sentence. This is the second sentence.");

  // This is the first sentence. |This is the second sentence. This is the last
  // sentence.
  ResetAndPlaceCaret(text, strlen("This is the first sentence. "));
  EXPECT_TRUE(SelectSentenceAroundCaret());
  EXPECT_EQ_SELECTED_TEXT(
      "This is the first sentence. This is the second sentence.");

  // This is the first sentence. T|his is the second sentence. This is the last
  // sentence.
  ResetAndPlaceCaret(text, strlen("This is the first sentence. T"));
  EXPECT_TRUE(SelectSentenceAroundCaret());
  EXPECT_EQ_SELECTED_TEXT("This is the second sentence.");
}

TEST_F(FrameSelectionTest, SelectAroundCaret_ShouldShowHandle) {
  Text* text = AppendTextNode("This is a sentence.");
  int selection_index = 12;  // This is a se|ntence.
  UpdateAllLifecyclePhasesForTest();

  // Test that handles are never visible if the the handle_visibility param is
  // set to not visible, regardless of the other params.
  ResetAndPlaceCaret(text, selection_index);
  EXPECT_TRUE(Selection().SelectAroundCaret(
      TextGranularity::kSentence, HandleVisibility::kNotVisible,
      ContextMenuVisibility::kNotVisible));
  EXPECT_FALSE(Selection().IsHandleVisible());

  ResetAndPlaceCaret(text, selection_index);
  EXPECT_TRUE(Selection().SelectAroundCaret(
      TextGranularity::kWord, HandleVisibility::kNotVisible,
      ContextMenuVisibility::kNotVisible));
  EXPECT_FALSE(Selection().IsHandleVisible());

  ResetAndPlaceCaret(text, selection_index);
  EXPECT_TRUE(Selection().SelectAroundCaret(TextGranularity::kSentence,
                                            HandleVisibility::kNotVisible,
                                            ContextMenuVisibility::kVisible));
  EXPECT_FALSE(Selection().IsHandleVisible());

  ResetAndPlaceCaret(text, selection_index);
  EXPECT_TRUE(Selection().SelectAroundCaret(TextGranularity::kWord,
                                            HandleVisibility::kNotVisible,
                                            ContextMenuVisibility::kVisible));
  EXPECT_FALSE(Selection().IsHandleVisible());

  // Make sure handles are always visible when the handle_visiblity param is
  // set to visible, regardless of the other parameters.
  ResetAndPlaceCaret(text, selection_index);
  EXPECT_TRUE(Selection().SelectAroundCaret(
      TextGranularity::kSentence, HandleVisibility::kVisible,
      ContextMenuVisibility::kNotVisible));
  EXPECT_TRUE(Selection().IsHandleVisible());

  ResetAndPlaceCaret(text, selection_index);
  EXPECT_TRUE(Selection().SelectAroundCaret(
      TextGranularity::kWord, HandleVisibility::kVisible,
      ContextMenuVisibility::kNotVisible));
  EXPECT_TRUE(Selection().IsHandleVisible());
}

TEST_F(FrameSelectionTest, SelectAroundCaret_ShouldShowContextMenu) {
  Text* text = AppendTextNode("This is a sentence.");
  int selection_index = 12;  // This is a se|ntence.
  UpdateAllLifecyclePhasesForTest();

  // Test that the context menu is never visible if the context_menu_visibility
  // param is set to not visible, regardless of the other params.
  ResetAndPlaceCaret(text, selection_index);
  EXPECT_TRUE(Selection().SelectAroundCaret(
      TextGranularity::kSentence, HandleVisibility::kNotVisible,
      ContextMenuVisibility::kNotVisible));
  EXPECT_FALSE(HasContextMenu());

  ResetAndPlaceCaret(text, selection_index);
  EXPECT_TRUE(Selection().SelectAroundCaret(
      TextGranularity::kSentence, HandleVisibility::kVisible,
      ContextMenuVisibility::kNotVisible));
  EXPECT_FALSE(HasContextMenu());

  ResetAndPlaceCaret(text, selection_index);
  EXPECT_TRUE(Selection().SelectAroundCaret(
      TextGranularity::kWord, HandleVisibility::kNotVisible,
      ContextMenuVisibility::kNotVisible));
  EXPECT_FALSE(HasContextMenu());

  ResetAndPlaceCaret(text, selection_index);
  EXPECT_TRUE(Selection().SelectAroundCaret(
      TextGranularity::kWord, HandleVisibility::kVisible,
      ContextMenuVisibility::kNotVisible));
  EXPECT_FALSE(HasContextMenu());

  // Make sure the context menu is always visible when the
  // context_menu_visibility param is set to visible, regardless of the other
  // parameters.
  ResetAndPlaceCaret(text, selection_index);
  EXPECT_TRUE(Selection().SelectAroundCaret(TextGranularity::kSentence,
                                            HandleVisibility::kNotVisible,
                                            ContextMenuVisibility::kVisible));
  EXPECT_TRUE(HasContextMenu());

  ResetAndPlaceCaret(text, selection_index);
  EXPECT_TRUE(Selection().SelectAroundCaret(TextGranularity::kSentence,
                                            HandleVisibility::kVisible,
                                            ContextMenuVisibility::kVisible));
  EXPECT_TRUE(HasContextMenu());

  ResetAndPlaceCaret(text, selection_index);
  EXPECT_TRUE(Selection().SelectAroundCaret(TextGranularity::kWord,
                                            HandleVisibility::kNotVisible,
                                            ContextMenuVisibility::kVisible));
  EXPECT_TRUE(HasContextMenu());

  ResetAndPlaceCaret(text, selection_index);
  EXPECT_TRUE(Selection().SelectAroundCaret(TextGranularity::kWord,
                                            HandleVisibility::kVisible,
                                            ContextMenuVisibility::kVisible));
  EXPECT_TRUE(HasContextMenu());
}

TEST_F(FrameSelectionTest, GetSelectionRangeAroundCaret_Word) {
  Text* text = AppendTextNode("This is a sentence.");
  UpdateAllLifecyclePhasesForTest();

  // Beginning of a text: |This is a sentence.
  ResetAndPlaceCaret(text, strlen(""));
  EphemeralRange range = Selection().GetWordSelectionRangeAroundCaret();
  EXPECT_EQ("This", PlainText(range));

  // Beginning of a word: This |is a sentence.
  ResetAndPlaceCaret(text, strlen("This "));
  range = Selection().GetWordSelectionRangeAroundCaret();
  EXPECT_EQ("is", PlainText(range));

  // Somewhere in a word: This is a s|entence.
  ResetAndPlaceCaret(text, strlen("This is a s"));
  range = Selection().GetWordSelectionRangeAroundCaret();
  EXPECT_EQ("sentence", PlainText(range));

  // End a word: This| is a sentence.
  ResetAndPlaceCaret(text, strlen("This"));
  range = Selection().GetWordSelectionRangeAroundCaret();
  EXPECT_EQ("This", PlainText(range));

  // End a word before punctuation: This is a sentence|.
  ResetAndPlaceCaret(text, strlen("This is a sentence"));
  range = Selection().GetWordSelectionRangeAroundCaret();
  EXPECT_EQ("sentence", PlainText(range));

  // End of text after punctuation (no selection): This is a sentence.|
  ResetAndPlaceCaret(text, strlen("This is a sentence."));
  range = Selection().GetWordSelectionRangeAroundCaret();
  EXPECT_EQ("", PlainText(range));

  // End of text without punctuation: This is a sentence|
  ResetAndPlaceCaret(text, strlen("This is a sentence"));
  range = Selection().GetWordSelectionRangeAroundCaret();
  EXPECT_EQ("sentence", PlainText(range));

  // After punctuation before whitespace (no selection): A word.| Another.
  text = AppendTextNode("A word. Another.");
  UpdateAllLifecyclePhasesForTest();
  ResetAndPlaceCaret(text, strlen("A word."));
  range = Selection().GetWordSelectionRangeAroundCaret();
  EXPECT_EQ("", PlainText(range));

  // Beginning of a symbol: Some emojis |😀 🍀.
  text = AppendTextNode(String::FromUTF8("Some emojis 😀 🍀."));
  UpdateAllLifecyclePhasesForTest();
  ResetAndPlaceCaret(text, String::FromUTF8("Some emojis ").length());
  range = Selection().GetWordSelectionRangeAroundCaret();
  EXPECT_EQ(String::FromUTF8(" 😀"), PlainText(range));

  // End of a symbol: Some emojis 😀| 🍀.
  ResetAndPlaceCaret(text, String::FromUTF8("Some emojis 😀").length());
  range = Selection().GetWordSelectionRangeAroundCaret();
  EXPECT_EQ(String::FromUTF8("😀"), PlainText(range));
}

TEST_F(FrameSelectionTest, GetSelectionRangeAroundCaret_Sentence) {
  Text* text = AppendTextNode(
      "This is the first sentence. This is the second sentence. This is the "
      "last sentence.");
  UpdateAllLifecyclePhasesForTest();

  // |This is the first sentence. This is the second sentence. This is the last
  // sentence.
  ResetAndPlaceCaret(text, strlen(""));
  EphemeralRange range = Selection().GetSelectionRangeAroundCaretForTesting(
      TextGranularity::kSentence);
  EXPECT_EQ("This is the first sentence.", PlainText(range));

  // This is the first sentence|. This is the second sentence. This is the last
  // sentence.
  ResetAndPlaceCaret(text, strlen("This is the first sentence"));
  range = Selection().GetSelectionRangeAroundCaretForTesting(
      TextGranularity::kSentence);
  EXPECT_EQ("This is the first sentence.", PlainText(range));

  // TODO(crbug.com/1273856): This should only select one sentence.
  // This is the first sentence.| This is the second sentence. This is the last
  // sentence.
  ResetAndPlaceCaret(text, strlen("This is the first sentence."));
  range = Selection().GetSelectionRangeAroundCaretForTesting(
      TextGranularity::kSentence);
  EXPECT_EQ("This is the first sentence. This is the second sentence.",
            PlainText(range));

  // TODO(crbug.com/1273856): This should only select one sentence.
  // This is the first sentence. |This is the second sentence. This is the last
  // sentence.
  ResetAndPlaceCaret(text, strlen("This is the first sentence. "));
  range = Selection().GetSelectionRangeAroundCaretForTesting(
      TextGranularity::kSentence);
  EXPECT_EQ("This is the first sentence. This is the second sentence.",
            PlainText(range));

  // This is the first sentence. Th|is is the second sentence. This is the last
  // sentence.
  ResetAndPlaceCaret(text, strlen("This is the first sentence. Th"));
  range = Selection().GetSelectionRangeAroundCaretForTesting(
      TextGranularity::kSentence);
  EXPECT_EQ("This is the second sentence.", PlainText(range));

  // This is the first sentence. This is the second sentence. This is the last
  // sentence|.
  ResetAndPlaceCaret(text,
                     strlen("This is the first sentence. This is the second "
                            "sentence. This is the last sentence"));
  range = Selection().GetSelectionRangeAroundCaretForTesting(
      TextGranularity::kSentence);
  EXPECT_EQ("This is the last sentence.", PlainText(range));

  // This is the first sentence. This is the second sentence. This is the last
  // sentence.|
  ResetAndPlaceCaret(text,
                     strlen("This is the first sentence. This is the second "
                            "sentence. This is the last sentence."));
  range = Selection().GetSelectionRangeAroundCaretForTesting(
      TextGranularity::kSentence);
  EXPECT_EQ("This is the last sentence.", PlainText(range));
}

TEST_F(FrameSelectionTest, ModifyExtendWithFlatTree) {
  SetBodyContent("<span id=host></span>one");
  SetShadowContent("two<slot></slot>", "host");
  Element* host = GetDocument().getElementById(AtomicString("host"));
  Node* const two = FlatTreeTraversal::FirstChild(*host);
  // Select "two" for selection in DOM tree
  // Select "twoone" for selection in Flat tree
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .Collapse(ToPositionInDOMTree(PositionInFlatTree(host, 0)))
          .Extend(
              ToPositionInDOMTree(PositionInFlatTree(GetDocument().body(), 2)))
          .Build(),
      SetSelectionOptions());
  Selection().Modify(SelectionModifyAlteration::kExtend,
                     SelectionModifyDirection::kForward, TextGranularity::kWord,
                     SetSelectionBy::kSystem);
  EXPECT_EQ(Position(two, 0), VisibleSelectionInDOMTree().Start());
  EXPECT_EQ(Position(two, 3), VisibleSelectionInDOMTree().End());
  EXPECT_EQ(PositionInFlatTree(two, 0),
            GetVisibleSelectionInFlatTree().Start());
  EXPECT_EQ(PositionInFlatTree(two, 3), GetVisibleSelectionInFlatTree().End());
}

TEST_F(FrameSelectionTest, ModifyWithUserTriggered) {
  SetBodyContent("<div id=sample>abc</div>");
  Element* sample = GetDocument().getElementById(AtomicString("sample"));
  const Position end_of_text(sample->firstChild(), 3);
  Selection().SetSelection(
      SelectionInDOMTree::Builder().Collapse(end_of_text).Build(),
      SetSelectionOptions());

  EXPECT_FALSE(Selection().Modify(
      SelectionModifyAlteration::kMove, SelectionModifyDirection::kForward,
      TextGranularity::kCharacter, SetSelectionBy::kSystem))
      << "Selection.modify() returns false for non-user-triggered call when "
         "selection isn't modified.";
  EXPECT_EQ(end_of_text, Selection().ComputeVisibleSelectionInDOMTree().Start())
      << "Selection isn't modified";

  EXPECT_TRUE(Selection().Modify(
      SelectionModifyAlteration::kMove, SelectionModifyDirection::kForward,
      TextGranularity::kCharacter, SetSelectionBy::kUser))
      << "Selection.modify() returns true for user-triggered call";
  EXPECT_EQ(end_of_text, Selection().ComputeVisibleSelectionInDOMTree().Start())
      << "Selection isn't modified";
}

TEST_F(FrameSelectionTest, MoveRangeSelectionTest) {
  // "Foo Bar Baz,"
  Text* text = AppendTextNode("Foo Bar Baz,");
  UpdateAllLifecyclePhasesForTest();

  // Itinitializes with "Foo B|a>r Baz," (| means start and > means end).
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 5), Position(text, 6))
          .Build(),
      SetSelectionOptions());
  EXPECT_EQ_SELECTED_TEXT("a");

  // "Foo B|ar B>az," with the Character granularity.
  MoveRangeSelectionInternal(Position(text, 5), Position(text, 9),
                             TextGranularity::kCharacter);
  EXPECT_EQ_SELECTED_TEXT("ar B");
  // "Foo B|ar B>az," with the Word granularity.
  MoveRangeSelectionInternal(Position(text, 5), Position(text, 9),
                             TextGranularity::kWord);
  EXPECT_EQ_SELECTED_TEXT("Bar Baz");
  // "Fo<o B|ar Baz," with the Character granularity.
  MoveRangeSelectionInternal(Position(text, 5), Position(text, 2),
                             TextGranularity::kCharacter);
  EXPECT_EQ_SELECTED_TEXT("o B");
  // "Fo<o B|ar Baz," with the Word granularity.
  MoveRangeSelectionInternal(Position(text, 5), Position(text, 2),
                             TextGranularity::kWord);
  EXPECT_EQ_SELECTED_TEXT("Foo Bar");
}

TEST_F(FrameSelectionTest, MoveRangeSelectionNoLiveness) {
  SetBodyContent("<span id=sample>xyz</span>");
  Element* const sample = GetDocument().getElementById(AtomicString("sample"));
  // Select as: <span id=sample>^xyz|</span>
  MoveRangeSelectionInternal(Position(sample->firstChild(), 1),
                             Position(sample->firstChild(), 1),
                             TextGranularity::kWord);
  EXPECT_EQ("xyz", Selection().SelectedText());
  sample->insertBefore(Text::Create(GetDocument(), "abc"),
                       sample->firstChild());
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  const VisibleSelection& selection =
      Selection().ComputeVisibleSelectionInDOMTree();
  // Inserting "abc" before "xyz" should not affect to selection.
  EXPECT_EQ(Position(sample->lastChild(), 0), selection.Start());
  EXPECT_EQ(Position(sample->lastChild(), 3), selection.End());
  EXPECT_EQ("xyz", Selection().SelectedText());
  EXPECT_EQ("abcxyz", sample->innerText());
}

// For http://crbug.com/695317
TEST_F(FrameSelectionTest, SelectAllWithInputElement) {
  SetBodyContent("<input>123");
  Element* const input = GetDocument().QuerySelector(AtomicString("input"));
  Node* const last_child = GetDocument().body()->lastChild();
  Selection().SelectAll();
  const SelectionInDOMTree& result_in_dom_tree =
      Selection().ComputeVisibleSelectionInDOMTree().AsSelection();
  const SelectionInFlatTree& result_in_flat_tree =
      Selection().ComputeVisibleSelectionInFlatTree().AsSelection();
  EXPECT_EQ(SelectionInDOMTree::Builder(result_in_dom_tree)
                .Collapse(Position::BeforeNode(*input))
                .Extend(Position(last_child, 3))
                .Build(),
            result_in_dom_tree);
  EXPECT_EQ(SelectionInFlatTree::Builder(result_in_flat_tree)
                .Collapse(PositionInFlatTree::BeforeNode(*input))
                .Extend(PositionInFlatTree(last_child, 3))
                .Build(),
            result_in_flat_tree);
}

TEST_F(FrameSelectionTest, SelectAllWithUnselectableRoot) {
  Element* select = GetDocument().CreateRawElement(html_names::kSelectTag);
  GetDocument().ReplaceChild(select, GetDocument().documentElement());
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Selection().SelectAll();
  EXPECT_TRUE(Selection().ComputeVisibleSelectionInDOMTree().IsNone())
      << "Nothing should be selected if the "
         "content of the documentElement is not "
         "selctable.";
}

TEST_F(FrameSelectionTest, SelectAllPreservesHandle) {
  SetBodyContent("<div id=sample>abc</div>");
  Element* sample = GetDocument().getElementById(AtomicString("sample"));
  const Position end_of_text(sample->firstChild(), 3);
  Selection().SetSelection(
      SelectionInDOMTree::Builder().Collapse(end_of_text).Build(),
      SetSelectionOptions());
  EXPECT_FALSE(Selection().IsHandleVisible());
  Selection().SelectAll();
  EXPECT_FALSE(Selection().IsHandleVisible())
      << "If handles weren't present before "
         "selectAll. Then they shouldn't be present "
         "after it.";

  Selection().SetSelection(
      SelectionInDOMTree::Builder().Collapse(end_of_text).Build(),
      SetSelectionOptions::Builder()
          .SetShouldCloseTyping(true)
          .SetShouldClearTypingStyle(true)
          .SetShouldShowHandle(true)
          .Build());
  EXPECT_TRUE(Selection().IsHandleVisible());
  Selection().SelectAll();
  EXPECT_TRUE(Selection().IsHandleVisible())
      << "If handles were present before "
         "selectAll. Then they should be present "
         "after it.";
}

TEST_F(FrameSelectionTest, BoldCommandPreservesHandle) {
  SetBodyContent("<div id=sample contenteditable>abc</div>");
  Element* sample = GetDocument().getElementById(AtomicString("sample"));
  const Position end_of_text(sample->firstChild(), 3);
  Selection().SetSelection(
      SelectionInDOMTree::Builder().Collapse(end_of_text).Build(),
      SetSelectionOptions());
  EXPECT_FALSE(Selection().IsHandleVisible());
  Selection().SelectAll();
  GetDocument().execCommand("bold", false, "", ASSERT_NO_EXCEPTION);
  EXPECT_FALSE(Selection().IsHandleVisible())
      << "If handles weren't present before "
         "bold command. Then they shouldn't "
         "be present after it.";

  Selection().SetSelection(
      SelectionInDOMTree::Builder().Collapse(end_of_text).Build(),
      SetSelectionOptions::Builder()
          .SetShouldCloseTyping(true)
          .SetShouldClearTypingStyle(true)
          .SetShouldShowHandle(true)
          .Build());
  EXPECT_TRUE(Selection().IsHandleVisible());
  Selection().SelectAll();
  GetDocument().execCommand("bold", false, "", ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(Selection().IsHandleVisible())
      << "If handles were present before "
         "bold command. Then they should "
         "be present after it.";
}

TEST_F(FrameSelectionTest, SelectionOnRangeHidesHandles) {
  Text* text = AppendTextNode("Hello, World!");
  UpdateAllLifecyclePhasesForTest();
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .SetBaseAndExtent(EphemeralRange(
                                   Position(text, 0), Position(text, 12)))
                               .Build(),
                           SetSelectionOptions());

  EXPECT_FALSE(Selection().IsHandleVisible())
      << "After SetSelection on Range, handles shouldn't be present.";

  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 0), Position(text, 5))
          .Build(),
      SetSelectionOptions::Builder()
          .SetShouldCloseTyping(true)
          .SetShouldClearTypingStyle(true)
          .SetShouldShowHandle(true)
          .Build());

  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .SetBaseAndExtent(EphemeralRange(
                                   Position(text, 0), Position(text, 12)))
                               .Build(),
                           SetSelectionOptions());

  EXPECT_FALSE(Selection().IsHandleVisible())
      << "After SetSelection on Range, handles shouldn't be present.";
}

// Regression test for crbug.com/702756
// Test case excerpted from editing/undo/redo_correct_selection.html
TEST_F(FrameSelectionTest, SelectInvalidPositionInFlatTreeDoesntCrash) {
  SetBodyContent("foo<option><select></select></option>");
  Element* body = GetDocument().body();
  Element* select = GetDocument().QuerySelector(AtomicString("select"));
  Node* foo = body->firstChild();
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .Collapse(Position(body, 0))
                               // SELECT@AfterAnchor is invalid in flat tree.
                               .Extend(Position::AfterNode(*select))
                               .Build(),
                           SetSelectionOptions());
  // Should not crash inside.
  const VisibleSelectionInFlatTree& selection =
      Selection().ComputeVisibleSelectionInFlatTree();

  // This only records the current behavior. It might be changed in the future.
  EXPECT_EQ(PositionInFlatTree(foo, 0), selection.Anchor());
  EXPECT_EQ(PositionInFlatTree(foo, 0), selection.Focus());
}

TEST_F(FrameSelectionTest, CaretInShadowTree) {
  SetBodyContent("<p id=host></p>bar");
  ShadowRoot* shadow_root =
      SetShadowContent("<div contenteditable id='ce'>foo</div>", "host");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const ce = shadow_root->getElementById(AtomicString("ce"));
  ce->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  ce->blur();  // Move focus to document body.
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());  // Caret is now hidden.
}

TEST_F(FrameSelectionTest, CaretInTextControl) {
  SetBodyContent("<input id='field'>");  // <input> hosts a shadow tree.
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const field = GetDocument().getElementById(AtomicString("field"));
  field->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  field->blur();  // Move focus to document body.
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());  // Caret is now hidden.
}

TEST_F(FrameSelectionTest, RangeInShadowTree) {
  SetBodyContent("<p id='host'></p>");
  ShadowRoot* shadow_root = SetShadowContent("hey", "host");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Node* text_node = shadow_root->firstChild();
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text_node, 0), Position(text_node, 3))
          .Build(),
      SetSelectionOptions());
  EXPECT_EQ_SELECTED_TEXT("hey");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  GetDocument().body()->Focus();  // Move focus to document body.
  EXPECT_EQ_SELECTED_TEXT("hey");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());
}

TEST_F(FrameSelectionTest, RangeInTextControl) {
  SetBodyContent("<input id='field' value='hola'>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const field = GetDocument().getElementById(AtomicString("field"));
  field->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Selection().SelectAll();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  field->blur();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());
}

// crbug.com/692898
TEST_F(FrameSelectionTest, FocusingLinkHidesCaretInTextControl) {
  SetBodyContent(
      "<input id='field'>"
      "<a href='www' id='alink'>link</a>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const field = GetDocument().getElementById(AtomicString("field"));
  field->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Element* const alink = GetDocument().getElementById(AtomicString("alink"));
  alink->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());
}

// crbug.com/692898
TEST_F(FrameSelectionTest, FocusingLinkHidesRangeInTextControl) {
  SetBodyContent(
      "<input id='field' value='hola'>"
      "<a href='www' id='alink'>link</a>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const field = GetDocument().getElementById(AtomicString("field"));
  field->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Selection().SelectAll();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Element* const alink = GetDocument().getElementById(AtomicString("alink"));
  alink->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());
}

TEST_F(FrameSelectionTest, FocusingButtonHidesRangeInReadOnlyTextControl) {
  SetBodyContent(
      "<textarea readonly>Berlin</textarea>"
      "<input type='submit' value='Submit'>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const textarea =
      GetDocument().QuerySelector(AtomicString("textarea"));
  textarea->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());

  Selection().SelectAll();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Element* const submit = GetDocument().QuerySelector(AtomicString("input"));
  submit->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());
}

TEST_F(FrameSelectionTest, FocusingButtonHidesRangeInDisabledTextControl) {
  SetBodyContent(
      "<textarea disabled>Berlin</textarea>"
      "<input type='submit' value='Submit'>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const textarea =
      GetDocument().QuerySelector(AtomicString("textarea"));
  textarea->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());

  // We use a double click to create the selection [Berlin].
  // FrameSelection::SelectAll (= textarea.select() in JavaScript) would have
  // been shorter, but currently that doesn't work on a *disabled* text control.
  const gfx::Rect elem_bounds = textarea->BoundsInWidget();
  WebMouseEvent double_click(WebMouseEvent::Type::kMouseDown, 0,
                             WebInputEvent::GetStaticTimeStampForTests());
  double_click.SetPositionInWidget(elem_bounds.x(), elem_bounds.y());
  double_click.SetPositionInScreen(elem_bounds.x(), elem_bounds.y());
  double_click.button = WebMouseEvent::Button::kLeft;
  double_click.click_count = 2;
  double_click.SetFrameScale(1);

  GetFrame().GetEventHandler().HandleMousePressEvent(double_click);
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Element* const submit = GetDocument().QuerySelector(AtomicString("input"));
  submit->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());
}

// crbug.com/713051
TEST_F(FrameSelectionTest, FocusingNonEditableParentHidesCaretInTextControl) {
  SetBodyContent(
      "<div tabindex='-1' id='parent'>"
      "  <input id='field'>"
      "</div>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const field = GetDocument().getElementById(AtomicString("field"));
  field->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  // Here the selection belongs to <input>'s shadow tree and that tree has a
  // non-editable parent that is focused.
  Element* const parent = GetDocument().getElementById(AtomicString("parent"));
  parent->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());  // Focus is outside <input>
                                        // so caret should not be visible.

  parent->blur();  // Move focus to document body.
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());  // Caret is still hidden.
}

// crbug.com/713051
TEST_F(FrameSelectionTest, FocusingNonEditableParentHidesRangeInTextControl) {
  SetBodyContent(
      "<div tabindex='-1' id='parent'>"
      "  <input id='field' value='hola'>"
      "</div>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const field = GetDocument().getElementById(AtomicString("field"));
  field->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Selection().SelectAll();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  // Here the selection belongs to <input>'s shadow tree and that tree has a
  // non-editable parent that is focused.
  Element* const parent = GetDocument().getElementById(AtomicString("parent"));
  parent->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());  // Focus is outside <input>
                                        // so range should not be visible.

  parent->blur();  // Move focus to document body.
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());  // Range is still hidden.
}

TEST_F(FrameSelectionTest, CaretInEditableDiv) {
  SetBodyContent("<div contenteditable id='ce'>blabla</div>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const ce = GetDocument().getElementById(AtomicString("ce"));
  ce->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  ce->blur();  // Move focus to document body.
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());  // Caret is now hidden.
}

TEST_F(FrameSelectionTest, RangeInEditableDiv) {
  SetBodyContent("<div contenteditable id='ce'>blabla</div>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const ce = GetDocument().getElementById(AtomicString("ce"));
  ce->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Selection().SelectAll();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  ce->blur();  // Move focus to document body.
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());  // Range is still visible.
}

TEST_F(FrameSelectionTest, RangeInEditableDivInShadowTree) {
  SetBodyContent("<p id='host'></p>");
  ShadowRoot* shadow_root =
      SetShadowContent("<div id='ce' contenteditable>foo</div>", "host");

  Element* const ce = shadow_root->getElementById(AtomicString("ce"));
  ce->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Selection().SelectAll();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  ce->blur();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());  // Range is still visible.
}

TEST_F(FrameSelectionTest, FocusingLinkHidesCaretInContentEditable) {
  SetBodyContent(
      "<div contenteditable id='ce'>blabla</div>"
      "<a href='www' id='alink'>link</a>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const ce = GetDocument().getElementById(AtomicString("ce"));
  ce->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Element* const alink = GetDocument().getElementById(AtomicString("alink"));
  alink->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());
}

TEST_F(FrameSelectionTest, FocusingLinkKeepsRangeInContentEditable) {
  SetBodyContent(
      "<div contenteditable id='ce'>blabla</div>"
      "<a href='www' id='alink'>link</a>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const ce = GetDocument().getElementById(AtomicString("ce"));
  ce->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Selection().SelectAll();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Element* const alink = GetDocument().getElementById(AtomicString("alink"));
  alink->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());
}

TEST_F(FrameSelectionTest, FocusingEditableParentKeepsEditableCaret) {
  SetBodyContent(
      "<div contenteditable tabindex='-1' id='parent'>"
      "<div contenteditable id='ce'>blabla</div>"
      "</div>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  // TODO(editing-dev): Blink should be able to focus the inner <div>.
  //  Element* const ce = GetDocument().getElementById(AtomicString("ce"));
  //  ce->Focus();
  //  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  //  EXPECT_FALSE(Selection().IsHidden());

  Element* const parent = GetDocument().getElementById(AtomicString("parent"));
  parent->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());  // Focus is within editing boundary,
                                         // caret should be visible.

  parent->blur();  // Move focus to document body.
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());  // Focus is outside editing boundary
                                        // so caret should be hidden.
}

TEST_F(FrameSelectionTest, FocusingEditableParentKeepsEditableRange) {
  SetBodyContent(
      "<div contenteditable tabindex='-1' id='parent'>"
      "<div contenteditable id='ce'>blabla</div>"
      "</div>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  // TODO(editing-dev): Blink should be able to focus the inner <div>.
  //  Element* const ce = GetDocument().getElementById(AtomicString("ce"));
  //  ce->Focus();
  //  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  //  EXPECT_FALSE(Selection().IsHidden());

  //  Selection().SelectAll();
  //  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  //  EXPECT_FALSE(Selection().IsHidden());

  Element* const parent = GetDocument().getElementById(AtomicString("parent"));
  parent->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());  // Focus is within editing boundary,
                                         // range should be visible.

  Selection().SelectAll();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  parent->blur();  // Move focus to document body.
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());  // Focus is outside editing boundary
                                         // but range should still be visible.
}

TEST_F(FrameSelectionTest, FocusingNonEditableParentHidesEditableCaret) {
  SetBodyContent(
      "<div tabindex='-1' id='parent'>"
      "<div contenteditable id='ce'>blabla</div>"
      "</div>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const ce = GetDocument().getElementById(AtomicString("ce"));
  ce->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  // Here the selection belongs to <div>'s shadow tree and that tree has a
  // non-editable parent that is focused.
  Element* const parent = GetDocument().getElementById(AtomicString("parent"));
  parent->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());  // Focus is outside editing boundary
                                        // so caret should be hidden.

  parent->blur();  // Move focus to document body.
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());  // Caret is still hidden.
}

TEST_F(FrameSelectionTest, FocusingNonEditableParentKeepsEditableRange) {
  SetBodyContent(
      "<div tabindex='-1' id='parent'>"
      "<div contenteditable id='ce'>blabla</div>"
      "</div>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const ce = GetDocument().getElementById(AtomicString("ce"));
  ce->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Selection().SelectAll();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  // Here the selection belongs to <div>'s shadow tree and that tree has a
  // non-editable parent that is focused.
  Element* const parent = GetDocument().getElementById(AtomicString("parent"));
  parent->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());  // Focus is outside editing boundary
                                         // but range should still be visible.

  parent->blur();  // Move focus to document body.
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());  // Range is still visible.
}

// crbug.com/707143
TEST_F(FrameSelectionTest, RangeContainsFocus) {
  SetBodyContent(
      "<div>"
      "  <div>"
      "    <span id='start'>start</span>"
      "  </div>"
      "  <a href='www' id='alink'>link</a>"
      "  <div>line 1</div>"
      "  <div>line 2</div>"
      "  <div>line 3</div>"
      "  <div>line 4</div>"
      "  <span id='end'>end</span>"
      "  <div></div>"
      "</div>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const start = GetDocument().getElementById(AtomicString("start"));
  Element* const end = GetDocument().getElementById(AtomicString("end"));
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(start, 0), Position(end, 1))
          .Build(),
      SetSelectionOptions());
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Element* const alink = GetDocument().getElementById(AtomicString("alink"));
  alink->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());  // Range still visible.
}

// crbug.com/707143
TEST_F(FrameSelectionTest, RangeOutsideFocus) {
  // Here the selection sits on a sub tree that hasn't the focused element.
  // This test case is the reason why we separate FrameSelection::HasFocus() and
  // FrameSelection::IsHidden(). Even when the selection's DOM nodes are
  // completely disconnected from the focused node, we still want the selection
  // to be visible (not hidden).
  SetBodyContent(
      "<a href='www' id='alink'>link</a>"
      "<div>"
      "  <div>"
      "    <span id='start'>start</span>"
      "  </div>"
      "  <div>line 1</div>"
      "  <div>line 2</div>"
      "  <div>line 3</div>"
      "  <div>line 4</div>"
      "  <span id='end'>end</span>"
      "  <div></div>"
      "</div>");
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsNone());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_TRUE(Selection().IsHidden());

  Element* const start = GetDocument().getElementById(AtomicString("start"));
  Element* const end = GetDocument().getElementById(AtomicString("end"));
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(start, 0), Position(end, 1))
          .Build(),
      SetSelectionOptions());
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_TRUE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());

  Element* const alink = GetDocument().getElementById(AtomicString("alink"));
  alink->Focus();
  EXPECT_TRUE(Selection().GetSelectionInDOMTree().IsRange());
  EXPECT_FALSE(Selection().SelectionHasFocus());
  EXPECT_FALSE(Selection().IsHidden());  // Range still visible.
}

// crbug.com/725457
TEST_F(FrameSelectionTest, InconsistentVisibleSelectionNoCrash) {
  SetBodyContent("foo<div id=host><span id=anchor>bar</span></div>baz");
  SetShadowContent("shadow", "host");

  Element* anchor = GetDocument().getElementById(AtomicString("anchor"));

  // |start| and |end| are valid Positions in DOM tree, but do not participate
  // in flat tree. They should be canonicalized to null VisiblePositions, but
  // are currently not. See crbug.com/729636 for details.
  const Position& start = Position::BeforeNode(*anchor);
  const Position& end = Position::AfterNode(*anchor);
  Selection().SetSelection(
      SelectionInDOMTree::Builder().Collapse(start).Extend(end).Build(),
      SetSelectionOptions());

  // Shouldn't crash inside.
  EXPECT_FALSE(Selection().SelectionHasFocus());
}

TEST_F(FrameSelectionTest, SelectionBounds) {
  SetBodyContent(
      "<style>"
      "  * { margin: 0; } "
      "  html, body { height: 2000px; }"
      "  div {"
      "    width: 20px;"
      "    height: 1000px;"
      "    font-size: 30px;"
      "    overflow: hidden;"
      "    margin-top: 2px;"
      "  }"
      "</style>"
      "<div>"
      "  a<br>b<br>c<br>d<br>e<br>f<br>g<br>h<br>i<br>j<br>k<br>l<br>m<br>n<br>"
      "  a<br>b<br>c<br>d<br>e<br>f<br>g<br>h<br>i<br>j<br>k<br>l<br>m<br>n<br>"
      "  a<br>b<br>c<br>d<br>e<br>f<br>g<br>h<br>i<br>j<br>k<br>l<br>m<br>n<br>"
      "</div>");
  Selection().SelectAll();

  const int node_width = 20;
  const int node_height = 1000;
  const int node_margin_top = 2;
  // The top of the node should be visible but the bottom should be outside
  // by the viewport. The unclipped selection bounds should not be clipped.
  EXPECT_EQ(PhysicalRect(0, node_margin_top, node_width, node_height),
            Selection().AbsoluteUnclippedBounds());

  // Scroll 500px down so the top of the node is outside the viewport and the
  // bottom is visible. The unclipped selection bounds should not be clipped.
  const int scroll_offset = 500;
  LocalFrameView* frame_view = GetDocument().View();
  frame_view->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, scroll_offset), mojom::blink::ScrollType::kProgrammatic);
  EXPECT_EQ(PhysicalRect(0, node_margin_top, node_width, node_height),
            frame_view->FrameToDocument(Selection().AbsoluteUnclippedBounds()));

  // Adjust the page scale factor which changes the selection bounds as seen
  // through the viewport. The unclipped selection bounds should not be clipped.
  const int page_scale_factor = 2;
  GetPage().SetPageScaleFactor(page_scale_factor);
  EXPECT_EQ(PhysicalRect(0, node_margin_top, node_width, node_height),
            frame_view->FrameToDocument(Selection().AbsoluteUnclippedBounds()));
}

TEST_F(FrameSelectionTest, AbosluteSelectionBoundsAfterScroll) {
  SetBodyContent(
      "<style>"
      "  html, body { height: 2000px; }"
      "</style>"
      "<div style='height:1000px;'>"
      "  <p style='margin-top:100px; font-size:30px'>text</p>"
      "</div>");
  Selection().SelectAll();

  gfx::Rect initial_anchor, initial_focus;
  Selection().ComputeAbsoluteBounds(initial_anchor, initial_focus);

  // Scroll 50px down.
  const int scroll_offset = 50;
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, scroll_offset), mojom::blink::ScrollType::kProgrammatic);

  // Check absolute selection bounds are updated.
  gfx::Rect anchor_after_scroll, focus_after_scroll;
  Selection().ComputeAbsoluteBounds(anchor_after_scroll, focus_after_scroll);
  EXPECT_EQ(anchor_after_scroll,
            initial_anchor - gfx::Vector2d(0, scroll_offset));
  EXPECT_EQ(focus_after_scroll,
            initial_focus - gfx::Vector2d(0, scroll_offset));
}

TEST_F(FrameSelectionTest, SelectionContainsBidiBoundary) {
  InsertStyleElement("div{font:10px/10px Ahem}");
  // Rendered as abcFED
  Selection().SetSelection(
      SetSelectionTextToBody("<div dir=ltr>^abc<bdo dir=trl>DEF|</bdo></div>"),
      SetSelectionOptions());

  // Check the right half of 'c'
  const PhysicalOffset c_right(35, 13);
  EXPECT_TRUE(Selection().Contains(c_right));

  // Check the left half of "F"
  const PhysicalOffset f_left(45, 13);
  EXPECT_TRUE(Selection().Contains(f_left));
}

// This is a regression test for https://crbug.com/927394 where 'copy' operation
// stopped copying content from inside text controls.
// Note that this is a non-standard behavior.
TEST_F(FrameSelectionTest, SelectedTextForClipboardEntersTextControls) {
  Selection().SetSelection(
      SetSelectionTextToBody("^foo<input value=\"bar\">baz|"),
      SetSelectionOptions());
  EXPECT_EQ("foo\nbar\nbaz", Selection().SelectedTextForClipboard());
}

// For https://crbug.com/1177295
TEST_F(FrameSelectionTest, PositionDisconnectedInFlatTree) {
  SetBodyContent("<div id=host>x</div>y");
  SetShadowContent("", "host");
  Element* host = GetElementById("host");
  Node* text = host->firstChild();
  Position positions[] = {
      Position::BeforeNode(*host),         Position::FirstPositionInNode(*host),
      Position::LastPositionInNode(*host), Position::AfterNode(*host),
      Position::BeforeNode(*text),         Position::FirstPositionInNode(*text),
      Position::LastPositionInNode(*text), Position::AfterNode(*text)};
  for (const Position& base : positions) {
    EXPECT_TRUE(base.IsConnected());
    bool flat_base_is_connected = ToPositionInFlatTree(base).IsConnected();
    EXPECT_EQ(base.AnchorNode() == host, flat_base_is_connected);
    for (const Position& extent : positions) {
      const SelectionInDOMTree& selection =
          SelectionInDOMTree::Builder().SetBaseAndExtent(base, extent).Build();
      Selection().SetSelection(selection, SetSelectionOptions());
      EXPECT_TRUE(extent.IsConnected());
      bool flat_extent_is_connected =
          ToPositionInFlatTree(selection.Focus()).IsConnected();
      EXPECT_EQ(flat_base_is_connected || flat_extent_is_connected
                    ? "<div id=\"host\"></div>|y"
                    : "<div id=\"host\"></div>y",
                GetSelectionTextInFlatTreeFromBody(
                    GetVisibleSelectionInFlatTree().AsSelection()));
    }
  }
}

TEST_F(FrameSelectionTest, PaintCaretRecordsSelectionWithNoSelectionHandles) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kHiddenSelectionBounds);

  Text* text = AppendTextNode("Hello, World!");
  UpdateAllLifecyclePhasesForTest();

  GetDocument().body()->setContentEditable("true", ASSERT_NO_EXCEPTION);
  GetDocument().body()->Focus();
  EXPECT_TRUE(GetDocument().body()->IsFocused());

  Selection().SetCaretEnabled(true);
  Selection().SetSelection(
      SelectionInDOMTree::Builder().Collapse(Position(text, 0)).Build(),
      SetSelectionOptions());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(Selection().ComputeVisibleSelectionInDOMTree().IsCaret());
  EXPECT_TRUE(Selection().ShouldPaintCaret(
      *To<LayoutBlock>(GetDocument().body()->GetLayoutObject())));

  PaintController paint_controller;
  {
    GraphicsContext context(paint_controller);
    paint_controller.UpdateCurrentPaintChunkProperties(
        root_paint_chunk_id_, *root_paint_property_client_,
        PropertyTreeState::Root());
    Selection().PaintCaret(context, PhysicalOffset());
  }
  auto& paint_artifact = paint_controller.CommitNewDisplayItems();

  const PaintChunk& chunk = paint_artifact.GetPaintChunks()[0];
  EXPECT_THAT(chunk.layer_selection_data, Not(IsNull()));
  LayerSelectionData* selection_data = chunk.layer_selection_data;
  EXPECT_TRUE(selection_data->start.has_value());
  EXPECT_EQ(gfx::SelectionBound::HIDDEN, selection_data->start->type);
  EXPECT_TRUE(selection_data->end.has_value());
  EXPECT_EQ(gfx::SelectionBound::HIDDEN, selection_data->end->type);
}

}  // namespace blink
