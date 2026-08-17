// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/attempt_form_filling_tool_java_script_feature.h"

#import "base/strings/stringprintf.h"
#import "base/test/test_future.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_java_script_feature_test_base.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace actor {

// Test fixture for `AttemptFormFillingToolJavaScriptFeature`.
class AttemptFormFillingToolJavaScriptFeatureTest
    : public ActorToolJavaScriptFeatureTestBase {
 protected:
  AttemptFormFillingToolJavaScriptFeatureTest()
      : ActorToolJavaScriptFeatureTestBase() {}

  void SetUp() override { ActorToolJavaScriptFeatureTestBase::SetUp(); }

  AttemptFormFillingToolJavaScriptFeature* feature() {
    return AttemptFormFillingToolJavaScriptFeature::GetInstance();
  }

  // Mocks `getAutofillRendererIds` JavaScript method to return
  // `mock_return_value`.
  void MockGetAutofillRendererIds(const std::string& mock_return_value) {
    MockJsFunction(feature(), "attempt_form_filling", "getAutofillRendererIds",
                   mock_return_value);
  }

  // Create target with given coordinates.
  ActionTarget CreateTargetWithCoordinates() {
    optimization_guide::proto::ActionTarget target;
    // Use arbitrary values since the JS function is mocked.
    target.mutable_coordinate()->set_x(10);
    target.mutable_coordinate()->set_y(20);
    target.mutable_coordinate()->set_pixel_type(
        optimization_guide::proto::Coordinate::PIXEL_TYPE_UNSPECIFIED);
    return ActionTarget::FromProto(target);
  }

  ActionTarget CreateTargetWithNodeId() {
    optimization_guide::proto::ActionTarget target;
    // Use arbitrary values since the JS function is mocked.
    target.set_content_node_id(456);
    target.mutable_document_identifier()->set_serialized_token("doc_id");
    return ActionTarget::FromProto(target);
  }
};

// Test that when JavaScript returns success, GetAutofillRendererIds succeeds
// and returns the renderer IDs as a vector of uint32_t.
TEST_F(AttemptFormFillingToolJavaScriptFeatureTest, CoordinatesSuccess) {
  MockGetAutofillRendererIds("{ resultCode: 0, uniqueIds: ['12345'] }");
  ActionTarget target_coord = CreateTargetWithCoordinates();
  base::test::TestFuture<
      base::expected<std::vector<uint32_t>, ToolExecutionResult>>
      future;

  feature()->GetAutofillRendererIds(GetMainFrame(feature())->AsWeakPtr(),
                                    {target_coord}, future.GetCallback());

  auto result = future.Get();
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result.value().size(), 1u);
  EXPECT_EQ(result.value()[0], 12345u);
}

// Test that when JavaScript returns a coordinates out of bounds result code,
// GetAutofillRendererIds fails.
TEST_F(AttemptFormFillingToolJavaScriptFeatureTest, CoordinatesOutOfBounds) {
  MockGetAutofillRendererIds("{ resultCode: 2, uniqueIds: [] }");
  ActionTarget target_coord = CreateTargetWithCoordinates();
  base::test::TestFuture<
      base::expected<std::vector<uint32_t>, ToolExecutionResult>>
      future;

  feature()->GetAutofillRendererIds(GetMainFrame(feature())->AsWeakPtr(),
                                    {target_coord}, future.GetCallback());

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code(),
            mojom::ActionResultCode::kCoordinatesOutOfBounds);
}

// Test that when JavaScript returns a non-dictionary value,
// GetAutofillRendererIds fails.
TEST_F(AttemptFormFillingToolJavaScriptFeatureTest, NonDictResultFails) {
  MockGetAutofillRendererIds("[{ resultCode: 0, uniqueIds: ['12345'] }]");
  ActionTarget target_coord = CreateTargetWithCoordinates();
  base::test::TestFuture<
      base::expected<std::vector<uint32_t>, ToolExecutionResult>>
      future;

  feature()->GetAutofillRendererIds(GetMainFrame(feature())->AsWeakPtr(),
                                    {target_coord}, future.GetCallback());

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().internal_code().value(),
            InternalToolErrorCode::kJavascriptFeatureGotInvalidResult);
}

// Test that when JavaScript returns success for a Node ID target,
// GetAutofillRendererIds succeeds and returns the renderer ID.
TEST_F(AttemptFormFillingToolJavaScriptFeatureTest, NodeIdSuccess) {
  MockGetAutofillRendererIds("{ resultCode: 0, uniqueIds: ['98765'] }");
  ActionTarget target_node = CreateTargetWithNodeId();
  base::test::TestFuture<
      base::expected<std::vector<uint32_t>, ToolExecutionResult>>
      future;

  feature()->GetAutofillRendererIds(GetMainFrame(feature())->AsWeakPtr(),
                                    {target_node}, future.GetCallback());

  auto result = future.Get();
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result.value().size(), 1u);
  EXPECT_EQ(result.value()[0], 98765u);
}

