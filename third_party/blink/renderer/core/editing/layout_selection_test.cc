// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/layout_selection.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_shadow_root_init.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

static LayoutTextFragment* FirstLetterPartFor(
    const LayoutObject* layout_object) {
  if (const auto* layout_text = DynamicTo<LayoutText>(layout_object))
    return DynamicTo<LayoutTextFragment>(layout_text->GetFirstLetterPart());
  return nullptr;
}

class LayoutSelectionTestBase : public EditingTestBase {
 protected:
  static void PrintText(std::ostream& ostream, const Text& text) {
    ostream << "'" << text.data().Utf8() << "'";
  }

  static void PrintLayoutTextInfo(const FrameSelection& selection,
                                  std::ostream& ostream,
                                  const LayoutText& layout_text,
                                  SelectionState state) {
    if (layout_text.IsInLayoutNGInlineFormattingContext()) {
      InlineCursor cursor(*layout_text.FragmentItemsContainer());
      cursor.MoveTo(layout_text);
      if (!cursor)
        return;
      const unsigned text_start = cursor.Current().TextStartOffset();
      for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
        const LayoutSelectionStatus status =
            selection.ComputeLayoutSelectionStatus(cursor);
        if (state == SelectionState::kNone && status.start == status.end)
          continue;
        ostream << "(" << status.start - text_start << ","
                << status.end - text_start << ")";
      }
      return;
    }

    const LayoutTextSelectionStatus& status =
        selection.ComputeLayoutSelectionStatus(layout_text);
    if (state == SelectionState::kNone && status.start == status.end)
      return;
    ostream << "(" << status.start << "," << status.end << ")";
  }

  static void PrintLayoutObjectInfo(const FrameSelection& selection,
                                    std::ostream& ostream,
                                    LayoutObject* layout_object) {
    const SelectionState& state = layout_object->GetSelectionState();
    ostream << ", " << state;
    if (layout_object->IsText()) {
      PrintLayoutTextInfo(selection, ostream, To<LayoutText>(*layout_object),
                          state);
    }

    ostream << (layout_object->ShouldInvalidateSelection()
                    ? ", ShouldInvalidate "
                    : ", NotInvalidate ");
  }
  static void PrintSelectionInfo(const FrameSelection& selection,
                                 std::ostream& ostream,
                                 const Node& node,
                                 wtf_size_t depth) {
    if (const Text* text = DynamicTo<Text>(node))
      PrintText(ostream, *text);
    else if (const auto* element = DynamicTo<Element>(node))
      ostream << element->tagName().Utf8();
    else
      ostream << node;

    LayoutObject* layout_object = node.GetLayoutObject();
    if (!layout_object) {
      ostream << ", <null LayoutObject> ";
      return;
    }
    PrintLayoutObjectInfo(selection, ostream, layout_object);
    if (LayoutTextFragment* first_letter = FirstLetterPartFor(layout_object)) {
      ostream << std::endl
              << RepeatString("  ", depth + 1).Utf8() << ":first-letter";
      PrintLayoutObjectInfo(selection, ostream, first_letter);
    }
  }

  static void PrintDOMTreeInternal(const FrameSelection& selection,
                                   std::ostream& ostream,
                                   const Node& node,
                                   wtf_size_t depth) {
    ostream << RepeatString("  ", depth).Utf8();
    if (IsA<HTMLStyleElement>(node)) {
      ostream << "<style> ";
      return;
    }
    PrintSelectionInfo(selection, ostream, node, depth);
    if (ShadowRoot* shadow_root = node.GetShadowRoot()) {
      ostream << std::endl << RepeatString("  ", depth + 1).Utf8();
      ostream << "#shadow-root ";
      for (Node* child = shadow_root->firstChild(); child;
           child = child->nextSibling()) {
        ostream << std::endl;
        PrintDOMTreeInternal(selection, ostream, *child, depth + 2);
      }
    }

    for (Node* child = node.firstChild(); child; child = child->nextSibling()) {
      ostream << std::endl;
      PrintDOMTreeInternal(selection, ostream, *child, depth + 1);
    }
  }

#ifndef NDEBUG
  void PrintDOMTreeForDebug() {
    std::stringstream stream;
    stream << "\nPrintDOMTreeForDebug";
    PrintDOMTreeInternal(Selection(), stream, *GetDocument().body(), 0u);
    LOG(INFO) << stream.str();
  }
#endif

  std::string DumpSelectionInfo() {
    std::stringstream stream;
    PrintDOMTreeInternal(Selection(), stream, *GetDocument().body(), 0u);
    return stream.str();
  }
};

class LayoutSelectionTest : public LayoutSelectionTestBase {
 protected:
  LayoutSelectionTest() = default;
};

TEST_F(LayoutSelectionTest, TraverseLayoutObject) {
  SetBodyContent("foo<br>bar");
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .SelectAllChildren(*GetDocument().body())
                               .Build(),
                           SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  'foo', Start(0,3), ShouldInvalidate \n"
      "  BR, Inside(0,1), ShouldInvalidate \n"
      "  'bar', End(0,3), ShouldInvalidate ",
      DumpSelectionInfo());
}

TEST_F(LayoutSelectionTest, TraverseLayoutObjectTruncateVisibilityHidden) {
  SetBodyContent(
      "<span style='visibility:hidden;'>before</span>"
      "foo"
      "<span style='visibility:hidden;'>after</span>");
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .SelectAllChildren(*GetDocument().body())
                               .Build(),
                           SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  SPAN, None, NotInvalidate \n"
      "    'before', None, NotInvalidate \n"
      "  'foo', StartAndEnd(0,3), ShouldInvalidate \n"
      "  SPAN, None, NotInvalidate \n"
      "    'after', None, NotInvalidate ",
      DumpSelectionInfo());
}

