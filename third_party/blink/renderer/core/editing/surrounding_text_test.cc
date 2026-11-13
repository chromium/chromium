// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/surrounding_text.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class SurroundingTextTest : public PageTestBase {
 protected:
  void SetHtml(const String&);
  EphemeralRange Select(int offset) { return Select(offset, offset); }
  EphemeralRange Select(int start, int end);
};

void SurroundingTextTest::SetHtml(const String& content) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(content);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
}

EphemeralRange SurroundingTextTest::Select(int start, int end) {
  Element* element = GetElementById("selection");
  return EphemeralRange(Position(element->firstChild(), start),
                        Position(element->firstChild(), end));
}

TEST_F(SurroundingTextTest, BasicCaretSelection) {
  SetHtml(String("<p id='selection'>foo bar</p>"));

  {
    EphemeralRange selection = Select(0);
    SurroundingText surrounding_text(selection, 1);

    EXPECT_EQ("f", surrounding_text.TextContent());
    EXPECT_EQ(0u, surrounding_text.StartOffsetInTextContent());
    EXPECT_EQ(0u, surrounding_text.EndOffsetInTextContent());
  }

  {
    EphemeralRange selection = Select(0);
    SurroundingText surrounding_text(selection, 5);

    // maxlength/2 is used on the left and right.
    EXPECT_EQ("foo",
              String(surrounding_text.TextContent()).SimplifyWhiteSpace());
    EXPECT_EQ(1u, surrounding_text.StartOffsetInTextContent());
    EXPECT_EQ(1u, surrounding_text.EndOffsetInTextContent());
  }

  {
    EphemeralRange selection = Select(0);
    SurroundingText surrounding_text(selection, 42);

    EXPECT_EQ("foo bar",
              String(surrounding_text.TextContent()).SimplifyWhiteSpace());
    EXPECT_EQ(1u, surrounding_text.StartOffsetInTextContent());
    EXPECT_EQ(1u, surrounding_text.EndOffsetInTextContent());
  }

  {
    EphemeralRange selection = Select(7);
    SurroundingText surrounding_text(selection, 42);

    EXPECT_EQ("foo bar",
              String(surrounding_text.TextContent()).SimplifyWhiteSpace());
    EXPECT_EQ(8u, surrounding_text.StartOffsetInTextContent());
    EXPECT_EQ(8u, surrounding_text.EndOffsetInTextContent());
  }

  {
    EphemeralRange selection = Select(6);
    SurroundingText surrounding_text(selection, 2);

    EXPECT_EQ("ar", surrounding_text.TextContent());
    EXPECT_EQ(1u, surrounding_text.StartOffsetInTextContent());
    EXPECT_EQ(1u, surrounding_text.EndOffsetInTextContent());
  }

  {
    EphemeralRange selection = Select(6);
    SurroundingText surrounding_text(selection, 42);

    EXPECT_EQ("foo bar",
              String(surrounding_text.TextContent()).SimplifyWhiteSpace());
    EXPECT_EQ(7u, surrounding_text.StartOffsetInTextContent());
    EXPECT_EQ(7u, surrounding_text.EndOffsetInTextContent());
  }
}

