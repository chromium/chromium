// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/action_target.h"

#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {

using ActionTargetTest = PlatformTest;

// Test that ActionTarget::FromProto correctly parses a coordinate target.
TEST_F(ActionTargetTest, TestFromProtoCoordinateValid) {
  optimization_guide::proto::ActionTarget proto;
  proto.mutable_coordinate()->set_x(10);
  proto.mutable_coordinate()->set_y(20);
  proto.mutable_coordinate()->set_pixel_type(
      optimization_guide::proto::Coordinate::PIXEL_TYPE_DIPS);

  ActionTarget target = ActionTarget::FromProto(proto);
  EXPECT_TRUE(target.is_valid());
  ASSERT_TRUE(target.coordinate().has_value());
  EXPECT_EQ(target.coordinate()->x, 10);
  EXPECT_EQ(target.coordinate()->y, 20);
  EXPECT_EQ(target.coordinate()->pixel_type,
            optimization_guide::proto::Coordinate::PIXEL_TYPE_DIPS);
  EXPECT_FALSE(target.node_id().has_value());
}

// Test that ActionTarget::FromProto correctly parses a node ID target.
TEST_F(ActionTargetTest, TestFromProtoNodeIdValid) {
  optimization_guide::proto::ActionTarget proto;
  proto.set_content_node_id(123);
  proto.mutable_document_identifier()->set_serialized_token("doc_token");

  ActionTarget target = ActionTarget::FromProto(proto);
  EXPECT_TRUE(target.is_valid());
  ASSERT_TRUE(target.node_id().has_value());
  EXPECT_EQ(target.node_id()->content_node_id, 123);
  EXPECT_EQ(target.node_id()->document_identifier, "doc_token");
  EXPECT_FALSE(target.coordinate().has_value());
}

// Test that an empty proto produces an invalid ActionTarget.
TEST_F(ActionTargetTest, TestFromProtoEmptyInvalid) {
  optimization_guide::proto::ActionTarget proto;
  ActionTarget target = ActionTarget::FromProto(proto);
  EXPECT_FALSE(target.is_valid());
  EXPECT_FALSE(target.coordinate().has_value());
  EXPECT_FALSE(target.node_id().has_value());
}

// Test that a node ID target without document identifier is invalid.
TEST_F(ActionTargetTest, TestFromProtoNodeIdWithoutDocumentIdentifierInvalid) {
  optimization_guide::proto::ActionTarget proto;
  proto.set_content_node_id(123);

  ActionTarget target = ActionTarget::FromProto(proto);
  EXPECT_FALSE(target.is_valid());
  EXPECT_FALSE(target.node_id().has_value());
}

// Test that a node ID target without content node ID is invalid.
TEST_F(ActionTargetTest, TestFromProtoNodeIdWithoutContentNodeIdInvalid) {
  optimization_guide::proto::ActionTarget proto;
  proto.mutable_document_identifier()->set_serialized_token("doc_token");

  ActionTarget target = ActionTarget::FromProto(proto);
  EXPECT_FALSE(target.is_valid());
  EXPECT_FALSE(target.node_id().has_value());
}

// Test that specifying both coordinate and node ID targeting uses coordinate.
TEST_F(ActionTargetTest, TestFromProtoBothTargetingTypesUsesCoordinate) {
  optimization_guide::proto::ActionTarget proto;
  proto.mutable_coordinate()->set_x(10);
  proto.mutable_coordinate()->set_y(20);
  proto.mutable_coordinate()->set_pixel_type(
      optimization_guide::proto::Coordinate::PIXEL_TYPE_DIPS);
  proto.set_content_node_id(123);
  proto.mutable_document_identifier()->set_serialized_token("doc_token");

  ActionTarget target = ActionTarget::FromProto(proto);
  EXPECT_TRUE(target.is_valid());
  ASSERT_TRUE(target.coordinate().has_value());
  EXPECT_EQ(target.coordinate()->x, 10);
  EXPECT_EQ(target.coordinate()->y, 20);
  EXPECT_EQ(target.coordinate()->pixel_type,
            optimization_guide::proto::Coordinate::PIXEL_TYPE_DIPS);
  EXPECT_FALSE(target.node_id().has_value());
}

// Test that UpdateCoordinate updates the coordinates of a valid coordinate
// target.
TEST_F(ActionTargetTest, TestUpdateCoordinate) {
  optimization_guide::proto::ActionTarget proto;
  proto.mutable_coordinate()->set_x(10);
  proto.mutable_coordinate()->set_y(20);

  ActionTarget target = ActionTarget::FromProto(proto);
  ASSERT_TRUE(target.coordinate().has_value());
  target.UpdateCoordinate(30, 40);

  ASSERT_TRUE(target.coordinate().has_value());
  EXPECT_EQ(target.coordinate()->x, 30);
  EXPECT_EQ(target.coordinate()->y, 40);
}

}  // namespace actor