TEST_F(LayoutSelectionTest, TraverseLayoutObjectBRs) {
  SetBodyContent("<br><br>foo<br><br>");
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .SelectAllChildren(*GetDocument().body())
                               .Build(),
                           SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  BR, Start(0,1), ShouldInvalidate \n"
      "  BR, Inside(0,1), ShouldInvalidate \n"
      "  'foo', Inside(0,3), ShouldInvalidate \n"
      "  BR, Inside(0,1), ShouldInvalidate \n"
      "  BR, End(0,1), ShouldInvalidate ",
      DumpSelectionInfo());
}

TEST_F(LayoutSelectionTest, TraverseLayoutObjectListStyleImage) {
  SetBodyContent(
      "<style>ul {list-style-image:url(data:"
      "image/gif;base64,R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs=)}"
      "</style>"
      "<ul><li>foo<li>bar</ul>");
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .SelectAllChildren(*GetDocument().body())
                               .Build(),
                           SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  <style> \n"
      "  UL, Contain, NotInvalidate \n"
      "    LI, Contain, NotInvalidate \n"
      "      'foo', Start(0,3), ShouldInvalidate \n"
      "    LI, Contain, NotInvalidate \n"
      "      'bar', End(0,3), ShouldInvalidate ",
      DumpSelectionInfo());
}

TEST_F(LayoutSelectionTest, TraverseLayoutObjectCrossingShadowBoundary) {
  Selection().SetSelection(
      SetSelectionTextToBody(
          "^foo"
          "<div>"
          "<template data-mode=open>"
          "Foo<slot name=s2></slot><slot name=s1></slot>"
          "</template>"
          // Set selection at SPAN@0 instead of "bar1"@0
          "<span slot=s1><!--|-->bar1</span><span slot=s2>bar2</span>"
          "</div>"),
      SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  'foo', Start(0,3), ShouldInvalidate \n"
      "  DIV, Contain, NotInvalidate \n"
      "    #shadow-root \n"
      "      'Foo', Inside(0,3), ShouldInvalidate \n"
      "      SLOT, <null LayoutObject> \n"
      "      SLOT, <null LayoutObject> \n"
      "    SPAN, None, NotInvalidate \n"
      "      'bar1', None, NotInvalidate \n"
      "    SPAN, Contain, NotInvalidate \n"
      "      'bar2', End(0,4), ShouldInvalidate ",
      DumpSelectionInfo());
}

// crbug.com/752715
TEST_F(LayoutSelectionTest,
       InvalidationShouldNotChangeRefferedLayoutObjectState) {
  SetBodyContent(
      "<div id='d1'>div1</div><div id='d2'>foo<span>bar</span>baz</div>");
  Node* span = GetDocument().QuerySelector(AtomicString("span"));
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(span->firstChild(), 0),
                            Position(span->firstChild(), 3))
          .Build(),
      SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  DIV, None, NotInvalidate \n"
      "    'div1', None, NotInvalidate \n"
      "  DIV, Contain, NotInvalidate \n"
      "    'foo', None, NotInvalidate \n"
      "    SPAN, Contain, NotInvalidate \n"
      "      'bar', StartAndEnd(0,3), ShouldInvalidate \n"
      "    'baz', None, NotInvalidate ",
      DumpSelectionInfo());

  Node* d1 = GetDocument().QuerySelector(AtomicString("#d1"));
  Node* d2 = GetDocument().QuerySelector(AtomicString("#d2"));
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(d1, 0), Position(d2, 0))
          .Build(),
      SetSelectionOptions());
  // This commit should not crash.
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  DIV, Contain, NotInvalidate \n"
      "    'div1', StartAndEnd(0,4), ShouldInvalidate \n"
      "  DIV, None, NotInvalidate \n"
      "    'foo', None, NotInvalidate \n"
      "    SPAN, None, NotInvalidate \n"
      "      'bar', None, ShouldInvalidate \n"
      "    'baz', None, NotInvalidate ",
      DumpSelectionInfo());
}

TEST_F(LayoutSelectionTest, TraverseLayoutObjectLineWrap) {
  SetBodyContent("bar\n");
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .SelectAllChildren(*GetDocument().body())
                               .Build(),
                           SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  'bar\n', StartAndEnd(0,3), ShouldInvalidate ",
      DumpSelectionInfo());
}

TEST_F(LayoutSelectionTest, FirstLetter) {
  SetBodyContent(
      "<style>::first-letter { color: red; }</style>"
      "<span>foo</span>");
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .SelectAllChildren(*GetDocument().body())
                               .Build(),
                           SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  <style> \n"
      "  SPAN, Contain, NotInvalidate \n"
      "    'foo', StartAndEnd(0,2), ShouldInvalidate \n"
      "      :first-letter, None(0,1), ShouldInvalidate ",
      DumpSelectionInfo());
}

TEST_F(LayoutSelectionTest, FirstLetterMultiple) {
  Selection().SetSelection(
      SetSelectionTextToBody("<style>::first-letter { color: red; }</style>"
                             "<span> [^f]o|o</span>"),
      SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  <style> \n"
      "  SPAN, Contain, NotInvalidate \n"
      "    ' [f]oo', StartAndEnd(0,1), ShouldInvalidate \n"
      "      :first-letter, None(1,3), ShouldInvalidate ",
      DumpSelectionInfo());
}

TEST_F(LayoutSelectionTest, FirstLetterClearSeletion) {
  InsertStyleElement("div::first-letter { color: red; }");
  Selection().SetSelection(SetSelectionTextToBody("fo^o<div>bar</div>b|az"),
                           SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  'foo', Start(2,3), ShouldInvalidate \n"
      "  DIV, Contain, NotInvalidate \n"
      "    'bar', Inside(0,2), ShouldInvalidate \n"
      "      :first-letter, None(0,1), ShouldInvalidate \n"
      "  'baz', End(0,1), ShouldInvalidate ",
      DumpSelectionInfo());

  Selection().Clear();
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, None, NotInvalidate \n"
      "  'foo', None, ShouldInvalidate \n"
      "  DIV, None, NotInvalidate \n"
      "    'bar', None, ShouldInvalidate \n"
      "      :first-letter, None, ShouldInvalidate \n"
      "  'baz', None, ShouldInvalidate ",
      DumpSelectionInfo());
}

