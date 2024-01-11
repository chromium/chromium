// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/actions_parser.h"

#include <utility>

#include "base/json/json_reader.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(ActionsParserTestDriverTest, ParseMousePointerActionSequence) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"type": "pointer",
                "actions": [{"type": "pointerDown", "x": 2, "y": 3,
                             "button": 0},
                            {"type": "pointerUp", "x": 2, "y": 3,
                             "button": 0}],
                "parameters": {"pointerType": "mouse"},
                "id": "1"}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_TRUE(actions_parser.Parse());
  SyntheticPointerActionListParams action_list_params =
      actions_parser.pointer_action_params();
  EXPECT_EQ(content::mojom::GestureSourceType::kMouseInput,
            action_list_params.gesture_source_type);
  EXPECT_EQ(2U, action_list_params.params.size());
  EXPECT_EQ(1U, action_list_params.params[0].size());
  EXPECT_EQ(SyntheticPointerActionParams::PointerActionType::PRESS,
            action_list_params.params[0][0].pointer_action_type());
  EXPECT_EQ(gfx::PointF(2, 3), action_list_params.params[0][0].position());
  EXPECT_EQ(SyntheticPointerActionParams::Button::LEFT,
            action_list_params.params[0][0].button());
  EXPECT_EQ(SyntheticPointerActionParams::PointerActionType::RELEASE,
            action_list_params.params[1][0].pointer_action_type());
}

TEST(ActionsParserTestDriverTest, ParseTouchPointerActionSequence) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"type": "pointer",
                "actions": [{"type": "pointerDown", "x": 3, "y": 5},
                            {"type": "pointerMove", "x": 30, "y": 30},
                            {"type": "pointerUp" }],
                "parameters": {"pointerType": "touch"},
                "id": "pointer1"},
               {"type": "pointer",
                "actions": [{"type": "pointerDown", "x": 10, "y": 10},
                            {"type": "pointerMove", "x": 50, "y": 50},
                            {"type": "pointerUp" }],
                "parameters": {"pointerType": "touch"},
                "id": "pointer2"}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_TRUE(actions_parser.Parse());
  SyntheticPointerActionListParams action_list_params =
      actions_parser.pointer_action_params();
  EXPECT_EQ(content::mojom::GestureSourceType::kTouchInput,
            action_list_params.gesture_source_type);
  EXPECT_EQ(3U, action_list_params.params.size());
  EXPECT_EQ(2U, action_list_params.params[0].size());
  EXPECT_EQ(SyntheticPointerActionParams::PointerActionType::PRESS,
            action_list_params.params[0][0].pointer_action_type());
  EXPECT_EQ(gfx::PointF(3, 5), action_list_params.params[0][0].position());
  EXPECT_EQ(0U, action_list_params.params[0][0].pointer_id());
  EXPECT_EQ(SyntheticPointerActionParams::PointerActionType::PRESS,
            action_list_params.params[0][1].pointer_action_type());
  EXPECT_EQ(gfx::PointF(10, 10), action_list_params.params[0][1].position());
  EXPECT_EQ(1U, action_list_params.params[0][1].pointer_id());
}

