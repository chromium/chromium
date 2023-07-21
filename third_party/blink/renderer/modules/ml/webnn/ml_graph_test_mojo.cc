// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_service.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ml_context_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_builder_test.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_mojo.h"
#include "third_party/blink/renderer/modules/ml/webnn/ml_graph_test_base.h"

namespace blink {

namespace blink_mojom = webnn::mojom::blink;

class MLGraphTestMojo : public MLGraphTestBase {
 public:
  void SetGraphInfo(blink_mojom::GraphInfoPtr graph_info) {
    graph_info_ = std::move(graph_info);
  }

  blink_mojom::GraphInfoPtr GetGraphInfo() { return std::move(graph_info_); }

 private:
  blink_mojom::GraphInfoPtr graph_info_;
};

class FakeWebNNGraph : public blink_mojom::WebNNGraph {
 public:
  FakeWebNNGraph() = default;
  FakeWebNNGraph(const FakeWebNNGraph&) = delete;
  FakeWebNNGraph(FakeWebNNGraph&&) = delete;
  ~FakeWebNNGraph() override = default;

 private:
  // Override methods from webnn::mojom::WebNNGraph.
  // TODO(crbug.com/1273291): Add build and compute methods.
};

class FakeWebNNContext : public blink_mojom::WebNNContext {
 public:
  explicit FakeWebNNContext(MLGraphTestMojo& helper) : helper_(helper) {}
  FakeWebNNContext(const FakeWebNNContext&) = delete;
  FakeWebNNContext(FakeWebNNContext&&) = delete;
  ~FakeWebNNContext() override = default;

 private:
  // Override methods from webnn::mojom::WebNNContext.
  void CreateGraph(blink_mojom::GraphInfoPtr graph_info,
                   CreateGraphCallback callback) override {
    helper_.SetGraphInfo(std::move(graph_info));

    mojo::PendingRemote<blink_mojom::WebNNGraph> blink_remote;
    // The receiver bind to FakeWebNNGraph.
    mojo::MakeSelfOwnedReceiver<blink_mojom::WebNNGraph>(
        std::make_unique<FakeWebNNGraph>(),
        blink_remote.InitWithNewPipeAndPassReceiver());

    std::move(callback).Run(std::move(blink_remote));
  }
  MLGraphTestMojo& helper_;
};

class FakeWebNNContextProvider : public blink_mojom::WebNNContextProvider {
 public:
  explicit FakeWebNNContextProvider(MLGraphTestMojo& helper)
      : helper_(helper), receiver_(this) {}
  FakeWebNNContextProvider(const FakeWebNNContextProvider&) = delete;
  FakeWebNNContextProvider(FakeWebNNContextProvider&&) = delete;
  ~FakeWebNNContextProvider() override = default;

  void BindRequest(mojo::ScopedMessagePipeHandle handle) {
    DCHECK(!receiver_.is_bound());
    receiver_.Bind(mojo::PendingReceiver<blink_mojom::WebNNContextProvider>(
        std::move(handle)));
    receiver_.set_disconnect_handler(WTF::BindOnce(
        &FakeWebNNContextProvider::OnConnectionError, WTF::Unretained(this)));
  }

  bool IsBound() const { return receiver_.is_bound(); }

  void OnConnectionError() { receiver_.reset(); }

 private:
  // Override methods from webnn::mojom::WebNNContextProvider.
  void CreateWebNNContext(blink_mojom::CreateContextOptionsPtr options,
                          CreateWebNNContextCallback callback) override {
    mojo::PendingRemote<blink_mojom::WebNNContext> blink_remote;
    // The receiver bind to FakeWebNNContext.
    mojo::MakeSelfOwnedReceiver<blink_mojom::WebNNContext>(
        std::make_unique<FakeWebNNContext>(helper_),
        blink_remote.InitWithNewPipeAndPassReceiver());

    std::move(callback).Run(blink_mojom::CreateContextResult::kOk,
                            std::move(blink_remote));
  }

