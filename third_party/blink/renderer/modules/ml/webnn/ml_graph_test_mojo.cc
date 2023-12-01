// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ref.h"
#include "base/test/scoped_feature_list.h"
#include "components/ml/webnn/features.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom-blink.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/features.h"
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

// Helper struct to create faked mojom result of inference.
struct ComputeResult {
  blink_mojom::ComputeResult result;
  WTF::HashMap<WTF::String, WTF::Vector<uint8_t>> output;
};

class MLGraphTestMojo : public MLGraphTestBase {
 public:
  void SetGraphInfo(blink_mojom::GraphInfoPtr graph_info) {
    graph_info_ = std::move(graph_info);
  }

  blink_mojom::GraphInfoPtr GetGraphInfo() { return std::move(graph_info_); }

  void SetComputeResult(const ComputeResult& compute_result) {
    compute_result_ = std::move(compute_result);
  }

  const ComputeResult& GetComputeResult() const { return compute_result_; }

  void SetInputArrayBuffers(HashMap<String, mojo_base::BigBuffer> buffers) {
    input_array_buffers_ = std::move(buffers);
  }

  const HashMap<String, mojo_base::BigBuffer>& GetInputArrayBuffers() const {
    return input_array_buffers_;
  }

 private:
  blink_mojom::GraphInfoPtr graph_info_;
  HashMap<String, mojo_base::BigBuffer> input_array_buffers_;
  ComputeResult compute_result_;
};

class FakeWebNNGraph : public blink_mojom::WebNNGraph {
 public:
  explicit FakeWebNNGraph(MLGraphTestMojo& helper) : helper_(helper) {}
  FakeWebNNGraph(const FakeWebNNGraph&) = delete;
  FakeWebNNGraph(FakeWebNNGraph&&) = delete;
  ~FakeWebNNGraph() override = default;

 private:
  void Compute(HashMap<String, mojo_base::BigBuffer> inputs,
               blink_mojom::WebNNGraph::ComputeCallback callback) override {
    // Set the input array buffers for validation in the test.
    helper_->SetInputArrayBuffers(std::move(inputs));

    // Return the compute result with shared memory.
    auto& compute_result = helper_->GetComputeResult();
    HashMap<String, mojo_base::BigBuffer> mojo_outputs;
    for (const auto& [name, output_data] : compute_result.output) {
      mojo_outputs.insert(
          name, base::make_span(output_data.data(), output_data.size()));
    }
    std::move(callback).Run(compute_result.result, std::move(mojo_outputs));
  }

  const raw_ref<MLGraphTestMojo, DanglingUntriaged> helper_;
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
    helper_->SetGraphInfo(std::move(graph_info));

    mojo::PendingRemote<blink_mojom::WebNNGraph> blink_remote;
    // The receiver bind to FakeWebNNGraph.
    mojo::MakeSelfOwnedReceiver<blink_mojom::WebNNGraph>(
        std::make_unique<FakeWebNNGraph>(*helper_),
        blink_remote.InitWithNewPipeAndPassReceiver());

    std::move(callback).Run(blink_mojom::CreateGraphResult::NewGraphRemote(
        std::move(blink_remote)));
  }
  const raw_ref<MLGraphTestMojo, DanglingUntriaged> helper_;
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
        std::make_unique<FakeWebNNContext>(*helper_),
        blink_remote.InitWithNewPipeAndPassReceiver());

    std::move(callback).Run(blink_mojom::CreateContextResult::NewContextRemote(
        std::move(blink_remote)));
  }

  const raw_ref<MLGraphTestMojo, ExperimentalRenderer> helper_;
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
    interface_broker_->SetBinderForTesting(
        blink_mojom::WebNNContextProvider::Name_,
        WTF::BindRepeating(
            &FakeWebNNContextProvider::BindRequest,
            WTF::Unretained(fake_webnn_context_provider_.get())));
  }

  ~ScopedWebNNServiceBinder() {
    interface_broker_->SetBinderForTesting(
        blink_mojom::WebNNContextProvider::Name_, base::NullCallback());
  }

  bool IsWebNNContextBound() const {
    return fake_webnn_context_provider_->IsBound();
  }

 private:
  std::unique_ptr<FakeWebNNContextProvider> fake_webnn_context_provider_;
  const raw_ref<const BrowserInterfaceBrokerProxy, ExperimentalRenderer>
      interface_broker_;
};

MLGraphMojo* ToMLGraphMojo(V8TestingScope* scope, ScriptValue value) {
  return NativeValueTraits<MLGraphMojo>::NativeValue(
      scope->GetIsolate(), value.V8Value(), scope->GetExceptionState());
}

// Build a simple MLGraph asynchronously with only one relu operator.
ScriptPromise BuildSimpleGraph(V8TestingScope& scope,
                               MLContextOptions* context_options) {
  auto* builder = MLGraphTestBase::CreateGraphBuilder(scope, context_options);
  if (builder == nullptr) {
    return ScriptPromise::RejectWithDOMException(
        scope.GetScriptState(),
        DOMException::Create(
            "Unable to create graph builder.",
            DOMException::GetErrorName(DOMExceptionCode::kOperationError)));
  }

  auto* lhs_operand =
      BuildInput(builder, "lhs", {3, 4, 5}, V8MLOperandDataType::Enum::kFloat32,
                 scope.GetExceptionState());
  auto* rhs_operand =
      BuildInput(builder, "rhs", {3, 4, 5}, V8MLOperandDataType::Enum::kFloat32,
                 scope.GetExceptionState());
  auto* output =
      builder->add(lhs_operand, rhs_operand, scope.GetExceptionState());
  EXPECT_NE(output, nullptr);
  return builder->build(scope.GetScriptState(), {{"output", output}},
                        scope.GetExceptionState());
}

struct OperandInfoMojo {
  blink_mojom::Operand::DataType data_type;
  Vector<uint32_t> dimensions;
};

using OperandInfoBlink = OperandInfo<float>;

TEST_P(MLGraphTestMojo, CreateWebNNGraphTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);

  auto* script_state = scope.GetScriptState();
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);

  {
    // Test disabling WebNN Service by default. The promise should be rejected
    // since the WebNN Service is disabled.
    ScriptPromiseTester tester(scope.GetScriptState(),
                               CreateContext(scope, options));
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
        webnn::features::kEnableMachineLearningNeuralNetworkService);

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
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
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
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_EQ(operation->is_clamp(), true);
    auto& clamp = operation->get_clamp();
    EXPECT_EQ(clamp->min_value, expected_attributes.min_value);
    EXPECT_EQ(clamp->max_value, expected_attributes.max_value);
    EXPECT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->data_type,
              expected_operand.data_type);
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
      webnn::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_NE(builder, nullptr);
  {
    // Test clamp operator with default options that no minimum and maximum
    // values are defined.
    ClampTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 2, 1}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
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
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {2, 4}},
        .options = {0.0, absl::nullopt},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {2, 4}},
        .expected_attributes = {.min_value = 0.0,
                                .max_value =
                                    +std::numeric_limits<float>::infinity()}}
        .Test(*this, scope, builder);
  }
  {
    // Test clamp operator with the maximum value defined.
    ClampTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kInt32,
                  .dimensions = {3, 1, 6}},
        .options = {absl::nullopt, 6.0},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kInt32,
                             .dimensions = {3, 1, 6}},
        .expected_attributes = {.min_value =
                                    -std::numeric_limits<float>::infinity(),
                                .max_value = 6.0}}
        .Test(*this, scope, builder);
  }
  {
    // Test clamp operator with both the minimum and maximum values defined.
    ClampTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint8,
                          .dimensions = {7}},
                .options = {0.0, 6.0},
                .expected_operand = {.data_type =
                                         blink_mojom::Operand::DataType::kUint8,
                                     .dimensions = {7}},
                .expected_attributes = {.min_value = 0.0, .max_value = 6.0}}
        .Test(*this, scope, builder);
  }
  {
    // Test clamp operator with scalar.
    ClampTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint8,
                          .dimensions = {}},
                .options = {0.0, 6.0},
                .expected_operand = {.data_type =
                                         blink_mojom::Operand::DataType::kUint8,
                                     .dimensions = {}},
                .expected_attributes = {.min_value = 0.0, .max_value = 6.0}}
        .Test(*this, scope, builder);
  }
}

struct ConcatTester {
  Vector<OperandInfoBlink> inputs;
  uint32_t axis;
  OperandInfoMojo expected_operand;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    HeapVector<Member<MLOperand>> input_operands;
    input_operands.reserve(inputs.size());
    for (wtf_size_t i = 0; i < inputs.size(); ++i) {
      input_operands.push_back(BuildInput(
          builder, String::Format("input%u", i), inputs[i].dimensions,
          inputs[i].data_type, scope.GetExceptionState()));
    }
    auto* output_operand =
        builder->concat(input_operands, axis, scope.GetExceptionState());
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_NE(graph, nullptr);

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    ASSERT_TRUE(operation->is_concat());
    auto& concat = operation->get_concat();

    EXPECT_EQ(concat->axis, axis);

    // Validate the input operands.
    ASSERT_EQ(graph_info->input_operands.size(), inputs.size());
    for (wtf_size_t i = 0; i < input_operands.size(); ++i) {
      auto input_operand_id = graph_info->input_operands[i];
      auto input_operand_iter =
          graph_info->id_to_operand_map.find(input_operand_id);
      ASSERT_TRUE(input_operand_iter != graph_info->id_to_operand_map.end());
      EXPECT_EQ(input_operand_iter->value->data_type,
                expected_operand.data_type);
      EXPECT_EQ(input_operand_iter->value->dimensions, inputs[i].dimensions);
    }

    // Validate the output operand.
    ASSERT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->data_type,
              expected_operand.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions,
              expected_operand.dimensions);
  }
};

TEST_P(MLGraphTestMojo, ConcatTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      webnn::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  {
    // Test concat operator with one input.
    ConcatTester{
        .inputs = {{.data_type = V8MLOperandDataType::Enum::kFloat32,
                    .dimensions = {3, 1, 5, 6}}},
        .axis = 2,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {3, 1, 5, 6}}}
        .Test(*this, scope, builder);
  }
  {
    // Test concat operator with two inputs.
    ConcatTester{
        .inputs = {{.data_type = V8MLOperandDataType::Enum::kFloat16,
                    .dimensions = {3, 1, 5, 6}},
                   {.data_type = V8MLOperandDataType::Enum::kFloat16,
                    .dimensions = {3, 2, 5, 6}}},
        .axis = 1,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {3, 3, 5, 6}}}
        .Test(*this, scope, builder);
  }
  {
    // Test concat operator with three inputs.
    ConcatTester{
        .inputs = {{.data_type = V8MLOperandDataType::Enum::kInt32,
                    .dimensions = {3, 4, 1, 5}},
                   {.data_type = V8MLOperandDataType::Enum::kInt32,
                    .dimensions = {3, 4, 2, 5}},
                   {.data_type = V8MLOperandDataType::Enum::kInt32,
                    .dimensions = {3, 4, 3, 5}}},
        .axis = 2,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kInt32,
                             .dimensions = {3, 4, 6, 5}}}
        .Test(*this, scope, builder);
  }
  {
    // Test concat operator with two 1-D inputs.
    ConcatTester{
        .inputs = {{.data_type = V8MLOperandDataType::Enum::kInt8,
                    .dimensions = {1}},
                   {.data_type = V8MLOperandDataType::Enum::kInt8,
                    .dimensions = {1}}},
        .axis = 0,
        .expected_operand = {.data_type = blink_mojom::Operand::DataType::kInt8,
                             .dimensions = {2}}}
        .Test(*this, scope, builder);
  }
}

struct Activation {
  MLOperator::OperatorKind kind;
  absl::optional<ClampTester::ClampOptions> clamp_options;
  absl::optional<float> elu_alpha;
  absl::optional<float> leaky_relu_alpha;
};

MLActivation* CreateActivation(V8TestingScope& scope,
                               MLGraphBuilder* builder,
                               const Activation& activation) {
  switch (activation.kind) {
    case MLOperator::OperatorKind::kClamp: {
      auto* clamp_options = MLClampOptions::Create();
      CHECK(clamp_options);
      clamp_options->setMinValue(activation.clamp_options->min_value.value());
      clamp_options->setMaxValue(activation.clamp_options->max_value.value());
      return builder->clamp(clamp_options, scope.GetExceptionState());
    }
    case MLOperator::OperatorKind::kElu: {
      auto* elu_options = MLEluOptions::Create();
      CHECK(elu_options);
      if (activation.elu_alpha.has_value()) {
        elu_options->setAlpha(activation.elu_alpha.value());
      }
      return builder->elu(elu_options, scope.GetExceptionState());
    }
    case MLOperator::OperatorKind::kLeakyRelu: {
      auto* leaky_relu_options = MLLeakyReluOptions::Create();
      CHECK(leaky_relu_options);
      if (activation.leaky_relu_alpha.has_value()) {
        leaky_relu_options->setAlpha(activation.leaky_relu_alpha.value());
      }
      return builder->leakyRelu(leaky_relu_options, scope.GetExceptionState());
    }
    case MLOperator::OperatorKind::kRelu:
      return builder->relu(scope.GetExceptionState());
    case MLOperator::OperatorKind::kSigmoid:
      return builder->sigmoid(scope.GetExceptionState());
    case MLOperator::OperatorKind::kSoftmax:
      return builder->softmax(scope.GetExceptionState());
    case MLOperator::OperatorKind::kTanh:
      return builder->tanh(scope.GetExceptionState());
    default:
      NOTREACHED_NORETURN();
  }
}

void CheckActivation(const webnn::mojom::blink::ActivationPtr& mojom_activation,
                     const Activation& expected_activation) {
  switch (expected_activation.kind) {
    case MLOperator::OperatorKind::kClamp: {
      ASSERT_TRUE(mojom_activation->is_clamp());
      auto& clamp = mojom_activation->get_clamp();
      CHECK(clamp);
      auto& clamp_options = expected_activation.clamp_options;
      CHECK(clamp_options);
      EXPECT_EQ(clamp->min_value, clamp_options->min_value);
      EXPECT_EQ(clamp->max_value, clamp_options->max_value);
      break;
    }
    case MLOperator::OperatorKind::kElu: {
      ASSERT_TRUE(mojom_activation->is_elu());
      auto& elu = mojom_activation->get_elu();
      CHECK(elu);
      CHECK(expected_activation.elu_alpha.has_value());
      EXPECT_EQ(elu->alpha, expected_activation.elu_alpha.value());
      break;
    }
    case MLOperator::OperatorKind::kLeakyRelu: {
      ASSERT_TRUE(mojom_activation->is_leaky_relu());
      auto& leaky_relu = mojom_activation->get_leaky_relu();
      CHECK(leaky_relu);
      CHECK(expected_activation.leaky_relu_alpha.has_value());
      EXPECT_EQ(leaky_relu->alpha,
                expected_activation.leaky_relu_alpha.value());
      break;
    }
    case MLOperator::OperatorKind::kRelu:
      EXPECT_TRUE(mojom_activation->is_relu());
      break;
    case MLOperator::OperatorKind::kSigmoid:
      EXPECT_TRUE(mojom_activation->is_sigmoid());
      break;
    case MLOperator::OperatorKind::kSoftmax:
      EXPECT_TRUE(mojom_activation->is_softmax());
      break;
    case MLOperator::OperatorKind::kTanh:
      EXPECT_TRUE(mojom_activation->is_tanh());
      break;
    default:
      NOTREACHED_NORETURN();
  }
}