TEST_F(SurroundingTextTest, BasicRangeSelection) {
  SetHtml("<p id='selection'>Lorem ipsum dolor sit amet</p>");

  {
    EphemeralRange selection = Select(0, 5);
    SurroundingText surrounding_text(selection, 1);

    EXPECT_EQ("Lorem ", surrounding_text.TextContent());
    EXPECT_EQ(0u, surrounding_text.StartOffsetInTextContent());
    EXPECT_EQ(5u, surrounding_text.EndOffsetInTextContent());
  }

  {
    EphemeralRange selection = Select(0, 5);
    SurroundingText surrounding_text(selection, 5);

    EXPECT_EQ("Lorem ip",
              String(surrounding_text.TextContent()).SimplifyWhiteSpace());
    EXPECT_EQ(1u, surrounding_text.StartOffsetInTextContent());
    EXPECT_EQ(6u, surrounding_text.EndOffsetInTextContent());
  }

  {
    EphemeralRange selection = Select(0, 5);
    SurroundingText surrounding_text(selection, 42);

    EXPECT_EQ("Lorem ipsum dolor sit amet",
              String(surrounding_text.TextContent()).SimplifyWhiteSpace());
    EXPECT_EQ(1u, surrounding_text.StartOffsetInTextContent());
    EXPECT_EQ(6u, surrounding_text.EndOffsetInTextContent());
  }

  {
    EphemeralRange selection = Select(6, 11);
    SurroundingText surrounding_text(selection, 2);

    EXPECT_EQ(" ipsum ", surrounding_text.TextContent());
    EXPECT_EQ(1u, surrounding_text.StartOffsetInTextContent());
    EXPECT_EQ(6u, surrounding_text.EndOffsetInTextContent());
  }

  {
    EphemeralRange selection = Select(6, 11);
    SurroundingText surrounding_text(selection, 42);

    EXPECT_EQ("Lorem ipsum dolor sit amet",
              String(surrounding_text.TextContent()).SimplifyWhiteSpace());
    EXPECT_EQ(7u, surrounding_text.StartOffsetInTextContent());
    EXPECT_EQ(12u, surrounding_text.EndOffsetInTextContent());
  }

  {
    // Last word.
    EphemeralRange selection = Select(22, 26);
    SurroundingText surrounding_text(selection, 8);

    EXPECT_EQ("sit amet", surrounding_text.TextContent());
    EXPECT_EQ(4u, surrounding_text.StartOffsetInTextContent());
    EXPECT_EQ(8u, surrounding_text.EndOffsetInTextContent());
  }
}

TEST_F(SurroundingTextTest, TreeCaretSelection) {
  SetHtml(
      "<div>This is outside of <p id='selection'>foo bar</p> the "
      "selected node</div>");

  {
    EphemeralRange selection = Select(0);
    SurroundingText surrounding_text(selection, 1);

    EXPECT_EQ("f", surrounding_text.TextContent());
    EXPECT_EQ(0u, surrounding_text.StartOffsetInTextContent());
    EXPECT_EQ(0u, surrounding_text.EndOffsetInTextContent());
  }

  {
    EphemeralRange selection = Select(0);
    SurroundingText surrounding_text(selection, 5);

    EXPECT_EQ("foo",
              String(surrounding_text.TextContent()).SimplifyWhiteSpace());
    EXPECT_EQ(1u, surrounding_text.StartOffsetInTextContent());
    EXPECT_EQ(1u, surrounding_text.EndOffsetInTextContent());
  }

  {
    EphemeralRange selection = Select(0);
    SurroundingText surrounding_text(selection, 1337);

    EXPECT_EQ("This is outside of foo bar the selected node",
              String(surrounding_text.TextContent()).SimplifyWhiteSpace());
    EXPECT_EQ(20u, surrounding_text.StartOffsetInTextContent());
    EXPECT_EQ(20u, surrounding_text.EndOffsetInTextContent());
  }

  {
    EphemeralRange selection = Select(6);
    SurroundingText surrounding_text(selection, 2);

    EXPECT_EQ("ar", surrounding_text.TextContent());
    EXPECT_EQ(1u, surrounding_text.StartOffsetInTextContent());
    EXPECT_EQ(1u, surrounding_text.EndOffsetInTextContent());
  }

  {
    EphemeralRange selection = Select(6);
    SurroundingText surrounding_text(selection, 1337);

    EXPECT_EQ("This is outside of foo bar the selected node",
              String(surrounding_text.TextContent()).SimplifyWhiteSpace());
    EXPECT_EQ(26u, surrounding_text.StartOffsetInTextContent());
    EXPECT_EQ(26u, surrounding_text.EndOffsetInTextContent());
  }
}