  MLGraphTestMojo& helper_;
  mojo::Receiver<blink_mojom::WebNNContextProvider> receiver_;
};

class ScopedWebNNServiceBinder {
 public:
  explicit ScopedWebNNServiceBinder(MLGraphTestMojo& helper,
                                    V8TestingScope& scope)
      : fake_webnn_context_provider_(
            std::make_unique<FakeWebNNContextProvider>(helper)),
        interface_broker_(
            scope.GetExecutionContext()->GetBrowserInterfaceBroker()) {
    interface_broker_.SetBinderForTesting(
        blink_mojom::WebNNContextProvider::Name_,
        WTF::BindRepeating(
            &FakeWebNNContextProvider::BindRequest,
            WTF::Unretained(fake_webnn_context_provider_.get())));
  }

  ~ScopedWebNNServiceBinder() {
    interface_broker_.SetBinderForTesting(
        blink_mojom::WebNNContextProvider::Name_, base::NullCallback());
  }

  bool IsWebNNContextBound() const {
    return fake_webnn_context_provider_->IsBound();
  }

 private:
  std::unique_ptr<FakeWebNNContextProvider> fake_webnn_context_provider_;
  const BrowserInterfaceBrokerProxy& interface_broker_;
};

MLGraphMojo* ToMLGraphMojo(V8TestingScope* scope, ScriptValue value) {
  return NativeValueTraits<MLGraphMojo>::NativeValue(
      scope->GetIsolate(), value.V8Value(), scope->GetExceptionState());
}

// Build a simple MLGraph asynchronously with only one relu operator.
ScriptPromise BuildSimpleGraph(V8TestingScope& scope,
                               MLContextOptions* context_options) {
  auto* builder =
      CreateMLGraphBuilder(scope.GetExecutionContext(), context_options);
  auto* lhs_operand =
      BuildInput(builder, "lhs", {3, 4, 5}, V8MLOperandType::Enum::kFloat32,
                 scope.GetExceptionState());
  auto* rhs_operand =
      BuildInput(builder, "rhs", {3, 4, 5}, V8MLOperandType::Enum::kFloat32,
                 scope.GetExceptionState());
  auto* output =
      builder->add(lhs_operand, rhs_operand, scope.GetExceptionState());
  EXPECT_NE(output, nullptr);
  return builder->build(scope.GetScriptState(), {{"output", output}},
                        scope.GetExceptionState());
}

struct OperandInfoMojo {
  blink_mojom::Operand::DataType type;
  Vector<uint32_t> dimensions;
};

using OperandInfoBlink = OperandInfo<float>;

TEST_P(MLGraphTestMojo, CreateWebNNGraphTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* script_state = scope.GetScriptState();
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device preference.
  options->setDevicePreference(V8MLDevicePreference::Enum::kGpu);

  {
    // Test disabling WebNN Service by default. The promise should be rejected
    // since the WebNN Service is disabled.
    ScriptPromiseTester tester(script_state, BuildSimpleGraph(scope, options));
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsRejected());
    auto* exception = V8DOMException::ToWrappable(scope.GetIsolate(),
                                                  tester.Value().V8Value());
    EXPECT_NE(exception, nullptr);
    EXPECT_EQ(exception->name(), "NotSupportedError");
    EXPECT_EQ(exception->message(), "Not implemented");
    EXPECT_FALSE(scoped_setup_binder.IsWebNNContextBound());
  }

  {
    // Test enabling WebNN Service in feature list. The promise should be
    // resoveld with an MLGraphMojo object.
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(
        blink::features::kEnableMachineLearningNeuralNetworkService);

    ScriptPromiseTester tester(script_state, BuildSimpleGraph(scope, options));
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsFulfilled());
    auto* mojo_graph = ToMLGraphMojo(&scope, tester.Value());
    EXPECT_NE(mojo_graph, nullptr);
    EXPECT_TRUE(scoped_setup_binder.IsWebNNContextBound());
  }
}