struct BatchNormalizationTester {
  OperandInfoBlink input;
  OperandInfoBlink mean;
  OperandInfoBlink variance;
  struct BatchNormalizationOptions {
    absl::optional<OperandInfoBlink> scale;
    absl::optional<OperandInfoBlink> bias;
    absl::optional<uint32_t> axis;
    absl::optional<float> epsilon;
    absl::optional<Activation> activation;
  };
  struct BatchNormalizationAttributes {
    absl::optional<OperandInfoMojo> scale;
    absl::optional<OperandInfoMojo> bias;
    uint32_t axis = 1;
    float epsilon = 1e-5;
    absl::optional<Activation> activation;
  };
  BatchNormalizationOptions options;
  OperandInfoMojo expected_operand;
  BatchNormalizationAttributes expected_attributes;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* mean_operand = BuildInput(builder, "mean", mean.dimensions,
                                    mean.data_type, scope.GetExceptionState());
    auto* variance_operand =
        BuildInput(builder, "variance", variance.dimensions, variance.data_type,
                   scope.GetExceptionState());
    MLBatchNormalizationOptions* batch_normalization_options =
        MLBatchNormalizationOptions::Create();
    if (options.scale) {
      batch_normalization_options->setScale(
          BuildInput(builder, "scale", options.scale->dimensions,
                     options.scale->data_type, scope.GetExceptionState()));
    }
    if (options.bias) {
      batch_normalization_options->setBias(
          BuildInput(builder, "bias", options.bias->dimensions,
                     options.bias->data_type, scope.GetExceptionState()));
    }
    if (options.axis) {
      batch_normalization_options->setAxis(options.axis.value());
    }
    if (options.epsilon) {
      batch_normalization_options->setEpsilon(options.epsilon.value());
    }
    if (options.activation) {
      auto* activation =
          CreateActivation(scope, builder, options.activation.value());
      CHECK(activation);
      batch_normalization_options->setActivation(activation);
    }

    auto* output_operand = builder->batchNormalization(
        input_operand, mean_operand, variance_operand,
        batch_normalization_options, scope.GetExceptionState());
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_NE(graph, nullptr);

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_EQ(operation->is_batch_normalization(), true);
    auto& batch_normalization = operation->get_batch_normalization();
    EXPECT_EQ(batch_normalization->axis, expected_attributes.axis);
    EXPECT_FLOAT_EQ(batch_normalization->epsilon, expected_attributes.epsilon);
    if (options.scale) {
      auto scale_operand_iter = graph_info->id_to_operand_map.find(
          batch_normalization->scale_operand_id.value());
      ASSERT_TRUE(scale_operand_iter != graph_info->id_to_operand_map.end());
      EXPECT_EQ(scale_operand_iter->value->data_type,
                expected_attributes.scale->data_type);
      EXPECT_EQ(scale_operand_iter->value->dimensions,
                expected_attributes.scale->dimensions);
    }
    if (options.bias) {
      auto bias_operand_iter = graph_info->id_to_operand_map.find(
          batch_normalization->bias_operand_id.value());
      ASSERT_TRUE(bias_operand_iter != graph_info->id_to_operand_map.end());
      EXPECT_EQ(bias_operand_iter->value->data_type,
                expected_attributes.bias->data_type);
      EXPECT_EQ(bias_operand_iter->value->dimensions,
                expected_attributes.bias->dimensions);
    }
    if (options.activation) {
      CHECK(expected_attributes.activation);
      CheckActivation(batch_normalization->activation,
                      expected_attributes.activation.value());
    }
    EXPECT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->data_type,
              expected_operand.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions,
              expected_operand.dimensions);
  }
};

TEST_P(MLGraphTestMojo, BatchNormalizationTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      webnn::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_NE(builder, nullptr);
  {
    // Test batchNormalization with default options.
    BatchNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .mean = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                 .dimensions = {3}},
        .variance = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {3}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 5, 5}},
        .expected_attributes = {.scale = absl::nullopt,
                                .bias = absl::nullopt,
                                .axis = 1,
                                .epsilon = 1e-5,
                                .activation = absl::nullopt}}
        .Test(*this, scope, builder);
  }
  {
    // Test batchnormalization with axis = 3.
    BatchNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 4, 5}},
        .mean = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                 .dimensions = {5}},
        .variance = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {5}},
        .options = {.axis = 3},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 4, 5}},
        .expected_attributes = {.scale = absl::nullopt,
                                .bias = absl::nullopt,
                                .axis = 3,
                                .epsilon = 1e-5,
                                .activation = absl::nullopt}}
        .Test(*this, scope, builder);
  }
  {
    // Test batchnormalization with epsilon = 0.01.
    BatchNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 4, 5}},
        .mean = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                 .dimensions = {3}},
        .variance = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {3}},
        .options = {.epsilon = 0.01},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 4, 5}},
        .expected_attributes = {.scale = absl::nullopt,
                                .bias = absl::nullopt,
                                .axis = 1,
                                .epsilon = 0.01,
                                .activation = absl::nullopt}}
        .Test(*this, scope, builder);
  }
  {
    // Test batchnormalization with scale and bias.
    BatchNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .mean = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                 .dimensions = {3}},
        .variance = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {3}},
        .options = {.scale =
                        OperandInfoBlink{
                            .data_type = V8MLOperandDataType::Enum::kFloat32,
                            .dimensions = {3}},
                    .bias =
                        OperandInfoBlink{
                            .data_type = V8MLOperandDataType::Enum::kFloat32,
                            .dimensions = {3}}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 5, 5}},
        .expected_attributes =
            {.scale =
                 OperandInfoMojo{
                     .data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {3}},
             .bias =
                 OperandInfoMojo{
                     .data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {3}},
             .axis = 1,
             .epsilon = 1e-5,
             .activation = absl::nullopt}}
        .Test(*this, scope, builder);
  }
  {
    // Test batchNormalization with clamp activation.
    BatchNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .mean = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                 .dimensions = {3}},
        .variance = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {3}},
        .options = {.activation =
                        Activation{
                            .kind = MLOperator::OperatorKind::kClamp,
                            .clamp_options =
                                ClampTester::ClampOptions{.min_value = 1.0,
                                                          .max_value = 6.0}}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 5, 5}},
        .expected_attributes =
            {.scale = absl::nullopt,
             .bias = absl::nullopt,
             .axis = 1,
             .epsilon = 1e-5,
             .activation =
                 Activation{.kind = MLOperator::OperatorKind::kClamp,
                            .clamp_options =
                                ClampTester::ClampOptions{.min_value = 1.0,
                                                          .max_value = 6.0}}}}
        .Test(*this, scope, builder);
  }
  {
    // Test batchNormalization with elu activation with default options.
    BatchNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .mean = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                 .dimensions = {3}},
        .variance = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {3}},
        .options = {.activation =
                        Activation{.kind = MLOperator::OperatorKind::kElu}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 5, 5}},
        .expected_attributes = {.scale = absl::nullopt,
                                .bias = absl::nullopt,
                                .axis = 1,
                                .epsilon = 1e-5,
                                .activation =
                                    Activation{
                                        .kind = MLOperator::OperatorKind::kElu,
                                        .elu_alpha = 1.0}}}
        .Test(*this, scope, builder);
  }
  {
    // Test batchNormalization with elu activation with given options.
    BatchNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .mean = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                 .dimensions = {3}},
        .variance = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {3}},
        .options = {.activation =
                        Activation{.kind = MLOperator::OperatorKind::kElu,
                                   .elu_alpha = 0.5}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 5, 5}},
        .expected_attributes = {.scale = absl::nullopt,
                                .bias = absl::nullopt,
                                .axis = 1,
                                .epsilon = 1e-5,
                                .activation =
                                    Activation{
                                        .kind = MLOperator::OperatorKind::kElu,
                                        .elu_alpha = 0.5}}}
        .Test(*this, scope, builder);
  }
  {
    // Test batchNormalization with leaky relu activation with default options.
    BatchNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .mean = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                 .dimensions = {3}},
        .variance = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {3}},
        .options = {.activation =
                        Activation{.kind =
                                       MLOperator::OperatorKind::kLeakyRelu}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 5, 5}},
        .expected_attributes =
            {.scale = absl::nullopt,
             .bias = absl::nullopt,
             .axis = 1,
             .epsilon = 1e-5,
             .activation =
                 Activation{.kind = MLOperator::OperatorKind::kLeakyRelu,
                            .leaky_relu_alpha = 0.01}}}
        .Test(*this, scope, builder);
  }
  {
    // Test batchNormalization with leaky relu activation with given options.
    BatchNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .mean = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                 .dimensions = {3}},
        .variance = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {3}},
        .options = {.activation =
                        Activation{.kind = MLOperator::OperatorKind::kLeakyRelu,
                                   .leaky_relu_alpha = 0.02}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 5, 5}},
        .expected_attributes =
            {.scale = absl::nullopt,
             .bias = absl::nullopt,
             .axis = 1,
             .epsilon = 1e-5,
             .activation =
                 Activation{.kind = MLOperator::OperatorKind::kLeakyRelu,
                            .leaky_relu_alpha = 0.02}}}
        .Test(*this, scope, builder);
  }
  {
    // Test batchNormalization with relu activation.
    BatchNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .mean = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                 .dimensions = {3}},
        .variance = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {3}},
        .options = {.activation =
                        Activation{.kind = MLOperator::OperatorKind::kRelu}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 5, 5}},
        .expected_attributes =
            {.scale = absl::nullopt,
             .bias = absl::nullopt,
             .axis = 1,
             .epsilon = 1e-5,
             .activation = Activation{.kind = MLOperator::OperatorKind::kRelu}}}
        .Test(*this, scope, builder);
  }
  {
    // Test batchNormalization with sigmoid activation.
    BatchNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .mean = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                 .dimensions = {3}},
        .variance = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {3}},
        .options = {.activation =
                        Activation{.kind = MLOperator::OperatorKind::kSigmoid}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 5, 5}},
        .expected_attributes =
            {.scale = absl::nullopt,
             .bias = absl::nullopt,
             .axis = 1,
             .epsilon = 1e-5,
             .activation =
                 Activation{.kind = MLOperator::OperatorKind::kSigmoid}}}
        .Test(*this, scope, builder);
  }
  {
    // Test batchNormalization with softmax activation.
    BatchNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .mean = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                 .dimensions = {3}},
        .variance = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {3}},
        .options = {.activation =
                        Activation{.kind = MLOperator::OperatorKind::kSoftmax}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 5, 5}},
        .expected_attributes =
            {.scale = absl::nullopt,
             .bias = absl::nullopt,
             .axis = 1,
             .epsilon = 1e-5,
             .activation =
                 Activation{.kind = MLOperator::OperatorKind::kSoftmax}}}
        .Test(*this, scope, builder);
  }
  {
    // Test batchNormalization with tanh activation.
    BatchNormalizationTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .mean = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                 .dimensions = {3}},
        .variance = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {3}},
        .options = {.activation =
                        Activation{.kind = MLOperator::OperatorKind::kTanh}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 5, 5}},
        .expected_attributes =
            {.scale = absl::nullopt,
             .bias = absl::nullopt,
             .axis = 1,
             .epsilon = 1e-5,
             .activation = Activation{.kind = MLOperator::OperatorKind::kTanh}}}
        .Test(*this, scope, builder);
  }
}

struct Conv2dTester {
  OperandInfoBlink input;
  OperandInfoBlink filter;
  struct Conv2dOptions {
    absl::optional<Vector<uint32_t>> padding;
    absl::optional<Vector<uint32_t>> strides;
    absl::optional<Vector<uint32_t>> dilations;
    absl::optional<blink::V8MLAutoPad::Enum> auto_pad;
    absl::optional<uint32_t> groups;
    absl::optional<blink::V8MLInputOperandLayout::Enum> input_layout;
    absl::optional<blink::V8MLConv2dFilterOperandLayout::Enum> filter_layout;
    absl::optional<OperandInfoBlink> bias;
    absl::optional<Activation> activation;
  };
  struct Conv2dAttributes {
    Vector<uint32_t> padding = {0, 0, 0, 0};
    Vector<uint32_t> strides = {1, 1};
    Vector<uint32_t> dilations = {1, 1};
    uint32_t groups = 1;
    blink_mojom::InputOperandLayout input_layout =
        blink_mojom::InputOperandLayout::kChannelsFirst;
    absl::optional<OperandInfoMojo> bias;
    absl::optional<Activation> activation;
  };
  Conv2dOptions options;
  OperandInfoMojo expected_operand;
  Conv2dAttributes expected_attributes;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* filter_operand =
        BuildInput(builder, "filter", filter.dimensions, filter.data_type,
                   scope.GetExceptionState());
    MLConv2dOptions* ml_conv2d_options = MLConv2dOptions::Create();
    if (options.padding) {
      ml_conv2d_options->setPadding(options.padding.value());
    }
    if (options.strides) {
      ml_conv2d_options->setStrides(options.strides.value());
    }
    if (options.dilations) {
      ml_conv2d_options->setDilations(options.dilations.value());
    }
    if (options.auto_pad) {
      ml_conv2d_options->setAutoPad(options.auto_pad.value());
    }
    if (options.groups) {
      ml_conv2d_options->setGroups(options.groups.value());
    }
    if (options.input_layout) {
      ml_conv2d_options->setInputLayout(options.input_layout.value());
    }
    if (options.filter_layout) {
      ml_conv2d_options->setFilterLayout(options.filter_layout.value());
    }
    if (options.bias) {
      ml_conv2d_options->setBias(
          BuildInput(builder, "bias", options.bias->dimensions,
                     options.bias->data_type, scope.GetExceptionState()));
    }
    if (options.activation) {
      auto* activation =
          CreateActivation(scope, builder, options.activation.value());
      CHECK(activation);
      ml_conv2d_options->setActivation(activation);
    }
    auto* output_operand =
        builder->conv2d(input_operand, filter_operand, ml_conv2d_options,
                        scope.GetExceptionState());
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_NE(graph, nullptr);

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_EQ(operation->is_conv2d(), true);
    auto& conv2d = operation->get_conv2d();
    // Validate explicit padding.
    auto& expected_padding = expected_attributes.padding;
    EXPECT_EQ(conv2d->padding->beginning->height, expected_padding[0]);
    EXPECT_EQ(conv2d->padding->ending->height, expected_padding[1]);
    EXPECT_EQ(conv2d->padding->beginning->width, expected_padding[2]);
    EXPECT_EQ(conv2d->padding->ending->width, expected_padding[3]);
    // Validate strides
    EXPECT_EQ(conv2d->strides->height, expected_attributes.strides[0]);
    EXPECT_EQ(conv2d->strides->width, expected_attributes.strides[1]);
    // Validate dilations.
    EXPECT_EQ(conv2d->dilations->height, expected_attributes.dilations[0]);
    EXPECT_EQ(conv2d->dilations->width, expected_attributes.dilations[1]);
    EXPECT_EQ(conv2d->groups, expected_attributes.groups);
    EXPECT_EQ(conv2d->input_layout, expected_attributes.input_layout);
    if (options.bias) {
      auto bias_operand_iter =
          graph_info->id_to_operand_map.find(conv2d->bias_operand_id.value());
      ASSERT_TRUE(bias_operand_iter != graph_info->id_to_operand_map.end());
      EXPECT_EQ(bias_operand_iter->value->data_type,
                expected_attributes.bias->data_type);
      EXPECT_EQ(bias_operand_iter->value->dimensions,
                expected_attributes.bias->dimensions);
    }
    if (options.activation) {
      CHECK(expected_attributes.activation);
      CheckActivation(conv2d->activation,
                      expected_attributes.activation.value());
    }
    EXPECT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->data_type,
              expected_operand.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions,
              expected_operand.dimensions);
  }
};