TEST_F(SurroundingTextTest, TreeRangeSelection) {
  SetHtml(
      "<div>This is outside of <p id='selection'>foo bar</p> the "
      "selected node</div>");

  {
    EphemeralRange selection = Select(0, 1);
    SurroundingText surrounding_text(selection, 1);

    EXPECT_EQ("fo",
              String(surrounding_text.TextContent()).SimplifyWhiteSpace());
    EXPECT_EQ(0u, surrounding_text.StartOffsetInTextContent());
    EXPECT_EQ(1u, surrounding_text.EndOffsetInTextContent());
  }

  {
    EphemeralRange selection = Select(0, 3);
    SurroundingText surrounding_text(selection, 12);

    EXPECT_EQ("e of foo bar",
              String(surrounding_text.TextContent()).SimplifyWhiteSpace());
    EXPECT_EQ(5u, surrounding_text.StartOffsetInTextContent());
    EXPECT_EQ(8u, surrounding_text.EndOffsetInTextContent());
  }

  {
    EphemeralRange selection = Select(0, 3);
    SurroundingText surrounding_text(selection, 1337);

    EXPECT_EQ("This is outside of foo bar the selected node",
              String(surrounding_text.TextContent()).SimplifyWhiteSpace());
    EXPECT_EQ(20u, surrounding_text.StartOffsetInTextContent());
    EXPECT_EQ(23u, surrounding_text.EndOffsetInTextContent());
  }

  {
    EphemeralRange selection = Select(4, 7);
    SurroundingText surrounding_text(selection, 12);

    EXPECT_EQ("foo bar the se",
              String(surrounding_text.TextContent()).SimplifyWhiteSpace());
    EXPECT_EQ(5u, surrounding_text.StartOffsetInTextContent());
    EXPECT_EQ(8u, surrounding_text.EndOffsetInTextContent());
  }

  {
    EphemeralRange selection = Select(0, 7);
    SurroundingText surrounding_text(selection, 1337);

    EXPECT_EQ("This is outside of foo bar the selected node",
              String(surrounding_text.TextContent()).SimplifyWhiteSpace());
    EXPECT_EQ(20u, surrounding_text.StartOffsetInTextContent());
    EXPECT_EQ(27u, surrounding_text.EndOffsetInTextContent());
  }
}

TEST_F(SurroundingTextTest, TextAreaSelection) {
  SetHtml(
      "<p>First paragraph</p>"
      "<textarea id='selection'>abc def ghi</textarea>"
      "<p>Second paragraph</p>");

  TextControlElement* text_ctrl =
      reinterpret_cast<TextControlElement*>(GetElementById("selection"));

  text_ctrl->SetSelectionRange(4, 7);
  EphemeralRange selection = text_ctrl->Selection().ComputeRange();

  SurroundingText surrounding_text(selection, 20);

  EXPECT_EQ("abc def ghi",
            String(surrounding_text.TextContent()).SimplifyWhiteSpace());
  EXPECT_EQ(4u, surrounding_text.StartOffsetInTextContent());
  EXPECT_EQ(7u, surrounding_text.EndOffsetInTextContent());
}

TEST_F(SurroundingTextTest, EmptyInputElementWithChild) {
  SetHtml("<input type=\"text\" id=\"input_name\"/>");

  TextControlElement* input_element =
      reinterpret_cast<TextControlElement*>(GetElementById("input_name"));
  input_element->SetInnerEditorValue("John Smith");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  // BODY
  //   INPUT
  //     #shadow-root
  // *      DIV id="inner-editor" (editable)
  //          #text "John Smith"

  const Element* inner_editor = input_element->InnerEditorElement();
  const Position start = Position(inner_editor, 0);
  const Position end = Position(inner_editor, 0);

  // Surrounding text should not crash. See http://crbug.com/758438.
  SurroundingText surrounding_text(EphemeralRange(start, end), 8);
  EXPECT_TRUE(surrounding_text.TextContent().empty());
}

TEST_F(SurroundingTextTest, ButtonsAndParagraph) {
  SetHtml(
      "<button>.</button>12345"
      "<p id='selection'>6789 12345</p>"
      "6789<button>.</button>");

  {
    EphemeralRange selection = Select(0);
    SurroundingText surrounding_text(selection, 100);

    EXPECT_EQ("12345\n6789 12345\n\n6789", surrounding_text.TextContent());
    EXPECT_EQ(6u, surrounding_text.StartOffsetInTextContent());
    EXPECT_EQ(6u, surrounding_text.EndOffsetInTextContent());
  }

  {
    EphemeralRange selection = Select(5);
    SurroundingText surrounding_text(selection, 6);

    EXPECT_EQ("89 123", surrounding_text.TextContent());
    EXPECT_EQ(3u, surrounding_text.StartOffsetInTextContent());
    EXPECT_EQ(3u, surrounding_text.EndOffsetInTextContent());
  }

  {
    EphemeralRange selection = Select(0);
    SurroundingText surrounding_text(selection, 0);

    EXPECT_TRUE(surrounding_text.TextContent().empty());
  }

  {
    EphemeralRange selection = Select(5);
    SurroundingText surrounding_text(selection, 1);

    EXPECT_EQ("1", surrounding_text.TextContent());
    EXPECT_EQ(0u, surrounding_text.StartOffsetInTextContent());
    EXPECT_EQ(0u, surrounding_text.EndOffsetInTextContent());
  }

  {
    EphemeralRange selection = Select(6);
    SurroundingText surrounding_text(selection, 2);

    EXPECT_EQ("12", surrounding_text.TextContent());
    EXPECT_EQ(1u, surrounding_text.StartOffsetInTextContent());
    EXPECT_EQ(1u, surrounding_text.EndOffsetInTextContent());
  }
}