TEST(ActionsParserTestDriverTest, ParseTouchPointerActionSequenceWithPause) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"type": "pointer",
                "actions": [{"type": "pointerDown", "x": 3, "y": 5},
                            {"type": "pointerMove", "x": 30, "y": 30},
                            {"type": "pointerUp" }],
                "parameters": {"pointerType": "touch"},
                "id": "pointer1"},
               {"type":"none",
                "actions":[{"type":"pause", "duration":0},
                           {"type":"pause", "duration":50},
                           {"type":"pause", "duration":0}],
                "id":"0"}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_TRUE(actions_parser.Parse());
  SyntheticPointerActionListParams action_list_params =
      actions_parser.pointer_action_params();
  EXPECT_EQ(content::mojom::GestureSourceType::kTouchInput,
            action_list_params.gesture_source_type);
  EXPECT_EQ(5U, action_list_params.params.size());
  EXPECT_EQ(2U, action_list_params.params[0].size());
  EXPECT_EQ(SyntheticPointerActionParams::PointerActionType::PRESS,
            action_list_params.params[0][0].pointer_action_type());
  EXPECT_EQ(gfx::PointF(3, 5), action_list_params.params[0][0].position());
  EXPECT_EQ(0U, action_list_params.params[0][0].pointer_id());
  EXPECT_EQ(SyntheticPointerActionParams::PointerActionType::IDLE,
            action_list_params.params[0][1].pointer_action_type());
  EXPECT_EQ(SyntheticPointerActionParams::PointerActionType::MOVE,
            action_list_params.params[1][0].pointer_action_type());
  EXPECT_EQ(gfx::PointF(30, 30), action_list_params.params[1][0].position());
  EXPECT_EQ(0U, action_list_params.params[1][0].pointer_id());
  EXPECT_EQ(SyntheticPointerActionParams::PointerActionType::IDLE,
            action_list_params.params[1][1].pointer_action_type());
  EXPECT_EQ(SyntheticPointerActionParams::PointerActionType::IDLE,
            action_list_params.params[2][0].pointer_action_type());
  EXPECT_EQ(SyntheticPointerActionParams::PointerActionType::IDLE,
            action_list_params.params[2][1].pointer_action_type());
  EXPECT_EQ(SyntheticPointerActionParams::PointerActionType::IDLE,
            action_list_params.params[3][0].pointer_action_type());
  EXPECT_EQ(SyntheticPointerActionParams::PointerActionType::IDLE,
            action_list_params.params[3][1].pointer_action_type());
  EXPECT_EQ(SyntheticPointerActionParams::PointerActionType::RELEASE,
            action_list_params.params[4][0].pointer_action_type());
  EXPECT_EQ(0U, action_list_params.params[4][0].pointer_id());
  EXPECT_EQ(SyntheticPointerActionParams::PointerActionType::IDLE,
            action_list_params.params[4][1].pointer_action_type());
}

TEST(ActionsParserTestDriverTest, ParseTouchPointerActionSequenceIdNotString) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"type": "pointer",
                "actions": [{"type": "pointerDown", "x": 0, "y": 0},
                            {"type": "pointerMove", "x": 30, "y": 30},
                            {"type": "pointerUp"}],
                "parameters": {"pointerType": "touch"},
                "id": 1},
               {"type": "pointer",
                "actions": [{"type": "pointerDown", "x": 10, "y": 10},
                            {"type": "pointerMove", "x": 50, "y": 50},
                            {"type": "pointerUp"}],
                "parameters": {"pointerType": "touch"},
                "id": 2}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_FALSE(actions_parser.Parse());
  EXPECT_EQ("pointer name is not defined or not a string",
            actions_parser.error_message());
}

TEST(ActionsParserTestDriverTest, ParseTouchPointerActionSequenceDuplicateId) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"type": "pointer",
                "actions": [{"type": "pointerDown", "x": 0, "y": 0},
                            {"type": "pointerMove", "x": 30, "y": 30},
                            {"type": "pointerUp"}],
                "parameters": {"pointerType": "touch"},
                "id": "pointer1"},
               {"type": "pointer",
                "actions": [{"type": "pointerDown", "x": 10, "y": 10},
                            {"type": "pointerMove", "x": 50, "y": 50},
                            {"type": "pointerUp"}],
                "parameters": {"pointerType": "touch"},
                "id": "pointer1"}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_FALSE(actions_parser.Parse());
  EXPECT_EQ("pointer name already exists", actions_parser.error_message());
}