TEST_F(LayoutSelectionTest, FirstLetterUpdateSeletion) {
  SetBodyContent(
      "<style>div::first-letter { color: red; }</style>"
      "foo<div>bar</div>baz");
  Node* const foo = GetDocument().body()->firstChild()->nextSibling();
  Node* const baz = GetDocument()
                        .body()
                        ->firstChild()
                        ->nextSibling()
                        ->nextSibling()
                        ->nextSibling();
  // <div>fo^o</div><div>bar</div>b|az
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .SetBaseAndExtent({foo, 2}, {baz, 1})
                               .Build(),
                           SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  <style> \n"
      "  'foo', Start(2,3), ShouldInvalidate \n"
      "  DIV, Contain, NotInvalidate \n"
      "    'bar', Inside(0,2), ShouldInvalidate \n"
      "      :first-letter, None(0,1), ShouldInvalidate \n"
      "  'baz', End(0,1), ShouldInvalidate ",
      DumpSelectionInfo());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  <style> \n"
      "  'foo', Start(2,3), NotInvalidate \n"
      "  DIV, Contain, NotInvalidate \n"
      "    'bar', Inside(0,2), NotInvalidate \n"
      "      :first-letter, None(0,1), NotInvalidate \n"
      "  'baz', End(0,1), NotInvalidate ",
      DumpSelectionInfo());
  UpdateAllLifecyclePhasesForTest();

  // <div>foo</div><div>bar</div>ba^z|
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .SetBaseAndExtent({baz, 2}, {baz, 3})
                               .Build(),
                           SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  <style> \n"
      "  'foo', None, ShouldInvalidate \n"
      "  DIV, None, NotInvalidate \n"
      "    'bar', None, ShouldInvalidate \n"
      "      :first-letter, None, ShouldInvalidate \n"
      "  'baz', StartAndEnd(2,3), ShouldInvalidate ",
      DumpSelectionInfo());
}

TEST_F(LayoutSelectionTest, CommitAppearanceIfNeededNotCrash) {
  Selection().SetSelection(
      SetSelectionTextToBody(
          "<div>"
          "<template data-mode=open>foo</template>"
          "<span>|bar<span>"  // <span> is not appeared in flat tree.
          "</div>"
          "<div>baz^</div>"),
      SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
}

TEST_F(LayoutSelectionTest, SelectImage) {
  const SelectionInDOMTree& selection =
      SetSelectionTextToBody("^<img style=\"width:100px; height:100px\"/>|");
  Selection().SetSelection(selection, SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  IMG, StartAndEnd, ShouldInvalidate ",
      DumpSelectionInfo());
}

TEST_F(LayoutSelectionTest, MoveOnSameNode_Start) {
  const SelectionInDOMTree& selection =
      SetSelectionTextToBody("f^oo<span>b|ar</span>");
  Selection().SetSelection(selection, SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  'foo', Start(1,3), ShouldInvalidate \n"
      "  SPAN, Contain, NotInvalidate \n"
      "    'bar', End(0,1), ShouldInvalidate ",
      DumpSelectionInfo());

  // Paint virtually and clear ShouldInvalidate flag.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  'foo', Start(1,3), NotInvalidate \n"
      "  SPAN, Contain, NotInvalidate \n"
      "    'bar', End(0,1), NotInvalidate ",
      DumpSelectionInfo());

  // "fo^o<span>b|ar</span>"
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent({selection.Anchor().AnchorNode(), 2},
                            selection.Focus())
          .Build(),
      SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  // Only "foo" should be invalidated.
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  'foo', Start(2,3), ShouldInvalidate \n"
      "  SPAN, Contain, NotInvalidate \n"
      "    'bar', End(0,1), NotInvalidate ",
      DumpSelectionInfo());
}

TEST_F(LayoutSelectionTest, MoveOnSameNode_End) {
  const SelectionInDOMTree& selection =
      SetSelectionTextToBody("f^oo<span>b|ar</span>");
  Selection().SetSelection(selection, SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  'foo', Start(1,3), ShouldInvalidate \n"
      "  SPAN, Contain, NotInvalidate \n"
      "    'bar', End(0,1), ShouldInvalidate ",
      DumpSelectionInfo());

  // Paint virtually and clear ShouldInvalidate flag.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  'foo', Start(1,3), NotInvalidate \n"
      "  SPAN, Contain, NotInvalidate \n"
      "    'bar', End(0,1), NotInvalidate ",
      DumpSelectionInfo());

  // "fo^o<span>ba|r</span>"
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(selection.Anchor(),
                            {selection.Focus().AnchorNode(), 2})
          .Build(),
      SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  // Only "bar" should be invalidated.
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  'foo', Start(1,3), NotInvalidate \n"
      "  SPAN, Contain, NotInvalidate \n"
      "    'bar', End(0,2), ShouldInvalidate ",
      DumpSelectionInfo());
}

TEST_F(LayoutSelectionTest, MoveOnSameNode_StartAndEnd) {
  const SelectionInDOMTree& selection = SetSelectionTextToBody("f^oob|ar");
  Selection().SetSelection(selection, SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  'foobar', StartAndEnd(1,4), ShouldInvalidate ",
      DumpSelectionInfo());

  // Paint virtually and clear ShouldInvalidate flag.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  'foobar', StartAndEnd(1,4), NotInvalidate ",
      DumpSelectionInfo());

  // "f^ooba|r"
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(selection.Anchor(),
                            {selection.Focus().AnchorNode(), 5})
          .Build(),
      SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  // "foobar" should be invalidated.
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  'foobar', StartAndEnd(1,5), ShouldInvalidate ",
      DumpSelectionInfo());
}

TEST_F(LayoutSelectionTest, MoveOnSameNode_StartAndEnd_Collapse) {
  const SelectionInDOMTree& selection = SetSelectionTextToBody("f^oob|ar");
  Selection().SetSelection(selection, SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  'foobar', StartAndEnd(1,4), ShouldInvalidate ",
      DumpSelectionInfo());

  // Paint virtually and clear ShouldInvalidate flag.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  'foobar', StartAndEnd(1,4), NotInvalidate ",
      DumpSelectionInfo());

  // "foo^|bar"
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .Collapse({selection.Anchor().AnchorNode(), 3})
                               .Build(),
                           SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  // "foobar" should be invalidated.
  EXPECT_EQ(
      "BODY, None, NotInvalidate \n"
      "  'foobar', None, ShouldInvalidate ",
      DumpSelectionInfo());
}