TEST_P(MLGraphTestMojo, Conv2dTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      webnn::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_NE(builder, nullptr);
  {
    // Test conv2d with default options.
    Conv2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 3, 3}},
        .expected_attributes = {.padding = {0, 0, 0, 0},
                                .strides = {1, 1},
                                .dilations = {1, 1},
                                .groups = 1}}
        .Test(*this, scope, builder);
  }
  {
    // Test conv2d with autoPad="same-upper".
    Conv2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .options = {.auto_pad = V8MLAutoPad::Enum::kSameUpper},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 5, 5}},
        .expected_attributes = {.padding = {1, 1, 1, 1},
                                .strides = {1, 1},
                                .dilations = {1, 1},
                                .groups = 1}}
        .Test(*this, scope, builder);
  }
  {
    // Test conv2d with autoPad="same-lower".
    Conv2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .options = {.auto_pad = V8MLAutoPad::Enum::kSameLower},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 5, 5}},
        .expected_attributes = {.padding = {1, 1, 1, 1},
                                .strides = {1, 1},
                                .dilations = {1, 1},
                                .groups = 1}}
        .Test(*this, scope, builder);
  }
  {
    // Test conv2d with strides=2 and padding=1.
    Conv2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .options = {.padding = Vector<uint32_t>({1, 1, 1, 1}),
                    .strides = Vector<uint32_t>({2, 2})},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 3, 3}},
        .expected_attributes = {.padding = {1, 1, 1, 1},
                                .strides = {2, 2},
                                .dilations = {1, 1},
                                .groups = 1}}
        .Test(*this, scope, builder);
  }
  {
    // Test depthwise conv2d by setting groups to input channels.
    Conv2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 4, 2, 2}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {4, 1, 2, 2}},
        .options = {.groups = 4},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 4, 1, 1}},
        .expected_attributes = {.padding = {0, 0, 0, 0},
                                .strides = {1, 1},
                                .dilations = {1, 1},
                                .groups = 4}}
        .Test(*this, scope, builder);
  }
  {
    // Test conv2d with clamp activation.
    Conv2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .options = {.activation =
                        Activation{
                            .kind = MLOperator::OperatorKind::kClamp,
                            .clamp_options =
                                ClampTester::ClampOptions{.min_value = 1.0,
                                                          .max_value = 6.0}}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 3, 3}},
        .expected_attributes =
            {.padding = Vector<uint32_t>({0, 0, 0, 0}),
             .strides = Vector<uint32_t>({1, 1}),
             .dilations = Vector<uint32_t>({1, 1}),
             .groups = 1,
             .activation =
                 Activation{.kind = MLOperator::OperatorKind::kClamp,
                            .clamp_options =
                                ClampTester::ClampOptions{.min_value = 1.0,
                                                          .max_value = 6.0}}}}
        .Test(*this, scope, builder);
  }
  {
    // Test conv2d with elu activation with default options.
    Conv2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .options = {.activation =
                        Activation{.kind = MLOperator::OperatorKind::kElu}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 3, 3}},
        .expected_attributes = {.padding = Vector<uint32_t>({0, 0, 0, 0}),
                                .strides = Vector<uint32_t>({1, 1}),
                                .dilations = Vector<uint32_t>({1, 1}),
                                .groups = 1,
                                .activation =
                                    Activation{
                                        .kind = MLOperator::OperatorKind::kElu,
                                        .elu_alpha = 1.0}}}
        .Test(*this, scope, builder);
  }
  {
    // Test conv2d with elu activation with given alpha.
    Conv2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .options = {.activation =
                        Activation{.kind = MLOperator::OperatorKind::kElu,
                                   .elu_alpha = 0.5}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 3, 3}},
        .expected_attributes = {.padding = Vector<uint32_t>({0, 0, 0, 0}),
                                .strides = Vector<uint32_t>({1, 1}),
                                .dilations = Vector<uint32_t>({1, 1}),
                                .groups = 1,
                                .activation =
                                    Activation{
                                        .kind = MLOperator::OperatorKind::kElu,
                                        .elu_alpha = 0.5}}}
        .Test(*this, scope, builder);
  }
  {
    // Test conv2d with leaky relu activation with default options.
    Conv2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .options = {.activation =
                        Activation{.kind =
                                       MLOperator::OperatorKind::kLeakyRelu}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 3, 3}},
        .expected_attributes =
            {.padding = Vector<uint32_t>({0, 0, 0, 0}),
             .strides = Vector<uint32_t>({1, 1}),
             .dilations = Vector<uint32_t>({1, 1}),
             .groups = 1,
             .activation =
                 Activation{.kind = MLOperator::OperatorKind::kLeakyRelu,
                            .leaky_relu_alpha = 0.01}}}
        .Test(*this, scope, builder);
  }
  {
    // Test conv2d with leaky relu activation with given alpha.
    Conv2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .options = {.activation =
                        Activation{.kind = MLOperator::OperatorKind::kLeakyRelu,
                                   .leaky_relu_alpha = 0.02}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 3, 3}},
        .expected_attributes =
            {.padding = Vector<uint32_t>({0, 0, 0, 0}),
             .strides = Vector<uint32_t>({1, 1}),
             .dilations = Vector<uint32_t>({1, 1}),
             .groups = 1,
             .activation =
                 Activation{.kind = MLOperator::OperatorKind::kLeakyRelu,
                            .leaky_relu_alpha = 0.02}}}
        .Test(*this, scope, builder);
  }
  {
    // Test conv2d with relu activation.
    Conv2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .options = {.activation =
                        Activation{.kind = MLOperator::OperatorKind::kRelu}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 3, 3}},
        .expected_attributes =
            {.padding = Vector<uint32_t>({0, 0, 0, 0}),
             .strides = Vector<uint32_t>({1, 1}),
             .dilations = Vector<uint32_t>({1, 1}),
             .groups = 1,
             .activation = Activation{.kind = MLOperator::OperatorKind::kRelu}}}
        .Test(*this, scope, builder);
  }
  {
    // Test conv2d with sigmoid activation.
    Conv2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .options = {.activation =
                        Activation{.kind = MLOperator::OperatorKind::kSigmoid}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 3, 3}},
        .expected_attributes =
            {.padding = Vector<uint32_t>({0, 0, 0, 0}),
             .strides = Vector<uint32_t>({1, 1}),
             .dilations = Vector<uint32_t>({1, 1}),
             .groups = 1,
             .activation =
                 Activation{.kind = MLOperator::OperatorKind::kSigmoid}}}
        .Test(*this, scope, builder);
  }
  {
    // Test conv2d with softmax activation.
    Conv2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                   .dimensions = {1, 1, 3, 3}},
        .options = {.activation =
                        Activation{.kind = MLOperator::OperatorKind::kSoftmax}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {1, 1, 3, 3}},
        .expected_attributes =
            {.padding = Vector<uint32_t>({0, 0, 0, 0}),
             .strides = Vector<uint32_t>({1, 1}),
             .dilations = Vector<uint32_t>({1, 1}),
             .groups = 1,
             .activation =
                 Activation{.kind = MLOperator::OperatorKind::kSoftmax}}}
        .Test(*this, scope, builder);
  }
  {
    // Test conv2d with tanh activation.
    Conv2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 5, 5}},
        .filter = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                   .dimensions = {1, 1, 3, 3}},
        .options = {.activation =
                        Activation{.kind = MLOperator::OperatorKind::kTanh}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 3, 3}},
        .expected_attributes =
            {.padding = Vector<uint32_t>({0, 0, 0, 0}),
             .strides = Vector<uint32_t>({1, 1}),
             .dilations = Vector<uint32_t>({1, 1}),
             .groups = 1,
             .activation = Activation{.kind = MLOperator::OperatorKind::kTanh}}}
        .Test(*this, scope, builder);
  }
}

struct ElementWiseBinaryTester {
  OperandInfoBlink lhs;
  OperandInfoBlink rhs;
  OperandInfoMojo expected;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    Test(helper, scope, builder, ElementWiseBinaryKind::kAdd);
    Test(helper, scope, builder, ElementWiseBinaryKind::kSub);
    Test(helper, scope, builder, ElementWiseBinaryKind::kMul);
    Test(helper, scope, builder, ElementWiseBinaryKind::kDiv);
    Test(helper, scope, builder, ElementWiseBinaryKind::kMin);
    Test(helper, scope, builder, ElementWiseBinaryKind::kMax);
    Test(helper, scope, builder, ElementWiseBinaryKind::kPow);
  }

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder,
            ElementWiseBinaryKind kind) {
    // Build the graph.
    auto* lhs_operand = BuildInput(builder, "lhs", lhs.dimensions,
                                   lhs.data_type, scope.GetExceptionState());
    auto* rhs_operand = BuildInput(builder, "rhs", rhs.dimensions,
                                   rhs.data_type, scope.GetExceptionState());
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
    EXPECT_EQ(lhs_operand_iter->value->data_type, expected.data_type);
    EXPECT_EQ(lhs_operand_iter->value->dimensions, lhs.dimensions);
    EXPECT_EQ(lhs_operand_iter->value->name, "lhs");
    // Verify the right `mojo::Operand`.
    auto rhs_operand_id = graph_info->input_operands[1];
    auto rhs_operand_iter = graph_info->id_to_operand_map.find(rhs_operand_id);
    ASSERT_TRUE(rhs_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(rhs_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kInput);
    EXPECT_EQ(rhs_operand_iter->value->data_type, expected.data_type);
    EXPECT_EQ(rhs_operand_iter->value->dimensions, rhs.dimensions);
    EXPECT_EQ(rhs_operand_iter->value->name, "rhs");
    // Verify the output `mojo::Operand`.
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kOutput);
    EXPECT_EQ(output_operand_iter->value->data_type, expected.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions, expected.dimensions);
    EXPECT_EQ(output_operand_iter->value->name, "output");
    // Verify the `mojo::Operator`.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_EQ(operation->is_element_wise_binary(), true);
    auto& binary_mojo = operation->get_element_wise_binary();

    blink_mojom::ElementWiseBinary::Kind binary_kind;
    switch (kind) {
      case ElementWiseBinaryKind::kAdd:
        binary_kind = blink_mojom::ElementWiseBinary::Kind::kAdd;
        break;
      case ElementWiseBinaryKind::kSub:
        binary_kind = blink_mojom::ElementWiseBinary::Kind::kSub;
        break;
      case ElementWiseBinaryKind::kMul:
        binary_kind = blink_mojom::ElementWiseBinary::Kind::kMul;
        break;
      case ElementWiseBinaryKind::kDiv:
        binary_kind = blink_mojom::ElementWiseBinary::Kind::kDiv;
        break;
      case ElementWiseBinaryKind::kMin:
        binary_kind = blink_mojom::ElementWiseBinary::Kind::kMin;
        break;
      case ElementWiseBinaryKind::kMax:
        binary_kind = blink_mojom::ElementWiseBinary::Kind::kMax;
        break;
      case ElementWiseBinaryKind::kPow:
        binary_kind = blink_mojom::ElementWiseBinary::Kind::kPow;
        break;
    }
    EXPECT_EQ(binary_mojo->kind, binary_kind);
    EXPECT_EQ(binary_mojo->lhs_operand, lhs_operand_id);
    EXPECT_EQ(binary_mojo->rhs_operand, rhs_operand_id);
    EXPECT_EQ(binary_mojo->output_operand, output_operand_id);
  }
};

TEST_P(MLGraphTestMojo, ElementWiseBinaryTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      webnn::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_NE(builder, nullptr);
  {
    // Test element-wise add operator for two 0-D scalars.
    ElementWiseBinaryTester{
        .lhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {}}}
        .Test(*this, scope, builder);
  }
  {
    // Test element-wise add operator for two 1-D tensors.
    ElementWiseBinaryTester{
        .lhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {2}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                .dimensions = {2}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {2}}}
        .Test(*this, scope, builder);
  }
  {
    // Test element-wise add operator for two 2-D tensors.
    ElementWiseBinaryTester{
        .lhs = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                .dimensions = {3, 7}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                .dimensions = {3, 7}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat16,
                     .dimensions = {3, 7}}}
        .Test(*this, scope, builder);
  }
  {
    // Test element-wise add operator for broadcasting to 2-D tensor.
    ElementWiseBinaryTester{
        .lhs = {.data_type = V8MLOperandDataType::Enum::kInt32,
                .dimensions = {5, 3}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kInt32,
                .dimensions = {5, 1}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kInt32,
                     .dimensions = {5, 3}}}
        .Test(*this, scope, builder);
  }
  {
    // Test element-wise add operator for broadcasting to 3-D tensor.
    ElementWiseBinaryTester{
        .lhs = {.data_type = V8MLOperandDataType::Enum::kInt8,
                .dimensions = {4, 2, 1}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kInt8,
                .dimensions = {4}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kInt8,
                     .dimensions = {4, 2, 4}}}
        .Test(*this, scope, builder);
  }
  {
    // Test element-wise add operator for broadcasting to 4-D tensors.
    ElementWiseBinaryTester{
        .lhs = {.data_type = V8MLOperandDataType::Enum::kUint8,
                .dimensions = {8, 1, 6, 1}},
        .rhs = {.data_type = V8MLOperandDataType::Enum::kUint8,
                .dimensions = {7, 1, 5}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kUint8,
                     .dimensions = {8, 7, 6, 5}}}
        .Test(*this, scope, builder);
  }
}

