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

struct GemmTester {
  OperandInfoBlink a;
  OperandInfoBlink b;
  struct GemmOptions {
    absl::optional<OperandInfoBlink> c;
    absl::optional<float> alpha;
    absl::optional<float> beta;
    absl::optional<bool> a_transpose;
    absl::optional<bool> b_transpose;
  };
  GemmOptions options;
  OperandInfoMojo expected_operand;
  struct GemmAttributes {
    absl::optional<OperandInfoMojo> c;
    float alpha;
    float beta;
    bool a_transpose;
    bool b_transpose;
  };
  GemmAttributes expected_attributes;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* a_operand = BuildInput(builder, "a", a.dimensions, a.type,
                                 scope.GetExceptionState());
    auto* b_operand = BuildInput(builder, "b", b.dimensions, b.type,
                                 scope.GetExceptionState());
    MLGemmOptions* ml_gemm_options = MLGemmOptions::Create();
    if (options.c) {
      ml_gemm_options->setC(BuildInput(builder, "c", options.c->dimensions,
                                       options.c->type,
                                       scope.GetExceptionState()));
    }
    if (options.alpha) {
      ml_gemm_options->setAlpha(options.alpha.value());
    }
    if (options.beta) {
      ml_gemm_options->setBeta(options.beta.value());
    }
    if (options.a_transpose) {
      ml_gemm_options->setATranspose(options.a_transpose.value());
    }
    if (options.b_transpose) {
      ml_gemm_options->setBTranspose(options.b_transpose.value());
    }
    auto* output_operand = builder->gemm(a_operand, b_operand, ml_gemm_options,
                                         scope.GetExceptionState());
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_NE(graph, nullptr);

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operators.size(), 1u);
    auto& operation = graph_info->operators[0];
    EXPECT_EQ(operation->kind, blink_mojom::Operator::Kind::kGemm);
    auto& gemm_attributes = operation->attributes->get_gemm();
    ASSERT_EQ(gemm_attributes.is_null(), false);
    if (options.c) {
      auto c_operand_iter = graph_info->id_to_operand_map.find(
          gemm_attributes->c_operand_id.value());
      ASSERT_TRUE(c_operand_iter != graph_info->id_to_operand_map.end());
      EXPECT_EQ(c_operand_iter->value->data_type, expected_attributes.c->type);
      EXPECT_EQ(c_operand_iter->value->dimensions,
                expected_attributes.c->dimensions);
    } else {
      EXPECT_EQ(gemm_attributes->c_operand_id, absl::nullopt);
    }
    EXPECT_EQ(gemm_attributes->alpha, expected_attributes.alpha);
    EXPECT_EQ(gemm_attributes->beta, expected_attributes.beta);
    EXPECT_EQ(gemm_attributes->a_transpose, expected_attributes.a_transpose);
    EXPECT_EQ(gemm_attributes->b_transpose, expected_attributes.b_transpose);
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

