// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/ime/stylus_writing_gesture.h"

#include <gtest/gtest.h>

#include "third_party/blink/public/mojom/input/stylus_writing_gesture.mojom-blink.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

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

// Used to name the tests according to the parameter passed to them.
auto BoolToDirection(const testing::TestParamInfo<bool> is_RTL) {
  return is_RTL.param ? "RTL" : "LTR";
}

class TestWebFrameClientImpl : public frame_test_helpers::TestWebFrameClient {
 public:
  void UpdateContextMenuDataForTesting(
      const ContextMenuData& data,
      const std::optional<gfx::Point>& /*host_context_menu_location*/)
      override {
    context_menu_data_ = data;
  }

  const ContextMenuData& GetContextMenuData() const {
    return context_menu_data_;
  }

 private:
  ContextMenuData context_menu_data_;
};

}  // namespace

class StylusWritingGestureTest : public SimTest,
                                 public testing::WithParamInterface<bool> {
 public:
  StylusWritingGestureTest() = default;

  // Callback to pass into HandleStylusWritingGestureAction which stores the
  // result of the last gesture.
  void ResultCallback(mojom::blink::HandwritingGestureResult result) {
    last_gesture_result = result;
  }

 protected:
  static Vector<char> ReadAhemWoff2() {
    return *test::ReadFromFile(test::CoreTestDataPath("Ahem.woff2"));
  }

  HTMLInputElement* SetUpSingleInput(bool);

  HTMLTextAreaElement* SetUpMultilineInput(bool);

  gfx::Rect GetRect(int, int, int, int, int, bool) const;

  WebFrameWidgetImpl* WidgetImpl() {
    return static_cast<WebFrameWidgetImpl*>(LocalFrameRoot().FrameWidget());
  }

  WebInputMethodController* Controller() {
    return WidgetImpl()->GetActiveWebInputMethodController();
  }

  std::unique_ptr<frame_test_helpers::TestWebFrameClient>
  CreateWebFrameClientForMainFrame() override {
    return std::make_unique<TestWebFrameClientImpl>();
  }

  TestWebFrameClientImpl& GetWebFrameClient() {
    return static_cast<TestWebFrameClientImpl&>(WebFrameClient());
  }

  mojom::blink::HandwritingGestureResult last_gesture_result =
      mojom::blink::HandwritingGestureResult::kUnknown;
  String text_alternative = "XX";

 private:
  // Used to create an input or textarea element by SetUpSingleInput() or
  // SetUpMultilineInput() for use during the lifetime of a test.
  // Should be run once per test.
  Element& SetUpElement(String element);
};

