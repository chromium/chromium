// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/ime/stylus_writing_gesture.h"
#include <gtest/gtest.h>
#include "third_party/blink/public/mojom/input/stylus_writing_gesture.mojom-blink.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

#include <vector>

namespace blink {

namespace {

struct TestCase {
  // X coordinate in pixels of the start of the gesture. (10px per character).
  int start;
  // X coordinate in pixels of the end of the gesture. (10px per character).
  int end;
  // Initial text to be inserted into the text element.
  String initial;
  // The expected text contained within the text element after the gesture has
  // been applied.
  String expected;

  TestCase(int start, int end, String initial, String expected)
      : start(start), end(end), initial(initial), expected(expected) {}
};

}  // namespace

class StylusWritingGestureTest : public SimTest {
 public:
  StylusWritingGestureTest() = default;

 protected:
  static Vector<char> ReadAhemWoff2() {
    return test::ReadFromFile(test::CoreTestDataPath("Ahem.woff2"))
        ->CopyAs<Vector<char>>();
  }

  HTMLInputElement* SetUpSingleInput();

  HTMLTextAreaElement* SetUpMultilineInput();

  WebFrameWidgetImpl* WidgetImpl() {
    return static_cast<WebFrameWidgetImpl*>(LocalFrameRoot().FrameWidget());
  }

  WebInputMethodController* Controller() {
    return WidgetImpl()->GetActiveWebInputMethodController();
  }

  String text_alternative = "XX";
};

HTMLInputElement* StylusWritingGestureTest::SetUpSingleInput() {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest font_resource("https://example.com/Ahem.woff2",
                                      "font/woff2");

  LoadURL("https://example.com");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
      @font-face {
        font-family: custom-font;
        src: url(https://example.com/Ahem.woff2) format("woff2");
      }
      body {
        margin: 0;
      }
      #target {
        font: 10px/1 custom-font, monospace;
        padding: none;
        border: none;
      }
    </style>
    <input type='text' id='target'/>
  )HTML");

  Compositor().BeginFrame();
  // Finish font loading, and trigger invalidations.
  font_resource.Complete(ReadAhemWoff2());
  GetDocument().GetStyleEngine().InvalidateStyleAndLayoutForFontUpdates();
  Compositor().BeginFrame();
  HTMLInputElement* input_element =
      DynamicTo<HTMLInputElement>(*GetDocument().getElementById("target"));
  input_element->Focus();
  return input_element;
}

HTMLTextAreaElement* StylusWritingGestureTest::SetUpMultilineInput() {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest font_resource("https://example.com/Ahem.woff2",
                                      "font/woff2");

  LoadURL("https://example.com");
  main_resource.Complete(R"HTML(
    <!doctype html>
    <style>
      @font-face {
        font-family: custom-font;
        src: url(https://example.com/Ahem.woff2) format("woff2");
      }
      body {
        margin: 0;
      }
      #target {
        font: 10px/1 custom-font, monospace;
        padding: none;
        border: none;
      }
    </style>
    <textarea type='text' id='target' rows='4'/>
  )HTML");

  Compositor().BeginFrame();
  // Finish font loading, and trigger invalidations.
  font_resource.Complete(ReadAhemWoff2());
  GetDocument().GetStyleEngine().InvalidateStyleAndLayoutForFontUpdates();
  Compositor().BeginFrame();
  HTMLTextAreaElement* input_element =
      DynamicTo<HTMLTextAreaElement>(*GetDocument().getElementById("target"));
  input_element->Focus();
  return input_element;
}