struct EluTester {
  OperandInfoBlink input;
  absl::optional<float> alpha;
  OperandInfoMojo expected_operand;
  float expected_alpha;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    MLEluOptions* ml_elu_options = MLEluOptions::Create();
    if (alpha) {
      ml_elu_options->setAlpha(alpha.value());
    }
    auto* output_operand =
        builder->elu(input_operand, ml_elu_options, scope.GetExceptionState());
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_NE(graph, nullptr);

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo is as expected.
    ASSERT_EQ(graph_info->id_to_operand_map.size(), 2u);

    // Verify the input `mojo::Operand`.
    ASSERT_EQ(graph_info->input_operands.size(), 1u);
    auto input_operand_id = graph_info->input_operands[0];
    auto input_operand_iter =
        graph_info->id_to_operand_map.find(input_operand_id);
    ASSERT_TRUE(input_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(input_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kInput);
    EXPECT_EQ(input_operand_iter->value->data_type, expected_operand.data_type);
    EXPECT_EQ(input_operand_iter->value->dimensions, input.dimensions);
    EXPECT_EQ(input_operand_iter->value->name, "input");

    // Verify the output `mojo::Operand`.
    ASSERT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kOutput);
    EXPECT_EQ(output_operand_iter->value->data_type,
              expected_operand.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions,
              expected_operand.dimensions);
    EXPECT_EQ(output_operand_iter->value->name, "output");

    // Verify the `mojo::Operator`.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    ASSERT_TRUE(operation->is_elu());
    auto& elu = operation->get_elu();
    EXPECT_EQ(elu->input_operand_id, input_operand_id);
    EXPECT_EQ(elu->output_operand_id, output_operand_id);
    EXPECT_EQ(elu->alpha, expected_alpha);
  }
};

TEST_P(MLGraphTestMojo, EluTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      webnn::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_NE(builder, nullptr);
  {
    // Test elu operator for 0-D tensor with default options.
    EluTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                        .dimensions = {}},
              .expected_operand = {.data_type =
                                       blink_mojom::Operand::DataType::kFloat32,
                                   .dimensions = {}},
              .expected_alpha = 1}
        .Test(*this, scope, builder);
  }
  {
    // Test elu operator for 1-D tensor with default options.
    EluTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                        .dimensions = {2}},
              .expected_operand = {.data_type =
                                       blink_mojom::Operand::DataType::kFloat32,
                                   .dimensions = {2}},
              .expected_alpha = 1}
        .Test(*this, scope, builder);
  }
  {
    // Test elu operator for 2-D tensor with default options.
    EluTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                        .dimensions = {3, 7}},
              .expected_operand = {.data_type =
                                       blink_mojom::Operand::DataType::kFloat16,
                                   .dimensions = {3, 7}},
              .expected_alpha = 1}
        .Test(*this, scope, builder);
  }
  {
    // Test elu operator for 3-D tensor with given alpha.
    EluTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                        .dimensions = {1, 5, 3}},
              .alpha = 0.5,
              .expected_operand = {.data_type =
                                       blink_mojom::Operand::DataType::kFloat32,
                                   .dimensions = {1, 5, 3}},
              .expected_alpha = 0.5}
        .Test(*this, scope, builder);
  }
  {
    // Test elu operator for 4-D tensor with given alpha.
    EluTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                        .dimensions = {1, 2, 2, 1}},
              .alpha = 0.7,
              .expected_operand = {.data_type =
                                       blink_mojom::Operand::DataType::kFloat16,
                                   .dimensions = {1, 2, 2, 1}},
              .expected_alpha = 0.7}
        .Test(*this, scope, builder);
  }
}

struct ExpandTester {
  OperandInfoBlink input;
  Vector<uint32_t> new_shape;
  OperandInfoMojo expected;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* output_operand =
        builder->expand(input_operand, new_shape, scope.GetExceptionState());
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_NE(graph, nullptr);

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_EQ(operation->is_expand(), true);
    EXPECT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->data_type, expected.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions, expected.dimensions);
  }
};

TEST_P(MLGraphTestMojo, ExpandTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      webnn::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_NE(builder, nullptr);
  {
    // Test building expand 0-D scalar to 3-D tensor.
    ExpandTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {}},
        .new_shape = {3, 4, 5},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {3, 4, 5}}}
        .Test(*this, scope, builder);
  }
  {
    // Test expanding the new shape that is the same as input.
    ExpandTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {3, 2}},
        .new_shape = {3, 2},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {3, 2}}}
        .Test(*this, scope, builder);
  }
  {
    // Test expanding the new shape that are broadcastable.
    ExpandTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {3, 1, 5}},
        .new_shape = {3, 4, 5},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat16,
                     .dimensions = {3, 4, 5}}}
        .Test(*this, scope, builder);
  }
  {
    // Test expanding the new shape that are broadcastable and the number of new
    // shapes larger than input.
    ExpandTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kInt32,
                  .dimensions = {2, 5}},
        .new_shape = {3, 2, 5},
        .expected = {.data_type = blink_mojom::Operand::DataType::kInt32,
                     .dimensions = {3, 2, 5}}}
        .Test(*this, scope, builder);
  }
}

struct GatherTester {
  OperandInfoBlink input;
  OperandInfoBlink indices;
  absl::optional<uint32_t> axis;
  OperandInfoMojo expected_operand;
  blink_mojom::Operand::DataType expected_indices_date_type;
  float expected_axis;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* indices_operand =
        BuildInput(builder, "indices", indices.dimensions, indices.data_type,
                   scope.GetExceptionState());
    MLGatherOptions* ml_gather_options = MLGatherOptions::Create();
    if (axis) {
      ml_gather_options->setAxis(axis.value());
    }
    auto* output_operand =
        builder->gather(input_operand, indices_operand, ml_gather_options,
                        scope.GetExceptionState());
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_NE(graph, nullptr);

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->id_to_operand_map.size(), 3u);

    // Verify the input `mojo::Operand`.
    ASSERT_EQ(graph_info->input_operands.size(), 2u);
    auto input_operand_id = graph_info->input_operands[0];
    auto input_operand_iter =
        graph_info->id_to_operand_map.find(input_operand_id);
    ASSERT_TRUE(input_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(input_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kInput);
    EXPECT_EQ(input_operand_iter->value->data_type, expected_operand.data_type);
    EXPECT_EQ(input_operand_iter->value->dimensions, input.dimensions);
    EXPECT_EQ(input_operand_iter->value->name, "input");

    // Verify the indices `mojo::Operand`.
    auto indices_operand_id = graph_info->input_operands[1];
    auto indices_operand_iter =
        graph_info->id_to_operand_map.find(indices_operand_id);
    ASSERT_TRUE(indices_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(indices_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kInput);
    EXPECT_EQ(indices_operand_iter->value->data_type,
              expected_indices_date_type);
    EXPECT_EQ(indices_operand_iter->value->dimensions, indices.dimensions);
    EXPECT_EQ(indices_operand_iter->value->name, "indices");

    // Verify the output `mojo::Operand`.
    ASSERT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kOutput);
    EXPECT_EQ(output_operand_iter->value->data_type,
              expected_operand.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions,
              expected_operand.dimensions);
    EXPECT_EQ(output_operand_iter->value->name, "output");

    // Verify the `mojo::Operator`.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    ASSERT_TRUE(operation->is_gather());
    auto& gather = operation->get_gather();
    EXPECT_EQ(gather->input_operand_id, input_operand_id);
    EXPECT_EQ(gather->indices_operand_id, indices_operand_id);
    EXPECT_EQ(gather->output_operand_id, output_operand_id);
    EXPECT_EQ(gather->axis, expected_axis);
  }
};

TEST_P(MLGraphTestMojo, GatherTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      webnn::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_NE(builder, nullptr);
  {
    // Test building gather with default options.
    GatherTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 3}},
        .indices = {.data_type = V8MLOperandDataType::Enum::kUint32,
                    .dimensions = {4, 5}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {4, 5, 3}},
        .expected_indices_date_type = blink_mojom::Operand::DataType::kUint32,
        .expected_axis = 0}
        .Test(*this, scope, builder);
  }
  {
    // Test building gather with axis = 2.
    GatherTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {1, 2, 3, 4}},
        .indices = {.data_type = V8MLOperandDataType::Enum::kUint64,
                    .dimensions = {6, 7, 8}},
        .axis = 2,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {1, 2, 6, 7, 8, 4}},
        .expected_indices_date_type = blink_mojom::Operand::DataType::kUint64,
        .expected_axis = 2}
        .Test(*this, scope, builder);
  }
  {
    // Test building gather with 0-D indices.
    GatherTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kUint32,
                  .dimensions = {3}},
        .indices = {.data_type = V8MLOperandDataType::Enum::kUint64,
                    .dimensions = {}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kUint32,
                             .dimensions = {}},
        .expected_indices_date_type = blink_mojom::Operand::DataType::kUint64,
        .expected_axis = 0}
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
    auto* a_operand = BuildInput(builder, "a", a.dimensions, a.data_type,
                                 scope.GetExceptionState());
    auto* b_operand = BuildInput(builder, "b", b.dimensions, b.data_type,
                                 scope.GetExceptionState());
    MLGemmOptions* ml_gemm_options = MLGemmOptions::Create();
    if (options.c) {
      ml_gemm_options->setC(BuildInput(builder, "c", options.c->dimensions,
                                       options.c->data_type,
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
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_EQ(operation->is_gemm(), true);
    auto& gemm_mojo = operation->get_gemm();
    if (options.c) {
      auto c_operand_iter =
          graph_info->id_to_operand_map.find(gemm_mojo->c_operand_id.value());
      ASSERT_TRUE(c_operand_iter != graph_info->id_to_operand_map.end());
      EXPECT_EQ(c_operand_iter->value->data_type,
                expected_attributes.c->data_type);
      EXPECT_EQ(c_operand_iter->value->dimensions,
                expected_attributes.c->dimensions);
    } else {
      EXPECT_EQ(gemm_mojo->c_operand_id, absl::nullopt);
    }
    EXPECT_EQ(gemm_mojo->alpha, expected_attributes.alpha);
    EXPECT_EQ(gemm_mojo->beta, expected_attributes.beta);
    EXPECT_EQ(gemm_mojo->a_transpose, expected_attributes.a_transpose);
    EXPECT_EQ(gemm_mojo->b_transpose, expected_attributes.b_transpose);
    EXPECT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->data_type,
              expected_operand.data_type);
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
      webnn::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_NE(builder, nullptr);
  {
    // Test building gemm with default option.
    GemmTester{
        .a = {.data_type = V8MLOperandDataType::Enum::kFloat32,
              .dimensions = {2, 3}},
        .b = {.data_type = V8MLOperandDataType::Enum::kFloat32,
              .dimensions = {3, 4}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
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
        .a = {.data_type = V8MLOperandDataType::Enum::kFloat32,
              .dimensions = {2, 3}},
        .b = {.data_type = V8MLOperandDataType::Enum::kFloat32,
              .dimensions = {2, 4}},
        .options = {.a_transpose = true},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
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
        .a = {.data_type = V8MLOperandDataType::Enum::kFloat32,
              .dimensions = {2, 3}},
        .b = {.data_type = V8MLOperandDataType::Enum::kFloat32,
              .dimensions = {4, 3}},
        .options = {.b_transpose = true},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
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
        .a = {.data_type = V8MLOperandDataType::Enum::kFloat32,
              .dimensions = {2, 3}},
        .b = {.data_type = V8MLOperandDataType::Enum::kFloat32,
              .dimensions = {3, 4}},
        .options =
            {
                .c = OperandInfoBlink{.data_type =
                                          V8MLOperandDataType::Enum::kFloat32,
                                      .dimensions = {4}},
                .alpha = 2.0,
                .beta = 3.0,
            },
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {2, 4}},
        .expected_attributes =
            {.c = OperandInfoMojo{.data_type =
                                      blink_mojom::Operand::DataType::kFloat32,
                                  .dimensions = {4}},
             .alpha = 2.0,
             .beta = 3.0,
             .a_transpose = false,
             .b_transpose = false}}
        .Test(*this, scope, builder);
  }
  {
    // Test building gemm with setting scalar C.
    GemmTester{
        .a = {.data_type = V8MLOperandDataType::Enum::kFloat32,
              .dimensions = {2, 3}},
        .b = {.data_type = V8MLOperandDataType::Enum::kFloat32,
              .dimensions = {3, 4}},
        .options =
            {
                .c = OperandInfoBlink{.data_type =
                                          V8MLOperandDataType::Enum::kFloat32,
                                      .dimensions = {}},
                .alpha = 2.0,
                .beta = 3.0,
            },
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {2, 4}},
        .expected_attributes =
            {.c = OperandInfoMojo{.data_type =
                                      blink_mojom::Operand::DataType::kFloat32,
                                  .dimensions = {}},
             .alpha = 2.0,
             .beta = 3.0,
             .a_transpose = false,
             .b_transpose = false}}
        .Test(*this, scope, builder);
  }
}