struct ClampTester {
  OperandInfoBlink input;
  struct ClampOptions {
    absl::optional<float> min_value;
    absl::optional<float> max_value;
  };
  ClampOptions options;
  OperandInfoMojo expected_operand;
  ClampOptions expected_attributes;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand = BuildInput(builder, "input", input.dimensions,
                                     input.type, scope.GetExceptionState());
    MLClampOptions* ml_clamp_options = MLClampOptions::Create();
    if (options.min_value) {
      ml_clamp_options->setMinValue(options.min_value.value());
    }
    if (options.max_value) {
      ml_clamp_options->setMaxValue(options.max_value.value());
    }
    auto* output_operand = builder->clamp(input_operand, ml_clamp_options,
                                          scope.GetExceptionState());
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_NE(graph, nullptr);

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operators.size(), 1u);
    auto& operation = graph_info->operators[0];
    EXPECT_EQ(operation->kind, blink_mojom::Operator::Kind::kClamp);
    auto& clamp_attributes = operation->attributes->get_clamp();
    EXPECT_EQ(clamp_attributes->min_value, expected_attributes.min_value);
    EXPECT_EQ(clamp_attributes->max_value, expected_attributes.max_value);
    EXPECT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->data_type, expected_operand.type);
    EXPECT_EQ(output_operand_iter->value->dimensions,
              expected_operand.dimensions);
  }
};

TEST_P(MLGraphTestMojo, ClampTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device preference.
  options->setDevicePreference(V8MLDevicePreference::Enum::kGpu);
  auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext(), options);
  {
    // Test clamp operator with default options that no minimum and maximum
    // values are defined.
    ClampTester{
        .input = {.type = V8MLOperandType::Enum::kFloat32,
                  .dimensions = {1, 2, 2, 1}},
        .expected_operand = {.type = blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 2, 2, 1}},
        .expected_attributes = {.min_value =
                                    -std::numeric_limits<float>::infinity(),
                                .max_value =
                                    +std::numeric_limits<float>::infinity()}}
        .Test(*this, scope, builder);
  }
  {
    // Test clamp operator with the minimum value defined.
    ClampTester{
        .input = {.type = V8MLOperandType::Enum::kFloat16,
                  .dimensions = {2, 4}},
        .options = {0.0, absl::nullopt},
        .expected_operand = {.type = blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {2, 4}},
        .expected_attributes = {.min_value = 0.0,
                                .max_value =
                                    +std::numeric_limits<float>::infinity()}}
        .Test(*this, scope, builder);
  }
  {
    // Test clamp operator with the maximum value defined.
    ClampTester{
        .input = {.type = V8MLOperandType::Enum::kInt32,
                  .dimensions = {3, 1, 6}},
        .options = {absl::nullopt, 6.0},
        .expected_operand = {.type = blink_mojom::Operand::DataType::kInt32,
                             .dimensions = {3, 1, 6}},
        .expected_attributes = {.min_value =
                                    -std::numeric_limits<float>::infinity(),
                                .max_value = 6.0}}
        .Test(*this, scope, builder);
  }
  {
    // Test clamp operator with both the minimum and maximum values defined.
    ClampTester{
        .input = {.type = V8MLOperandType::Enum::kUint8, .dimensions = {7}},
        .options = {0.0, 6.0},
        .expected_operand = {.type = blink_mojom::Operand::DataType::kUint8,
                             .dimensions = {7}},
        .expected_attributes = {.min_value = 0.0, .max_value = 6.0}}
        .Test(*this, scope, builder);
  }
}