TEST_P(MLGraphTestMojo, GemmTest) {
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
    // Test building gemm with default option.
    GemmTester{
        .a = {.type = V8MLOperandType::Enum::kFloat32, .dimensions = {2, 3}},
        .b = {.type = V8MLOperandType::Enum::kFloat32, .dimensions = {3, 4}},
        .expected_operand = {.type = blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {2, 4}},
        .expected_attributes = {.c = absl::nullopt,
                                .alpha = 1.0,
                                .beta = 1.0,
                                .a_transpose = false,
                                .b_transpose = false}}
        .Test(*this, scope, builder);
  }
  {
    // Test building gemm with aTranspose = true.
    // Transposed a_dimensions would be {3, 2} and it's compatible with
    // b_dimensions {2, 4}.
    GemmTester{
        .a = {.type = V8MLOperandType::Enum::kFloat32, .dimensions = {2, 3}},
        .b = {.type = V8MLOperandType::Enum::kFloat32, .dimensions = {2, 4}},
        .options = {.a_transpose = true},
        .expected_operand = {.type = blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {3, 4}},
        .expected_attributes = {.c = absl::nullopt,
                                .alpha = 1.0,
                                .beta = 1.0,
                                .a_transpose = true,
                                .b_transpose = false}}
        .Test(*this, scope, builder);
  }
  {
    // Test building gemm with bTranspose = true.
    // Transposed b_dimensions would be {3, 4} and it's compatible with
    // a_dimensions {2, 3}.
    GemmTester{
        .a = {.type = V8MLOperandType::Enum::kFloat32, .dimensions = {2, 3}},
        .b = {.type = V8MLOperandType::Enum::kFloat32, .dimensions = {4, 3}},
        .options = {.b_transpose = true},
        .expected_operand = {.type = blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {2, 4}},
        .expected_attributes = {.c = absl::nullopt,
                                .alpha = 1.0,
                                .beta = 1.0,
                                .a_transpose = false,
                                .b_transpose = true}}
        .Test(*this, scope, builder);
  }
  {
    // Test building gemm with setting optional input C.
    // The output dimensions of a * b would be {2, 4} and c_dimensions {4} is
    // able to broadcast to {2, 4}.
    GemmTester{
        .a = {.type = V8MLOperandType::Enum::kFloat32, .dimensions = {2, 3}},
        .b = {.type = V8MLOperandType::Enum::kFloat32, .dimensions = {3, 4}},
        .options =
            {
                .c = OperandInfoBlink{.type = V8MLOperandType::Enum::kFloat32,
                                      .dimensions = {4}},
                .alpha = 2.0,
                .beta = 3.0,
            },
        .expected_operand = {.type = blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {2, 4}},
        .expected_attributes =
            {.c = OperandInfoMojo{.type =
                                      blink_mojom::Operand::DataType::kFloat32,
                                  .dimensions = {4}},
             .alpha = 2.0,
             .beta = 3.0,
             .a_transpose = false,
             .b_transpose = false}}
        .Test(*this, scope, builder);
  }
}