TEST_F(SurroundingTextTest, SelectElementAndText) {
  SetHtml(
      "<select>.</select>"
      "<div>57th Street and Lake Shore Drive</div>"
      " <span>Chicago</span> <span id='selection'>IL</span> <span>60637</span>"
      "<select>.</select>");

  EphemeralRange selection = Select(0);
  SurroundingText surrounding_text(selection, 100);

  EXPECT_EQ("\xEF\xBF\xBC\n57th Street and Lake Shore Drive\nChicago IL 60637",
            surrounding_text.TextContent().Utf8());
  EXPECT_EQ(43u, surrounding_text.StartOffsetInTextContent());
  EXPECT_EQ(43u, surrounding_text.EndOffsetInTextContent());
}

TEST_F(SurroundingTextTest, FieldsetElementAndText) {
  SetHtml(
      "<fieldset>.</fieldset>12345<button>abc</button>"
      "<p>6789<br><span id='selection'>12345</span></p>"
      "6789<textarea>abc</textarea>0123<fieldset>.</fieldset>");

  EphemeralRange selection = Select(0);
  SurroundingText surrounding_text(selection, 100);

  EXPECT_EQ("\n6789\n12345\n\n6789", surrounding_text.TextContent());
  EXPECT_EQ(6u, surrounding_text.StartOffsetInTextContent());
  EXPECT_EQ(6u, surrounding_text.EndOffsetInTextContent());
}

TEST_F(SurroundingTextTest, ButtonScriptAndComment) {
  SetHtml(
      "<button>.</button>"
      "<div id='selection'>This is <!-- comment --!>a test "
      "<script language='javascript'></script>"
      "example<button>.</button>");

  EphemeralRange selection = Select(0);
  SurroundingText surrounding_text(selection, 100);

  EXPECT_EQ("\nThis is a test example", surrounding_text.TextContent());
  EXPECT_EQ(1u, surrounding_text.StartOffsetInTextContent());
  EXPECT_EQ(1u, surrounding_text.EndOffsetInTextContent());
}

TEST_F(SurroundingTextTest, ButtonAndLongDiv) {
  SetHtml(
      "<button>.</button>"
      "<div id='selection'>012345678901234567890123456789</div>"
      "<button>.</button>");

  EphemeralRange selection = Select(15);
  SurroundingText surrounding_text(selection, 12);

  EXPECT_EQ("901234567890", surrounding_text.TextContent());
  EXPECT_EQ(6u, surrounding_text.StartOffsetInTextContent());
  EXPECT_EQ(6u, surrounding_text.EndOffsetInTextContent());
}

TEST_F(SurroundingTextTest, EmptySurroundingTextInOptionsAndButton) {
  SetHtml(
      "<option>.</option>12345"
      "<button id='selection'>test</button>"
      "<option>.</option>");

  {
    EphemeralRange selection = Select(1);
    SurroundingText surrounding_text(selection, 100);

    EXPECT_TRUE(surrounding_text.TextContent().empty());
  }

  {
    EphemeralRange selection = Select(3);
    SurroundingText surrounding_text(selection, 100);

    EXPECT_TRUE(surrounding_text.TextContent().empty());
  }
}

TEST_F(SurroundingTextTest, SingleDotParagraph) {
  SetHtml("<p id='selection'>.</p>");

  EphemeralRange selection = Select(0);
  SurroundingText surrounding_text(selection, 2);

  EXPECT_EQ("\n.", surrounding_text.TextContent());
  EXPECT_EQ(1u, surrounding_text.StartOffsetInTextContent());
  EXPECT_EQ(1u, surrounding_text.EndOffsetInTextContent());
}