TEST_F(LayoutSelectionTest, ContentEditableButton) {
  SetBodyContent("<input type=button value=foo contenteditable>");
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .SelectAllChildren(*GetDocument().body())
                               .Build(),
                           SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  INPUT, Contain, NotInvalidate \n"
      "    #shadow-root \n"
      "      'foo', StartAndEnd(0,3), ShouldInvalidate ",
      DumpSelectionInfo());
}

TEST_F(LayoutSelectionTest, ClearSelection) {
  Selection().SetSelection(SetSelectionTextToBody("<div>f^o|o</div>"),
                           SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  DIV, Contain, NotInvalidate \n"
      "    'foo', StartAndEnd(1,2), ShouldInvalidate ",
      DumpSelectionInfo());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  DIV, Contain, NotInvalidate \n"
      "    'foo', StartAndEnd(1,2), NotInvalidate ",
      DumpSelectionInfo());

  Selection().Clear();
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, None, NotInvalidate \n"
      "  DIV, None, NotInvalidate \n"
      "    'foo', None, ShouldInvalidate ",
      DumpSelectionInfo());
}

TEST_F(LayoutSelectionTest, SVG) {
  const SelectionInDOMTree& selection =
      SetSelectionTextToBody("<svg><text x=10 y=10>fo^o|bar</text></svg>");
  Selection().SetSelection(selection, SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  // LayoutSVGText should be invalidate though it is kContain.
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  svg, Contain, NotInvalidate \n"
      "    text, Contain, ShouldInvalidate \n"
      "      'foobar', StartAndEnd(2,3), ShouldInvalidate ",
      DumpSelectionInfo());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  svg, Contain, NotInvalidate \n"
      "    text, Contain, NotInvalidate \n"
      "      'foobar', StartAndEnd(2,3), NotInvalidate ",
      DumpSelectionInfo());

  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(selection.Anchor(),
                            {selection.Focus().AnchorNode(), 4})
          .Build(),
      SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  svg, Contain, NotInvalidate \n"
      "    text, Contain, ShouldInvalidate \n"
      "      'foobar', StartAndEnd(2,4), ShouldInvalidate ",
      DumpSelectionInfo());
}

// crbug.com/781705
TEST_F(LayoutSelectionTest, SVGAncestor) {
  const SelectionInDOMTree& selection = SetSelectionTextToBody(
      "<svg><text x=10 y=10><tspan>fo^o|bar</tspan></text></svg>");
  Selection().SetSelection(selection, SetSelectionOptions());
  // LayoutSVGText should be invalidated.
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  svg, Contain, NotInvalidate \n"
      "    text, Contain, ShouldInvalidate \n"
      "      tspan, Contain, NotInvalidate \n"
      "        'foobar', StartAndEnd(2,3), ShouldInvalidate ",
      DumpSelectionInfo());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  svg, Contain, NotInvalidate \n"
      "    text, Contain, NotInvalidate \n"
      "      tspan, Contain, NotInvalidate \n"
      "        'foobar', StartAndEnd(2,3), NotInvalidate ",
      DumpSelectionInfo());

  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(selection.Anchor(),
                            {selection.Focus().AnchorNode(), 4})
          .Build(),
      SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  svg, Contain, NotInvalidate \n"
      "    text, Contain, ShouldInvalidate \n"
      "      tspan, Contain, NotInvalidate \n"
      "        'foobar', StartAndEnd(2,4), ShouldInvalidate ",
      DumpSelectionInfo());
}

TEST_F(LayoutSelectionTest, Embed) {
  Selection().SetSelection(
      SetSelectionTextToBody("^<embed type=foobar></embed>|"),
      SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  EMBED, StartAndEnd, ShouldInvalidate \n"
      "    #shadow-root \n"
      "      SLOT, <null LayoutObject> ",
      DumpSelectionInfo());
}

// http:/crbug.com/843144
TEST_F(LayoutSelectionTest, Ruby) {
  Selection().SetSelection(
      SetSelectionTextToBody("^<ruby>foo<rt>bar</rt></ruby>|"),
      SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  RUBY, Contain, NotInvalidate \n"
      "    'foo', Start(0,3), ShouldInvalidate \n"
      "    RT, Contain, NotInvalidate \n"
      "      'bar', End(0,3), ShouldInvalidate ",
      DumpSelectionInfo());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  RUBY, Contain, NotInvalidate \n"
      "    'foo', Start(0,3), NotInvalidate \n"
      "    RT, Contain, NotInvalidate \n"
      "      'bar', End(0,3), NotInvalidate ",
      DumpSelectionInfo());

  Selection().Clear();
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, None, NotInvalidate \n"
      "  RUBY, None, NotInvalidate \n"
      "    'foo', None, ShouldInvalidate \n"
      "    RT, None, NotInvalidate \n"
      "      'bar', None, ShouldInvalidate ",
      DumpSelectionInfo());
}

TEST_F(LayoutSelectionTest, ClearByRemoveNode) {
  Selection().SetSelection(SetSelectionTextToBody("^foo<span>bar</span>baz|"),
                           SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  'foo', Start(0,3), ShouldInvalidate \n"
      "  SPAN, Contain, NotInvalidate \n"
      "    'bar', Inside(0,3), ShouldInvalidate \n"
      "  'baz', End(0,3), ShouldInvalidate ",
      DumpSelectionInfo());

  Node* baz = GetDocument().body()->lastChild();
  baz->remove();
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  'foo', Start(0,3), ShouldInvalidate \n"
      "  SPAN, Contain, NotInvalidate \n"
      "    'bar', End(0,3), ShouldInvalidate ",
      DumpSelectionInfo());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  'foo', Start(0,3), NotInvalidate \n"
      "  SPAN, Contain, NotInvalidate \n"
      "    'bar', End(0,3), NotInvalidate ",
      DumpSelectionInfo());
}

