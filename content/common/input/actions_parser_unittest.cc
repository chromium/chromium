// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/actions_parser.h"

#include "base/json/json_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(ActionsParserTest, ParseMousePointerActionSequence) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"source": "mouse", "id": 0,
            "actions": [{"name": "pointerDown", "x": 2, "y": 3,
                         "button": "left"},
                        {"name": "pointerUp", "x": 2, "y": 3,
                         "button": "left"}]}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_TRUE(actions_parser.ParsePointerActionSequence());
  SyntheticPointerActionListParams action_list_params =
      actions_parser.gesture_params();
  EXPECT_EQ(SyntheticGestureParams::MOUSE_INPUT,
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

TEST(ActionsParserTest, ParseTouchPointerActionSequence) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"source": "touch", "id": 1,
            "actions": [{"name": "pointerDown", "x": 3, "y": 5},
                        {"name": "pointerMove", "x": 30, "y": 30},
                        {"name": "pointerUp" } ]},
           {"source": "touch", "id": 2,
            "actions": [{"name": "pointerDown", "x": 10, "y": 10},
                        {"name": "pointerMove", "x": 50, "y": 50},
                        {"name": "pointerUp" } ]}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_TRUE(actions_parser.ParsePointerActionSequence());
  SyntheticPointerActionListParams action_list_params =
      actions_parser.gesture_params();
  EXPECT_EQ(SyntheticGestureParams::TOUCH_INPUT,
            action_list_params.gesture_source_type);
  EXPECT_EQ(3U, action_list_params.params.size());
  EXPECT_EQ(2U, action_list_params.params[0].size());
  EXPECT_EQ(SyntheticPointerActionParams::PointerActionType::PRESS,
            action_list_params.params[0][0].pointer_action_type());
  EXPECT_EQ(gfx::PointF(3, 5), action_list_params.params[0][0].position());
  EXPECT_EQ(1U, action_list_params.params[0][0].pointer_id());
  EXPECT_EQ(SyntheticPointerActionParams::PointerActionType::PRESS,
            action_list_params.params[0][1].pointer_action_type());
  EXPECT_EQ(gfx::PointF(10, 10), action_list_params.params[0][1].position());
  EXPECT_EQ(2U, action_list_params.params[0][1].pointer_id());
}

TEST(ActionsParserTest, ParseTouchPointerActionSequenceWithoutId) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"source": "touch", "id": 0,
            "actions": [{"name": "pointerDown", "x": 3, "y": 5},
                        {"name": "pointerMove", "x": 30, "y": 30},
                        {"name": "pointerUp" } ]},
           {"source": "touch", "id": 1,
            "actions": [{"name": "pointerDown", "x": 10, "y": 10},
                        {"name": "pointerMove", "x": 50, "y": 50},
                        {"name": "pointerUp" } ]}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_TRUE(actions_parser.ParsePointerActionSequence());
  SyntheticPointerActionListParams action_list_params =
      actions_parser.gesture_params();
  EXPECT_EQ(SyntheticGestureParams::TOUCH_INPUT,
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

TEST(ActionsParserTest, ParseTouchPointerActionSequenceIdNotInt) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"source": "touch", "id": "0",
            "actions": [{"name": "pointerDown", "x": 0, "y": 0},
                        {"name": "pointerMove", "x": 30, "y": 30},
                        {"name": "pointerUp" } ]},
           {"source": "touch", "id": "1",
            "actions": [{"name": "pointerDown", "x": 10, "y": 10},
                        {"name": "pointerMove", "x": 50, "y": 50},
                        {"name": "pointerUp" } ]}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_FALSE(actions_parser.ParsePointerActionSequence());
  EXPECT_EQ("pointer id is not an integer", actions_parser.error_message());
}

TEST(ActionsParserTest, ParseTouchPointerActionSequenceIdNegative) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"source": "touch", "id": -1,
            "actions": [{"name": "pointerDown", "x": 0, "y": 0},
                        {"name": "pointerMove", "x": 30, "y": 30},
                        {"name": "pointerUp" } ]},
           {"source": "touch", "id": -2,
            "actions": [{"name": "pointerDown", "x": 10, "y": 10},
                        {"name": "pointerMove", "x": 50, "y": 50},
                        {"name": "pointerUp" } ]}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_FALSE(actions_parser.ParsePointerActionSequence());
  EXPECT_EQ("pointer id can not be negative", actions_parser.error_message());
}