TEST_F(StylusWritingGestureTest, TestGestureDelete) {
  auto* input = SetUpSingleInput();
  input->SetValue("ABCD EFGH");
  // Input value = "ABCD EFGH". Try to delete BCD.
  // Expected value after delete gesture = "A EFGH". And cursor to be after A.
  mojom::blink::StylusWritingGestureDataPtr gesture_data(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data->action = mojom::blink::StylusWritingGestureAction::DELETE_TEXT;
  gesture_data->start_rect = gfx::Rect(10, 6, 0, 0);
  gesture_data->end_rect = gfx::Rect(40, 6, 0, 0);
  gesture_data->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(std::move(gesture_data));
  WebRange range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("A EFGH", input->Value());
  EXPECT_EQ(1, range.StartOffset());
  EXPECT_EQ(1, range.EndOffset());

  // Try to do delete gesture outside the current input range.
  // This should insert the text alternative at current cursor.
  mojom::blink::StylusWritingGestureDataPtr gesture_data2(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data2->action = mojom::blink::StylusWritingGestureAction::DELETE_TEXT;
  gesture_data2->start_rect = gfx::Rect(80, 6, 0, 0);
  gesture_data2->end_rect = gfx::Rect(100, 6, 0, 0);
  gesture_data2->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(std::move(gesture_data2));
  range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("AXX EFGH", input->Value());
  EXPECT_EQ(3, range.StartOffset());
  EXPECT_EQ(3, range.EndOffset());
}

TEST_F(StylusWritingGestureTest, TestGestureDeleteWithWordGranularity) {
  auto* input = SetUpSingleInput();

  std::vector<TestCase> test_cases{
      // Crossing out the first word and half of the second should delete both
      // words. Because the selection doesn't include the space between BC and
      // DEF, it remains after the deletion.
      TestCase(0, 30, "A BC DEF", " DEF"),
      // Deleting a word and its trailing space in between two other words
      // should leave the words either side with a single space between them.
      TestCase(28, 52, "A BC DEF", "A DEF"),
      // Same as above but with the spaces on both sides.
      TestCase(12, 48, "A BC DEF", "A DEF"),
      // Removing the last word is an edge case as there's no word past it to
      // check.
      TestCase(32, 72, "ABCDE FGH", "ABCDE"),
      // Crossing out inside a word without crossing over the middle should not
      // affect the word.
      TestCase(0, 24, "ABCDEFG", "ABCDEFG"),
  };
  for (auto test_case : test_cases) {
    input->SetValue(test_case.initial);
    mojom::blink::StylusWritingGestureDataPtr gesture_data(
        mojom::blink::StylusWritingGestureData::New());
    gesture_data->action =
        mojom::blink::StylusWritingGestureAction::DELETE_TEXT;
    gesture_data->granularity =
        mojom::blink::StylusWritingGestureGranularity::WORD;
    gesture_data->start_rect = gfx::Rect(test_case.start, 6, 0, 0);
    gesture_data->end_rect = gfx::Rect(test_case.end, 6, 0, 0);
    gesture_data->text_alternative = text_alternative;

    WidgetImpl()->HandleStylusWritingGestureAction(std::move(gesture_data));
    EXPECT_EQ(test_case.expected, input->Value());
  }
}

// https://crbug.com/1407262
TEST_F(StylusWritingGestureTest, TestGestureAtEndOfLineWithWordGranularity) {
  auto* input = SetUpMultilineInput();
  auto* inner_editor = input->InnerEditorElement();
  Document& doc = GetDocument();
  inner_editor->appendChild(Text::Create(doc, "ABCD"));
  inner_editor->appendChild(Text::Create(doc, "\n"));
  inner_editor->appendChild(Text::Create(doc, "EFGH"));

  mojom::blink::StylusWritingGestureDataPtr gesture_data(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data->action = mojom::blink::StylusWritingGestureAction::DELETE_TEXT;
  gesture_data->granularity =
      mojom::blink::StylusWritingGestureGranularity::WORD;
  gesture_data->start_rect = gfx::Rect(0, 6, 0, 0);
  gesture_data->end_rect = gfx::Rect(60, 6, 0, 0);
  gesture_data->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(std::move(gesture_data));
  EXPECT_EQ("\nEFGH", input->Value());
}

TEST_F(StylusWritingGestureTest, TestGestureRemoveSpaces) {
  auto* input = SetUpSingleInput();
  input->SetValue("ABCD   EFGH");

  // Input value = "ABCD   EFGH". Try to remove spaces after ABCD.
  // Expected value after gesture = "ABCDEFGH". And cursor to be after D.
  // We start gesture from C to F, and all spaces between to be removed.
  mojom::blink::StylusWritingGestureDataPtr gesture_data(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data->action =
      mojom::blink::StylusWritingGestureAction::REMOVE_SPACES;
  gesture_data->start_rect = gfx::Rect(30, 6, 0, 0);
  gesture_data->end_rect = gfx::Rect(90, 6, 0, 0);
  gesture_data->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(std::move(gesture_data));
  WebRange range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("ABCDEFGH", input->Value());
  EXPECT_EQ(4, range.StartOffset());
  EXPECT_EQ(4, range.EndOffset());

  // Try to do remove space gesture outside the current input range.
  // This should insert the text alternative at current cursor.
  mojom::blink::StylusWritingGestureDataPtr gesture_data2(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data2->action =
      mojom::blink::StylusWritingGestureAction::REMOVE_SPACES;
  gesture_data2->start_rect = gfx::Rect(100, 6, 0, 0);
  gesture_data2->end_rect = gfx::Rect(120, 6, 0, 0);
  gesture_data2->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(std::move(gesture_data2));
  range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("ABCDXXEFGH", input->Value());
  EXPECT_EQ(6, range.StartOffset());
  EXPECT_EQ(6, range.EndOffset());
}

TEST_F(StylusWritingGestureTest, TestGestureRemoveFirstSpace) {
  auto* input = SetUpSingleInput();
  input->SetValue("AB CD EF GH");

  // Try to do remove space gesture over more than one space.
  // This should remove the first space only.
  mojom::blink::StylusWritingGestureDataPtr gesture_data(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data->action =
      mojom::blink::StylusWritingGestureAction::REMOVE_SPACES;
  gesture_data->start_rect = gfx::Rect(10, 6, 0, 0);
  gesture_data->end_rect = gfx::Rect(100, 6, 0, 0);
  gesture_data->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(std::move(gesture_data));
  WebRange range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("ABCD EF GH", input->Value());
  EXPECT_EQ(2, range.StartOffset());
  EXPECT_EQ(2, range.EndOffset());
}

TEST_F(StylusWritingGestureTest, TestGestureSelect) {
  auto* input = SetUpSingleInput();
  input->SetValue("AB CD EF GH");

  mojom::blink::StylusWritingGestureDataPtr gesture_data(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data->action = mojom::blink::StylusWritingGestureAction::SELECT_TEXT;
  gesture_data->start_rect = gfx::Rect(10, 6, 0, 0);
  gesture_data->end_rect = gfx::Rect(40, 6, 0, 0);
  gesture_data->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(std::move(gesture_data));
  WebRange range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("AB CD EF GH", input->Value());
  EXPECT_EQ(1, range.StartOffset());
  EXPECT_EQ(4, range.EndOffset());
}

TEST_F(StylusWritingGestureTest, TestGestureAddSpaceOrText) {
  auto* input = SetUpSingleInput();
  input->SetValue("ABCDEFGH");

  // Input value = "ABCDEFGH". Try to add space after ABCD.
  // Expected value after gesture = "ABCD EFGH". And cursor to be after space.
  mojom::blink::StylusWritingGestureDataPtr gesture_data(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data->action =
      mojom::blink::StylusWritingGestureAction::ADD_SPACE_OR_TEXT;
  gesture_data->start_rect = gfx::Rect(42, 6, 0, 0);
  gesture_data->text_to_insert = " ";
  gesture_data->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(std::move(gesture_data));
  WebRange range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("ABCD EFGH", input->Value());
  EXPECT_EQ(5, range.StartOffset());
  EXPECT_EQ(5, range.EndOffset());

  // Try to do add space gesture outside the current input range.
  // This should insert the text alternative at current cursor.
  mojom::blink::StylusWritingGestureDataPtr gesture_data2(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data2->action =
      mojom::blink::StylusWritingGestureAction::ADD_SPACE_OR_TEXT;
  gesture_data2->start_rect = gfx::Rect(120, 6, 0, 0);
  gesture_data2->text_to_insert = " ";
  gesture_data2->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(std::move(gesture_data2));
  range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("ABCD XXEFGH", input->Value());
  EXPECT_EQ(7, range.StartOffset());
  EXPECT_EQ(7, range.EndOffset());
}

TEST_F(StylusWritingGestureTest, TestGestureSplitOrMerge_RemovesAllSpaces) {
  auto* input = SetUpSingleInput();
  input->SetValue("ABCD    EFGH");
  // Input value = "ABCD    EFGH". Try to merge after ABCD|.
  // Expected value after gesture = "ABCDEFGH". And cursor to be after ABCD.
  mojom::blink::StylusWritingGestureDataPtr gesture_data(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data->action =
      mojom::blink::StylusWritingGestureAction::SPLIT_OR_MERGE;
  gesture_data->start_rect = gfx::Rect(42, 6, 0, 0);
  gesture_data->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(std::move(gesture_data));
  WebRange range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("ABCDEFGH", input->Value());
  EXPECT_EQ(4, range.StartOffset());
  EXPECT_EQ(4, range.EndOffset());

  input->SetValue("ABCD    EFGH");
  // Input value = "ABCD    EFGH". Try to merge before |EFGH.
  // Expected value after gesture = "ABCDEFGH". And cursor to be after ABCD.
  mojom::blink::StylusWritingGestureDataPtr gesture_data1(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data1->action =
      mojom::blink::StylusWritingGestureAction::SPLIT_OR_MERGE;
  gesture_data1->start_rect = gfx::Rect(78, 6, 0, 0);
  gesture_data1->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(std::move(gesture_data1));
  range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("ABCDEFGH", input->Value());
  EXPECT_EQ(4, range.StartOffset());
  EXPECT_EQ(4, range.EndOffset());
}

TEST_F(StylusWritingGestureTest, TestGestureSplitOrMerge_NonEmptyInput) {
  auto* input = SetUpSingleInput();
  input->SetValue("ABCDEFGH");

  // Input value = "ABCDEFGH". Try to split after ABCD|.
  // Expected value after gesture = "ABCD EFGH". And cursor to be after space.
  mojom::blink::StylusWritingGestureDataPtr gesture_data(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data->action =
      mojom::blink::StylusWritingGestureAction::SPLIT_OR_MERGE;
  gesture_data->start_rect = gfx::Rect(42, 6, 0, 0);
  gesture_data->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(std::move(gesture_data));
  WebRange range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("ABCD EFGH", input->Value());
  EXPECT_EQ(5, range.StartOffset());
  EXPECT_EQ(5, range.EndOffset());

  // Input value = "ABCD EFGH". Try to merge after ABCD|.
  // Expected value after gesture = "ABCDEFGH". And cursor to be after ABCD.
  mojom::blink::StylusWritingGestureDataPtr gesture_data1(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data1->action =
      mojom::blink::StylusWritingGestureAction::SPLIT_OR_MERGE;
  gesture_data1->start_rect = gfx::Rect(42, 6, 0, 0);
  gesture_data1->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(std::move(gesture_data1));
  range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("ABCDEFGH", input->Value());
  EXPECT_EQ(4, range.StartOffset());
  EXPECT_EQ(4, range.EndOffset());

  // Try to do split-merge gesture outside the current input range.
  // This should insert the text alternative at current cursor.
  mojom::blink::StylusWritingGestureDataPtr gesture_data2(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data2->action =
      mojom::blink::StylusWritingGestureAction::SPLIT_OR_MERGE;
  gesture_data2->start_rect = gfx::Rect(120, 6, 0, 0);
  gesture_data2->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(std::move(gesture_data2));
  range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("ABCDXXEFGH", input->Value());
  EXPECT_EQ(6, range.StartOffset());
  EXPECT_EQ(6, range.EndOffset());

  // Try to do split-merge gesture at the start of input text. Space should not
  // be inserted. Fallback text is inserted at cursor.
  mojom::blink::StylusWritingGestureDataPtr gesture_data3(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data3->action =
      mojom::blink::StylusWritingGestureAction::SPLIT_OR_MERGE;
  gesture_data3->start_rect = gfx::Rect(4, 6, 0, 0);
  gesture_data3->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(std::move(gesture_data3));
  range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("ABCDXXXXEFGH", input->Value());
  EXPECT_EQ(8, range.StartOffset());
  EXPECT_EQ(8, range.EndOffset());
}

TEST_F(StylusWritingGestureTest, TestGestureSplitOrMerge_EmptyInput) {
  auto* input = SetUpSingleInput();
  input->SetValue("");

  // Split merge gesture in empty input inserts fallback text.
  mojom::blink::StylusWritingGestureDataPtr gesture_data(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data->action =
      mojom::blink::StylusWritingGestureAction::SPLIT_OR_MERGE;
  gesture_data->start_rect = gfx::Rect(105, 6, 0, 0);
  gesture_data->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(std::move(gesture_data));
  WebRange range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("XX", input->Value());
  EXPECT_EQ(2, range.StartOffset());
  EXPECT_EQ(2, range.EndOffset());
}

}  // namespace blink