struct ElementWiseBinaryTester {
  ElementWiseBinaryKind kind;
  OperandInfoBlink lhs;
  OperandInfoBlink rhs;
  OperandInfoMojo expected;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* lhs_operand = BuildInput(builder, "lhs", lhs.dimensions, lhs.type,
                                   scope.GetExceptionState());
    auto* rhs_operand = BuildInput(builder, "rhs", rhs.dimensions, rhs.type,
                                   scope.GetExceptionState());
    auto* output_operand =
        BuildElementWiseBinary(scope, builder, kind, lhs_operand, rhs_operand);
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_NE(graph, nullptr);

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    EXPECT_EQ(graph_info->id_to_operand_map.size(), 3u);
    EXPECT_EQ(graph_info->input_operands.size(), 2u);
    // Verify the left `mojo::Operand`.
    auto lhs_operand_id = graph_info->input_operands[0];
    auto lhs_operand_iter = graph_info->id_to_operand_map.find(lhs_operand_id);
    ASSERT_TRUE(lhs_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(lhs_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kInput);
    EXPECT_EQ(lhs_operand_iter->value->data_type, expected.type);
    EXPECT_EQ(lhs_operand_iter->value->dimensions, lhs.dimensions);
    EXPECT_EQ(lhs_operand_iter->value->name, "lhs");
    // Verify the right `mojo::Operand`.
    auto rhs_operand_id = graph_info->input_operands[1];
    auto rhs_operand_iter = graph_info->id_to_operand_map.find(rhs_operand_id);
    ASSERT_TRUE(rhs_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(rhs_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kInput);
    EXPECT_EQ(rhs_operand_iter->value->data_type, expected.type);
    EXPECT_EQ(rhs_operand_iter->value->dimensions, rhs.dimensions);
    EXPECT_EQ(rhs_operand_iter->value->name, "rhs");
    // Verify the output `mojo::Operand`.
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kOutput);
    EXPECT_EQ(output_operand_iter->value->data_type, expected.type);
    EXPECT_EQ(output_operand_iter->value->dimensions, expected.dimensions);
    EXPECT_EQ(output_operand_iter->value->name, "output");
    // Verify the `mojo::Operator`.
    ASSERT_EQ(graph_info->operators.size(), 1u);
    auto& operation = graph_info->operators[0];
    ASSERT_EQ(operation->input_operands.size(), 2u);
    EXPECT_EQ(operation->input_operands[0], lhs_operand_id);
    EXPECT_EQ(operation->input_operands[1], rhs_operand_id);
    ASSERT_EQ(operation->output_operands.size(), 1u);
    EXPECT_EQ(operation->output_operands[0], output_operand_id);
  }
};

TEST_P(MLGraphTestMojo, ElementWiseBinaryTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device preference.
  options->setDevicePreference(V8MLDevicePreference::Enum::kGpu);
  auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext(), options);
  {
    // Test element-wise add operator for two 1-D tensors.
    ElementWiseBinaryTester{
        .kind = ElementWiseBinaryKind::kAdd,
        .lhs = {.type = V8MLOperandType::Enum::kFloat32, .dimensions = {2}},
        .rhs = {.type = V8MLOperandType::Enum::kFloat32, .dimensions = {2}},
        .expected = {.type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {2}}}
        .Test(*this, scope, builder);
  }
  {
    // Test element-wise add operator for two 2-D tensors.
    ElementWiseBinaryTester{
        .kind = ElementWiseBinaryKind::kAdd,
        .lhs = {.type = V8MLOperandType::Enum::kFloat16, .dimensions = {3, 7}},
        .rhs = {.type = V8MLOperandType::Enum::kFloat16, .dimensions = {3, 7}},
        .expected = {.type = blink_mojom::Operand::DataType::kFloat16,
                     .dimensions = {3, 7}}}
        .Test(*this, scope, builder);
  }
  {
    // Test element-wise add operator for broadcasting to 2-D tensor.
    ElementWiseBinaryTester{
        .kind = ElementWiseBinaryKind::kAdd,
        .lhs = {.type = V8MLOperandType::Enum::kInt32, .dimensions = {5, 3}},
        .rhs = {.type = V8MLOperandType::Enum::kInt32, .dimensions = {5, 1}},
        .expected = {.type = blink_mojom::Operand::DataType::kInt32,
                     .dimensions = {5, 3}}}
        .Test(*this, scope, builder);
  }
  {
    // Test element-wise add operator for broadcasting to 3-D tensor.
    ElementWiseBinaryTester{
        .kind = ElementWiseBinaryKind::kAdd,
        .lhs = {.type = V8MLOperandType::Enum::kInt8, .dimensions = {4, 2, 1}},
        .rhs = {.type = V8MLOperandType::Enum::kInt8, .dimensions = {4}},
        .expected = {.type = blink_mojom::Operand::DataType::kInt8,
                     .dimensions = {4, 2, 4}}}
        .Test(*this, scope, builder);
  }
  {
    // Test element-wise add operator for broadcasting to 4-D tensors.
    ElementWiseBinaryTester{
        .kind = ElementWiseBinaryKind::kAdd,
        .lhs = {.type = V8MLOperandType::Enum::kUint8,
                .dimensions = {8, 1, 6, 1}},
        .rhs = {.type = V8MLOperandType::Enum::kUint8, .dimensions = {7, 1, 5}},
        .expected = {.type = blink_mojom::Operand::DataType::kUint8,
                     .dimensions = {8, 7, 6, 5}}}
        .Test(*this, scope, builder);
  }
}