TEST_F(LayoutSelectionTest, ClearByRemoveLayoutObject) {
  Selection().SetSelection(
      SetSelectionTextToBody("^foo<span>bar</span><span>baz</span>|"),
      SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  'foo', Start(0,3), ShouldInvalidate \n"
      "  SPAN, Contain, NotInvalidate \n"
      "    'bar', Inside(0,3), ShouldInvalidate \n"
      "  SPAN, Contain, NotInvalidate \n"
      "    'baz', End(0,3), ShouldInvalidate ",
      DumpSelectionInfo());

  auto* span_baz = To<Element>(GetDocument().body()->lastChild());
  span_baz->SetInlineStyleProperty(CSSPropertyID::kDisplay, CSSValueID::kNone);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  'foo', Start(0,3), ShouldInvalidate \n"
      "  SPAN, Contain, NotInvalidate \n"
      "    'bar', End(0,3), ShouldInvalidate \n"
      "  SPAN, <null LayoutObject> \n"
      "    'baz', <null LayoutObject> ",
      DumpSelectionInfo());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  'foo', Start(0,3), NotInvalidate \n"
      "  SPAN, Contain, NotInvalidate \n"
      "    'bar', End(0,3), NotInvalidate \n"
      "  SPAN, <null LayoutObject> \n"
      "    'baz', <null LayoutObject> ",
      DumpSelectionInfo());
}

TEST_F(LayoutSelectionTest, ClearBySlotChange) {
  Selection().SetSelection(SetSelectionTextToBody("<div>"
                                                  "<template data-mode=open>"
                                                  "^Foo<slot name=s1></slot>|"
                                                  "</template>"
                                                  "baz<span slot=s1>bar</span>"
                                                  "</div>"),
                           SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  DIV, Contain, NotInvalidate \n"
      "    #shadow-root \n"
      "      'Foo', Start(0,3), ShouldInvalidate \n"
      "      SLOT, <null LayoutObject> \n"
      "    'baz', <null LayoutObject> \n"
      "    SPAN, Contain, NotInvalidate \n"
      "      'bar', End(0,3), ShouldInvalidate ",
      DumpSelectionInfo());
  Element* slot =
      GetDocument().body()->firstChild()->GetShadowRoot()->QuerySelector(
          AtomicString("slot"));
  slot->setAttribute(html_names::kNameAttr, AtomicString("s2"));
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  DIV, Contain, NotInvalidate \n"
      "    #shadow-root \n"
      "      'Foo', StartAndEnd(0,3), ShouldInvalidate \n"
      "      SLOT, <null LayoutObject> \n"
      "    'baz', <null LayoutObject> \n"
      "    SPAN, <null LayoutObject> \n"
      "      'bar', <null LayoutObject> ",
      DumpSelectionInfo());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  DIV, Contain, NotInvalidate \n"
      "    #shadow-root \n"
      "      'Foo', StartAndEnd(0,3), NotInvalidate \n"
      "      SLOT, <null LayoutObject> \n"
      "    'baz', <null LayoutObject> \n"
      "    SPAN, <null LayoutObject> \n"
      "      'bar', <null LayoutObject> ",
      DumpSelectionInfo());
}

TEST_F(LayoutSelectionTest, MoveNode) {
  Selection().SetSelection(
      SetSelectionTextToBody(
          "<div id='div1'></div><div id='div2'>^foo<b>ba|r</b></div>"),
      SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  DIV, None, NotInvalidate \n"
      "  DIV, Contain, NotInvalidate \n"
      "    'foo', Start(0,3), ShouldInvalidate \n"
      "    B, Contain, NotInvalidate \n"
      "      'bar', End(0,2), ShouldInvalidate ",
      DumpSelectionInfo());
  Node* div1 = GetDocument().QuerySelector(AtomicString("#div1"));
  Node* div2 = GetDocument().QuerySelector(AtomicString("#div2"));
  div1->appendChild(div2);
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  DIV, None, NotInvalidate \n"
      "    DIV, <null LayoutObject> \n"
      "      'foo', <null LayoutObject> \n"
      "      B, <null LayoutObject> \n"
      "        'bar', <null LayoutObject> ",
      DumpSelectionInfo());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      "BODY, None, NotInvalidate \n"
      "  DIV, None, NotInvalidate \n"
      "    DIV, None, NotInvalidate \n"
      "      'foo', None, NotInvalidate \n"
      "      B, None, NotInvalidate \n"
      "        'bar', None, NotInvalidate ",
      DumpSelectionInfo());
}

// http://crbug.com/870734
TEST_F(LayoutSelectionTest, InvalidateSlot) {
  Selection().SetSelection(SetSelectionTextToBody("^<div>"
                                                  "<template data-mode=open>"
                                                  "<slot></slot>"
                                                  "</template>"
                                                  "foo"
                                                  "</div>|"),
                           SetSelectionOptions());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  DIV, Contain, NotInvalidate \n"
      "    #shadow-root \n"
      "      SLOT, <null LayoutObject> \n"
      "    'foo', StartAndEnd(0,3), NotInvalidate ",
      DumpSelectionInfo());

  Selection().Clear();
  Selection().CommitAppearanceIfNeeded();
  EXPECT_EQ(
      "BODY, None, NotInvalidate \n"
      "  DIV, None, NotInvalidate \n"
      "    #shadow-root \n"
      "      SLOT, <null LayoutObject> \n"
      "    'foo', None, ShouldInvalidate ",
      DumpSelectionInfo());
}

