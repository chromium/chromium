// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include "third_party/blink/public/mojom/input/stylus_writing_gesture.mojom-blink.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class StylusWritingGestureTest : public SimTest {
 public:
  StylusWritingGestureTest() = default;

 protected:
  static Vector<char> ReadAhemWoff2() {
    return test::ReadFromFile(test::CoreTestDataPath("Ahem.woff2"))
        ->CopyAs<Vector<char>>();
  }

  HTMLInputElement* SetUpSingleInput();

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
        font: 25px/1 custom-font, monospace;
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

TEST_F(StylusWritingGestureTest, TestGestureDelete) {
  auto* input = SetUpSingleInput();
  input->SetValue("ABCD EFGH");
  // Input value = "ABCD EFGH". Try to delete BCD.
  // Expected value after delete gesture = "A EFGH". And cursor to be after A.
  mojom::blink::StylusWritingGestureDataPtr gesture_data(
      mojom::blink::StylusWritingGestureData::New());
  gesture_data->action = mojom::blink::StylusWritingGestureAction::DELETE_TEXT;
  gesture_data->start_point = gfx::Point(25, 15);
  gesture_data->end_point = gfx::Point(100, 15);
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
  gesture_data2->start_point = gfx::Point(200, 15);
  gesture_data2->end_point = gfx::Point(250, 15);
  gesture_data2->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(std::move(gesture_data2));
  range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("AXX EFGH", input->Value());
  EXPECT_EQ(3, range.StartOffset());
  EXPECT_EQ(3, range.EndOffset());
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
  gesture_data->start_point = gfx::Point(75, 15);
  gesture_data->end_point = gfx::Point(225, 15);
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
  gesture_data2->start_point = gfx::Point(250, 15);
  gesture_data2->end_point = gfx::Point(300, 15);
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
  gesture_data->start_point = gfx::Point(25, 15);
  gesture_data->end_point = gfx::Point(250, 15);
  gesture_data->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(std::move(gesture_data));
  WebRange range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("ABCD EF GH", input->Value());
  EXPECT_EQ(2, range.StartOffset());
  EXPECT_EQ(2, range.EndOffset());
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
  gesture_data->start_point = gfx::Point(105, 15);
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
  gesture_data2->start_point = gfx::Point(300, 15);
  gesture_data2->text_to_insert = " ";
  gesture_data2->text_alternative = text_alternative;

  WidgetImpl()->HandleStylusWritingGestureAction(std::move(gesture_data2));
  range = Controller()->GetSelectionOffsets();
  EXPECT_EQ("ABCD XXEFGH", input->Value());
  EXPECT_EQ(7, range.StartOffset());
  EXPECT_EQ(7, range.EndOffset());
}

}  // namespace blink