struct ReluTester {
  OperandInfoBlink input;
  OperandInfoMojo expected;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand = BuildInput(builder, "input", input.dimensions,
                                     input.type, scope.GetExceptionState());
    auto* output_operand =
        builder->relu(input_operand, scope.GetExceptionState());
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_NE(graph, nullptr);

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    EXPECT_EQ(graph_info->id_to_operand_map.size(), 2u);
    EXPECT_EQ(graph_info->input_operands.size(), 1u);
    // Verify the input `mojo::Operand`.
    auto input_operand_id = graph_info->input_operands[0];
    auto input_operand_iter =
        graph_info->id_to_operand_map.find(input_operand_id);
    ASSERT_TRUE(input_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(input_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kInput);
    EXPECT_EQ(input_operand_iter->value->data_type, expected.type);
    EXPECT_EQ(input_operand_iter->value->dimensions, input.dimensions);
    EXPECT_EQ(input_operand_iter->value->name, "input");
    // Verify the output `mojo::Operand`.
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kOutput);
    EXPECT_EQ(output_operand_iter->value->data_type, expected.type);
    EXPECT_EQ(output_operand_iter->value->dimensions, expected.dimensions);
    EXPECT_EQ(output_operand_iter->value->name, "output");
    // Verify the `mojo::Operator`.
    ASSERT_EQ(graph_info->operators.size(), 1u);
    auto& operation = graph_info->operators[0];
    EXPECT_EQ(operation->kind, blink_mojom::Operator::Kind::kRelu);
    ASSERT_EQ(operation->input_operands.size(), 1u);
    EXPECT_EQ(operation->input_operands[0], input_operand_id);
    ASSERT_EQ(operation->output_operands.size(), 1u);
    EXPECT_EQ(operation->output_operands[0], output_operand_id);
  }
};

TEST_P(MLGraphTestMojo, ReluTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device preference.
  options->setDevicePreference(V8MLDevicePreference::Enum::kGpu);
  auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext(), options);
  {
    // Test relu operator for 1-D tensor.
    ReluTester{
        .input = {.type = V8MLOperandType::Enum::kFloat32, .dimensions = {2}},
        .expected = {.type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {2}}}
        .Test(*this, scope, builder);
  }
  {
    // Test relu operator for 2-D tensor.
    ReluTester{.input = {.type = V8MLOperandType::Enum::kFloat16,
                         .dimensions = {3, 7}},
               .expected = {.type = blink_mojom::Operand::DataType::kFloat16,
                            .dimensions = {3, 7}}}
        .Test(*this, scope, builder);
  }
  {
    // Test relu operator for 3-D tensor.
    ReluTester{.input = {.type = V8MLOperandType::Enum::kInt32,
                         .dimensions = {1, 5, 3}},
               .expected = {.type = blink_mojom::Operand::DataType::kInt32,
                            .dimensions = {1, 5, 3}}}
        .Test(*this, scope, builder);
  }
  {
    // Test relu operator for 4-D tensor.
    ReluTester{.input = {.type = V8MLOperandType::Enum::kUint8,
                         .dimensions = {1, 2, 2, 1}},
               .expected = {.type = blink_mojom::Operand::DataType::kUint8,
                            .dimensions = {1, 2, 2, 1}}}
        .Test(*this, scope, builder);
  }
}