TEST(ActionsParserTestDriverTest, ParseMousePointerActionSequenceNoParameters) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"type": "pointer",
                "actions": [{"type": "pointerDown", "x": 2, "y": 3,
                             "button": 0},
                            {"type": "pointerUp", "x": 2, "y": 3,
                             "button": 0}],
                "id": "pointer1"}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_TRUE(actions_parser.Parse());
  SyntheticPointerActionListParams action_list_params =
      actions_parser.pointer_action_params();
  EXPECT_EQ(content::mojom::GestureSourceType::kMouseInput,
            action_list_params.gesture_source_type);
  EXPECT_EQ(2U, action_list_params.params.size());
  EXPECT_EQ(1U, action_list_params.params[0].size());
  EXPECT_EQ(SyntheticPointerActionParams::PointerActionType::PRESS,
            action_list_params.params[0][0].pointer_action_type());
  EXPECT_EQ(gfx::PointF(2, 3), action_list_params.params[0][0].position());
  EXPECT_EQ(SyntheticPointerActionParams::Button::LEFT,
            action_list_params.params[0][0].button());
  EXPECT_EQ(SyntheticPointerActionParams::PointerActionType::RELEASE,
            action_list_params.params[1][0].pointer_action_type());
}

TEST(ActionsParserTestDriverTest,
     ParseMousePointerActionSequenceNoPointerType) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"type": "pointer",
                "actions": [{"type": "pointerDown", "x": 2, "y": 3,
                             "button": 0},
                            {"type": "pointerUp", "x": 2, "y": 3,
                             "button": 0}],
                "parameters": {},
                "id": "pointer1"}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_FALSE(actions_parser.Parse());
  EXPECT_EQ("action sequence pointer type is not defined or not a string",
            actions_parser.error_message());
}

TEST(ActionsParserTestDriverTest, ParseMousePointerActionSequenceNoAction) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"type": "pointer", "parameters": {"pointerType": "mouse"},
                "id": "pointer1"}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_FALSE(actions_parser.Parse());
  EXPECT_EQ("action_sequence[0].actions is not defined or not a list",
            actions_parser.error_message());
}

TEST(ActionsParserTestDriverTest,
     ParseMousePointerActionSequenceUnsupportedButton) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"type": "pointer",
                "actions": [{"type": "pointerDown", "x": 2, "y": 3,
                             "button": -1},
                            {"type": "pointerUp", "x": 2, "y": 3,
                             "button": 0}],
                "parameters": {"pointerType": "mouse"},
                "id": "1"}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_FALSE(actions_parser.Parse());
  EXPECT_EQ("actions[0].actions.button is an unsupported button",
            actions_parser.error_message());
}

TEST(ActionsParserTestDriverTest,
     ParseTouchPointerActionSequenceMultiActionsType) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"type": "key",
                "actions": [{"type":"keyDown","value":"p"},
                            {"type":"keyUp","value":"p"},
                            {"type":"keyDown","value":"a"},
                            {"type":"keyUp","value":"a"}],
                "id": "1"}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_FALSE(actions_parser.Parse());
  EXPECT_EQ("we do not support action sequence type of key",
            actions_parser.error_message());
}

TEST(ActionsParserTestDriverTest,
     ParseTouchPointerActionSequenceMultiPointerType) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"type": "pointer",
                "actions": [{"type": "pointerDown", "x": 3, "y": 5},
                            {"type": "pointerMove", "x": 30, "y": 30},
                            {"type": "pointerUp" }],
                "parameters": {"pointerType": "touch"},
                "id": "pointer1"},
               {"type": "pointer",
                "actions": [{"type": "pointerDown", "x": 10, "y": 10},
                            {"type": "pointerMove", "x": 50, "y": 50},
                            {"type": "pointerUp" }],
                "parameters": {"pointerType": "mouse"},
                "id": "1"}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_FALSE(actions_parser.Parse());
  EXPECT_EQ("currently multiple action sequence pointer type are not supported",
            actions_parser.error_message());
}

TEST(ActionsParserTestDriverTest, ParseTouchPointerActionSequenceMultiMouse) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"type": "pointer",
                "actions": [{"type": "pointerDown", "x": 3, "y": 5},
                            {"type": "pointerMove", "x": 30, "y": 30},
                            {"type": "pointerUp" }],
                "parameters": {"pointerType": "mouse"},
                "id": "1"},
               {"type": "pointer",
                "actions": [{"type": "pointerDown", "x": 10, "y": 10},
                            {"type": "pointerMove", "x": 50, "y": 50},
                            {"type": "pointerUp" }],
                "parameters": {"pointerType": "mouse"},
                "id": "2"}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_FALSE(actions_parser.Parse());
  EXPECT_EQ("for input type of mouse and pen, we only support one device",
            actions_parser.error_message());
}