TEST(ActionsParserTest, ParseTouchPointerActionSequenceDuplicateId) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"source": "touch", "id": 0,
            "actions": [{"name": "pointerDown", "x": 0, "y": 0},
                        {"name": "pointerMove", "x": 30, "y": 30},
                        {"name": "pointerUp" } ]},
           {"source": "touch", "id": 0,
            "actions": [{"name": "pointerDown", "x": 10, "y": 10},
                        {"name": "pointerMove", "x": 50, "y": 50},
                        {"name": "pointerUp" } ]}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_FALSE(actions_parser.ParsePointerActionSequence());
  EXPECT_EQ("pointer id already exists", actions_parser.error_message());
}

TEST(ActionsParserTest, ParseTouchPointerActionSequenceNoId) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"source": "touch", "id": 0,
            "actions": [{"name": "pointerDown", "x": 0, "y": 0},
                        {"name": "pointerMove", "x": 30, "y": 30},
                        {"name": "pointerUp" } ]},
           {"source": "touch",
            "actions": [{"name": "pointerDown", "x": 10, "y": 10},
                        {"name": "pointerMove", "x": 50, "y": 50},
                        {"name": "pointerUp" } ]}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_FALSE(actions_parser.ParsePointerActionSequence());
  EXPECT_EQ("this pointer does not have a pointer id",
            actions_parser.error_message());
}

TEST(ActionsParserTest, ParseTouchPointerActionSequenceMissingId) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"source": "touch",
            "actions": [{"name": "pointerDown", "x": 0, "y": 0},
                        {"name": "pointerMove", "x": 30, "y": 30},
                        {"name": "pointerUp" } ]},
           {"source": "touch", "id": 0,
            "actions": [{"name": "pointerDown", "x": 10, "y": 10},
                        {"name": "pointerMove", "x": 50, "y": 50},
                        {"name": "pointerUp" } ]}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_FALSE(actions_parser.ParsePointerActionSequence());
  EXPECT_EQ("some pointers do not have a pointer id",
            actions_parser.error_message());
}

TEST(ActionsParserTest, ParseMousePointerActionSequenceNoSource) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"id": 0, "actions": [{"name": "pointerDown", "x": 2, "y": 3,
                                  "button": "left"},
                                 {"name": "pointerUp", "x": 2, "y": 3,
                                  "button": "left"}]}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_FALSE(actions_parser.ParsePointerActionSequence());
  EXPECT_EQ("source type is missing or not a string",
            actions_parser.error_message());
}

TEST(ActionsParserTest, ParseMousePointerActionSequenceNoAction) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"source": "mouse", "id": 0}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_FALSE(actions_parser.ParsePointerActionSequence());
  EXPECT_EQ("pointer[0].actions is missing or not a list",
            actions_parser.error_message());
}

TEST(ActionsParserTest, ParseMousePointerActionSequenceUnsupportedButton) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"source": "mouse", "id": 0,
            "actions": [{"name": "pointerDown", "x": 2, "y": 3,
                         "button": "l"},
                        {"name": "pointerUp", "x": 2, "y": 3,
                         "button": "left"}]}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_FALSE(actions_parser.ParsePointerActionSequence());
  EXPECT_EQ("actions[0].actions.button is an unsupported button",
            actions_parser.error_message());
}

TEST(ActionsParserTest, ParseTouchPointerActionSequenceMultiSource) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"source": "touch", "id": 1,
            "actions": [{"name": "pointerDown", "x": 3, "y": 5},
                        {"name": "pointerMove", "x": 30, "y": 30},
                        {"name": "pointerUp" } ]},
           {"source": "mouse", "id": 2,
            "actions": [{"name": "pointerDown", "x": 10, "y": 10},
                        {"name": "pointerMove", "x": 50, "y": 50},
                        {"name": "pointerUp" } ]}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_FALSE(actions_parser.ParsePointerActionSequence());
  EXPECT_EQ("currently multiple input sources are not not supported",
            actions_parser.error_message());
}

TEST(ActionsParserTest, ParseTouchPointerActionSequenceMultiMouse) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"source": "mouse", "id": 1,
            "actions": [{"name": "pointerDown", "x": 3, "y": 5},
                        {"name": "pointerMove", "x": 30, "y": 30},
                        {"name": "pointerUp" } ]},
           {"source": "mouse", "id": 2,
            "actions": [{"name": "pointerDown", "x": 10, "y": 10},
                        {"name": "pointerMove", "x": 50, "y": 50},
                        {"name": "pointerUp" } ]}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_FALSE(actions_parser.ParsePointerActionSequence());
  EXPECT_EQ(
      "for input source type of mouse and pen, we only support one device in "
      "one sequence",
      actions_parser.error_message());
}

}  // namespace content