struct ReshapeTester {
  OperandInfoBlink input;
  Vector<absl::optional<uint32_t>> new_shape;
  OperandInfoMojo expected;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand = BuildInput(builder, "input", input.dimensions,
                                     input.type, scope.GetExceptionState());
    auto* output_operand =
        builder->reshape(input_operand, new_shape, scope.GetExceptionState());
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_NE(graph, nullptr);

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operators.size(), 1u);
    auto& operation = graph_info->operators[0];
    EXPECT_EQ(operation->kind, blink_mojom::Operator::Kind::kReshape);
    EXPECT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->data_type, expected.type);
    EXPECT_EQ(output_operand_iter->value->dimensions, expected.dimensions);
  }
};

TEST_P(MLGraphTestMojo, ReshapeTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device preference.
  options->setDevicePreference(V8MLDevicePreference::Enum::kGpu);
  auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext(), options);
  {
    // Test reshaping 2-D tensor to 1-D tensor.
    ReshapeTester{.input = {.type = V8MLOperandType::Enum::kFloat32,
                            .dimensions = {2, 2}},
                  .new_shape = {4},
                  .expected = {.type = blink_mojom::Operand::DataType::kFloat32,
                               .dimensions = {4}}}
        .Test(*this, scope, builder);
  }
  {
    // Test reshaping from 2-D tensor to 1-D tensor with calculated dimension.
    ReshapeTester{.input = {.type = V8MLOperandType::Enum::kFloat16,
                            .dimensions = {2, 2}},
                  .new_shape = {absl::nullopt},
                  .expected = {.type = blink_mojom::Operand::DataType::kFloat16,
                               .dimensions = {4}}}
        .Test(*this, scope, builder);
  }
  {
    // Test reshaping from 4-D tensor to 2-D tensor.
    ReshapeTester{.input = {.type = V8MLOperandType::Enum::kInt32,
                            .dimensions = {1, 2, 2, 1}},
                  .new_shape = {1, 4},
                  .expected = {.type = blink_mojom::Operand::DataType::kInt32,
                               .dimensions = {1, 4}}}
        .Test(*this, scope, builder);
  }
  {
    // Test reshaping from 4-D tensor to 2-D tensor with calculated dimension.
    ReshapeTester{.input = {.type = V8MLOperandType::Enum::kUint8,
                            .dimensions = {1, 2, 2, 1}},
                  .new_shape = {1, absl::nullopt},
                  .expected = {.type = blink_mojom::Operand::DataType::kUint8,
                               .dimensions = {1, 4}}}
        .Test(*this, scope, builder);
  }
}

struct SoftmaxTester {
  OperandInfoBlink input;
  OperandInfoMojo expected;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand = BuildInput(builder, "input", input.dimensions,
                                     input.type, scope.GetExceptionState());
    auto* output_operand =
        builder->softmax(input_operand, scope.GetExceptionState());
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_NE(graph, nullptr);

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operators.size(), 1u);
    auto& operation = graph_info->operators[0];
    EXPECT_EQ(operation->kind, blink_mojom::Operator::Kind::kSoftmax);
    EXPECT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->data_type, expected.type);
    EXPECT_EQ(output_operand_iter->value->dimensions, expected.dimensions);
  }
};

TEST_P(MLGraphTestMojo, SoftmaxTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device preference.
  options->setDevicePreference(V8MLDevicePreference::Enum::kGpu);
  auto* builder = CreateMLGraphBuilder(scope.GetExecutionContext(), options);
  {
    // Test building softmax with float32 input.
    SoftmaxTester{.input = {.type = V8MLOperandType::Enum::kFloat32,
                            .dimensions = {2, 4}},
                  .expected = {.type = blink_mojom::Operand::DataType::kFloat32,
                               .dimensions = {2, 4}}}
        .Test(*this, scope, builder);
  }
  {
    // Test building softmax with float16 input.
    SoftmaxTester{.input = {.type = V8MLOperandType::Enum::kFloat16,
                            .dimensions = {1, 5}},
                  .expected = {.type = blink_mojom::Operand::DataType::kFloat16,
                               .dimensions = {1, 5}}}
        .Test(*this, scope, builder);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MLGraphTestMojo,
    testing::Combine(::testing::Values(BackendType::kWebNNService),
                     ::testing::Values(ExecutionMode::kAsync)),
    TestVarietyToString);

}  // namespace blink