TEST(ActionsParserTestDriverTest, ParseWheelScrollAction) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"type": "wheel",
                "actions": [{"type": "scroll", "x": 10, "y": 10,
                             "deltaX": 30, "deltaY": 50}]}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_TRUE(actions_parser.Parse());
  SyntheticSmoothScrollGestureParams scroll_gesture_params =
      actions_parser.smooth_scroll_params();
  EXPECT_EQ(content::mojom::GestureSourceType::kMouseInput,
            scroll_gesture_params.gesture_source_type);
  EXPECT_EQ(gfx::PointF(10, 10), scroll_gesture_params.anchor);
  EXPECT_EQ(1U, scroll_gesture_params.distances.size());
  EXPECT_EQ(-gfx::Vector2d(30, 50), scroll_gesture_params.distances[0]);
  EXPECT_EQ(true, scroll_gesture_params.prevent_fling);
  EXPECT_EQ(8000, scroll_gesture_params.speed_in_pixels_s);
  EXPECT_EQ(0, scroll_gesture_params.fling_velocity_x);
  EXPECT_EQ(0, scroll_gesture_params.fling_velocity_y);
  EXPECT_EQ(ui::ScrollGranularity::kScrollByPrecisePixel,
            scroll_gesture_params.granularity);
}

TEST(ActionsParserTestDriverTest, ParseWheelScrollActionNoSourceType) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{
            "actions": [{"type": "scroll", "x": 10, "y": 10,
                         "deltaX": 30, "deltaY": 50}]}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_FALSE(actions_parser.Parse());
  EXPECT_EQ("source type is not defined or not a string",
            actions_parser.error_message());
}

TEST(ActionsParserTestDriverTest, ParseWheelScrollActionInvalidDelta) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"type": "wheel",
                "actions": [{"type": "scroll", "x": 10, "y": 10,
                             "deltaX": 30.2, "deltaY": 50}]}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_FALSE(actions_parser.Parse());
  EXPECT_EQ("actions[0].actions.delta_x is not defined or not an integer",
            actions_parser.error_message());
}

TEST(ActionsParserTestDriverTest, ParseWheelScrollNoActionType) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"type": "wheel",
                "actions": [{"x": 10, "y": 10,
                             "deltaX": 30, "deltaY": 50}]}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_FALSE(actions_parser.Parse());
  EXPECT_EQ("actions[0].actions.type is not defined or not a string",
            actions_parser.error_message());
}

TEST(ActionsParserTestDriverTest, ParseWheelScrollInvalidActionType) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"type": "wheel",
                "actions": [{"type": "zoom", "x": 10, "y": 10,
                             "deltaX": 30, "deltaY": 30}]}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_FALSE(actions_parser.Parse());
  EXPECT_EQ(
      "actions[0].actions.type is not scroll or pause when source type is "
      "wheel",
      actions_parser.error_message());
}

TEST(ActionsParserTestDriverTest, ParseWheelScrollInvalidActionList) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"type": "wheel",
                "actions": [{"type": "scroll", "x": 10, "y": 10,
                             "deltaX": 30, "deltaY": 50},
                            {"type": "scroll", "x": 10, "y": 10,
                             "deltaX": 30, "deltaY": 50}]}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_FALSE(actions_parser.Parse());
  EXPECT_EQ(
      "action_sequence[0].actions should only have one action for the wheel "
      "input source",
      actions_parser.error_message());
}

TEST(ActionsParserTestDriverTest, ParseMultiInputSource) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"type": "pointer",
                "actions": [{"type": "pointerDown", "x": 3, "y": 5},
                            {"type": "pointerMove", "x": 30, "y": 30},
                            {"type": "pointerUp" },
                            {"type": "pause", "duration": 10 },
                            {"type": "pause", "duration": 10 }],
                "parameters": {"pointerType": "mouse"},
                "id": "1"},
               {"type": "key",
                "actions": [{"type": "pause", "duration": 10 },
                            {"type": "pause", "duration": 10 },
                            {"type": "pause", "duration": 10 },
                            {"type": "keyDown", "value": "a"},
                            {"type": "keyUp", "value": "a"}],
                "id": "2"}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_FALSE(actions_parser.Parse());
  EXPECT_EQ("currently multiple input source types are not supported",
            actions_parser.error_message());
}