struct LeakyReluTester {
  OperandInfoBlink input;
  absl::optional<float> alpha;
  OperandInfoMojo expected_operand;
  float expected_alpha;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    MLLeakyReluOptions* ml_leaky_relu_options = MLLeakyReluOptions::Create();
    if (alpha) {
      ml_leaky_relu_options->setAlpha(alpha.value());
    }
    auto* output_operand = builder->leakyRelu(
        input_operand, ml_leaky_relu_options, scope.GetExceptionState());
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_NE(graph, nullptr);

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->id_to_operand_map.size(), 2u);

    // Verify the input `mojo::Operand`.
    ASSERT_EQ(graph_info->input_operands.size(), 1u);
    auto input_operand_id = graph_info->input_operands[0];
    auto input_operand_iter =
        graph_info->id_to_operand_map.find(input_operand_id);
    ASSERT_TRUE(input_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(input_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kInput);
    EXPECT_EQ(input_operand_iter->value->data_type, expected_operand.data_type);
    EXPECT_EQ(input_operand_iter->value->dimensions, input.dimensions);
    EXPECT_EQ(input_operand_iter->value->name, "input");

    // Verify the output `mojo::Operand`.
    ASSERT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kOutput);
    EXPECT_EQ(output_operand_iter->value->data_type,
              expected_operand.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions,
              expected_operand.dimensions);
    EXPECT_EQ(output_operand_iter->value->name, "output");

    // Verify the `mojo::Operator`.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    ASSERT_TRUE(operation->is_leaky_relu());
    auto& leaky_relu = operation->get_leaky_relu();
    EXPECT_EQ(leaky_relu->input_operand_id, input_operand_id);
    EXPECT_EQ(leaky_relu->output_operand_id, output_operand_id);
    EXPECT_EQ(leaky_relu->alpha, expected_alpha);
  }
};

TEST_P(MLGraphTestMojo, LeakyReluTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      webnn::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_NE(builder, nullptr);
  {
    // Test leaky relu operator for 0-D scalar with default options.
    LeakyReluTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {}},
        .expected_alpha = 0.01}
        .Test(*this, scope, builder);
  }
  {
    // Test leaky relu operator for 1-D tensor with default options.
    LeakyReluTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {2}},
        .expected_alpha = 0.01}
        .Test(*this, scope, builder);
  }
  {
    // Test leaky relu operator for 2-D tensor with default options.
    LeakyReluTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {3, 7}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {3, 7}},
        .expected_alpha = 0.01}
        .Test(*this, scope, builder);
  }
  {
    // Test leaky relu operator for 3-D tensor with given alpha.
    LeakyReluTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 5, 3}},
        .alpha = 0.05,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 5, 3}},
        .expected_alpha = 0.05}
        .Test(*this, scope, builder);
  }
  {
    // Test leaky relu operator for 4-D tensor with given alpha.
    LeakyReluTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {1, 2, 2, 1}},
        .alpha = 0.07,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {1, 2, 2, 1}},
        .expected_alpha = 0.07}
        .Test(*this, scope, builder);
  }
}

struct MatmulTester {
  OperandInfoBlink a;
  OperandInfoBlink b;
  OperandInfoMojo expected_operand;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* a_operand = BuildInput(builder, "a", a.dimensions, a.data_type,
                                 scope.GetExceptionState());
    auto* b_operand = BuildInput(builder, "b", b.dimensions, b.data_type,
                                 scope.GetExceptionState());
    auto* output_operand =
        builder->matmul(a_operand, b_operand, scope.GetExceptionState());
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_NE(graph, nullptr);

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    EXPECT_EQ(graph_info->id_to_operand_map.size(), 3u);
    EXPECT_EQ(graph_info->input_operands.size(), 2u);
    // Verify the a `mojo::Operand`.
    auto a_operand_id = graph_info->input_operands[0];
    auto a_operand_iter = graph_info->id_to_operand_map.find(a_operand_id);
    ASSERT_TRUE(a_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(a_operand_iter->value->kind, blink_mojom::Operand::Kind::kInput);
    EXPECT_EQ(a_operand_iter->value->data_type, expected_operand.data_type);
    EXPECT_EQ(a_operand_iter->value->dimensions, a.dimensions);
    EXPECT_EQ(a_operand_iter->value->name, "a");
    // Verify the b `mojo::Operand`.
    auto b_operand_id = graph_info->input_operands[1];
    auto b_operand_iter = graph_info->id_to_operand_map.find(b_operand_id);
    ASSERT_TRUE(b_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(b_operand_iter->value->kind, blink_mojom::Operand::Kind::kInput);
    EXPECT_EQ(b_operand_iter->value->data_type, expected_operand.data_type);
    EXPECT_EQ(b_operand_iter->value->dimensions, b.dimensions);
    EXPECT_EQ(b_operand_iter->value->name, "b");
    // Verify the output `mojo::Operand`.
    ASSERT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->data_type,
              expected_operand.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions,
              expected_operand.dimensions);
    EXPECT_EQ(output_operand_iter->value->data_type,
              expected_operand.data_type);
    EXPECT_EQ(output_operand_iter->value->name, "output");
    // Verify the `mojo::Operator`.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_EQ(operation->is_matmul(), true);
  }
};

TEST_P(MLGraphTestMojo, MatmulTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      webnn::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_NE(builder, nullptr);
  {
    // Test building matmul with 2-D * 2-D.
    MatmulTester{
        .a = {.data_type = V8MLOperandDataType::Enum::kFloat32,
              .dimensions = {2, 3}},
        .b = {.data_type = V8MLOperandDataType::Enum::kFloat32,
              .dimensions = {3, 4}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {2, 4}}}
        .Test(*this, scope, builder);
  }
  {
    // Test building matmul with 3-D * 4-D using broadcasting.
    MatmulTester{
        .a = {.data_type = V8MLOperandDataType::Enum::kFloat16,
              .dimensions = {2, 2, 3}},
        .b = {.data_type = V8MLOperandDataType::Enum::kFloat16,
              .dimensions = {3, 1, 3, 4}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {3, 2, 2, 4}}}
        .Test(*this, scope, builder);
  }
}

struct PadTester {
  OperandInfoBlink input;
  Vector<uint32_t> beginning_padding;
  Vector<uint32_t> ending_padding;
  struct PadOptions {
    absl::optional<V8MLPaddingMode::Enum> mode;
    absl::optional<float> value;
  };
  PadOptions options;

  blink_mojom::PaddingMode::Tag expected_mode =
      blink_mojom::PaddingMode::Tag::kConstant;
  float expected_value = 0;
  OperandInfoMojo expected_operand;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    MLPadOptions* ml_pad_options = MLPadOptions::Create();
    if (options.mode) {
      ml_pad_options->setMode(options.mode.value());
    }
    if (options.value) {
      ml_pad_options->setValue(options.value.value());
    }

    auto* output_operand =
        BuildPad(scope, builder, input_operand, beginning_padding,
                 ending_padding, ml_pad_options);
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_NE(graph, nullptr);

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_EQ(operation->is_pad(), true);
    auto& pad_mojo = operation->get_pad();

    // Validate the beginning padding and the ending padding.
    EXPECT_EQ(pad_mojo->beginning_padding, beginning_padding);
    EXPECT_EQ(pad_mojo->ending_padding, ending_padding);
    // Validate the padding mode.
    auto& padding_mode = pad_mojo->mode;
    EXPECT_EQ(padding_mode->which(), expected_mode);
    // Validate the padding value.
    if (padding_mode->is_constant()) {
      EXPECT_EQ(padding_mode->get_constant()->value, expected_value);
    }
    // Validate the output operand.
    EXPECT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->data_type,
              expected_operand.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions,
              expected_operand.dimensions);
  }
};

TEST_P(MLGraphTestMojo, PadTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      webnn::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_NE(builder, nullptr);
  {
    // Test pad with default options, beginningPadding = {1, 2} and
    // endingPadding = {1, 2}.
    PadTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                        .dimensions = {2, 3}},
              .beginning_padding = {1, 2},
              .ending_padding = {1, 2},
              .expected_operand = {.data_type =
                                       blink_mojom::Operand::DataType::kFloat32,
                                   .dimensions = {4, 7}}}
        .Test(*this, scope, builder);
  }
  {
    // Test pad with mode = "constant", value = 1, beginningPadding = {1, 2} and
    // endingPadding = {1, 2}.
    PadTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                        .dimensions = {2, 3}},
              .beginning_padding = {1, 2},
              .ending_padding = {1, 2},
              .options = {.mode = V8MLPaddingMode::Enum::kConstant, .value = 1},
              .expected_mode = blink_mojom::PaddingMode::Tag::kConstant,
              .expected_value = 1,
              .expected_operand = {.data_type =
                                       blink_mojom::Operand::DataType::kFloat32,
                                   .dimensions = {4, 7}}}
        .Test(*this, scope, builder);
  }
  {
    // Test pad with mode = "edge", beginningPadding = {1, 2} and
    // endingPadding = {1, 2}.
    PadTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                        .dimensions = {2, 3}},
              .beginning_padding = {1, 2},
              .ending_padding = {1, 2},
              .options = {.mode = V8MLPaddingMode::Enum::kEdge},
              .expected_mode = blink_mojom::PaddingMode::Tag::kEdge,
              .expected_operand = {.data_type =
                                       blink_mojom::Operand::DataType::kFloat32,
                                   .dimensions = {4, 7}}}
        .Test(*this, scope, builder);
  }
  {
    // Test pad with mode = "reflection", beginningPadding = {1, 2} and
    // endingPadding = {1, 2}.
    PadTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                        .dimensions = {2, 3}},
              .beginning_padding = {1, 2},
              .ending_padding = {1, 2},
              .options = {.mode = V8MLPaddingMode::Enum::kReflection},
              .expected_mode = blink_mojom::PaddingMode::Tag::kReflection,
              .expected_operand = {.data_type =
                                       blink_mojom::Operand::DataType::kFloat32,
                                   .dimensions = {4, 7}}}
        .Test(*this, scope, builder);
  }
  {
    // Test pad with mode = "symmetric", beginningPadding = {1, 2} and
    // endingPadding = {1, 2}.
    PadTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                        .dimensions = {2, 3}},
              .beginning_padding = {1, 2},
              .ending_padding = {1, 2},
              .options = {.mode = V8MLPaddingMode::Enum::kSymmetric},
              .expected_mode = blink_mojom::PaddingMode::Tag::kSymmetric,
              .expected_operand = {.data_type =
                                       blink_mojom::Operand::DataType::kFloat32,
                                   .dimensions = {4, 7}}}
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
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
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
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_EQ(operation->is_pool2d(), true);
    auto& poo2d_mojo = operation->get_pool2d();
    switch (kind) {
      case Pool2dKind::kAverage:
        EXPECT_EQ(poo2d_mojo->kind, blink_mojom::Pool2d::Kind::kAveragePool2d);
        break;
      case Pool2dKind::kMax:
        EXPECT_EQ(poo2d_mojo->kind, blink_mojom::Pool2d::Kind::kMaxPool2d);
        break;
      default:
        NOTREACHED();
    }
    // Validate window dimensions.
    EXPECT_EQ(poo2d_mojo->window_dimensions->height,
              expected_attributes.window_dimensions[0]);
    EXPECT_EQ(poo2d_mojo->window_dimensions->width,
              expected_attributes.window_dimensions[1]);
    // Validate explicit padding.
    auto& expected_padding = expected_attributes.padding;
    EXPECT_EQ(poo2d_mojo->padding->beginning->height, expected_padding[0]);
    EXPECT_EQ(poo2d_mojo->padding->ending->height, expected_padding[1]);
    EXPECT_EQ(poo2d_mojo->padding->beginning->width, expected_padding[2]);
    EXPECT_EQ(poo2d_mojo->padding->ending->width, expected_padding[3]);
    // Validate strides
    EXPECT_EQ(poo2d_mojo->strides->height, expected_attributes.strides[0]);
    EXPECT_EQ(poo2d_mojo->strides->width, expected_attributes.strides[1]);
    // Validate dilations.
    EXPECT_EQ(poo2d_mojo->dilations->height, expected_attributes.dilations[0]);
    EXPECT_EQ(poo2d_mojo->dilations->width, expected_attributes.dilations[1]);
    EXPECT_EQ(poo2d_mojo->layout, expected_attributes.layout);
    EXPECT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->data_type,
              expected_operand.data_type);
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
      webnn::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_NE(builder, nullptr);
  {
    // Test pool2d with default options.
    Pool2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 4, 4}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 1, 1}},
        .expected_attributes = {.window_dimensions = {4, 4},
                                .strides = {1, 1},
                                .dilations = {1, 1}}}
        .Test(*this, scope, builder);
  }
  {
    // Test pool2d without padding.
    Pool2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 4, 4}},
        .options = {.window_dimensions = Vector<uint32_t>({3, 3})},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
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
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .options = {.window_dimensions = Vector<uint32_t>({5, 5}),
                    .auto_pad = V8MLAutoPad::Enum::kSameUpper},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
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
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 5, 5}},
        .options = {.window_dimensions = Vector<uint32_t>({5, 5}),
                    .auto_pad = V8MLAutoPad::Enum::kSameLower},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
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
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 7, 7}},
        .options = {.window_dimensions = Vector<uint32_t>({4, 4}),
                    .padding = Vector<uint32_t>({1, 1, 1, 1}),
                    .strides = Vector<uint32_t>({2, 2}),
                    .rounding_type = V8MLRoundingType::Enum::kFloor},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
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
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 7, 7}},
        .options = {.window_dimensions = Vector<uint32_t>({4, 4}),
                    .padding = Vector<uint32_t>({1, 1, 1, 1}),
                    .strides = Vector<uint32_t>({2, 2}),
                    .rounding_type = V8MLRoundingType::Enum::kCeil},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
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
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 7, 7}},
        .options = {.window_dimensions = Vector<uint32_t>({4, 4}),
                    .padding = Vector<uint32_t>({1, 1, 1, 1}),
                    .strides = Vector<uint32_t>({2, 2}),
                    .rounding_type = V8MLRoundingType::Enum::kCeil,
                    .output_sizes = Vector<uint32_t>({3, 3})},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
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
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 7, 7, 3}},
        .options = {.window_dimensions = Vector<uint32_t>({4, 4}),
                    .padding = Vector<uint32_t>({1, 1, 1, 1}),
                    .strides = Vector<uint32_t>({2, 2}),
                    .layout = V8MLInputOperandLayout::Enum::kNhwc,
                    .rounding_type = V8MLRoundingType::Enum::kCeil,
                    .output_sizes = Vector<uint32_t>({3, 3})},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
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

struct PreluTester {
  OperandInfoBlink input;
  OperandInfoBlink slope;
  OperandInfoMojo expected;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* slope_operand =
        BuildInput(builder, "slope", slope.dimensions, slope.data_type,
                   scope.GetExceptionState());
    auto* output_operand =
        builder->prelu(input_operand, slope_operand, scope.GetExceptionState());
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_NE(graph, nullptr);

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    ASSERT_TRUE(operation->is_prelu());
    auto& prelu = operation->get_prelu();