class NGLayoutSelectionTest
    : public LayoutSelectionTestBase,
      private ScopedPaintUnderInvalidationCheckingForTest {
 public:
  NGLayoutSelectionTest() : ScopedPaintUnderInvalidationCheckingForTest(true) {}

 protected:
  const Text* GetFirstTextNode() {
    for (const Node& runner : NodeTraversal::StartsAt(*GetDocument().body())) {
      if (auto* text_node = DynamicTo<Text>(runner))
        return text_node;
    }
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  bool IsFirstTextLineBreak(const std::string& selection_text) {
    SetSelectionAndUpdateLayoutSelection(selection_text);
    const LayoutText& first_text = *GetFirstTextNode()->GetLayoutObject();
    const LayoutSelectionStatus& status =
        ComputeLayoutSelectionStatus(first_text);
    return status.line_break == SelectSoftLineBreak::kSelected;
  }

  LayoutSelectionStatus ComputeLayoutSelectionStatus(const Node& node) {
    return ComputeLayoutSelectionStatus(*node.GetLayoutObject());
  }

  LayoutSelectionStatus ComputeLayoutSelectionStatus(
      const LayoutObject& layout_object) const {
    DCHECK(layout_object.IsText());
    InlineCursor cursor(*layout_object.FragmentItemsContainer());
    cursor.MoveTo(layout_object);
    return Selection().ComputeLayoutSelectionStatus(cursor);
  }

  SelectionState ComputePaintingSelectionStateForCursor(
      const LayoutObject& layout_object) const {
    DCHECK(layout_object.IsText());
    InlineCursor cursor;
    cursor.MoveTo(layout_object);
    return Selection().ComputePaintingSelectionStateForCursor(cursor.Current());
  }

  void SetSelectionAndUpdateLayoutSelection(const std::string& selection_text) {
    const SelectionInDOMTree& selection =
        SetSelectionTextToBody(selection_text);
    Selection().SetSelection(selection, SetSelectionOptions());
    Selection().CommitAppearanceIfNeeded();
  }
};

std::ostream& operator<<(std::ostream& ostream,
                         const LayoutSelectionStatus& status) {
  const String line_break =
      (status.line_break == SelectSoftLineBreak::kSelected) ? "kSelected"
                                                            : "kNotSelected";
  return ostream << status.start << ", " << status.end << ", " << std::boolalpha
                 << line_break;
}

TEST_F(NGLayoutSelectionTest, SelectOnOneText) {
  SetSelectionAndUpdateLayoutSelection("foo<span>b^a|r</span>");
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  'foo', None, NotInvalidate \n"
      "  SPAN, Contain, NotInvalidate \n"
      "    'bar', StartAndEnd(1,2), ShouldInvalidate ",
      DumpSelectionInfo());
}

TEST_F(NGLayoutSelectionTest, FirstLetterInAnotherBlockFlow) {
  SetSelectionAndUpdateLayoutSelection(
      "<style>:first-letter { float: right}</style>^fo|o");
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  <style> \n"
      "  'foo', StartAndEnd(0,1), ShouldInvalidate \n"
      "    :first-letter, None(0,1), ShouldInvalidate ",
      DumpSelectionInfo());
}

TEST_F(NGLayoutSelectionTest, TwoNGBlockFlows) {
  SetSelectionAndUpdateLayoutSelection("<div>f^oo</div><div>ba|r</div>");
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  DIV, Contain, NotInvalidate \n"
      "    'foo', Start(1,3), ShouldInvalidate \n"
      "  DIV, Contain, NotInvalidate \n"
      "    'bar', End(0,2), ShouldInvalidate ",
      DumpSelectionInfo());
  LayoutObject* const foo =
      GetDocument().body()->firstChild()->firstChild()->GetLayoutObject();
  EXPECT_EQ(LayoutSelectionStatus(1u, 3u, SelectSoftLineBreak::kSelected),
            ComputeLayoutSelectionStatus(*foo));
  EXPECT_EQ(SelectionState::kStart,
            ComputePaintingSelectionStateForCursor(*foo));
  LayoutObject* const bar = GetDocument()
                                .body()
                                ->firstChild()
                                ->nextSibling()
                                ->firstChild()
                                ->GetLayoutObject();
  EXPECT_EQ(LayoutSelectionStatus(0u, 2u, SelectSoftLineBreak::kNotSelected),
            ComputeLayoutSelectionStatus(*bar));
  EXPECT_EQ(SelectionState::kEnd, ComputePaintingSelectionStateForCursor(*bar));
}

TEST_F(NGLayoutSelectionTest, StartAndEndState) {
  SetSelectionAndUpdateLayoutSelection("<div>f^oo|</div><div>bar</div>");
  LayoutObject* const foo =
      GetDocument().body()->firstChild()->firstChild()->GetLayoutObject();
  EXPECT_EQ(LayoutSelectionStatus(1u, 3u, SelectSoftLineBreak::kNotSelected),
            ComputeLayoutSelectionStatus(*foo));
  EXPECT_EQ(SelectionState::kStartAndEnd,
            ComputePaintingSelectionStateForCursor(*foo));
  LayoutObject* const bar = GetDocument()
                                .body()
                                ->firstChild()
                                ->nextSibling()
                                ->firstChild()
                                ->GetLayoutObject();
  EXPECT_EQ(LayoutSelectionStatus(0u, 0u, SelectSoftLineBreak::kNotSelected),
            ComputeLayoutSelectionStatus(*bar));
  EXPECT_EQ(SelectionState::kNone,
            ComputePaintingSelectionStateForCursor(*bar));
}

TEST_F(NGLayoutSelectionTest, UnpaintedStartAndEndState) {
  SetSelectionAndUpdateLayoutSelection(
      "<img width=10px height=10px>^<div>\n<span "
      "id=selected>foo</span>\n</div>|<img width=10px height=10px>"
      "<div id=trailing>bar</div>");
  LayoutObject* const foo =
      GetElementById("selected")->firstChild()->GetLayoutObject();
  EXPECT_EQ(LayoutSelectionStatus(0u, 3u, SelectSoftLineBreak::kSelected),
            ComputeLayoutSelectionStatus(*foo));
  EXPECT_EQ(SelectionState::kStartAndEnd,
            ComputePaintingSelectionStateForCursor(*foo));
  LayoutObject* const bar =
      GetElementById("trailing")->firstChild()->GetLayoutObject();
  EXPECT_EQ(LayoutSelectionStatus(0u, 0u, SelectSoftLineBreak::kNotSelected),
            ComputeLayoutSelectionStatus(*bar));
  EXPECT_EQ(SelectionState::kNone,
            ComputePaintingSelectionStateForCursor(*bar));
}