TEST(ActionsParserTestDriverTest, ParseActionSequenceInvalidInputSourceType) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"type": "touchpad",
                "actions": [{"type": "scroll", "x": 10, "y": 10,
                             "deltaX": 30, "deltaY": 30}]}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_FALSE(actions_parser.Parse());
  EXPECT_EQ("the input source type touchpad is invalid",
            actions_parser.error_message());
}

TEST(ActionsParserTestDriverTest, ParseActionSequenceWithoutY) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"type": "pointer",
                "actions": [{"type": "pointerDown", "x": 3},
                            {"type": "pointerMove", "x": 30, "y": 30},
                            {"type": "pointerUp" }],
                "parameters": {"pointerType": "mouse"},
                "id": "1"}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_FALSE(actions_parser.Parse());
  EXPECT_EQ("actions[0].actions.y is not defined or not a number",
            actions_parser.error_message());
}

TEST(ActionsParserTestDriverTest, ParseActionSequencePenProperties) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"type": "pointer",
                "actions": [{"type": "pointerMove", "x": 10, "y": 5},
                            {"type": "pointerDown", "x": 10, "y": 5,
                             "button": 0, "pressure": 0.36, "tiltX": 21,
                             "twist": 53},
                            {"type": "pointerMove", "x": 30, "y": 20},
                            {"type": "pointerUp" }],
                "parameters": {"pointerType": "pen"},
                "id": "1"}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_TRUE(actions_parser.Parse());
  SyntheticPointerActionListParams action_list_params =
      actions_parser.pointer_action_params();
  EXPECT_EQ(mojom::GestureSourceType::kPenInput,
            action_list_params.gesture_source_type);
  EXPECT_EQ(4U, action_list_params.params.size());
  EXPECT_EQ(1U, action_list_params.params[1].size());
  EXPECT_EQ(SyntheticPointerActionParams::PointerActionType::PRESS,
            action_list_params.params[1][0].pointer_action_type());
  EXPECT_EQ(gfx::PointF(10, 5), action_list_params.params[1][0].position());
  EXPECT_EQ(SyntheticPointerActionParams::Button::LEFT,
            action_list_params.params[1][0].button());
  EXPECT_EQ(0.36f, action_list_params.params[1][0].force());
  EXPECT_EQ(21, action_list_params.params[1][0].tilt_x());
  EXPECT_EQ(53, action_list_params.params[1][0].rotation_angle());
}

TEST(ActionsParserTestDriverTest, ParseActionSequenceInvalidForce) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"type": "pointer",
                "actions": [{"type": "pointerMove", "x": 10, "y": 10},
                            {"type": "pointerDown", "x": 10, "y": 10,
                             "button": 0, "pressure": 6.36},
                            {"type": "pointerMove", "x": 30, "y": 30},
                            {"type": "pointerUp" }],
                "parameters": {"pointerType": "pen"},
                "id": "1"}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_FALSE(actions_parser.Parse());
  EXPECT_EQ(
      "actions[0].actions.pressure must be a non-negative number in the range "
      "of [0,1]",
      actions_parser.error_message());
}

TEST(ActionsParserTestDriverTest, ParseActionSequenceInvalidTiltX) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"type": "pointer",
                "actions": [{"type": "pointerMove", "x": 10, "y": 10},
                            {"type": "pointerDown", "x": 10, "y": 10,
                             "button": 0, "tiltX": 110},
                            {"type": "pointerMove", "x": 30, "y": 30},
                            {"type": "pointerUp" }],
                "parameters": {"pointerType": "pen"},
                "id": "1"}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_FALSE(actions_parser.Parse());
  EXPECT_EQ(
      "actions[0].actions.tiltX must be an integer in the range of [-90,90]",
      actions_parser.error_message());
}

}  // namespace content
