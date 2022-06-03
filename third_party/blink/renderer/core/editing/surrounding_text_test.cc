// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/surrounding_text.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"

namespace blink {

class SurroundingTextTest : public testing::Test {
 protected:
  Document& GetDocument() const { return dummy_page_holder_->GetDocument(); }
  void SetHTML(const String&);
  EphemeralRange Select(int offset) { return Select(offset, offset); }
  EphemeralRange Select(int start, int end);

 private:
  void SetUp() override;

  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

void SurroundingTextTest::SetUp() {
  dummy_page_holder_ = std::make_unique<DummyPageHolder>(IntSize(800, 600));
}

void SurroundingTextTest::SetHTML(const String& content) {
  GetDocument().body()->setInnerHTML(content);
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
}

EphemeralRange SurroundingTextTest::Select(int start, int end) {
  Element* element = GetDocument().getElementById("selection");
  return EphemeralRange(Position(element->firstChild(), start),
                        Position(element->firstChild(), end));
}

TEST_F(SurroundingTextTest, BasicCaretSelection) {
  SetHTML(String("<p id='selection'>foo bar</p>"));

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
  SetHTML(String("<p id='selection'>Lorem ipsum dolor sit amet</p>"));

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
  SetHTML(
      String("<div>This is outside of <p id='selection'>foo bar</p> the "
             "selected node</div>"));

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
  SetHTML(
      String("<div>This is outside of <p id='selection'>foo bar</p> the "
             "selected node</div>"));

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
  SetHTML(
      String("<p>First paragraph</p>"
             "<textarea id='selection'>abc def ghi</textarea>"
             "<p>Second paragraph</p>"));

  TextControlElement* text_ctrl = reinterpret_cast<TextControlElement*>(
      GetDocument().getElementById("selection"));

  text_ctrl->SetSelectionRange(4, 7);
  EphemeralRange selection = text_ctrl->Selection().ComputeRange();

  SurroundingText surrounding_text(selection, 20);

  EXPECT_EQ("abc def ghi",
            String(surrounding_text.TextContent()).SimplifyWhiteSpace());
  EXPECT_EQ(4u, surrounding_text.StartOffsetInTextContent());
  EXPECT_EQ(7u, surrounding_text.EndOffsetInTextContent());
}

TEST_F(SurroundingTextTest, EmptyInputElementWithChild) {
  SetHTML(String("<input type=\"text\" id=\"input_name\"/>"));

  TextControlElement* input_element = reinterpret_cast<TextControlElement*>(
      GetDocument().getElementById("input_name"));
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
  EXPECT_TRUE(surrounding_text.TextContent().IsEmpty());
}

TEST_F(SurroundingTextTest, ButtonsAndParagraph) {
  SetHTML(
      String("<button>.</button>12345"
             "<p id='selection'>6789 12345</p>"
             "6789<button>.</button>"));

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

    EXPECT_TRUE(surrounding_text.TextContent().IsEmpty());
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
  SetHTML(String(
      "<select>.</select>"
      "<div>57th Street and Lake Shore Drive</div>"
      " <span>Chicago</span> <span id='selection'>IL</span> <span>60637</span>"
      "<select>.</select>"));

  EphemeralRange selection = Select(0);
  SurroundingText surrounding_text(selection, 100);

  EXPECT_EQ("\xEF\xBF\xBC\n57th Street and Lake Shore Drive\nChicago IL 60637",
            surrounding_text.TextContent().Utf8());
  EXPECT_EQ(43u, surrounding_text.StartOffsetInTextContent());
  EXPECT_EQ(43u, surrounding_text.EndOffsetInTextContent());
}

TEST_F(SurroundingTextTest, FieldsetElementAndText) {
  SetHTML(
      String("<fieldset>.</fieldset>12345<button>abc</button>"
             "<p>6789<br><span id='selection'>12345</span></p>"
             "6789<textarea>abc</textarea>0123<fieldset>.</fieldset>"));

  EphemeralRange selection = Select(0);
  SurroundingText surrounding_text(selection, 100);

  EXPECT_EQ("\n6789\n12345\n\n6789", surrounding_text.TextContent());
  EXPECT_EQ(6u, surrounding_text.StartOffsetInTextContent());
  EXPECT_EQ(6u, surrounding_text.EndOffsetInTextContent());
}

TEST_F(SurroundingTextTest, ButtonScriptAndComment) {
  SetHTML(
      String("<button>.</button>"
             "<div id='selection'>This is <!-- comment --!>a test "
             "<script language='javascript'></script>"
             "example<button>.</button>"));

  EphemeralRange selection = Select(0);
  SurroundingText surrounding_text(selection, 100);

  EXPECT_EQ("\nThis is a test example", surrounding_text.TextContent());
  EXPECT_EQ(1u, surrounding_text.StartOffsetInTextContent());
  EXPECT_EQ(1u, surrounding_text.EndOffsetInTextContent());
}

TEST_F(SurroundingTextTest, ButtonAndLongDiv) {
  SetHTML(
      String("<button>.</button>"
             "<div id='selection'>012345678901234567890123456789</div>"
             "<button>.</button>"));

  EphemeralRange selection = Select(15);
  SurroundingText surrounding_text(selection, 12);

  EXPECT_EQ("901234567890", surrounding_text.TextContent());
  EXPECT_EQ(6u, surrounding_text.StartOffsetInTextContent());
  EXPECT_EQ(6u, surrounding_text.EndOffsetInTextContent());
}

TEST_F(SurroundingTextTest, EmptySurroundingTextInOptionsAndButton) {
  SetHTML(
      String("<option>.</option>12345"
             "<button id='selection'>test</button>"
             "<option>.</option>"));

  {
    EphemeralRange selection = Select(1);
    SurroundingText surrounding_text(selection, 100);

    EXPECT_TRUE(surrounding_text.TextContent().IsEmpty());
  }

  {
    EphemeralRange selection = Select(3);
    SurroundingText surrounding_text(selection, 100);

    EXPECT_TRUE(surrounding_text.TextContent().IsEmpty());
  }
}

TEST_F(SurroundingTextTest, SingleDotParagraph) {
  SetHTML(String("<p id='selection'>.</p>"));

  EphemeralRange selection = Select(0);
  SurroundingText surrounding_text(selection, 2);

  EXPECT_EQ("\n.", surrounding_text.TextContent());
  EXPECT_EQ(1u, surrounding_text.StartOffsetInTextContent());
  EXPECT_EQ(1u, surrounding_text.EndOffsetInTextContent());
}

}  // namespace blink
