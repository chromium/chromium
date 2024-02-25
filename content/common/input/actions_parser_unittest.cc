// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/actions_parser.h"

#include <utility>

#include "base/json/json_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(ActionsParserTest, ParseMousePointerActionSequence) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"source": "mouse", "id": 0,
                "actions": [{"name": "pointerDown", "x": 2, "y": 3,
                             "button": 0},
                            {"name": "pointerUp", "x": 2, "y": 3,
                             "button": 0}]}] )JSON");

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

TEST(ActionsParserTest, ParseTouchPointerActionSequence1) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"source": "touch", "id": 1,
                "actions": [{"name": "pointerDown", "x": 3, "y": 5},
                            {"name": "pointerMove", "x": 30, "y": 30},
                            {"name": "pointerUp" } ]},
               {"source": "touch", "id": 2,
                "actions": [{"name": "pointerDown", "x": 10, "y": 10},
                            {"name": "pointerMove", "x": 50, "y": 50},
                            {"name": "pointerUp" } ]}] )JSON");

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

TEST(ActionsParserTest, ParseTouchPointerActionSequenceWithoutId) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"source": "touch", "id": 0,
                "actions": [{"name": "pointerDown", "x": 3, "y": 5},
                            {"name": "pointerMove", "x": 30, "y": 30},
                            {"name": "pointerUp" } ]},
               {"source": "touch", "id": 1,
                "actions": [{"name": "pointerDown", "x": 10, "y": 10},
                            {"name": "pointerMove", "x": 50, "y": 50},
                            {"name": "pointerUp" } ]}] )JSON");

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

TEST(ActionsParserTest, ParseMousePointerActionSequenceNoSource) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"id": 0,
                "actions": [{"name": "pointerDown", "x": 2, "y": 3,
                             "button": 0},
                            {"name": "pointerUp", "x": 2, "y": 3,
                             "button": 0}]}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_FALSE(actions_parser.Parse());
  EXPECT_EQ("source type is not defined or not a string",
            actions_parser.error_message());
}

TEST(ActionsParserTest, ParseMousePointerActionSequenceNoAction) {
  std::optional<base::Value> value =
      base::JSONReader::Read(R"JSON( [{"source": "mouse", "id": 0}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_FALSE(actions_parser.Parse());
  EXPECT_EQ("action_sequence[0].actions is not defined or not a list",
            actions_parser.error_message());
}

TEST(ActionsParserTest, ParseMousePointerActionSequenceUnsupportedButton) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"source": "mouse", "id": 0,
                "actions": [{"name": "pointerDown", "x": 2, "y": 3,
                             "button": -1},
                            {"name": "pointerUp", "x": 2, "y": 3,
                             "button": 0}]}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_FALSE(actions_parser.Parse());
  EXPECT_EQ("actions[0].actions.button is an unsupported button",
            actions_parser.error_message());
}

TEST(ActionsParserTest, ParseTouchPointerActionSequenceMultiSource) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"source": "touch", "id": 1,
                "actions": [{"name": "pointerDown", "x": 3, "y": 5},
                            {"name": "pointerMove", "x": 30, "y": 30},
                            {"name": "pointerUp" } ]},
               {"source": "mouse", "id": 2,
                "actions": [{"name": "pointerDown", "x": 10, "y": 10},
                            {"name": "pointerMove", "x": 50, "y": 50},
                            {"name": "pointerUp" } ]}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_FALSE(actions_parser.Parse());
  EXPECT_EQ("currently multiple input types are not not supported",
            actions_parser.error_message());
}

TEST(ActionsParserTest, ParseTouchPointerActionSequenceMultiMouse) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"source": "mouse", "id": 1,
                "actions": [{"name": "pointerDown", "x": 3, "y": 5},
                            {"name": "pointerMove", "x": 30, "y": 30},
                            {"name": "pointerUp" } ]},
               {"source": "mouse", "id": 2,
                "actions": [{"name": "pointerDown", "x": 10, "y": 10},
                            {"name": "pointerMove", "x": 50, "y": 50},
                            {"name": "pointerUp" } ]}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_FALSE(actions_parser.Parse());
  EXPECT_EQ(
      "for source type of mouse and pen, we only support one device in one "
      "sequence",
      actions_parser.error_message());
}

TEST(ActionsParserTest, ParsePointerActionSequenceInvalidKey) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"source": "mouse", "id": 0,
                "actions": [{"name": "pointerDown", "x": 3, "y": 5,
                             "keys": "Ctrl"} ]}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_FALSE(actions_parser.Parse());
  EXPECT_EQ("actions[0].actions.key is not a valid key",
            actions_parser.error_message());
}

TEST(ActionsParserTest, ParsePointerActionSequenceEmptyActionList) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"source": "mouse", "id": 0,
                "actions": []}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_FALSE(actions_parser.Parse());
  EXPECT_EQ("action_sequence[0].actions is an empty list",
            actions_parser.error_message());
}

TEST(ActionsParserTest, ParsePointerActionSequenceInvalidPointerType) {
  std::optional<base::Value> value = base::JSONReader::Read(
      R"JSON( [{"source": "wheel", "id": 0,
                "actions": [{"name": "pointerDown", "x": 3, "y": 5,
                             "keys": "Ctrl"} ]}] )JSON");

  ActionsParser actions_parser(std::move(value.value()));
  EXPECT_FALSE(actions_parser.Parse());
  EXPECT_EQ("source type wheel is an unsupported input type",
            actions_parser.error_message());
}

}  // namespace content