    // Verify the input operand.
    ASSERT_EQ(graph_info->input_operands.size(), 2u);
    auto input_operand_id = graph_info->input_operands[0];
    auto input_operand_iter =
        graph_info->id_to_operand_map.find(input_operand_id);
    ASSERT_TRUE(input_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(input_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kInput);
    EXPECT_EQ(input_operand_iter->value->data_type, expected.data_type);
    EXPECT_EQ(input_operand_iter->value->dimensions, input.dimensions);
    EXPECT_EQ(input_operand_iter->value->name, "input");
    EXPECT_EQ(prelu->input_operand_id, input_operand_id);

    // Verify the slope operand.
    auto slope_operand_id = graph_info->input_operands[1];
    auto slope_operand_iter =
        graph_info->id_to_operand_map.find(slope_operand_id);
    ASSERT_TRUE(slope_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(slope_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kInput);
    EXPECT_EQ(slope_operand_iter->value->data_type, expected.data_type);
    EXPECT_EQ(slope_operand_iter->value->dimensions, slope.dimensions);
    EXPECT_EQ(slope_operand_iter->value->name, "slope");
    EXPECT_EQ(prelu->slope_operand_id, slope_operand_id);

    // Verify the output operand.
    ASSERT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kOutput);
    EXPECT_EQ(output_operand_iter->value->data_type, expected.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions, expected.dimensions);
    EXPECT_EQ(output_operand_iter->value->name, "output");
    EXPECT_EQ(prelu->output_operand_id, output_operand_id);
  }
};

TEST_P(MLGraphTestMojo, PreluTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      webnn::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_NE(builder, nullptr);
  {
    // Test prelu operator when input shape is the same as slope shape.
    PreluTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 3, 5}},
        .slope = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 3, 5}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {2, 3, 5}}}
        .Test(*this, scope, builder);
  }
  {
    // Test prelu operator with input shape as {2, 3, 5} and slope shape as {3,
    // 5}.
    PreluTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {2, 3, 5}},
        .slope = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {3, 5}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat16,
                     .dimensions = {2, 3, 5}}}
        .Test(*this, scope, builder);
  }
  {
    // Test prelu operator with input shape as {2, 3, 5} and slope shape as {5}.
    PreluTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {2, 3, 5}},
        .slope = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {5}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat16,
                     .dimensions = {2, 3, 5}}}
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
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
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
    EXPECT_EQ(input_operand_iter->value->data_type, expected.data_type);
    EXPECT_EQ(input_operand_iter->value->dimensions, input.dimensions);
    EXPECT_EQ(input_operand_iter->value->name, "input");
    // Verify the output `mojo::Operand`.
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kOutput);
    EXPECT_EQ(output_operand_iter->value->data_type, expected.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions, expected.dimensions);
    EXPECT_EQ(output_operand_iter->value->name, "output");
    // Verify the `mojo::Operator`.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_EQ(operation->is_relu(), true);
    auto& relu = operation->get_relu();
    EXPECT_EQ(relu->input_operand_id, input_operand_id);
    EXPECT_EQ(relu->output_operand_id, output_operand_id);
  }
};

TEST_P(MLGraphTestMojo, ReluTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      webnn::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_NE(builder, nullptr);
  {
    // Test relu operator for 0-D scalar.
    ReluTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {}}}
        .Test(*this, scope, builder);
  }
  {
    // Test relu operator for 1-D tensor.
    ReluTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {2}}}
        .Test(*this, scope, builder);
  }
  {
    // Test relu operator for 2-D tensor.
    ReluTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {3, 7}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat16,
                     .dimensions = {3, 7}}}
        .Test(*this, scope, builder);
  }
  {
    // Test relu operator for 3-D tensor.
    ReluTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt32,
                         .dimensions = {1, 5, 3}},
               .expected = {.data_type = blink_mojom::Operand::DataType::kInt32,
                            .dimensions = {1, 5, 3}}}
        .Test(*this, scope, builder);
  }
  {
    // Test relu operator for 4-D tensor.
    ReluTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint8,
                         .dimensions = {1, 2, 2, 1}},
               .expected = {.data_type = blink_mojom::Operand::DataType::kUint8,
                            .dimensions = {1, 2, 2, 1}}}
        .Test(*this, scope, builder);
  }
}

struct Resample2dTester {
  OperandInfoBlink input;
  struct Resample2dOptions {
    absl::optional<blink::V8MLInterpolationMode::Enum> mode;
    absl::optional<Vector<float>> scales;
    absl::optional<Vector<uint32_t>> sizes;
    absl::optional<Vector<uint32_t>> axes;
  };
  Resample2dOptions options;
  OperandInfoMojo expected_operand;
  blink_mojom::Resample2d::InterpolationMode expected_mode =
      blink_mojom::Resample2d::InterpolationMode::kNearestNeighbor;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    MLResample2dOptions* ml_resample2d_options = MLResample2dOptions::Create();
    if (options.mode) {
      ml_resample2d_options->setMode(options.mode.value());
    }
    if (options.scales) {
      ml_resample2d_options->setScales(options.scales.value());
    }
    if (options.sizes) {
      ml_resample2d_options->setSizes(options.sizes.value());
    }
    if (options.axes) {
      ml_resample2d_options->setAxes(options.axes.value());
    }
    auto* output_operand =
        BuildResample2d(scope, builder, input_operand, ml_resample2d_options);
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_NE(graph, nullptr);

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    ASSERT_EQ(graph_info->output_operands.size(), 1u);
    ASSERT_EQ(graph_info->input_operands.size(), 1u);
    ASSERT_EQ(graph_info->id_to_operand_map.size(), 2u);
    ASSERT_EQ(graph_info->constant_id_to_buffer_map.size(), 0u);
    auto& operation = graph_info->operations[0];
    EXPECT_EQ(operation->is_resample2d(), true);
    auto& resample2d_mojo = operation->get_resample2d();
    // Validate the mode.
    EXPECT_EQ(resample2d_mojo->mode, expected_mode);
    // Validate the output.
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->data_type,
              expected_operand.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions,
              expected_operand.dimensions);
  }
};

TEST_P(MLGraphTestMojo, Resample2dTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      webnn::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  {
    Resample2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 3, 4, 4}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 3, 4, 4}}}
        .Test(*this, scope, builder);
  }
  {  // Test resample2d with mode =
     // "blink::V8MLInterpolationMode::Enum::kLinear".
    Resample2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 2, 4}},
        .options = {.mode = blink::V8MLInterpolationMode::Enum::kLinear},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 2, 4}},
        .expected_mode = blink_mojom::Resample2d::InterpolationMode::kLinear}
        .Test(*this, scope, builder);
  }
  {
    // Test resample2d with scales = {2.0, 2.0}.
    Resample2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 2, 4}},
        .options = {.scales = Vector<float>{2.0, 2.0}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 4, 8}}}
        .Test(*this, scope, builder);
  }
  {
    // Test resample2d with scales = {0.5, 0.5}.
    Resample2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 2, 4}},
        .options = {.scales = Vector<float>{0.5, 0.5}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 1, 2}}}
        .Test(*this, scope, builder);
  }
  {
    // Test resample2d with sizes = {3, 6}.
    Resample2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 2, 4}},
        .options = {.sizes = Vector<uint32_t>{3, 6}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 3, 6}}}
        .Test(*this, scope, builder);
  }
  {
    // Test resample2d with sizes = {3, 6} and scales = {0.5, 0.5} which should
    // be ignored.
    Resample2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 2, 4}},
        .options = {.scales = Vector<float>{0.5, 0.5},
                    .sizes = Vector<uint32_t>{3, 6}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 3, 6}}}
        .Test(*this, scope, builder);
  }
  {
    // Test resample2d with scales = {1.0, 2.0} and axes = {0, 1}.
    Resample2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 2, 4}},
        .options = {.scales = Vector<float>{1.0, 2.0},
                    .axes = Vector<uint32_t>{0, 1}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 2, 2, 4}}}
        .Test(*this, scope, builder);
  }
  {
    // Test resample2d with scales = {1.0, 2.0} and axes = {1, 2}.
    Resample2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 2, 4}},
        .options = {.scales = Vector<float>{1.0, 2.0},
                    .axes = Vector<uint32_t>{1, 2}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 4, 4}}}
        .Test(*this, scope, builder);
  }
  {
    // Test resample2d with scales = {1.0, 2.0} and axes = {2, 3}.
    Resample2dTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 1, 2, 4}},
        .options = {.scales = Vector<float>{1.0, 2.0},
                    .axes = Vector<uint32_t>{2, 3}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 2, 8}}}
        .Test(*this, scope, builder);
  }
}

struct ReshapeTester {
  OperandInfoBlink input;
  Vector<uint32_t> new_shape;
  OperandInfoMojo expected;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* output_operand =
        builder->reshape(input_operand, new_shape, scope.GetExceptionState());
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_NE(graph, nullptr);

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_EQ(operation->is_reshape(), true);
    EXPECT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->data_type, expected.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions, expected.dimensions);
  }
};

TEST_P(MLGraphTestMojo, ReshapeTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      webnn::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_NE(builder, nullptr);
  {
    // Test reshaping 1-D tensor to 0-D scalar.
    ReshapeTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1}},
        .new_shape = {},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {}}}
        .Test(*this, scope, builder);
  }
  {
    // Test reshaping 0-D scalar to 1-D tensor.
    ReshapeTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {}},
        .new_shape = {1},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {1}}}
        .Test(*this, scope, builder);
  }
  {
    // Test reshaping 2-D tensor to 1-D tensor.
    ReshapeTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 2}},
        .new_shape = {4},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {4}}}
        .Test(*this, scope, builder);
  }
  {
    // Test reshaping from 4-D tensor to 2-D tensor.
    ReshapeTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kInt32,
                  .dimensions = {1, 2, 2, 1}},
        .new_shape = {1, 4},
        .expected = {.data_type = blink_mojom::Operand::DataType::kInt32,
                     .dimensions = {1, 4}}}
        .Test(*this, scope, builder);
  }
}

enum class FloatingPointUnaryKind { kSigmoid, kTanh };

struct FloatingPointUnaryTester {
  OperandInfoBlink input;
  OperandInfoMojo expected;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    Test(helper, scope, builder, FloatingPointUnaryKind::kSigmoid);
    Test(helper, scope, builder, FloatingPointUnaryKind::kTanh);
  }

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder,
            FloatingPointUnaryKind kind) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    MLOperand* output_operand = nullptr;
    switch (kind) {
      case FloatingPointUnaryKind::kSigmoid:
        output_operand =
            builder->sigmoid(input_operand, scope.GetExceptionState());
        break;
      case FloatingPointUnaryKind::kTanh:
        output_operand =
            builder->tanh(input_operand, scope.GetExceptionState());
        break;
    }
    ASSERT_NE(output_operand, nullptr);
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_NE(graph, nullptr);

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    EXPECT_EQ(graph_info->id_to_operand_map.size(), 2u);
    ASSERT_EQ(graph_info->input_operands.size(), 1u);
    ASSERT_EQ(graph_info->output_operands.size(), 1u);
    ASSERT_EQ(graph_info->operations.size(), 1u);

    // Verify the input `mojo::Operand`.
    auto input_operand_id = graph_info->input_operands[0];
    auto input_operand_iter =
        graph_info->id_to_operand_map.find(input_operand_id);
    ASSERT_TRUE(input_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(input_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kInput);
    EXPECT_EQ(input_operand_iter->value->data_type, expected.data_type);
    EXPECT_EQ(input_operand_iter->value->dimensions, input.dimensions);
    EXPECT_EQ(input_operand_iter->value->name, "input");

    // Verify the output `mojo::Operand`.
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kOutput);
    EXPECT_EQ(output_operand_iter->value->data_type, expected.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions, expected.dimensions);
    EXPECT_EQ(output_operand_iter->value->name, "output");

    // Verify the `mojo::Operator`.
    auto& operation = graph_info->operations[0];
    switch (kind) {
      case FloatingPointUnaryKind::kSigmoid: {
        EXPECT_TRUE(operation->is_sigmoid());
        auto& unary = operation->get_sigmoid();
        EXPECT_EQ(unary->input_operand_id, input_operand_id);
        EXPECT_EQ(unary->output_operand_id, output_operand_id);
        break;
      }
      case FloatingPointUnaryKind::kTanh: {
        EXPECT_TRUE(operation->is_tanh());
        auto& unary = operation->get_tanh();
        EXPECT_EQ(unary->input_operand_id, input_operand_id);
        EXPECT_EQ(unary->output_operand_id, output_operand_id);
        break;
      }
    }
  }
};

TEST_P(MLGraphTestMojo, FloatingPointUnaryTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      webnn::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_NE(builder, nullptr);
  {
    // Test unary operator for 0-D scalar.
    FloatingPointUnaryTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {}}}
        .Test(*this, scope, builder);
  }
  {
    // Test unary operator for 1-D tensor.
    FloatingPointUnaryTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {2}}}
        .Test(*this, scope, builder);
  }
  {
    // Test unary operator for 2-D tensor.
    FloatingPointUnaryTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {3, 7}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat16,
                     .dimensions = {3, 7}}}
        .Test(*this, scope, builder);
  }
  {
    // Test unary operator for 3-D tensor.
    FloatingPointUnaryTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 5, 3}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {1, 5, 3}}}
        .Test(*this, scope, builder);
  }
  {
    // Test unary operator for 4-D tensor.
    FloatingPointUnaryTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 2, 1}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {1, 2, 2, 1}}}
        .Test(*this, scope, builder);
  }
}

struct SliceTester {
  struct SliceAttributes {
    Vector<uint32_t> starts;
    Vector<uint32_t> sizes;
  };
  OperandInfoBlink input;
  SliceAttributes options;
  OperandInfoMojo expected_operand;
  SliceAttributes expected_attributes;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* output_operand =
        builder->slice(input_operand, options.starts, options.sizes,
                       scope.GetExceptionState());
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_NE(graph, nullptr);

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_EQ(operation->is_slice(), true);
    auto& slice_mojo = operation->get_slice();

    for (uint32_t i = 0; i < slice_mojo->starts_and_sizes.size(); ++i) {
      EXPECT_EQ(slice_mojo->starts_and_sizes[i]->start,
                expected_attributes.starts[i]);
      EXPECT_EQ(slice_mojo->starts_and_sizes[i]->size,
                expected_attributes.sizes[i]);
    }

    EXPECT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->data_type,
              expected_operand.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions,
              expected_operand.dimensions);
  }
};