TEST_F(SurroundingTextTest, ShadowDomRangeSelection) {
  SetHtml("<div>Before <span id='host'></span> After</div>");
  Element* host = GetElementById("host");
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.SetInnerHTMLWithoutTrustedTypes(
      "<span>Pafos </span><span id='selection'>12:45</span><span> Riga</span>");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Element* selection_el = shadow_root.getElementById(AtomicString("selection"));
  const EphemeralRange selection(Position(selection_el->firstChild(), 0),
                                 Position(selection_el->firstChild(), 5));

  {
    // Surrounding window stays inside the shadow tree.
    SurroundingText surrounding_text(selection, 4);
    EXPECT_EQ("s 12:45 R", surrounding_text.TextContent());
    EXPECT_EQ(2u, surrounding_text.StartOffsetInTextContent());
    EXPECT_EQ(7u, surrounding_text.EndOffsetInTextContent());
  }

  {
    // Surrounding window crosses out of the shadow tree into light DOM.
    SurroundingText surrounding_text(selection, 100);
    EXPECT_EQ("Before Pafos 12:45 Riga After",
              String(surrounding_text.TextContent()).SimplifyWhiteSpace());
    EXPECT_EQ("12:45", surrounding_text.TextContent().substr(
                           surrounding_text.StartOffsetInTextContent(),
                           surrounding_text.EndOffsetInTextContent() -
                               surrounding_text.StartOffsetInTextContent()));
  }

  {
    // FrameSelection-backed constructor (used by GetTextSurroundingSelection).
    Selection().SetSelection(
        SelectionInDomTree::Builder().SetBaseAndExtent(selection).Build(),
        SetSelectionOptions());
    SurroundingText surrounding_text(&GetFrame(), 100);
    EXPECT_FALSE(surrounding_text.IsEmpty());
    EXPECT_EQ("Before Pafos 12:45 Riga After",
              String(surrounding_text.TextContent()).SimplifyWhiteSpace());
    EXPECT_EQ("12:45", surrounding_text.TextContent().substr(
                           surrounding_text.StartOffsetInTextContent(),
                           surrounding_text.EndOffsetInTextContent() -
                               surrounding_text.StartOffsetInTextContent()));
  }
}

TEST_F(SurroundingTextTest, ShadowDomWithSlottedContentAndNestedShadow) {
  SetHtml(
      "<div id='outer'>"
      "<span slot='home'>Pafos</span>"
      "<span slot='away'>Riga</span>"
      "</div>");
  Element* outer = GetElementById("outer");
  ShadowRoot& outer_shadow =
      outer->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  outer_shadow.SetInnerHTMLWithoutTrustedTypes(
      "<slot name='home'></slot> <span id='inner'></span> "
      "<slot name='away'></slot>");

  Element* inner = outer_shadow.getElementById(AtomicString("inner"));
  ShadowRoot& inner_shadow =
      inner->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  inner_shadow.SetInnerHTMLWithoutTrustedTypes(
      "<span id='selection'>12:45</span>");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Element* selection_el =
      inner_shadow.getElementById(AtomicString("selection"));
  const EphemeralRange selection(Position(selection_el->firstChild(), 0),
                                 Position(selection_el->firstChild(), 5));
  Selection().SetSelection(
      SelectionInDomTree::Builder().SetBaseAndExtent(selection).Build(),
      SetSelectionOptions());

  SurroundingText surrounding_text(&GetFrame(), 100);
  EXPECT_EQ("Pafos 12:45 Riga",
            String(surrounding_text.TextContent()).SimplifyWhiteSpace());
  EXPECT_EQ("12:45", surrounding_text.TextContent().substr(
                         surrounding_text.StartOffsetInTextContent(),
                         surrounding_text.EndOffsetInTextContent() -
                             surrounding_text.StartOffsetInTextContent()));
}