TEST_F(NGLayoutSelectionTest, StartAndEndMultilineState) {
  SetSelectionAndUpdateLayoutSelection(
      "<div style='white-space:pre'>f^oo\nbar\nba|z</div>");
  LayoutObject* const div_text =
      GetDocument().body()->firstChild()->firstChild()->GetLayoutObject();

  InlineCursor cursor(*(div_text->FragmentItemsContainer()));
  cursor.MoveTo(*div_text);
  EXPECT_EQ(LayoutSelectionStatus(1u, 3u, SelectSoftLineBreak::kNotSelected),
            Selection().ComputeLayoutSelectionStatus(cursor));
  EXPECT_EQ(
      SelectionState::kStart,
      Selection().ComputePaintingSelectionStateForCursor(cursor.Current()));

  // Move to 'bar' text.
  cursor.MoveToNext();
  cursor.MoveToNext();
  cursor.MoveToNext();
  EXPECT_EQ(LayoutSelectionStatus(4u, 7u, SelectSoftLineBreak::kNotSelected),
            Selection().ComputeLayoutSelectionStatus(cursor));
  EXPECT_EQ(
      SelectionState::kInside,
      Selection().ComputePaintingSelectionStateForCursor(cursor.Current()));

  // Move to 'baz' text.
  cursor.MoveToNext();
  cursor.MoveToNext();
  cursor.MoveToNext();
  EXPECT_EQ(LayoutSelectionStatus(8u, 10u, SelectSoftLineBreak::kNotSelected),
            Selection().ComputeLayoutSelectionStatus(cursor));
  EXPECT_EQ(
      SelectionState::kEnd,
      Selection().ComputePaintingSelectionStateForCursor(cursor.Current()));
}

TEST_F(NGLayoutSelectionTest, BeforeStartAndAfterEndMultilineState) {
  SetSelectionAndUpdateLayoutSelection(
      "<div style='white-space:pre'>foo\nba^r</div><div "
      "style='white-space:pre'>ba|z\nquu</div>");
  LayoutObject* const div_text =
      GetDocument().body()->firstChild()->firstChild()->GetLayoutObject();
  InlineCursor cursor(*(div_text->FragmentItemsContainer()));
  cursor.MoveTo(*div_text);
  EXPECT_EQ(LayoutSelectionStatus(3u, 3u, SelectSoftLineBreak::kNotSelected),
            Selection().ComputeLayoutSelectionStatus(cursor));
  EXPECT_EQ(
      SelectionState::kNone,
      Selection().ComputePaintingSelectionStateForCursor(cursor.Current()));

  // Move to 'bar' text.
  cursor.MoveToNext();
  cursor.MoveToNext();
  cursor.MoveToNext();
  EXPECT_EQ(LayoutSelectionStatus(6u, 7u, SelectSoftLineBreak::kSelected),
            Selection().ComputeLayoutSelectionStatus(cursor));
  EXPECT_EQ(
      SelectionState::kStart,
      Selection().ComputePaintingSelectionStateForCursor(cursor.Current()));

  LayoutObject* const second_div_text =
      GetDocument().body()->lastChild()->firstChild()->GetLayoutObject();
  InlineCursor second_cursor(*(second_div_text->FragmentItemsContainer()));
  second_cursor.MoveTo(*second_div_text);
  EXPECT_EQ(LayoutSelectionStatus(0u, 2u, SelectSoftLineBreak::kNotSelected),
            Selection().ComputeLayoutSelectionStatus(second_cursor));
  EXPECT_EQ(SelectionState::kEnd,
            Selection().ComputePaintingSelectionStateForCursor(
                second_cursor.Current()));

  // Move to 'quu' text.
  second_cursor.MoveToNext();
  second_cursor.MoveToNext();
  second_cursor.MoveToNext();
  EXPECT_EQ(LayoutSelectionStatus(4u, 4u, SelectSoftLineBreak::kNotSelected),
            Selection().ComputeLayoutSelectionStatus(second_cursor));
  EXPECT_EQ(SelectionState::kNone,
            Selection().ComputePaintingSelectionStateForCursor(
                second_cursor.Current()));
}

// TODO(editing-dev): Once LayoutNG supports editing, we should change this
// test to use LayoutNG tree.
TEST_F(NGLayoutSelectionTest, MixedBlockFlowsAsSibling) {
  SetSelectionAndUpdateLayoutSelection(
      "<div>f^oo</div>"
      "<div contenteditable>ba|r</div>");
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  DIV, Contain, NotInvalidate \n"
      "    'foo', Start(1,3), ShouldInvalidate \n"
      "  DIV, Contain, NotInvalidate \n"
      "    'bar', End(0,2), ShouldInvalidate ",
      DumpSelectionInfo());
}

// TODO(editing-dev): Once LayoutNG supports editing, we should change this
// test to use LayoutNG tree.
TEST_F(NGLayoutSelectionTest, MixedBlockFlowsAnscestor) {
  // Both "foo" and "bar" for DIV elements should be legacy LayoutBlock.
  SetSelectionAndUpdateLayoutSelection(
      "<div contenteditable>f^oo"
      "<div contenteditable=false>ba|r</div></div>");
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  DIV, Contain, NotInvalidate \n"
      "    'foo', Start(1,3), ShouldInvalidate \n"
      "    DIV, Contain, NotInvalidate \n"
      "      'bar', End(0,2), ShouldInvalidate ",
      DumpSelectionInfo());
}

// TODO(editing-dev): Once LayoutNG supports editing, we should change this
// test to use LayoutNG tree.
TEST_F(NGLayoutSelectionTest, MixedBlockFlowsDecendant) {
  SetSelectionAndUpdateLayoutSelection(
      "<div contenteditable=false>f^oo"
      "<div contenteditable>ba|r</div></div>");
  EXPECT_EQ(
      "BODY, Contain, NotInvalidate \n"
      "  DIV, Contain, NotInvalidate \n"
      "    'foo', Start(1,3), ShouldInvalidate \n"
      "    DIV, Contain, NotInvalidate \n"
      "      'bar', End(0,2), ShouldInvalidate ",
      DumpSelectionInfo());
}