// Test that when JavaScript returns an invalid target result code,
// GetAutofillRendererIds fails with kArgumentsInvalid.
TEST_F(AttemptFormFillingToolJavaScriptFeatureTest, InvalidTargetFails) {
  MockGetAutofillRendererIds("{ resultCode: 1, uniqueIds: [] }");
  ActionTarget target_node = CreateTargetWithNodeId();
  base::test::TestFuture<
      base::expected<std::vector<uint32_t>, ToolExecutionResult>>
      future;

  feature()->GetAutofillRendererIds(GetMainFrame(feature())->AsWeakPtr(),
                                    {target_node}, future.GetCallback());

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code(), mojom::ActionResultCode::kArgumentsInvalid);
}

// Test that when JavaScript returns an invalid DOM node ID result code,
// GetAutofillRendererIds fails with kInvalidDomNodeId.
TEST_F(AttemptFormFillingToolJavaScriptFeatureTest, InvalidNodeIdFails) {
  MockGetAutofillRendererIds("{ resultCode: 3, uniqueIds: [] }");
  ActionTarget target_node = CreateTargetWithNodeId();
  base::test::TestFuture<
      base::expected<std::vector<uint32_t>, ToolExecutionResult>>
      future;

  feature()->GetAutofillRendererIds(GetMainFrame(feature())->AsWeakPtr(),
                                    {target_node}, future.GetCallback());

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code(), mojom::ActionResultCode::kInvalidDomNodeId);
}

// Test that when JavaScript returns a target not autofill element result code,
// GetAutofillRendererIds fails with kFormFillingFieldNotFound.
TEST_F(AttemptFormFillingToolJavaScriptFeatureTest,
       TargetNotAutofillElementFails) {
  MockGetAutofillRendererIds("{ resultCode: 4, uniqueIds: [] }");
  ActionTarget target_node = CreateTargetWithNodeId();
  base::test::TestFuture<
      base::expected<std::vector<uint32_t>, ToolExecutionResult>>
      future;

  feature()->GetAutofillRendererIds(GetMainFrame(feature())->AsWeakPtr(),
                                    {target_node}, future.GetCallback());

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code(),
            mojom::ActionResultCode::kFormFillingFieldNotFound);
}

// Test that when JavaScript returns a dictionary missing a uniqueIds list,
// GetAutofillRendererIds fails with kArgumentsInvalid.
TEST_F(AttemptFormFillingToolJavaScriptFeatureTest, MissingRendererIdsFails) {
  MockGetAutofillRendererIds("{ resultCode: 0 }");
  ActionTarget target_coord = CreateTargetWithCoordinates();
  base::test::TestFuture<
      base::expected<std::vector<uint32_t>, ToolExecutionResult>>
      future;

  feature()->GetAutofillRendererIds(GetMainFrame(feature())->AsWeakPtr(),
                                    {target_coord}, future.GetCallback());

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code(), mojom::ActionResultCode::kArgumentsInvalid);
  EXPECT_FALSE(result.error().internal_code().has_value());
}

// Test that when JavaScript returns a renderer ID value that is 0 or
// non-numeric, GetAutofillRendererIds fails with kFormFillingFieldNotFound.
TEST_F(AttemptFormFillingToolJavaScriptFeatureTest,
       InvalidRendererIdValueFails) {
  // Test case with renderer ID '0'
  {
    MockGetAutofillRendererIds("{ resultCode: 0, uniqueIds: ['0'] }");
    ActionTarget target_coord = CreateTargetWithCoordinates();
    base::test::TestFuture<
        base::expected<std::vector<uint32_t>, ToolExecutionResult>>
        future;

    feature()->GetAutofillRendererIds(GetMainFrame(feature())->AsWeakPtr(),
                                      {target_coord}, future.GetCallback());

    auto result = future.Get();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(),
              mojom::ActionResultCode::kFormFillingFieldNotFound);
  }

  // Test case with non-numeric renderer ID 'abc'
  {
    MockGetAutofillRendererIds("{ resultCode: 0, uniqueIds: ['abc'] }");
    ActionTarget target_coord = CreateTargetWithCoordinates();
    base::test::TestFuture<
        base::expected<std::vector<uint32_t>, ToolExecutionResult>>
        future;

    feature()->GetAutofillRendererIds(GetMainFrame(feature())->AsWeakPtr(),
                                      {target_coord}, future.GetCallback());

    auto result = future.Get();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code(),
              mojom::ActionResultCode::kFormFillingFieldNotFound);
  }
}

}  // namespace actor