TEST_P(MLGraphTestMojo, SliceTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      webnn::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  {
    SliceTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {4, 4}},
        .options = {.starts = Vector<uint32_t>({0, 0}),
                    .sizes = Vector<uint32_t>({4, 4})},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {4, 4}},
        .expected_attributes = {.starts = {0, 0}, .sizes = {4, 4}}}
        .Test(*this, scope, builder);
    SliceTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 3, 4, 5}},
        .options = {.starts = Vector<uint32_t>({0, 1, 2, 3, 4}),
                    .sizes = Vector<uint32_t>({1, 1, 1, 1, 1})},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {1, 1, 1, 1, 1}},
        .expected_attributes = {.starts = {0, 1, 2, 3, 4},
                                .sizes = {1, 1, 1, 1, 1}}}
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
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* output_operand =
        builder->softmax(input_operand, scope.GetExceptionState());
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_NE(graph, nullptr);

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_EQ(operation->is_softmax(), true);
    EXPECT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->data_type, expected.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions, expected.dimensions);
  }
};

TEST_P(MLGraphTestMojo, SoftmaxTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      webnn::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_NE(builder, nullptr);
  {
    // Test building softmax with float32 input.
    SoftmaxTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 4}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {2, 4}}}
        .Test(*this, scope, builder);
  }
  {
    // Test building softmax with float16 input.
    SoftmaxTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {1, 5}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat16,
                     .dimensions = {1, 5}}}
        .Test(*this, scope, builder);
  }
}

struct TransposeTester {
  OperandInfoBlink input;
  absl::optional<Vector<uint32_t>> permutation;
  OperandInfoMojo expected_operand;
  Vector<uint32_t> expected_permutation;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    MLTransposeOptions* options = MLTransposeOptions::Create();
    if (permutation.has_value()) {
      options->setPermutation(permutation.value());
    }
    auto* output_operand =
        builder->transpose(input_operand, options, scope.GetExceptionState());
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_NE(graph, nullptr);

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    ASSERT_TRUE(operation->is_transpose());
    auto& transpose = operation->get_transpose();

    // Validate the permutation of transpose operation.
    EXPECT_EQ(transpose->permutation, expected_permutation);

    // Validate the input operand.
    EXPECT_EQ(graph_info->input_operands.size(), 1u);
    auto input_operand_id = graph_info->input_operands[0];
    EXPECT_EQ(transpose->input_operand_id, input_operand_id);
    auto input_operand_iter =
        graph_info->id_to_operand_map.find(input_operand_id);
    ASSERT_TRUE(input_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(input_operand_iter->value->data_type, expected_operand.data_type);
    EXPECT_EQ(input_operand_iter->value->dimensions, input.dimensions);

    // Validate the output operand.
    EXPECT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    EXPECT_EQ(transpose->output_operand_id, output_operand_id);
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->data_type,
              expected_operand.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions,
              expected_operand.dimensions);
  }
};

TEST_P(MLGraphTestMojo, TransposeTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      webnn::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  {
    // Test transpose operator with default options.
    TransposeTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 3, 4}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {4, 3, 2, 1}},
        .expected_permutation = {3, 2, 1, 0}}
        .Test(*this, scope, builder);
  }
  {
    // Test transpose operator with a given permutation.
    TransposeTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {1, 2, 3, 4}},
        .permutation = Vector<uint32_t>{3, 0, 2, 1},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {4, 1, 3, 2}},
        .expected_permutation = {3, 0, 2, 1}}
        .Test(*this, scope, builder);
  }
}

struct WhereTester {
  OperandInfoBlink condition;
  OperandInfoBlink true_value;
  OperandInfoBlink false_value;
  OperandInfoMojo expected_operand;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* condition_operand =
        BuildInput(builder, "condition", condition.dimensions,
                   condition.data_type, scope.GetExceptionState());
    auto* true_value_operand =
        BuildInput(builder, "true_value", true_value.dimensions,
                   true_value.data_type, scope.GetExceptionState());
    auto* false_value_operand =
        BuildInput(builder, "false_value", false_value.dimensions,
                   false_value.data_type, scope.GetExceptionState());
    auto* output_operand =
        builder->where(condition_operand, true_value_operand,
                       false_value_operand, scope.GetExceptionState());
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_NE(graph, nullptr);

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    EXPECT_EQ(graph_info->id_to_operand_map.size(), 4u);
    EXPECT_EQ(graph_info->input_operands.size(), 3u);

    // Verify the condition `mojo::Operand`.
    auto condition_operand_id = graph_info->input_operands[0];
    auto condition_operand_iter =
        graph_info->id_to_operand_map.find(condition_operand_id);
    ASSERT_TRUE(condition_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(condition_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kInput);
    EXPECT_EQ(condition_operand_iter->value->data_type,
              blink_mojom::Operand::DataType::kUint8);
    EXPECT_EQ(condition_operand_iter->value->dimensions, condition.dimensions);
    EXPECT_EQ(condition_operand_iter->value->name, "condition");

    // Verify the true value `mojo::Operand`.
    auto true_value_operand_id = graph_info->input_operands[1];
    auto true_value_operand_iter =
        graph_info->id_to_operand_map.find(true_value_operand_id);
    ASSERT_TRUE(true_value_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(true_value_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kInput);
    EXPECT_EQ(true_value_operand_iter->value->data_type,
              expected_operand.data_type);
    EXPECT_EQ(true_value_operand_iter->value->dimensions,
              true_value.dimensions);
    EXPECT_EQ(true_value_operand_iter->value->name, "true_value");

    // Verify the false value `mojo::Operand`.
    auto false_value_operand_id = graph_info->input_operands[2];
    auto false_value_operand_iter =
        graph_info->id_to_operand_map.find(false_value_operand_id);
    ASSERT_TRUE(false_value_operand_iter !=
                graph_info->id_to_operand_map.end());
    EXPECT_EQ(false_value_operand_iter->value->kind,
              blink_mojom::Operand::Kind::kInput);
    EXPECT_EQ(false_value_operand_iter->value->data_type,
              expected_operand.data_type);
    EXPECT_EQ(false_value_operand_iter->value->dimensions,
              false_value.dimensions);
    EXPECT_EQ(false_value_operand_iter->value->name, "false_value");

    // Verify the output `mojo::Operand`.
    ASSERT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->data_type,
              expected_operand.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions,
              expected_operand.dimensions);
    EXPECT_EQ(output_operand_iter->value->name, "output");

    // Verify the `mojo::Operator`.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_EQ(operation->is_where(), true);
  }
};

TEST_P(MLGraphTestMojo, WhereTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      webnn::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_NE(builder, nullptr);
  {
    // Test building where with 2-D condition, 2-D true_value and 2-D
    // false_value using broadcast.
    WhereTester{
        .condition = {.data_type = V8MLOperandDataType::Enum::kUint8,
                      .dimensions = {2, 1}},
        .true_value = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                       .dimensions = {2, 4}},
        .false_value = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                        .dimensions = {2, 4}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {2, 4}}}
        .Test(*this, scope, builder);
  }
  {
    // Test building where with 2-D condition, 2-D true_value and 3-D
    // false_value using broadcast.
    WhereTester{
        .condition = {.data_type = V8MLOperandDataType::Enum::kUint8,
                      .dimensions = {1, 4}},
        .true_value = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                       .dimensions = {3, 4}},
        .false_value = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                        .dimensions = {2, 3, 4}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {2, 3, 4}}}
        .Test(*this, scope, builder);
  }
  {
    // Test building where with 3-D condition, 3-D true_value and 2-D
    // false_value using broadcast.
    WhereTester{
        .condition = {.data_type = V8MLOperandDataType::Enum::kUint8,
                      .dimensions = {2, 1, 4}},
        .true_value = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                       .dimensions = {2, 3, 4}},
        .false_value = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                        .dimensions = {1, 4}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {2, 3, 4}}}
        .Test(*this, scope, builder);
  }
  {
    // Test building where with 4-D condition, 3-D true_value and 2-D
    // false_value using broadcast.
    WhereTester{
        .condition = {.data_type = V8MLOperandDataType::Enum::kUint8,
                      .dimensions = {2, 3, 4, 5}},
        .true_value = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                       .dimensions = {3, 4, 5}},
        .false_value = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                        .dimensions = {4, 5}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {2, 3, 4, 5}}}
        .Test(*this, scope, builder);
  }
}

struct ReduceTester {
  OperandInfoBlink input;
  absl::optional<Vector<uint32_t>> axes;
  absl::optional<bool> keep_dimensions;
  OperandInfoMojo expected_operand;
  Vector<uint32_t> expected_axes;
  bool expected_keep_dimensions;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    Test(helper, scope, builder, ReduceKind::kL1);
    Test(helper, scope, builder, ReduceKind::kL2);
    Test(helper, scope, builder, ReduceKind::kLogSum);
    Test(helper, scope, builder, ReduceKind::kLogSumExp);
    Test(helper, scope, builder, ReduceKind::kMax);
    Test(helper, scope, builder, ReduceKind::kMean);
    Test(helper, scope, builder, ReduceKind::kMin);
    Test(helper, scope, builder, ReduceKind::kProduct);
    Test(helper, scope, builder, ReduceKind::kSum);
    Test(helper, scope, builder, ReduceKind::kSumSquare);
  }

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder,
            ReduceKind kind) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    MLReduceOptions* options = MLReduceOptions::Create();
    if (axes.has_value()) {
      options->setAxes(axes.value());
    }
    if (keep_dimensions.has_value()) {
      options->setKeepDimensions(keep_dimensions.value());
    }
    auto* output_operand =
        BuildReduce(scope, builder, kind, input_operand, options);
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_NE(graph, nullptr);

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    ASSERT_TRUE(operation->is_reduce());
    auto& reduce = operation->get_reduce();

    blink_mojom::Reduce::Kind reduce_kind;
    switch (kind) {
      case ReduceKind::kL1:
        reduce_kind = blink_mojom::Reduce::Kind::kL1;
        break;
      case ReduceKind::kL2:
        reduce_kind = blink_mojom::Reduce::Kind::kL2;
        break;
      case ReduceKind::kLogSum:
        reduce_kind = blink_mojom::Reduce::Kind::kLogSum;
        break;
      case ReduceKind::kLogSumExp:
        reduce_kind = blink_mojom::Reduce::Kind::kLogSumExp;
        break;
      case ReduceKind::kMax:
        reduce_kind = blink_mojom::Reduce::Kind::kMax;
        break;
      case ReduceKind::kMean:
        reduce_kind = blink_mojom::Reduce::Kind::kMean;
        break;
      case ReduceKind::kMin:
        reduce_kind = blink_mojom::Reduce::Kind::kMin;
        break;
      case ReduceKind::kProduct:
        reduce_kind = blink_mojom::Reduce::Kind::kProduct;
        break;
      case ReduceKind::kSum:
        reduce_kind = blink_mojom::Reduce::Kind::kSum;
        break;
      case ReduceKind::kSumSquare:
        reduce_kind = blink_mojom::Reduce::Kind::kSumSquare;
        break;
    }
    EXPECT_EQ(reduce->kind, reduce_kind);
    // Validate the axes of reduce operation.
    EXPECT_EQ(reduce->axes, expected_axes);
    // Validate the keep_dimensions of reduce operation.
    EXPECT_EQ(reduce->keep_dimensions, expected_keep_dimensions);

    // Validate the input operand.
    EXPECT_EQ(graph_info->input_operands.size(), 1u);
    auto input_operand_id = graph_info->input_operands[0];
    EXPECT_EQ(reduce->input_operand_id, input_operand_id);
    auto input_operand_iter =
        graph_info->id_to_operand_map.find(input_operand_id);
    ASSERT_TRUE(input_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(input_operand_iter->value->data_type, expected_operand.data_type);
    EXPECT_EQ(input_operand_iter->value->dimensions, input.dimensions);

    // Validate the output operand.
    EXPECT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    EXPECT_EQ(reduce->output_operand_id, output_operand_id);
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->data_type,
              expected_operand.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions,
              expected_operand.dimensions);
  }
};

TEST_P(MLGraphTestMojo, ReduceTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      webnn::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_NE(builder, nullptr);
  {
    // Test reduce operator with default options.
    ReduceTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {1, 2, 3, 4}},
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {}},
        .expected_axes = {0, 1, 2, 3},
        .expected_keep_dimensions = false}
        .Test(*this, scope, builder);
  }
  {
    // Test reduce operator with a given axes and keep_dimensions.
    ReduceTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {1, 2, 3, 4}},
        .axes = Vector<uint32_t>{1},
        .keep_dimensions = true,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {1, 1, 3, 4}},
        .expected_axes = {1},
        .expected_keep_dimensions = true}
        .Test(*this, scope, builder);
  }
}
template <typename T>
struct ConstantTester {
  OperandInfo<T> constant;
  OperandInfoMojo expected;
  Vector<T> expected_constant_data;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* constant_operand =
        BuildConstant(builder, constant.dimensions, constant.data_type,
                      constant.values, scope.GetExceptionState());
    auto* output_operand =
        builder->relu(constant_operand, scope.GetExceptionState());
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_NE(graph, nullptr);

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    EXPECT_EQ(graph_info->id_to_operand_map.size(), 2u);
    EXPECT_EQ(graph_info->constant_id_to_buffer_map.size(), 1u);
    // Verify the constant `mojo::Operand`.
    for (auto& [constant_id, constant_buffer] :
         graph_info->constant_id_to_buffer_map) {
      auto constant_operand_iter =
          graph_info->id_to_operand_map.find(constant_id);
      ASSERT_TRUE(constant_operand_iter != graph_info->id_to_operand_map.end());
      EXPECT_EQ(constant_operand_iter->value->kind,
                blink_mojom::Operand::Kind::kConstant);
      EXPECT_EQ(constant_operand_iter->value->data_type, expected.data_type);
      EXPECT_EQ(constant_operand_iter->value->dimensions, expected.dimensions);
      EXPECT_EQ(constant_operand_iter->value->name.empty(), true);
      // Verify the constant data in the mojo.
      const wtf_size_t constant_size =
          base::checked_cast<wtf_size_t>(constant_buffer.size() / sizeof(T));
      Vector<T> constant_data(constant_size);
      memcpy(constant_data.data(), constant_buffer.data(),
             constant_buffer.size());
      EXPECT_EQ(expected_constant_data, constant_data);
    }
  }
};