struct Pool2dTester {
  OperandInfoBlink input;
  struct Pool2dOptions {
    absl::optional<Vector<uint32_t>> window_dimensions;
    absl::optional<Vector<uint32_t>> padding;
    absl::optional<Vector<uint32_t>> strides;
    absl::optional<Vector<uint32_t>> dilations;
    absl::optional<blink::V8MLAutoPad::Enum> auto_pad;
    absl::optional<blink::V8MLInputOperandLayout::Enum> layout;
    absl::optional<blink::V8MLRoundingType::Enum> rounding_type;
    absl::optional<Vector<uint32_t>> output_sizes;
  };
  struct Pool2dAttributes {
    Vector<uint32_t> window_dimensions;
    Vector<uint32_t> padding = {0, 0, 0, 0};
    Vector<uint32_t> strides = {1, 1};
    Vector<uint32_t> dilations = {1, 1};
    blink_mojom::InputOperandLayout layout =
        blink_mojom::InputOperandLayout::kChannelsFirst;
  };
  Pool2dOptions options;
  OperandInfoMojo expected_operand;
  Pool2dAttributes expected_attributes;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    Test(helper, scope, builder, Pool2dKind::kAverage);
    Test(helper, scope, builder, Pool2dKind::kMax);
  }

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder,
            Pool2dKind kind) {
    // Build the graph.
    auto* input_operand = BuildInput(builder, "input", input.dimensions,
                                     input.type, scope.GetExceptionState());
    MLPool2dOptions* ml_pool2d_options = MLPool2dOptions::Create();
    if (options.window_dimensions) {
      ml_pool2d_options->setWindowDimensions(options.window_dimensions.value());
    }
    if (options.padding) {
      ml_pool2d_options->setPadding(options.padding.value());
    }
    if (options.strides) {
      ml_pool2d_options->setStrides(options.strides.value());
    }
    if (options.dilations) {
      ml_pool2d_options->setDilations(options.dilations.value());
    }
    if (options.auto_pad) {
      ml_pool2d_options->setAutoPad(options.auto_pad.value());
    }
    if (options.layout) {
      ml_pool2d_options->setLayout(options.layout.value());
    }
    if (options.rounding_type) {
      ml_pool2d_options->setRoundingType(options.rounding_type.value());
    }
    if (options.output_sizes) {
      ml_pool2d_options->setOutputSizes(options.output_sizes.value());
    }
    auto* output_operand =
        BuildPool2d(scope, builder, kind, input_operand, ml_pool2d_options);
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_NE(graph, nullptr);

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operators.size(), 1u);
    auto& operation = graph_info->operators[0];
    switch (kind) {
      case Pool2dKind::kAverage:
        EXPECT_EQ(operation->kind, blink_mojom::Operator::Kind::kAveragePool2d);
        break;
      case Pool2dKind::kMax:
        EXPECT_EQ(operation->kind, blink_mojom::Operator::Kind::kMaxPool2d);
        break;
      default:
        NOTREACHED();
    }
    auto& pool2d_attributes = operation->attributes->get_pool2d();
    // Validate window dimensions.
    EXPECT_EQ(pool2d_attributes->window_dimensions->height,
              expected_attributes.window_dimensions[0]);
    EXPECT_EQ(pool2d_attributes->window_dimensions->width,
              expected_attributes.window_dimensions[1]);
    // Validate explicit padding.
    auto& expected_padding = expected_attributes.padding;
    EXPECT_EQ(pool2d_attributes->padding->beginning->height,
              expected_padding[0]);
    EXPECT_EQ(pool2d_attributes->padding->ending->height, expected_padding[1]);
    EXPECT_EQ(pool2d_attributes->padding->beginning->width,
              expected_padding[2]);
    EXPECT_EQ(pool2d_attributes->padding->ending->width, expected_padding[3]);
    // Validate strides
    EXPECT_EQ(pool2d_attributes->strides->height,
              expected_attributes.strides[0]);
    EXPECT_EQ(pool2d_attributes->strides->width,
              expected_attributes.strides[1]);
    // Validate dilations.
    EXPECT_EQ(pool2d_attributes->dilations->height,
              expected_attributes.dilations[0]);
    EXPECT_EQ(pool2d_attributes->dilations->width,
              expected_attributes.dilations[1]);
    EXPECT_EQ(pool2d_attributes->layout, expected_attributes.layout);
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