TEST_F(NGLayoutSelectionTest, LineBreakBasic) {
  LoadAhem();
  EXPECT_FALSE(IsFirstTextLineBreak("<div>f^oo<br>ba|r</div>"));
  EXPECT_FALSE(IsFirstTextLineBreak("<div>^foo<br><br>|</div>"));
  EXPECT_TRUE(IsFirstTextLineBreak(
      "<div style='font: Ahem; width: 2em'>f^oo ba|r</div>"));
  EXPECT_TRUE(IsFirstTextLineBreak("<div>f^oo</div><div>b|ar</div>"));
  EXPECT_FALSE(IsFirstTextLineBreak("<div>f^oo |</div>"));
  EXPECT_FALSE(IsFirstTextLineBreak("<div>f^oo <!--|--></div>"));
  EXPECT_FALSE(IsFirstTextLineBreak("<div>f^oo </div>|"));
  EXPECT_FALSE(IsFirstTextLineBreak("<div>f^oo|</div>"));
  EXPECT_FALSE(IsFirstTextLineBreak("<div>f^oo<!--|--></div>"));
  EXPECT_FALSE(IsFirstTextLineBreak("<div>f^oo</div>|"));
}

TEST_F(NGLayoutSelectionTest, LineBreakInlineBlock) {
  LoadAhem();
  EXPECT_FALSE(
      IsFirstTextLineBreak("<div style='display:inline-block'>^x</div>y|"));
  EXPECT_FALSE(
      IsFirstTextLineBreak("<div style='display:inline-block'>f^oo</div>bar|"));
}

TEST_F(NGLayoutSelectionTest, LineBreakImage) {
  SetSelectionAndUpdateLayoutSelection(
      "<div>^<img id=img1 width=10px height=10px>foo<br>"
      "bar<img id=img2 width=10px height=10px>|</div>");
  Node* const foo =
      GetDocument().body()->firstChild()->firstChild()->nextSibling();
  EXPECT_EQ(SelectSoftLineBreak::kNotSelected,
            ComputeLayoutSelectionStatus(*foo).line_break);
  Node* const bar = foo->nextSibling()->nextSibling();
  EXPECT_EQ(SelectSoftLineBreak::kNotSelected,
            ComputeLayoutSelectionStatus(*bar).line_break);
}

TEST_F(NGLayoutSelectionTest, BRStatus) {
  const SelectionInDOMTree& selection =
      SetSelectionTextToBody("<div>foo<!--^--><br><!--|-->bar</div>");
  Selection().SetSelection(selection, SetSelectionOptions());
  Selection().CommitAppearanceIfNeeded();
  LayoutObject* const layout_br =
      GetDocument().QuerySelector(AtomicString("br"))->GetLayoutObject();
  CHECK(layout_br->IsBR());
  EXPECT_EQ(LayoutSelectionStatus(3u, 4u, SelectSoftLineBreak::kNotSelected),
            ComputeLayoutSelectionStatus(*layout_br));
  EXPECT_EQ(SelectionState::kStartAndEnd,
            ComputePaintingSelectionStateForCursor(*layout_br));
}

// https://crbug.com/907186
TEST_F(NGLayoutSelectionTest, WBRStatus) {
  SetSelectionAndUpdateLayoutSelection(
      "<div style=\"width:0\">^foo<wbr>bar|</div>");
  const LayoutObject* layout_wbr =
      GetDocument().QuerySelector(AtomicString("wbr"))->GetLayoutObject();
  EXPECT_EQ(LayoutSelectionStatus(3u, 4u, SelectSoftLineBreak::kSelected),
            ComputeLayoutSelectionStatus(*layout_wbr));
  EXPECT_EQ(SelectionState::kInside,
            ComputePaintingSelectionStateForCursor(*layout_wbr));
}

TEST_F(NGLayoutSelectionTest, SoftHyphen0to1) {
  SetSelectionAndUpdateLayoutSelection(
      "<div id='container' style='width:3ch'>^0|123&shy;456</div>");
  auto* element = GetElementById("container");
  auto* block_flow = To<LayoutBlockFlow>(element->GetLayoutObject());
  InlineCursor cursor(*block_flow);
  while (!cursor.Current()->IsLayoutGeneratedText())
    cursor.MoveToNext();
  auto status = Selection().ComputeLayoutSelectionStatus(cursor);
  EXPECT_FALSE(status.HasValidRange());
}

TEST_F(NGLayoutSelectionTest, SoftHyphen0to4) {
  SetSelectionAndUpdateLayoutSelection(
      "<div id='container' style='width:3ch'>^0123|&shy;456</div>");
  auto* element = GetElementById("container");
  auto* block_flow = To<LayoutBlockFlow>(element->GetLayoutObject());
  InlineCursor cursor(*block_flow);
  while (!cursor.Current()->IsLayoutGeneratedText())
    cursor.MoveToNext();
  auto status = Selection().ComputeLayoutSelectionStatus(cursor);
  EXPECT_FALSE(status.HasValidRange());
}

TEST_F(NGLayoutSelectionTest, SoftHyphen1to5) {
  SetSelectionAndUpdateLayoutSelection(
      "<div id='container' style='width:3ch'>0^123&shy;|456</div>");
  auto* element = GetElementById("container");
  auto* block_flow = To<LayoutBlockFlow>(element->GetLayoutObject());
  InlineCursor cursor(*block_flow);
  while (!cursor.Current()->IsLayoutGeneratedText())
    cursor.MoveToNext();
  auto status = Selection().ComputeLayoutSelectionStatus(cursor);
  EXPECT_TRUE(status.HasValidRange());
  EXPECT_EQ(LayoutSelectionStatus(0u, 1u, SelectSoftLineBreak::kNotSelected),
            status);
}

}  // namespace blink