TEST_F(SurroundingTextTest, ShadowDomCrossBoundarySelection) {
  SetHtml("<div id='light'>Before <span id='host'></span> After</div>");
  Element* light = GetElementById("light");
  Element* host = GetElementById("host");
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.SetInnerHTMLWithoutTrustedTypes(
      "<span id='inner'>Inside</span> Shadow");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Element* inner = shadow_root.getElementById(AtomicString("inner"));
  Selection().SetSelection(
      SelectionInDomTree::Builder()
          .SetBaseAndExtent(Position(light->firstChild(), 0),
                            Position(inner->firstChild(), 6))
          .Build(),
      SetSelectionOptions());

  SurroundingText surrounding_text(&GetFrame(), 100);
  EXPECT_EQ("Before Inside Shadow After",
            String(surrounding_text.TextContent()).SimplifyWhiteSpace());
  EXPECT_EQ("Before Inside",
            surrounding_text.TextContent().substr(
                surrounding_text.StartOffsetInTextContent(),
                surrounding_text.EndOffsetInTextContent() -
                    surrounding_text.StartOffsetInTextContent()));
}

TEST_F(SurroundingTextTest, ShadowDomClosedRootRangeSelection) {
  SetHtml("<div>Before <span id='host'></span> After</div>");
  Element* host = GetElementById("host");
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kClosed);
  shadow_root.SetInnerHTMLWithoutTrustedTypes(
      "<span>Pafos </span><span id='selection'>12:45</span><span> Riga</span>");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Element* selection_el = shadow_root.getElementById(AtomicString("selection"));
  const EphemeralRange selection(Position(selection_el->firstChild(), 0),
                                 Position(selection_el->firstChild(), 5));
  Selection().SetSelection(
      SelectionInDomTree::Builder().SetBaseAndExtent(selection).Build(),
      SetSelectionOptions());

  SurroundingText surrounding_text(&GetFrame(), 100);
  EXPECT_EQ("Before Pafos 12:45 Riga After",
            String(surrounding_text.TextContent()).SimplifyWhiteSpace());
  EXPECT_EQ("12:45", surrounding_text.TextContent().substr(
                         surrounding_text.StartOffsetInTextContent(),
                         surrounding_text.EndOffsetInTextContent() -
                             surrounding_text.StartOffsetInTextContent()));
}

TEST_F(SurroundingTextTest, ShadowDomFormControlSelectionReturnsEmpty) {
  SetHtml("<div id='host'></div>");
  Element* host = GetElementById("host");
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.SetInnerHTMLWithoutTrustedTypes(
      "<span>Before</span><button id='selection'>Click</button>"
      "<span>After</span>");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  Element* button = shadow_root.getElementById(AtomicString("selection"));
  const EphemeralRange selection(Position(button->firstChild(), 0),
                                 Position(button->firstChild(), 5));
  SurroundingText surrounding_text(selection, 100);
  EXPECT_TRUE(surrounding_text.IsEmpty());
}

TEST_F(SurroundingTextTest, LegacyDomTreeWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kSurroundingTextUseFlatTree);

  SetHtml("<span>Before <span id='selection'>target</span> After</span>");
  const EphemeralRange selection = Select(0, 6);
  {
    SurroundingText surrounding_text(selection, 100);
    EXPECT_EQ("Before target After", surrounding_text.TextContent());
    EXPECT_EQ(7u, surrounding_text.StartOffsetInTextContent());
    EXPECT_EQ(13u, surrounding_text.EndOffsetInTextContent());
  }

  {
    Selection().SetSelection(
        SelectionInDomTree::Builder().SetBaseAndExtent(selection).Build(),
        SetSelectionOptions());
    SurroundingText surrounding_text(&GetFrame(), 100);
    EXPECT_EQ("Before target After", surrounding_text.TextContent());
    EXPECT_EQ(7u, surrounding_text.StartOffsetInTextContent());
    EXPECT_EQ(13u, surrounding_text.EndOffsetInTextContent());
  }

  // With the feature disabled, slotted light DOM selections traverse only the
  // DOM tree and do not include shadow tree text.
  SetHtml(
      "<div id='outer'>"
      "<span slot='home' id='selection'>Pafos</span>"
      "<span slot='away'>Riga</span>"
      "</div>");
  Element* outer = GetElementById("outer");
  ShadowRoot& outer_shadow =
      outer->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  outer_shadow.SetInnerHTMLWithoutTrustedTypes(
      "<slot name='home'></slot> <span>12:45</span> <slot name='away'></slot>");
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);

  const EphemeralRange slotted_selection = Select(0, 5);
  SurroundingText slotted_surrounding_text(slotted_selection, 100);
  EXPECT_EQ(
      "PafosRiga",
      String(slotted_surrounding_text.TextContent()).SimplifyWhiteSpace());
}

}  // namespace blink