TEST_P(MLGraphTestMojo, Pool2dTest) {
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
    // Test pool2d with default options.
    Pool2dTester{
        .input = {.type = V8MLOperandType::Enum::kFloat32,
                  .dimensions = {1, 3, 4, 4}},
        .expected_operand = {.type = blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 1, 1}},
        .expected_attributes = {.window_dimensions = {4, 4},
                                .strides = {1, 1},
                                .dilations = {1, 1}}}
        .Test(*this, scope, builder);
  }
  {
    // Test pool2d without padding.
    Pool2dTester{
        .input = {.type = V8MLOperandType::Enum::kFloat32,
                  .dimensions = {1, 3, 4, 4}},
        .options = {.window_dimensions = Vector<uint32_t>({3, 3})},
        .expected_operand = {.type = blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 2, 2}},
        .expected_attributes = {.window_dimensions = {3, 3},
                                .padding = {0, 0, 0, 0},
                                .strides = {1, 1},
                                .dilations = {1, 1}}}
        .Test(*this, scope, builder);
  }
  {
    // Test pool2d with autoPad="same-upper".
    Pool2dTester{
        .input = {.type = V8MLOperandType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .options = {.window_dimensions = Vector<uint32_t>({5, 5}),
                    .auto_pad = V8MLAutoPad::Enum::kSameUpper},
        .expected_operand = {.type = blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 5, 5}},
        .expected_attributes = {.window_dimensions = {5, 5},
                                .padding = {2, 2, 2, 2},
                                .strides = {1, 1},
                                .dilations = {1, 1}}}
        .Test(*this, scope, builder);
  }
  {
    // Test pool2d with autoPad="same-lower".
    Pool2dTester{
        .input = {.type = V8MLOperandType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .options = {.window_dimensions = Vector<uint32_t>({5, 5}),
                    .auto_pad = V8MLAutoPad::Enum::kSameLower},
        .expected_operand = {.type = blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 5, 5}},
        .expected_attributes = {.window_dimensions = {5, 5},
                                .padding = {2, 2, 2, 2},
                                .strides = {1, 1},
                                .dilations = {1, 1}}}
        .Test(*this, scope, builder);
  }
  {
    // Test pool2d with strides=2, padding=1 and roundingType="floor".
    Pool2dTester{
        .input = {.type = V8MLOperandType::Enum::kFloat32,
                  .dimensions = {1, 3, 7, 7}},
        .options = {.window_dimensions = Vector<uint32_t>({4, 4}),
                    .padding = Vector<uint32_t>({1, 1, 1, 1}),
                    .strides = Vector<uint32_t>({2, 2}),
                    .rounding_type = V8MLRoundingType::Enum::kFloor},
        .expected_operand = {.type = blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 3, 3}},
        .expected_attributes = {.window_dimensions = {4, 4},
                                .padding = {1, 1, 1, 1},
                                .strides = {2, 2},
                                .dilations = {1, 1}}}
        .Test(*this, scope, builder);
  }
  {
    // Test pool2d with strides=2, padding=1 and roundingType="ceil".
    Pool2dTester{
        .input = {.type = V8MLOperandType::Enum::kFloat32,
                  .dimensions = {1, 3, 7, 7}},
        .options = {.window_dimensions = Vector<uint32_t>({4, 4}),
                    .padding = Vector<uint32_t>({1, 1, 1, 1}),
                    .strides = Vector<uint32_t>({2, 2}),
                    .rounding_type = V8MLRoundingType::Enum::kCeil},
        .expected_operand = {.type = blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 4, 4}},
        .expected_attributes = {.window_dimensions = {4, 4},
                                .padding = {1, 1, 1, 1},
                                .strides = {2, 2},
                                .dilations = {1, 1}}}
        .Test(*this, scope, builder);
  }
  {
    // Test pool2d with strides=2, padding=1 and outputSizes=[3, 3].
    // When the output sizes are explicitly specified, the
    // options.roundingType is ignored.
    Pool2dTester{
        .input = {.type = V8MLOperandType::Enum::kFloat32,
                  .dimensions = {1, 3, 7, 7}},
        .options = {.window_dimensions = Vector<uint32_t>({4, 4}),
                    .padding = Vector<uint32_t>({1, 1, 1, 1}),
                    .strides = Vector<uint32_t>({2, 2}),
                    .rounding_type = V8MLRoundingType::Enum::kCeil,
                    .output_sizes = Vector<uint32_t>({3, 3})},
        .expected_operand = {.type = blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 3, 3}},
        .expected_attributes = {.window_dimensions = {4, 4},
                                .padding = {1, 1, 1, 1},
                                .strides = {2, 2},
                                .dilations = {1, 1}}}
        .Test(*this, scope, builder);
  }
  {
    // Test pool2d with layout="nhwc".
    Pool2dTester{
        .input = {.type = V8MLOperandType::Enum::kFloat32,
                  .dimensions = {1, 7, 7, 3}},
        .options = {.window_dimensions = Vector<uint32_t>({4, 4}),
                    .padding = Vector<uint32_t>({1, 1, 1, 1}),
                    .strides = Vector<uint32_t>({2, 2}),
                    .layout = V8MLInputOperandLayout::Enum::kNhwc,
                    .rounding_type = V8MLRoundingType::Enum::kCeil,
                    .output_sizes = Vector<uint32_t>({3, 3})},
        .expected_operand = {.type = blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 3, 3}},
        .expected_attributes =
            {.window_dimensions = {4, 4},
             .padding = {1, 1, 1, 1},
             .strides = {2, 2},
             .dilations = {1, 1},
             .layout = blink_mojom::InputOperandLayout::kChannelsLast}}
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