TEST_P(MLGraphTestMojo, ConstantTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      webnn::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_NE(builder, nullptr);
  {
    // Test scalar constant operand.
    ConstantTester<float>{
        .constant = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {},
                     .values = {1.0}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {}},
        .expected_constant_data = {1.0}}
        .Test(*this, scope, builder);
  }
  {
    // Test Constant operand for Float32 data type.
    ConstantTester<float>{
        .constant = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                     .dimensions = {2, 3},
                     .values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat32,
                     .dimensions = {2, 3}},
        .expected_constant_data = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0}}
        .Test(*this, scope, builder);
  }
  {
    // Test Constant operand for Float16 data type.
    ConstantTester<uint16_t>{
        .constant = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                     .dimensions = {2, 3},
                     .values = {1, 2, 3, 4, 5, 6}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kFloat16,
                     .dimensions = {2, 3}},
        .expected_constant_data = {1, 2, 3, 4, 5, 6}}
        .Test(*this, scope, builder);
  }
  {
    // Test Constant operand for Int32 data type.
    ConstantTester<int32_t>{
        .constant = {.data_type = V8MLOperandDataType::Enum::kInt32,
                     .dimensions = {2, 3},
                     .values = {1, 2, 3, 4, 5, 6}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kInt32,
                     .dimensions = {2, 3}},
        .expected_constant_data = {1, 2, 3, 4, 5, 6}}
        .Test(*this, scope, builder);
  }
  {
    // Test Constant operand for UInt32 data type.
    ConstantTester<uint32_t>{
        .constant = {.data_type = V8MLOperandDataType::Enum::kUint32,
                     .dimensions = {2, 3},
                     .values = {1, 2, 3, 4, 5, 6}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kUint32,
                     .dimensions = {2, 3}},
        .expected_constant_data = {1, 2, 3, 4, 5, 6}}
        .Test(*this, scope, builder);
  }
  {
    // Test Constant operand for Int8 data type.
    ConstantTester<int8_t>{
        .constant = {.data_type = V8MLOperandDataType::Enum::kInt8,
                     .dimensions = {2, 3},
                     .values = {1, 2, 3, 4, 5, 6}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kInt8,
                     .dimensions = {2, 3}},
        .expected_constant_data = {1, 2, 3, 4, 5, 6}}
        .Test(*this, scope, builder);
  }
  {
    // Test Constant operand for UInt8 data type.
    ConstantTester<uint8_t>{
        .constant = {.data_type = V8MLOperandDataType::Enum::kUint8,
                     .dimensions = {2, 3},
                     .values = {1, 2, 3, 4, 5, 6}},
        .expected = {.data_type = blink_mojom::Operand::DataType::kUint8,
                     .dimensions = {2, 3}},
        .expected_constant_data = {1, 2, 3, 4, 5, 6}}
        .Test(*this, scope, builder);
  }
}

struct SplitTester {
  OperandInfoBlink input;
  absl::variant<uint32_t, Vector<uint32_t>> splits;
  absl::optional<uint32_t> axis;
  Vector<OperandInfoMojo> expected;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* attributes = MLSplitOptions::Create();
    if (axis.has_value()) {
      attributes->setAxis(axis.value());
    }
    HeapVector<Member<MLOperand>> output_operands;
    if (absl::holds_alternative<uint32_t>(splits)) {
      output_operands.assign(
          builder->split(input_operand, absl::get<uint32_t>(splits), attributes,
                         scope.GetExceptionState()));
    } else if (absl::holds_alternative<Vector<uint32_t>>(splits)) {
      output_operands.assign(
          builder->split(input_operand, absl::get<Vector<uint32_t>>(splits),
                         attributes, scope.GetExceptionState()));
    }
    MLNamedOperands output_named_operand;
    for (wtf_size_t i = 0; i < output_operands.size(); ++i) {
      output_named_operand.push_back(
          std::make_pair(String::Format("output%u", i), output_operands.at(i)));
    }
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, output_named_operand);
    ASSERT_NE(graph, nullptr);

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_TRUE(operation->is_split());
    EXPECT_EQ(graph_info->output_operands.size(), expected.size());
    for (wtf_size_t i = 0; i < expected.size(); ++i) {
      auto output_operand_id = graph_info->output_operands[i];
      auto output_operand_iter =
          graph_info->id_to_operand_map.find(output_operand_id);
      ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
      EXPECT_EQ(output_operand_iter->value->data_type, expected[i].data_type);
      EXPECT_EQ(output_operand_iter->value->dimensions, expected[i].dimensions);
    }
  }
};

TEST_P(MLGraphTestMojo, SplitTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      webnn::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  using v8 = V8MLOperandDataType::Enum;
  using blink = blink_mojom::Operand::DataType;
  {
    SplitTester{
        .input = {.data_type = v8::kFloat32, .dimensions = {2, 2}},
        .splits = 2u,
        .expected = {{.data_type = blink::kFloat32, .dimensions = {1, 2}},
                     {.data_type = blink::kFloat32, .dimensions = {1, 2}}}}
        .Test(*this, scope, builder);
  }
  {
    SplitTester{
        .input = {.data_type = v8::kFloat32, .dimensions = {2, 2}},
        .splits = 2u,
        .axis = 1,
        .expected = {{.data_type = blink::kFloat32, .dimensions = {2, 1}},
                     {.data_type = blink::kFloat32, .dimensions = {2, 1}}}}
        .Test(*this, scope, builder);
  }
  {
    SplitTester{
        .input = {.data_type = v8::kFloat32, .dimensions = {6, 2}},
        .splits = Vector<uint32_t>{1, 2, 3},
        .expected = {{.data_type = blink::kFloat32, .dimensions = {1, 2}},
                     {.data_type = blink::kFloat32, .dimensions = {2, 2}},
                     {.data_type = blink::kFloat32, .dimensions = {3, 2}}}}
        .Test(*this, scope, builder);
  }
}

struct CastTester {
  OperandInfoBlink input;
  V8MLOperandDataType::Enum output_data_type;
  OperandInfoMojo expected_operand;

  void Test(MLGraphTestMojo& helper,
            V8TestingScope& scope,
            MLGraphBuilder* builder) {
    // Build the graph.
    auto* input_operand =
        BuildInput(builder, "input", input.dimensions, input.data_type,
                   scope.GetExceptionState());
    auto* output_operand =
        builder->cast(input_operand, V8MLOperandDataType(output_data_type),
                      scope.GetExceptionState());
    auto [graph, build_exception] =
        helper.BuildGraph(scope, builder, {{"output", output_operand}});
    ASSERT_NE(graph, nullptr);

    auto graph_info = helper.GetGraphInfo();
    // Verify the graph information of mojo are as expected.
    ASSERT_EQ(graph_info->operations.size(), 1u);
    auto& operation = graph_info->operations[0];
    EXPECT_EQ(operation->is_element_wise_unary(), true);
    webnn::mojom::blink::ElementWiseUnaryPtr& element_wise_unary =
        operation->get_element_wise_unary();
    EXPECT_EQ(element_wise_unary->kind,
              blink_mojom::ElementWiseUnary::Kind::kCast);
    EXPECT_EQ(graph_info->output_operands.size(), 1u);
    auto output_operand_id = graph_info->output_operands[0];
    auto output_operand_iter =
        graph_info->id_to_operand_map.find(output_operand_id);
    ASSERT_TRUE(output_operand_iter != graph_info->id_to_operand_map.end());
    EXPECT_EQ(output_operand_iter->value->data_type,
              expected_operand.data_type);
    EXPECT_EQ(output_operand_iter->value->dimensions,
              expected_operand.dimensions);
  }
};

TEST_P(MLGraphTestMojo, CastTester) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      webnn::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  {
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kInt32,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kInt32,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kFloat16,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kUint32,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kUint32,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kInt8,
        .expected_operand = {.data_type = blink_mojom::Operand::DataType::kInt8,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat32,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kUint8,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kUint8,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kFloat32,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kInt32,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kInt32,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kUint32,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kUint32,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kInt8,
        .expected_operand = {.data_type = blink_mojom::Operand::DataType::kInt8,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kFloat16,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kUint8,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kUint8,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kInt32,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kFloat32,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kInt32,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kFloat16,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt32,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kUint32,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kUint32,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kInt32,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kInt8,
        .expected_operand = {.data_type = blink_mojom::Operand::DataType::kInt8,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt32,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kUint8,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kUint8,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kUint32,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kFloat32,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kUint32,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kFloat16,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint32,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kInt32,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kInt32,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kUint32,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kInt8,
        .expected_operand = {.data_type = blink_mojom::Operand::DataType::kInt8,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint32,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kUint8,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kUint8,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kInt8,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kFloat32,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kInt8,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kFloat16,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt8,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kUint32,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kUint32,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt8,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kUint8,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kUint8,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kInt8,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kInt32,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kInt32,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kUint8,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kFloat32,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat32,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kUint8,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kFloat16,
        .expected_operand = {.data_type =
                                 blink_mojom::Operand::DataType::kFloat16,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint8,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kInt32,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kInt32,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{
        .input = {.data_type = V8MLOperandDataType::Enum::kUint8,
                  .dimensions = {2, 2}},
        .output_data_type = V8MLOperandDataType::Enum::kInt8,
        .expected_operand = {.data_type = blink_mojom::Operand::DataType::kInt8,
                             .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
    CastTester{.input = {.data_type = V8MLOperandDataType::Enum::kUint8,
                         .dimensions = {2, 2}},
               .output_data_type = V8MLOperandDataType::Enum::kInt32,
               .expected_operand = {.data_type =
                                        blink_mojom::Operand::DataType::kInt32,
                                    .dimensions = {2, 2}}}
        .Test(*this, scope, builder);
  }
}

TEST_P(MLGraphTestMojo, WebNNGraphComputeTest) {
  V8TestingScope scope;
  // Bind fake WebNN Context in the service for testing.
  ScopedWebNNServiceBinder scoped_setup_binder(*this, scope);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      webnn::features::kEnableMachineLearningNeuralNetworkService);
  auto* options = MLContextOptions::Create();
  // Create WebNN Context with GPU device type.
  options->setDeviceType(V8MLDeviceType::Enum::kGpu);
  auto* builder = CreateGraphBuilder(scope, options);
  ASSERT_NE(builder, nullptr);
  const Vector<uint32_t> dimensions = {3, 5};
  const wtf_size_t number_of_elements = base::checked_cast<wtf_size_t>(
      webnn::ValidateAndCalculateElementsNumber(dimensions).value());

  // Build the graph.
  auto* lhs_operand =
      BuildInput(builder, "lhs", dimensions, V8MLOperandDataType::Enum::kUint8,
                 scope.GetExceptionState());
  auto* rhs_operand =
      BuildInput(builder, "rhs", dimensions, V8MLOperandDataType::Enum::kUint8,
                 scope.GetExceptionState());
  auto* output_operand = BuildElementWiseBinary(
      scope, builder, ElementWiseBinaryKind::kAdd, lhs_operand, rhs_operand);
  auto [graph, build_exception] =
      BuildGraph(scope, builder, {{"output", output_operand}});
  ASSERT_NE(graph, nullptr);

  MLNamedArrayBufferViews inputs(
      {{"lhs", CreateArrayBufferViewForOperand(lhs_operand)},
       {"rhs", CreateArrayBufferViewForOperand(rhs_operand)}});
  MLNamedArrayBufferViews outputs(
      {{"output", CreateArrayBufferViewForOperand(output_operand)}});

  {
    // Compute successfully.
    SetComputeResult(ComputeResult{
        .result = blink_mojom::ComputeResult::kOk,
        .output = {{"output", Vector<uint8_t>(number_of_elements, 2)}}});
    auto* compute_exception = ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    auto results = GetArrayBufferViewValues<uint8_t>(outputs[0].second);
    EXPECT_EQ(results, Vector<uint8_t>(number_of_elements, 2));

    // Compute again successfully.
    SetComputeResult(ComputeResult{
        .result = blink_mojom::ComputeResult::kOk,
        .output = {{"output", Vector<uint8_t>(number_of_elements, 7)}}});
    compute_exception = ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_EQ(compute_exception, nullptr);
    results = GetArrayBufferViewValues<uint8_t>(outputs[0].second);
    EXPECT_EQ(results, Vector<uint8_t>(number_of_elements, 7));

    // Validate the input array buffers.
    auto& name_to_buffer_map = GetInputArrayBuffers();
    auto lhs_input_iter = name_to_buffer_map.find("lhs");
    EXPECT_NE(lhs_input_iter, name_to_buffer_map.end());
    EXPECT_EQ(lhs_input_iter->value.size(), number_of_elements);
    auto rhs_input_iter = name_to_buffer_map.find("rhs");
    EXPECT_NE(rhs_input_iter, name_to_buffer_map.end());
    EXPECT_EQ(rhs_input_iter->value.size(), number_of_elements);
  }
  {
    // Unknown error.
    SetComputeResult(
        ComputeResult{.result = blink_mojom::ComputeResult::kUnknownError});
    auto* compute_exception = ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_NE(compute_exception, nullptr);
    EXPECT_EQ(compute_exception->name(), "OperationError");
    EXPECT_EQ(compute_exception->message(),
              "Failed to obtain the computation result.");
  }
  {
    // Reset the inputs which are detached in above failed tests.
    inputs[0].second = CreateArrayBufferViewForOperand(lhs_operand);
    inputs[1].second = CreateArrayBufferViewForOperand(rhs_operand);
    outputs[0].second = CreateArrayBufferViewForOperand(output_operand);
    // Output name in computation result isn't expected.
    SetComputeResult(
        ComputeResult{.result = blink_mojom::ComputeResult::kOk,
                      .output = {{"a_different_out_name",
                                  Vector<uint8_t>(number_of_elements)}}});
    auto* compute_exception = ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_NE(compute_exception, nullptr);
    EXPECT_EQ(compute_exception->name(), "OperationError");
    EXPECT_EQ(
        compute_exception->message(),
        "There is an unknown output tensor in the computation result: output");
  }
  {
    // Reset the inputs which are detached in above failed tests.
    inputs[0].second = CreateArrayBufferViewForOperand(lhs_operand);
    inputs[1].second = CreateArrayBufferViewForOperand(rhs_operand);
    outputs[0].second = CreateArrayBufferViewForOperand(output_operand);
    // The size of output in computation result isn't expected.
    SetComputeResult(
        ComputeResult{.result = blink_mojom::ComputeResult::kOk,
                      .output = {{"output", Vector<uint8_t>(20)}}});
    auto* compute_exception = ComputeGraph(scope, graph, inputs, outputs);
    EXPECT_NE(compute_exception, nullptr);
    EXPECT_EQ(compute_exception->name(), "UnknownError");
    EXPECT_EQ(
        compute_exception->message(),
        "The output tensor size does not match graph's expectation: output");
  }
}

const TestVariety kGraphMojoTestVariety[] = {
    {BackendType::kWebNNService, ExecutionMode::kAsync},
};

INSTANTIATE_TEST_SUITE_P(All,
                         MLGraphTestMojo,
                         testing::ValuesIn(kGraphMojoTestVariety),
                         TestVarietyToString);

}  // namespace blink