Element& StylusWritingGestureTest::SetUpElement(String element) {
  SimRequest main_resource("https://example.com", "text/html");
  SimSubresourceRequest font_resource("https://example.com/Ahem.woff2",
                                      "font/woff2");

  LoadURL("https://example.com");
  main_resource.Complete(String(R"HTML(
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
    )HTML") + element);

  Compositor().BeginFrame();
  // Finish font loading, and trigger invalidations.
  font_resource.Complete(ReadAhemWoff2());
  GetDocument().GetStyleEngine().InvalidateStyleAndLayoutForFontUpdates();
  Compositor().BeginFrame();
  return *GetDocument().getElementById(AtomicString("target"));
}

HTMLInputElement* StylusWritingGestureTest::SetUpSingleInput(bool is_RTL) {
  HTMLInputElement* input_element = DynamicTo<HTMLInputElement>(SetUpElement(
      String("<input type='text' id='target' ") +
      String(is_RTL ? "style='unicode-bidi: bidi-override; direction: rtl;'"
                    : "") +
      String("/>")));
  input_element->Focus();
  return input_element;
}

HTMLTextAreaElement* StylusWritingGestureTest::SetUpMultilineInput(
    bool is_RTL) {
  HTMLTextAreaElement* text_area = DynamicTo<HTMLTextAreaElement>(SetUpElement(
      String("<textarea type='text' id='target' rows='4' ") +
      String(is_RTL ? "style='unicode-bidi: bidi-override; direction: rtl;'"
                    : "") +
      String("/>")));
  text_area->Focus();
  return text_area;
}

gfx::Rect StylusWritingGestureTest::GetRect(int x,
                                            int y,
                                            int width,
                                            int height,
                                            int input_width,
                                            bool is_RTL) const {
  // Start of text in RTL input is input_width - 1
  return is_RTL ? gfx::Rect(input_width - x - width - 1, y, width, height)
                : gfx::Rect(x, y, width, height);
}

TEST_P(StylusWritingGestureTest, TestGestureDelete) {
  const bool is_RTL = GetParam();
  auto* input = SetUpSingleInput(is_RTL);
  input->SetValue("ABCD EFGH");
  const int width = input->BoundsInWidget().width();
  // Input value = "ABCD EFGH". Try to delete BCD.
  // Expected value after delete gesture = "A EFGH". And cursor to be after A.
  mojom::blink::StylusWritingGestureDataPtr gesture_data(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data->action = mojom::blink::StylusWritingGestureAction::DELETE_TEXT;
  gesture_data->start_rect = GetRect(10, 6, 0, 0, width, is_RTL);
  gesture_data->end_rect = GetRect(40, 6, 0, 0, width, is_RTL);
  gesture_data->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(
      std::move(gesture_data),
      WTF::BindOnce(&StylusWritingGestureTest::ResultCallback,
                    base::Unretained(this)));
  WebRange range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("A EFGH", input->Value());
  EXPECT_EQ(1, range.StartOffset());
  EXPECT_EQ(1, range.EndOffset());
  EXPECT_EQ(mojom::blink::HandwritingGestureResult::kSuccess,
            last_gesture_result);

  // Try to do delete gesture outside the current input range.
  // This should insert the text alternative at current cursor.
  mojom::blink::StylusWritingGestureDataPtr gesture_data2(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data2->action = mojom::blink::StylusWritingGestureAction::DELETE_TEXT;
  gesture_data2->start_rect = GetRect(80, 6, 0, 0, width, is_RTL);
  gesture_data2->end_rect = GetRect(100, 6, 0, 0, width, is_RTL);
  gesture_data2->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(
      std::move(gesture_data2),
      WTF::BindOnce(&StylusWritingGestureTest::ResultCallback,
                    base::Unretained(this)));
  range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("AXX EFGH", input->Value());
  EXPECT_EQ(3, range.StartOffset());
  EXPECT_EQ(3, range.EndOffset());
  EXPECT_EQ(mojom::blink::HandwritingGestureResult::kFallback,
            last_gesture_result);
}

TEST_P(StylusWritingGestureTest, TestGestureDeleteWithWordGranularity) {
  const bool is_RTL = GetParam();
  auto* input = SetUpSingleInput(is_RTL);

  std::vector<TestCase> test_cases{
      // Crossing out the first word and half of the second should delete both
      // words. Because the selection doesn't include the space between BC and
      // DEF, it remains after the deletion.
      TestCase(0, 30, "A BC DEF", " DEF"),
      // Deleting a word and its trailing space in between two other words
      // should leave the words either side with a single space between them.
      TestCase(28, 52, "A BC DEF", "A DEF"),
      // Deleting a word and its leading space in between two other words
      // should leave the words either side with a single space between them.
      TestCase(10, 40, "A BC DEF", "A DEF"),
      // Same as above but with the spaces on both sides.
      TestCase(12, 48, "A BC DEF", "A DEF"),
      // Similar to above but spaces aren't originally part of the selection.
      TestCase(30, 50, "AB CD EF GH", "AB EF GH"),
      // Removing the last word is an edge case as there's no word past it to
      // check.
      TestCase(32, 72, "ABCDE FGH", "ABCDE"),
      // Crossing out inside a word without crossing over the middle should not
      // affect the word.
      TestCase(0, 24, "ABCDEFG", "ABCDEFG"),
      // Deleting a word with spaces either side removes one space.
      TestCase(32, 45, "AB CDE FGH", "AB FGH")};
  for (auto test_case : test_cases) {
    input->SetValue(test_case.initial);
    const int width = input->BoundsInWidget().width();
    mojom::blink::StylusWritingGestureDataPtr gesture_data(
        mojom::blink::StylusWritingGestureData::New());
    gesture_data->action =
        mojom::blink::StylusWritingGestureAction::DELETE_TEXT;
    gesture_data->granularity =
        mojom::blink::StylusWritingGestureGranularity::WORD;
    gesture_data->start_rect = GetRect(test_case.start, 6, 0, 0, width, is_RTL);
    gesture_data->end_rect = GetRect(test_case.end, 6, 0, 0, width, is_RTL);
    gesture_data->text_alternative = text_alternative;

    WidgetImpl()->HandleStylusWritingGestureAction(
        std::move(gesture_data),
        WTF::BindOnce(&StylusWritingGestureTest::ResultCallback,
                      base::Unretained(this)));
    EXPECT_EQ(test_case.expected, input->Value());
    EXPECT_EQ(mojom::blink::HandwritingGestureResult::kSuccess,
              last_gesture_result);
  }
}

TEST_P(StylusWritingGestureTest, TestGestureDeleteNotFirstLine) {
  const bool is_RTL = GetParam();
  auto* input = SetUpMultilineInput(is_RTL);
  input->SetValue("ABCD\nEFGH");
  const int width = input->BoundsInWidget().width();

  mojom::blink::StylusWritingGestureDataPtr gesture_data(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data->action = mojom::blink::StylusWritingGestureAction::DELETE_TEXT;
  gesture_data->granularity =
      mojom::blink::StylusWritingGestureGranularity::CHARACTER;
  gesture_data->start_rect = GetRect(0, 16, 0, 0, width, is_RTL);
  gesture_data->end_rect = GetRect(20, 16, 0, 0, width, is_RTL);
  gesture_data->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(
      std::move(gesture_data),
      WTF::BindOnce(&StylusWritingGestureTest::ResultCallback,
                    base::Unretained(this)));
  EXPECT_EQ("ABCD\nGH", input->Value());
  EXPECT_EQ(mojom::blink::HandwritingGestureResult::kSuccess,
            last_gesture_result);
}

// https://crbug.com/1407262
TEST_P(StylusWritingGestureTest, TestGestureAtEndOfLineWithWordGranularity) {
  const bool is_RTL = GetParam();
  auto* input = SetUpMultilineInput(is_RTL);
  auto* inner_editor = input->InnerEditorElement();
  Document& doc = GetDocument();
  inner_editor->appendChild(Text::Create(doc, "ABCD"));
  inner_editor->appendChild(Text::Create(doc, "\n"));
  inner_editor->appendChild(Text::Create(doc, "EFGH"));
  const int width = input->BoundsInWidget().width();

  mojom::blink::StylusWritingGestureDataPtr gesture_data(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data->action = mojom::blink::StylusWritingGestureAction::DELETE_TEXT;
  gesture_data->granularity =
      mojom::blink::StylusWritingGestureGranularity::WORD;
  gesture_data->start_rect = GetRect(0, 6, 0, 0, width, is_RTL);
  gesture_data->end_rect = GetRect(60, 6, 0, 0, width, is_RTL);
  gesture_data->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(
      std::move(gesture_data),
      WTF::BindOnce(&StylusWritingGestureTest::ResultCallback,
                    base::Unretained(this)));
  EXPECT_EQ("\nEFGH", input->Value());
  EXPECT_EQ(mojom::blink::HandwritingGestureResult::kSuccess,
            last_gesture_result);
}

TEST_P(StylusWritingGestureTest, TestGestureDeleteMultiline) {
  const bool is_RTL = GetParam();
  auto* input = SetUpMultilineInput(is_RTL);
  input->SetValue("ABCD\nEFGH");
  const int width = input->BoundsInWidget().width();

  mojom::blink::StylusWritingGestureDataPtr gesture_data(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data->action = mojom::blink::StylusWritingGestureAction::DELETE_TEXT;
  gesture_data->granularity =
      mojom::blink::StylusWritingGestureGranularity::CHARACTER;
  gesture_data->start_rect = GetRect(22, 2, 18, 4, width, is_RTL);
  gesture_data->end_rect = GetRect(0, 16, 20, 0, width, is_RTL);
  gesture_data->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(
      std::move(gesture_data),
      WTF::BindOnce(&StylusWritingGestureTest::ResultCallback,
                    base::Unretained(this)));
  EXPECT_EQ("ABGH", input->Value());
  EXPECT_EQ(mojom::blink::HandwritingGestureResult::kSuccess,
            last_gesture_result);
}

TEST_P(StylusWritingGestureTest,
       TestGestureDeleteMultilinePartiallyOutsideBounds) {
  const bool is_RTL = GetParam();
  auto* input = SetUpMultilineInput(is_RTL);
  input->SetValue("ABCD\nEFGH");
  const int width = input->BoundsInWidget().width();

  mojom::blink::StylusWritingGestureDataPtr gesture_data(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data->action = mojom::blink::StylusWritingGestureAction::DELETE_TEXT;
  gesture_data->start_rect = GetRect(20, 2, 100, 4, width, is_RTL);
  gesture_data->end_rect = GetRect(-10, 16, 30, 0, width, is_RTL);
  gesture_data->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(
      std::move(gesture_data),
      WTF::BindOnce(&StylusWritingGestureTest::ResultCallback,
                    base::Unretained(this)));
  WebRange range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("ABGH", input->Value());
  EXPECT_EQ(2, range.StartOffset());
  EXPECT_EQ(2, range.EndOffset());
  EXPECT_EQ(mojom::blink::HandwritingGestureResult::kSuccess,
            last_gesture_result);
}

TEST_P(StylusWritingGestureTest, TestGestureRemoveSpaces) {
  const bool is_RTL = GetParam();
  auto* input = SetUpSingleInput(is_RTL);
  input->SetValue("ABCD   EFGH");
  const int width = input->BoundsInWidget().width();

  // Input value = "ABCD   EFGH". Try to remove spaces after ABCD.
  // Expected value after gesture = "ABCDEFGH". And cursor to be after D.
  // We start gesture from C to F, and all spaces between to be removed.
  mojom::blink::StylusWritingGestureDataPtr gesture_data(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data->action =
      mojom::blink::StylusWritingGestureAction::REMOVE_SPACES;
  gesture_data->start_rect = GetRect(30, 6, 0, 0, width, is_RTL);
  gesture_data->end_rect = GetRect(90, 6, 0, 0, width, is_RTL);
  gesture_data->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(
      std::move(gesture_data),
      WTF::BindOnce(&StylusWritingGestureTest::ResultCallback,
                    base::Unretained(this)));
  WebRange range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("ABCDEFGH", input->Value());
  EXPECT_EQ(4, range.StartOffset());
  EXPECT_EQ(4, range.EndOffset());
  EXPECT_EQ(mojom::blink::HandwritingGestureResult::kSuccess,
            last_gesture_result);

  // Try to do remove space gesture outside the current input range.
  // This should insert the text alternative at current cursor.
  mojom::blink::StylusWritingGestureDataPtr gesture_data2(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data2->action =
      mojom::blink::StylusWritingGestureAction::REMOVE_SPACES;
  gesture_data2->start_rect = GetRect(100, 6, 0, 0, width, is_RTL);
  gesture_data2->end_rect = GetRect(120, 6, 0, 0, width, is_RTL);
  gesture_data2->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(
      std::move(gesture_data2),
      WTF::BindOnce(&StylusWritingGestureTest::ResultCallback,
                    base::Unretained(this)));
  range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("ABCDXXEFGH", input->Value());
  EXPECT_EQ(6, range.StartOffset());
  EXPECT_EQ(6, range.EndOffset());
  EXPECT_EQ(mojom::blink::HandwritingGestureResult::kFallback,
            last_gesture_result);
}

TEST_P(StylusWritingGestureTest, TestGestureRemoveFirstSpace) {
  const bool is_RTL = GetParam();
  auto* input = SetUpSingleInput(is_RTL);
  input->SetValue("AB CD EF GH");
  const int width = input->BoundsInWidget().width();

  // Try to do remove space gesture over more than one space.
  // This should remove the first space only.
  mojom::blink::StylusWritingGestureDataPtr gesture_data(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data->action =
      mojom::blink::StylusWritingGestureAction::REMOVE_SPACES;
  gesture_data->start_rect = GetRect(10, 6, 0, 0, width, is_RTL);
  gesture_data->end_rect = GetRect(100, 6, 0, 0, width, is_RTL);
  gesture_data->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(
      std::move(gesture_data),
      WTF::BindOnce(&StylusWritingGestureTest::ResultCallback,
                    base::Unretained(this)));
  WebRange range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("ABCD EF GH", input->Value());
  EXPECT_EQ(2, range.StartOffset());
  EXPECT_EQ(2, range.EndOffset());
  EXPECT_EQ(mojom::blink::HandwritingGestureResult::kSuccess,
            last_gesture_result);
}

TEST_P(StylusWritingGestureTest, TestGestureSelect) {
  const bool is_RTL = GetParam();
  auto* input = SetUpSingleInput(is_RTL);
  input->SetValue("AB CD EF GH");
  const int width = input->BoundsInWidget().width();

  mojom::blink::StylusWritingGestureDataPtr gesture_data(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data->action = mojom::blink::StylusWritingGestureAction::SELECT_TEXT;
  gesture_data->start_rect = GetRect(10, 6, 0, 0, width, is_RTL);
  gesture_data->end_rect = GetRect(40, 6, 0, 0, width, is_RTL);
  gesture_data->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(
      std::move(gesture_data),
      WTF::BindOnce(&StylusWritingGestureTest::ResultCallback,
                    base::Unretained(this)));
  WebRange range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("AB CD EF GH", input->Value());
  EXPECT_EQ(1, range.StartOffset());
  EXPECT_EQ(4, range.EndOffset());
  EXPECT_EQ(mojom::blink::HandwritingGestureResult::kSuccess,
            last_gesture_result);

  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ("B C", context_menu_data.selected_text);
  EXPECT_TRUE(GetDocument().GetFrame()->Selection().IsHandleVisible());
}

TEST_P(StylusWritingGestureTest, TestGestureSelectsNoSpacesEitherSide) {
  const bool is_RTL = GetParam();
  auto* input = SetUpSingleInput(is_RTL);
  input->SetValue("AB CD EF GH");
  const int width = input->BoundsInWidget().width();

  mojom::blink::StylusWritingGestureDataPtr gesture_data(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data->action = mojom::blink::StylusWritingGestureAction::SELECT_TEXT;
  gesture_data->start_rect = GetRect(20, 6, 0, 0, width, is_RTL);
  gesture_data->end_rect = GetRect(90, 6, 0, 0, width, is_RTL);
  gesture_data->text_alternative = text_alternative;
  gesture_data->granularity =
      mojom::blink::StylusWritingGestureGranularity::WORD;

  WidgetImpl()->HandleStylusWritingGestureAction(
      std::move(gesture_data),
      WTF::BindOnce(&StylusWritingGestureTest::ResultCallback,
                    base::Unretained(this)));
  WebRange range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("AB CD EF GH", input->Value());
  EXPECT_EQ(3, range.StartOffset());
  EXPECT_EQ(8, range.EndOffset());
  EXPECT_EQ(mojom::blink::HandwritingGestureResult::kSuccess,
            last_gesture_result);

  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ("CD EF", context_menu_data.selected_text);
  EXPECT_TRUE(GetDocument().GetFrame()->Selection().IsHandleVisible());
}

TEST_P(StylusWritingGestureTest, TestGestureSelectMultiline) {
  const bool is_RTL = GetParam();
  auto* input = SetUpMultilineInput(is_RTL);
  input->SetValue("ABCD\nEFGH");
  const int width = input->BoundsInWidget().width();

  mojom::blink::StylusWritingGestureDataPtr gesture_data(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data->action = mojom::blink::StylusWritingGestureAction::SELECT_TEXT;
  gesture_data->start_rect = GetRect(22, 6, 18, 0, width, is_RTL);
  gesture_data->end_rect = GetRect(0, 12, 20, 4, width, is_RTL);
  gesture_data->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(
      std::move(gesture_data),
      WTF::BindOnce(&StylusWritingGestureTest::ResultCallback,
                    base::Unretained(this)));
  WebRange range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("ABCD\nEFGH", input->Value());
  EXPECT_EQ(2, range.StartOffset());
  EXPECT_EQ(7, range.EndOffset());
  EXPECT_EQ(mojom::blink::HandwritingGestureResult::kSuccess,
            last_gesture_result);

  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ("CD\nEF", context_menu_data.selected_text);
  EXPECT_TRUE(GetDocument().GetFrame()->Selection().IsHandleVisible());
}

TEST_P(StylusWritingGestureTest, TestGestureSelectPartiallyOutsideBounds) {
  const bool is_RTL = GetParam();
  auto* input = SetUpSingleInput(is_RTL);
  input->SetValue("ABCD EFGH");
  const int width = input->BoundsInWidget().width();

  mojom::blink::StylusWritingGestureDataPtr gesture_data(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data->action = mojom::blink::StylusWritingGestureAction::SELECT_TEXT;
  gesture_data->start_rect = GetRect(-10, 6, 0, 0, width, is_RTL);
  gesture_data->end_rect = GetRect(30, 6, 0, 0, width, is_RTL);
  gesture_data->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(
      std::move(gesture_data),
      WTF::BindOnce(&StylusWritingGestureTest::ResultCallback,
                    base::Unretained(this)));
  WebRange range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("ABCD EFGH", input->Value());
  EXPECT_EQ(0, range.StartOffset());
  EXPECT_EQ(3, range.EndOffset());
  EXPECT_EQ(mojom::blink::HandwritingGestureResult::kSuccess,
            last_gesture_result);

  ContextMenuData context_menu_data = GetWebFrameClient().GetContextMenuData();
  EXPECT_EQ("ABC", context_menu_data.selected_text);
  EXPECT_TRUE(GetDocument().GetFrame()->Selection().IsHandleVisible());
}

TEST_P(StylusWritingGestureTest, TestGestureAddSpaceOrText) {
  const bool is_RTL = GetParam();
  auto* input = SetUpSingleInput(is_RTL);
  input->SetValue("ABCDEFGH");
  const int width = input->BoundsInWidget().width();

  // Input value = "ABCDEFGH". Try to add space after ABCD.
  // Expected value after gesture = "ABCD EFGH". And cursor to be after space.
  mojom::blink::StylusWritingGestureDataPtr gesture_data(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data->action =
      mojom::blink::StylusWritingGestureAction::ADD_SPACE_OR_TEXT;
  gesture_data->start_rect = GetRect(42, 6, 0, 0, width, is_RTL);
  gesture_data->text_to_insert = " ";
  gesture_data->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(
      std::move(gesture_data),
      WTF::BindOnce(&StylusWritingGestureTest::ResultCallback,
                    base::Unretained(this)));
  WebRange range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("ABCD EFGH", input->Value());
  EXPECT_EQ(5, range.StartOffset());
  EXPECT_EQ(5, range.EndOffset());
  EXPECT_EQ(mojom::blink::HandwritingGestureResult::kSuccess,
            last_gesture_result);

  // Try to do add space gesture outside the current input range.
  // This should insert the text alternative at current cursor.
  mojom::blink::StylusWritingGestureDataPtr gesture_data2(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data2->action =
      mojom::blink::StylusWritingGestureAction::ADD_SPACE_OR_TEXT;
  gesture_data2->start_rect = GetRect(120, 6, 0, 0, width, is_RTL);
  gesture_data2->text_to_insert = " ";
  gesture_data2->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(
      std::move(gesture_data2),
      WTF::BindOnce(&StylusWritingGestureTest::ResultCallback,
                    base::Unretained(this)));
  range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("ABCD XXEFGH", input->Value());
  EXPECT_EQ(7, range.StartOffset());
  EXPECT_EQ(7, range.EndOffset());
  EXPECT_EQ(mojom::blink::HandwritingGestureResult::kFallback,
            last_gesture_result);
}

TEST_P(StylusWritingGestureTest, TestGestureSplitOrMerge_RemovesAllSpaces) {
  const bool is_RTL = GetParam();
  auto* input = SetUpSingleInput(is_RTL);
  input->SetValue("ABCD    EFGH");
  const int width = input->BoundsInWidget().width();
  // Input value = "ABCD    EFGH". Try to merge after ABCD|.
  // Expected value after gesture = "ABCDEFGH". And cursor to be after ABCD.
  mojom::blink::StylusWritingGestureDataPtr gesture_data(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data->action =
      mojom::blink::StylusWritingGestureAction::SPLIT_OR_MERGE;
  gesture_data->start_rect = GetRect(42, 6, 0, 0, width, is_RTL);
  gesture_data->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(
      std::move(gesture_data),
      WTF::BindOnce(&StylusWritingGestureTest::ResultCallback,
                    base::Unretained(this)));
  WebRange range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("ABCDEFGH", input->Value());
  EXPECT_EQ(4, range.StartOffset());
  EXPECT_EQ(4, range.EndOffset());
  EXPECT_EQ(mojom::blink::HandwritingGestureResult::kSuccess,
            last_gesture_result);

  input->SetValue("ABCD    EFGH");
  // Input value = "ABCD    EFGH". Try to merge before |EFGH.
  // Expected value after gesture = "ABCDEFGH". And cursor to be after ABCD.
  mojom::blink::StylusWritingGestureDataPtr gesture_data1(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data1->action =
      mojom::blink::StylusWritingGestureAction::SPLIT_OR_MERGE;
  gesture_data1->start_rect = GetRect(78, 6, 0, 0, width, is_RTL);
  gesture_data1->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(
      std::move(gesture_data1),
      WTF::BindOnce(&StylusWritingGestureTest::ResultCallback,
                    base::Unretained(this)));
  range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("ABCDEFGH", input->Value());
  EXPECT_EQ(4, range.StartOffset());
  EXPECT_EQ(4, range.EndOffset());
  EXPECT_EQ(mojom::blink::HandwritingGestureResult::kSuccess,
            last_gesture_result);
}

TEST_P(StylusWritingGestureTest, TestGestureSplitOrMerge_NonEmptyInput) {
  const bool is_RTL = GetParam();
  auto* input = SetUpSingleInput(is_RTL);
  input->SetValue("ABCDEFGH");
  const int width = input->BoundsInWidget().width();

  // Input value = "ABCDEFGH". Try to split after ABCD|.
  // Expected value after gesture = "ABCD EFGH". And cursor to be after space.
  mojom::blink::StylusWritingGestureDataPtr gesture_data(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data->action =
      mojom::blink::StylusWritingGestureAction::SPLIT_OR_MERGE;
  gesture_data->start_rect = GetRect(42, 6, 0, 0, width, is_RTL);
  gesture_data->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(
      std::move(gesture_data),
      WTF::BindOnce(&StylusWritingGestureTest::ResultCallback,
                    base::Unretained(this)));
  WebRange range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("ABCD EFGH", input->Value());
  EXPECT_EQ(5, range.StartOffset());
  EXPECT_EQ(5, range.EndOffset());
  EXPECT_EQ(mojom::blink::HandwritingGestureResult::kSuccess,
            last_gesture_result);

  // Input value = "ABCD EFGH". Try to merge after ABCD|.
  // Expected value after gesture = "ABCDEFGH". And cursor to be after ABCD.
  mojom::blink::StylusWritingGestureDataPtr gesture_data1(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data1->action =
      mojom::blink::StylusWritingGestureAction::SPLIT_OR_MERGE;
  gesture_data1->start_rect = GetRect(42, 6, 0, 0, width, is_RTL);
  gesture_data1->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(
      std::move(gesture_data1),
      WTF::BindOnce(&StylusWritingGestureTest::ResultCallback,
                    base::Unretained(this)));
  range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("ABCDEFGH", input->Value());
  EXPECT_EQ(4, range.StartOffset());
  EXPECT_EQ(4, range.EndOffset());
  EXPECT_EQ(mojom::blink::HandwritingGestureResult::kSuccess,
            last_gesture_result);

  // Try to do split-merge gesture outside the current input range.
  // This should insert the text alternative at current cursor.
  mojom::blink::StylusWritingGestureDataPtr gesture_data2(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data2->action =
      mojom::blink::StylusWritingGestureAction::SPLIT_OR_MERGE;
  gesture_data2->start_rect = GetRect(120, 6, 0, 0, width, is_RTL);
  gesture_data2->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(
      std::move(gesture_data2),
      WTF::BindOnce(&StylusWritingGestureTest::ResultCallback,
                    base::Unretained(this)));
  range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("ABCDXXEFGH", input->Value());
  EXPECT_EQ(6, range.StartOffset());
  EXPECT_EQ(6, range.EndOffset());
  EXPECT_EQ(mojom::blink::HandwritingGestureResult::kFallback,
            last_gesture_result);

  // Try to do split-merge gesture at the start of input text. Space should not
  // be inserted. Fallback text is inserted at cursor.
  mojom::blink::StylusWritingGestureDataPtr gesture_data3(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data3->action =
      mojom::blink::StylusWritingGestureAction::SPLIT_OR_MERGE;
  gesture_data3->start_rect = GetRect(4, 6, 0, 0, width, is_RTL);
  gesture_data3->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(
      std::move(gesture_data3),
      WTF::BindOnce(&StylusWritingGestureTest::ResultCallback,
                    base::Unretained(this)));
  range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("ABCDXXXXEFGH", input->Value());
  EXPECT_EQ(8, range.StartOffset());
  EXPECT_EQ(8, range.EndOffset());
  EXPECT_EQ(mojom::blink::HandwritingGestureResult::kFallback,
            last_gesture_result);
}

TEST_P(StylusWritingGestureTest, TestGestureSplitOrMerge_EmptyInput) {
  const bool is_RTL = GetParam();
  auto* input = SetUpSingleInput(is_RTL);
  input->SetValue("");
  const int width = input->BoundsInWidget().width();

  // Split merge gesture in empty input inserts fallback text.
  mojom::blink::StylusWritingGestureDataPtr gesture_data(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data->action =
      mojom::blink::StylusWritingGestureAction::SPLIT_OR_MERGE;
  gesture_data->start_rect = GetRect(105, 6, 0, 0, width, is_RTL);
  gesture_data->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(
      std::move(gesture_data),
      WTF::BindOnce(&StylusWritingGestureTest::ResultCallback,
                    base::Unretained(this)));
  WebRange range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("XX", input->Value());
  EXPECT_EQ(2, range.StartOffset());
  EXPECT_EQ(2, range.EndOffset());
  EXPECT_EQ(mojom::blink::HandwritingGestureResult::kFallback,
            last_gesture_result);
}

INSTANTIATE_TEST_SUITE_P(BiDirectional,
                         StylusWritingGestureTest,
                         testing::Bool(),
                         &BoolToDirection);

}  // namespace blink
