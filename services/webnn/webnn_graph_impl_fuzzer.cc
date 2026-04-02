// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <limits>
#include <memory>
#include <numeric>
#include <ranges>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/test/allow_check_is_test_for_testing.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/test_future.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/types/expected_macros.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/cpp/graph_validation_utils.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/webnn_types.h"
#include "services/webnn/public/mojom/features.mojom-features.h"
#include "services/webnn/public/mojom/webnn_context.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_context_provider_impl.h"
#include "services/webnn/webnn_test_environment.h"
#include "services/webnn/webnn_test_utils.h"
#include "services/webnn/webnn_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"
#include "third_party/fuzztest/src/fuzztest/googletest_fixture_adapter.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif  // BUILDFLAG(IS_MAC)

namespace webnn::test {

namespace {

#define ASSIGN_OR_RETURN_VOID(lhs, rexpr) \
  ASSIGN_OR_RETURN(lhs, rexpr, [](std::string error) { return; });

// Registers a fuzz test for all three device types (CPU, GPU, NPU).
// The variadic args carry the .WithDomains()/.WithSeeds() chain.
#define WEBNN_FUZZ_TEST_F(func, ...)                       \
  FUZZ_TEST_F(WebNNGraphImplFuzzer_CPU, func) __VA_ARGS__; \
  FUZZ_TEST_F(WebNNGraphImplFuzzer_GPU, func) __VA_ARGS__; \
  FUZZ_TEST_F(WebNNGraphImplFuzzer_NPU, func) __VA_ARGS__

auto AnyOperandDataType() {
  return fuzztest::ElementOf<OperandDataType>(
      {OperandDataType::kFloat32, OperandDataType::kFloat16,
       OperandDataType::kInt32, OperandDataType::kUint32,
       OperandDataType::kInt64, OperandDataType::kUint64,
       OperandDataType::kInt8, OperandDataType::kUint8, OperandDataType::kUint4,
       OperandDataType::kInt4});
}

struct BuildConv2dAttributes {
  std::vector<uint32_t> padding;
  std::vector<uint32_t> strides;
  std::vector<uint32_t> dilations;
  uint32_t groups;
};

struct BuildPool2dAttributes {
  std::vector<uint32_t> window_dimensions;
  std::vector<uint32_t> padding;
  std::vector<uint32_t> strides;
  std::vector<uint32_t> dilations;
};

struct Conv2dParams {
  OperandDataType data_type;
  mojom::Conv2d::Kind conv2d_kind;
  uint32_t batch;
  uint32_t input_channels;
  uint32_t input_height;
  uint32_t input_width;
  uint32_t output_channels;
  uint32_t filter_height;
  uint32_t filter_width;
  uint32_t beginning_pad_height;
  uint32_t beginning_pad_width;
  uint32_t ending_pad_height;
  uint32_t ending_pad_width;
  uint32_t stride_height;
  uint32_t stride_width;
  uint32_t dilation_height;
  uint32_t dilation_width;
  uint32_t output_padding_height;
  uint32_t output_padding_width;
  uint32_t groups;
  bool is_input_constant;
  bool is_filter_constant;
  bool is_bias_constant;
};

struct Pool2dParams {
  OperandDataType data_type;
  mojom::Pool2d::Kind pool2d_kind;
  RoundingType rounding_type;
  uint32_t batch;
  uint32_t channels;
  uint32_t input_height;
  uint32_t input_width;
  uint32_t window_height;
  uint32_t window_width;
  uint32_t beginning_pad_height;
  uint32_t beginning_pad_width;
  uint32_t ending_pad_height;
  uint32_t ending_pad_width;
  uint32_t stride_height;
  uint32_t stride_width;
  uint32_t dilation_height;
  uint32_t dilation_width;
  bool is_input_constant;
};

auto AnyConv2dKind() {
  return fuzztest::ElementOf<mojom::Conv2d::Kind>(
      {mojom::Conv2d::Kind::kDirect, mojom::Conv2d::Kind::kTransposed});
}

auto AnyPool2dKind() {
  return fuzztest::ElementOf<mojom::Pool2d::Kind>(
      {mojom::Pool2d::Kind::kMaxPool2d, mojom::Pool2d::Kind::kAveragePool2d,
       mojom::Pool2d::Kind::kL2Pool2d});
}

auto AnyRoundingType() {
  return fuzztest::ElementOf<RoundingType>(
      {RoundingType::kFloor, RoundingType::kCeil});
}

// Use fuzztest::OneOf to split the range into multiple sub-domains each with
// equal probability, biasing generation toward small values that are more
// likely to pass validation while keeping the large values to cover edge cases.
auto AnyDimSize() {
  return fuzztest::OneOf(
      fuzztest::InRange<uint32_t>(1, std::numeric_limits<int8_t>::max()),
      fuzztest::InRange<uint32_t>(1, std::numeric_limits<int16_t>::max()),
      fuzztest::InRange<uint32_t>(1, std::numeric_limits<int32_t>::max()),
      fuzztest::ElementOf<uint32_t>({1, 2, 3,
                                     std::numeric_limits<int16_t>::max() - 1,
                                     std::numeric_limits<int16_t>::max(),
                                     std::numeric_limits<uint16_t>::max() - 1,
                                     std::numeric_limits<uint16_t>::max(),
                                     std::numeric_limits<int32_t>::max() - 1,
                                     std::numeric_limits<int32_t>::max()}));
}

auto AnyDimSizeOrZero() {
  return fuzztest::OneOf(
      fuzztest::InRange<uint32_t>(0, std::numeric_limits<int8_t>::max()),
      fuzztest::InRange<uint32_t>(0, std::numeric_limits<int16_t>::max()),
      fuzztest::InRange<uint32_t>(0, std::numeric_limits<int32_t>::max()),
      fuzztest::ElementOf<uint32_t>({0, 1, 2, 3,
                                     std::numeric_limits<int16_t>::max() - 1,
                                     std::numeric_limits<int16_t>::max(),
                                     std::numeric_limits<uint16_t>::max() - 1,
                                     std::numeric_limits<uint16_t>::max(),
                                     std::numeric_limits<int32_t>::max() - 1,
                                     std::numeric_limits<int32_t>::max()}));
}

auto AnyConv2dParams() {
  return fuzztest::StructOf<Conv2dParams>(
      AnyOperandDataType(), AnyConv2dKind(),
      AnyDimSize(),                 // batch
      AnyDimSize(),                 // input_channels
      AnyDimSize(),                 // input_height
      AnyDimSize(),                 // input_width
      AnyDimSize(),                 // output_channels
      AnyDimSize(),                 // filter_height
      AnyDimSize(),                 // filter_width
      AnyDimSizeOrZero(),           // beginning_pad_height
      AnyDimSizeOrZero(),           // beginning_pad_width
      AnyDimSizeOrZero(),           // ending_pad_height
      AnyDimSizeOrZero(),           // ending_pad_width
      AnyDimSize(),                 // stride_height
      AnyDimSize(),                 // stride_width
      AnyDimSize(),                 // dilation_height
      AnyDimSize(),                 // dilation_width
      AnyDimSizeOrZero(),           // output_padding_height
      AnyDimSizeOrZero(),           // output_padding_width
      AnyDimSize(),                 // groups
      fuzztest::Arbitrary<bool>(),  // is_input_constant
      fuzztest::Arbitrary<bool>(),  // is_filter_constant
      fuzztest::Arbitrary<bool>()   // is_bias_constant
  );
}

auto AnyPool2dParams() {
  return fuzztest::StructOf<Pool2dParams>(
      AnyOperandDataType(), AnyPool2dKind(), AnyRoundingType(),
      AnyDimSize(),                // batch
      AnyDimSize(),                // channels
      AnyDimSize(),                // input_height
      AnyDimSize(),                // input_width
      AnyDimSize(),                // window_height
      AnyDimSize(),                // window_width
      AnyDimSizeOrZero(),          // beginning_pad_height
      AnyDimSizeOrZero(),          // beginning_pad_width
      AnyDimSizeOrZero(),          // ending_pad_height
      AnyDimSizeOrZero(),          // ending_pad_width
      AnyDimSize(),                // stride_height
      AnyDimSize(),                // stride_width
      AnyDimSize(),                // dilation_height
      AnyDimSize(),                // dilation_width
      fuzztest::Arbitrary<bool>()  // is_input_constant
  );
}

void PopulateConv2dAttributesBase(Conv2dAttributesBase& attributes,
                                  const Conv2dParams& params,
                                  InputOperandLayout input_layout,
                                  const OperandDescriptor& bias_desc) {
  attributes.padding.beginning = {params.beginning_pad_height,
                                  params.beginning_pad_width};
  attributes.padding.ending = {params.ending_pad_height,
                               params.ending_pad_width};
  attributes.strides = {params.stride_height, params.stride_width};
  attributes.dilations = {params.dilation_height, params.dilation_width};
  attributes.groups = params.groups;
  attributes.bias_operand = bias_desc;
  attributes.input_layout = input_layout;
}

void MaybeIncreaseTestTimeouts() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (!command_line->HasSwitch(switches::kTestLauncherTimeout)) {
    command_line->AppendSwitchUTF8(switches::kTestLauncherTimeout, "600000");
  }
  if (!command_line->HasSwitch(switches::kUiTestActionMaxTimeout)) {
    command_line->AppendSwitchUTF8(switches::kUiTestActionMaxTimeout, "300000");
  }
  if (!command_line->HasSwitch(switches::kUiTestActionTimeout)) {
    command_line->AppendSwitchUTF8(switches::kUiTestActionTimeout, "200000");
  }
}

class GlobalFuzzEnvironment {
 public:
  GlobalFuzzEnvironment() {
    base::test::AllowCheckIsTestForTesting();

    // On POSIX this initializes the command line with empty args. A custom main
    // function would be needed to forward command line args on Linux.
    base::CommandLine::Init(0, nullptr);

    // Increase the test timeouts since large fuzzed graphs may need more time
    // to compile and execute.
    MaybeIncreaseTestTimeouts();
    TestTimeouts::Initialize();

    mojo::core::Init();

    webnn_test_environment_ = std::make_unique<WebNNTestEnvironment>();

    // Also increase the runloop timeout.
    runloop_timeout_ = std::make_unique<base::test::ScopedRunLoopTimeout>(
        FROM_HERE, base::Minutes(10));
  }

  WebNNTestEnvironment& GetWebNNTestEnvironment() {
    return *webnn_test_environment_;
  }

 private:
  std::unique_ptr<WebNNTestEnvironment> webnn_test_environment_;
  std::unique_ptr<base::test::ScopedRunLoopTimeout> runloop_timeout_;
};

GlobalFuzzEnvironment& GetGlobalFuzzEnvironment() {
  static base::NoDestructor<GlobalFuzzEnvironment> instance;
  return *instance;
}

struct TensorRemoteAndHandle {
  mojo::AssociatedRemote<mojom::WebNNTensor> remote;
  blink::WebNNTensorToken handle;
};

TensorRemoteAndHandle CreateTensor(
    mojo::Remote<mojom::WebNNContext>& context_remote,
    mojom::TensorInfoPtr tensor_info) {
  mojo::AssociatedRemote<mojom::WebNNTensor> webnn_tensor_remote;

  base::test::TestFuture<mojom::CreateTensorResultPtr> create_tensor_future;
  context_remote->CreateTensor(std::move(tensor_info), mojo_base::BigBuffer(0),
                               create_tensor_future.GetCallback());
  mojom::CreateTensorResultPtr create_tensor_result =
      create_tensor_future.Take();
  CHECK(create_tensor_result->is_success());
  webnn_tensor_remote.Bind(
      std::move(create_tensor_result->get_success()->tensor_remote));

  return TensorRemoteAndHandle{
      .remote = std::move(webnn_tensor_remote),
      .handle = create_tensor_result->get_success()->tensor_handle};
}

TensorRemoteAndHandle CreateTensorWithValues(
    mojo::Remote<mojom::WebNNContext>& context_remote,
    mojom::TensorInfoPtr tensor_info,
    base::span<const uint8_t> data) {
  auto remote_and_handle = CreateTensor(context_remote, std::move(tensor_info));
  remote_and_handle.remote->WriteTensor(mojo_base::BigBuffer(data));
  return remote_and_handle;
}

void BuildAndCompute(
    mojo::Remote<mojom::WebNNContext>& context_remote,
    mojo::AssociatedRemote<mojom::WebNNGraphBuilder> graph_builder_remote,
    mojom::GraphInfoPtr graph_info,
    base::flat_map<std::string, base::span<const uint8_t>> named_inputs) {
  // Create input tensors.
  std::vector<std::pair<std::string, TensorRemoteAndHandle>>
      named_input_remotes_and_handles;
  named_input_remotes_and_handles.reserve(graph_info->input_operands.size());

  for (OperandId operand_id : graph_info->input_operands) {
    const mojom::Operand& operand =
        *graph_info->operands.at(operand_id.value());
    ASSERT_TRUE(operand.name.has_value());

    auto it = named_inputs.find(*operand.name);
    ASSERT_TRUE(it != named_inputs.end());

    auto tensor_info = mojom::TensorInfo::New(
        operand.descriptor, MLTensorUsage{MLTensorUsageFlags::kWrite});
    named_input_remotes_and_handles.emplace_back(
        *operand.name, CreateTensorWithValues(
                           context_remote, std::move(tensor_info), it->second));
  }

  // Create output tensors.
  std::vector<std::pair<std::string, TensorRemoteAndHandle>>
      named_output_remotes_and_handles;
  named_output_remotes_and_handles.reserve(graph_info->output_operands.size());

  for (OperandId operand_id : graph_info->output_operands) {
    const mojom::Operand& operand =
        *graph_info->operands.at(operand_id.value());
    ASSERT_TRUE(operand.name.has_value());

    auto tensor_info = mojom::TensorInfo::New(
        operand.descriptor, MLTensorUsage{MLTensorUsageFlags::kRead});
    named_output_remotes_and_handles.emplace_back(
        *operand.name, CreateTensor(context_remote, std::move(tensor_info)));
  }

  base::test::TestFuture<
      base::expected<mojom::CreateGraphSuccessPtr, mojom::ErrorPtr>>
      create_graph_future;

  graph_builder_remote->CreateGraph(std::move(graph_info),
                                    create_graph_future.GetCallback());
  auto create_graph_result = create_graph_future.Take();
  if (!create_graph_result.has_value()) {
    return;
  }

  mojo::AssociatedRemote<mojom::WebNNGraph> graph_remote;
  graph_remote.Bind(std::move(create_graph_result.value()->graph_remote));

  std::vector<std::pair<std::string, blink::WebNNTensorToken>>
      named_input_handles;
  named_input_handles.reserve(named_input_remotes_and_handles.size());
  std::ranges::transform(
      named_input_remotes_and_handles, std::back_inserter(named_input_handles),
      [](const auto& input) {
        return std::make_pair(input.first, input.second.handle);
      });

  std::vector<std::pair<std::string, blink::WebNNTensorToken>>
      named_output_handles;
  named_output_handles.reserve(named_output_remotes_and_handles.size());
  std::ranges::transform(
      named_output_remotes_and_handles,
      std::back_inserter(named_output_handles), [](const auto& output) {
        return std::make_pair(output.first, output.second.handle);
      });

  graph_remote->Dispatch(named_input_handles, named_output_handles);

  // Wait for the computation to complete.
  for (auto& output : named_output_remotes_and_handles) {
    base::test::TestFuture<mojom::ReadTensorResultPtr> read_tensor_future;
    output.second.remote->ReadTensor(read_tensor_future.GetCallback());
    EXPECT_TRUE(read_tensor_future.Wait());
  }

  graph_remote.reset();
  graph_builder_remote.reset();
}

}  // namespace

class WebNNGraphImplFuzzerBase : public testing::Test {
 public:
  WebNNGraphImplFuzzerBase()
      : scoped_feature_list_(
            webnn::mojom::features::kWebMachineLearningNeuralNetwork),
        context_properties_(GetContextPropertiesForTesting()) {}

  void SetUp() override;
  void TearDown() override;

  const ContextProperties& context_properties() const {
    return context_properties_;
  }

  mojo::AssociatedRemote<mojom::WebNNGraphBuilder> BindNewGraphBuilderRemote();

 protected:
  virtual mojom::Device GetDeviceType() const = 0;

  base::test::ScopedFeatureList scoped_feature_list_;

  ContextProperties context_properties_;

  mojo::Remote<mojom::WebNNContextProvider> provider_remote_;
  mojo::Remote<mojom::WebNNContext> context_;
};

void WebNNGraphImplFuzzerBase::SetUp() {
#if BUILDFLAG(IS_MAC)
  if (base::mac::MacOSVersion() < 14'00'00) {
    GTEST_SKIP() << "Skipping test because WebNN is not supported on Mac OS "
                 << base::mac::MacOSVersion();
  }
#endif  // BUILDFLAG(IS_MAC)

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().BindWebNNContextProvider(
      provider_remote_.BindNewPipeAndPassReceiver(), /*is_incognito=*/false);

  base::test::TestFuture<mojom::CreateContextResultPtr> create_context_future;
  provider_remote_->CreateWebNNContext(
      mojom::CreateContextOptions::New(
          GetDeviceType(),
          mojom::CreateContextOptions::PowerPreference::kDefault),
      create_context_future.GetCallback());
  mojom::CreateContextResultPtr create_context_result =
      create_context_future.Take();
  if (create_context_result->is_success()) {
    context_.Bind(
        std::move(create_context_result->get_success()->context_remote));
    context_properties_ =
        create_context_result->get_success()->context_properties;
  } else {
    GTEST_SKIP() << "Failed to create WebNN context: "
                 << create_context_result->get_error()->message;
  }
}

void WebNNGraphImplFuzzerBase::TearDown() {
  context_.reset();
  EXPECT_TRUE(base::test::RunUntil([&]() { return true; }));
  // Give WebNNContext a chance to run disconnect.
  provider_remote_.reset();
}

mojo::AssociatedRemote<mojom::WebNNGraphBuilder>
WebNNGraphImplFuzzerBase::BindNewGraphBuilderRemote() {
  mojo::AssociatedRemote<mojom::WebNNGraphBuilder> remote;
  context_->CreateGraphBuilder(remote.BindNewEndpointAndPassReceiver());
  return remote;
}

template <typename BaseFixture>
class WebNNGraphImplFuzzerImpl
    : public fuzztest::PerFuzzTestFixtureAdapter<BaseFixture> {
 public:
  void SingleOpConv2d(Conv2dParams params, uint8_t seed_for_data);
  void SingleOpPool2d(Pool2dParams params, uint8_t seed_for_data);
};

template <mojom::Device device_type>
class WebNNGraphImplFuzzerDevice : public WebNNGraphImplFuzzerBase {
 protected:
  mojom::Device GetDeviceType() const override { return device_type; }
};

class WebNNGraphImplFuzzer_CPU
    : public WebNNGraphImplFuzzerImpl<
          WebNNGraphImplFuzzerDevice<mojom::Device::kCpu>> {};

class WebNNGraphImplFuzzer_GPU
    : public WebNNGraphImplFuzzerImpl<
          WebNNGraphImplFuzzerDevice<mojom::Device::kGpu>> {};

class WebNNGraphImplFuzzer_NPU
    : public WebNNGraphImplFuzzerImpl<
          WebNNGraphImplFuzzerDevice<mojom::Device::kNpu>> {};

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::SingleOpConv2d(
    Conv2dParams params,
    uint8_t seed_for_data) {
  InputOperandLayout input_layout =
      this->context_properties().input_operand_layout;

  if (params.output_channels % params.groups != 0 ||
      (params.conv2d_kind == mojom::Conv2d::Kind::kDirect &&
       params.input_channels % params.groups != 0)) {
    params.groups = std::gcd(params.output_channels, params.input_channels);
  }

  bool is_depthwise = params.conv2d_kind == mojom::Conv2d::Kind::kDirect &&
                      IsDepthwiseConv2d(params.input_channels,
                                        params.output_channels, params.groups);

  std::vector<uint32_t> input_dims;
  std::vector<uint32_t> filter_dims;
  switch (input_layout) {
    case InputOperandLayout::kNhwc: {
      input_dims = {params.batch, params.input_height, params.input_width,
                    params.input_channels};
      if (params.conv2d_kind == mojom::Conv2d::Kind::kDirect) {
        if (is_depthwise) {
          filter_dims = {params.input_channels, params.filter_height,
                         params.filter_width, 1};
        } else {
          filter_dims = {params.output_channels, params.filter_height,
                         params.filter_width,
                         params.input_channels / params.groups};
        }
      } else {
        filter_dims = {params.output_channels / params.groups,
                       params.filter_height, params.filter_width,
                       params.input_channels};
      }
      break;
    }
    case InputOperandLayout::kNchw: {
      input_dims = {params.batch, params.input_channels, params.input_height,
                    params.input_width};
      if (params.conv2d_kind == mojom::Conv2d::Kind::kDirect) {
        filter_dims = {params.output_channels,
                       params.input_channels / params.groups,
                       params.filter_height, params.filter_width};
      } else {
        filter_dims = {params.input_channels,
                       params.output_channels / params.groups,
                       params.filter_height, params.filter_width};
      }
      break;
    }
  }

  ASSIGN_OR_RETURN_VOID(auto input_desc, OperandDescriptor::Create(
                                             this->context_properties(),
                                             params.data_type, input_dims, ""));
  ASSIGN_OR_RETURN_VOID(
      auto filter_desc,
      OperandDescriptor::Create(this->context_properties(), params.data_type,
                                filter_dims, ""));
  ASSIGN_OR_RETURN_VOID(
      auto bias_desc,
      OperandDescriptor::Create(this->context_properties(), params.data_type,
                                {params.output_channels}, ""));

  std::optional<OperandDescriptor> output_desc;
  switch (params.conv2d_kind) {
    case mojom::Conv2d::Kind::kDirect: {
      Conv2dAttributes attr;
      PopulateConv2dAttributesBase(attr, params, input_layout, bias_desc);
      switch (input_layout) {
        case InputOperandLayout::kNhwc:
          if (is_depthwise) {
            attr.filter_layout = Conv2dFilterOperandLayout::kIhwo;
          } else {
            attr.filter_layout = Conv2dFilterOperandLayout::kOhwi;
          }
          break;
        case InputOperandLayout::kNchw:
          attr.filter_layout = Conv2dFilterOperandLayout::kOihw;
          break;
      }

      auto output_desc_result = ValidateConv2dAndInferOutput(
          this->context_properties(), input_desc, filter_desc, attr);
      if (!output_desc_result.has_value()) {
        return;
      }
      output_desc = output_desc_result.value();
      break;
    }
    case mojom::Conv2d::Kind::kTransposed: {
      ConvTranspose2dAttributes attr;
      PopulateConv2dAttributesBase(attr, params, input_layout, bias_desc);
      attr.filter_layout = input_layout == InputOperandLayout::kNhwc
                               ? ConvTranspose2dFilterOperandLayout::kOhwi
                               : ConvTranspose2dFilterOperandLayout::kIohw;
      attr.output_padding = {params.output_padding_height,
                             params.output_padding_width};
      auto output_desc_result = ValidateConvTranspose2dAndInferOutput(
          this->context_properties(), input_desc, filter_desc, attr);
      if (!output_desc_result.has_value()) {
        return;
      }
      output_desc = output_desc_result.value();
      break;
    }
  }

  mojo::AssociatedRemote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  OperandId input_id;
  OperandId filter_id;
  OperandId bias_id;
  std::vector<uint8_t> input_data(input_desc.PackedByteLength(), seed_for_data);
  std::vector<uint8_t> filter_data(filter_desc.PackedByteLength(),
                                   seed_for_data);
  std::vector<uint8_t> bias_data(bias_desc.PackedByteLength(), seed_for_data);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  if (params.is_input_constant) {
    input_id = builder.BuildConstant(input_desc.shape(), input_desc.data_type(),
                                     base::as_byte_span(input_data));
  } else {
    input_id =
        builder.BuildInput("input", input_desc.shape(), input_desc.data_type());
    named_inputs.insert({"input", input_data});
  }
  if (params.is_filter_constant) {
    filter_id =
        builder.BuildConstant(filter_desc.shape(), filter_desc.data_type(),
                              base::as_byte_span(filter_data));
  } else {
    filter_id = builder.BuildInput("filter", filter_desc.shape(),
                                   filter_desc.data_type());
    named_inputs.insert({"filter", filter_data});
  }
  if (params.is_bias_constant) {
    bias_id = builder.BuildConstant(bias_desc.shape(), bias_desc.data_type(),
                                    base::as_byte_span(bias_data));
  } else {
    bias_id =
        builder.BuildInput("bias", bias_desc.shape(), bias_desc.data_type());
    named_inputs.insert({"bias", bias_data});
  }

  OperandId output_id = builder.BuildOutput("output", output_desc->shape(),
                                            output_desc->data_type());

  BuildConv2dAttributes conv2d_attr;
  conv2d_attr.padding = {params.beginning_pad_height, params.ending_pad_height,
                         params.beginning_pad_width, params.ending_pad_width};
  conv2d_attr.strides = {params.stride_height, params.stride_width};
  conv2d_attr.dilations = {params.dilation_height, params.dilation_width};
  conv2d_attr.groups = params.groups;
  builder.BuildConv2d(params.conv2d_kind, input_id, filter_id, output_id,
                      conv2d_attr, bias_id);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

template <typename BaseFixture>
void WebNNGraphImplFuzzerImpl<BaseFixture>::SingleOpPool2d(
    Pool2dParams params,
    uint8_t seed_for_data) {
  InputOperandLayout input_layout =
      this->context_properties().input_operand_layout;

  std::vector<uint32_t> input_dims;
  switch (input_layout) {
    case InputOperandLayout::kNchw: {
      input_dims = {params.batch, params.channels, params.input_height,
                    params.input_width};
      break;
    }
    case InputOperandLayout::kNhwc: {
      input_dims = {params.batch, params.input_height, params.input_width,
                    params.channels};
      break;
    }
  }

  ASSIGN_OR_RETURN_VOID(auto input_desc, OperandDescriptor::Create(
                                             this->context_properties(),
                                             params.data_type, input_dims, ""));

  Pool2dAttributes attr;
  attr.window_dimensions = Size2d<uint32_t>{.height = params.window_height,
                                            .width = params.window_width};
  attr.padding.beginning = {params.beginning_pad_height,
                            params.beginning_pad_width};
  attr.padding.ending = {params.ending_pad_height, params.ending_pad_width};
  attr.strides = {params.stride_height, params.stride_width};
  attr.dilations = {params.dilation_height, params.dilation_width};
  attr.layout = input_layout;
  attr.rounding_type = params.rounding_type;

  auto output_desc_result =
      ValidatePool2dAndInferOutput(this->context_properties(), input_desc, attr,
                                   FromMojoPool2dType(params.pool2d_kind));
  if (!output_desc_result.has_value()) {
    return;
  }
  auto& output_desc = output_desc_result.value();

  mojo::AssociatedRemote<mojom::WebNNGraphBuilder> remote =
      this->BindNewGraphBuilderRemote();
  GraphInfoBuilder builder(remote);

  OperandId input_id;
  std::vector<uint8_t> input_data(input_desc.PackedByteLength(), seed_for_data);

  base::flat_map<std::string, base::span<const uint8_t>> named_inputs;
  if (params.is_input_constant) {
    input_id = builder.BuildConstant(input_desc.shape(), input_desc.data_type(),
                                     base::as_byte_span(input_data));
  } else {
    input_id =
        builder.BuildInput("input", input_desc.shape(), input_desc.data_type());
    named_inputs.insert({"input", input_data});
  }

  OperandId output_id = builder.BuildOutput("output", output_desc.shape(),
                                            output_desc.data_type());

  BuildPool2dAttributes pool2d_attr;
  pool2d_attr.window_dimensions = {params.window_height, params.window_width};
  pool2d_attr.padding = {params.beginning_pad_height, params.ending_pad_height,
                         params.beginning_pad_width, params.ending_pad_width};
  pool2d_attr.strides = {params.stride_height, params.stride_width};
  pool2d_attr.dilations = {params.dilation_height, params.dilation_width};
  builder.BuildPool2d(params.pool2d_kind, input_id, output_id, pool2d_attr);

  if (!builder.IsValidGraphForTesting(this->context_properties())) {
    return;
  }
  BuildAndCompute(this->context_, std::move(remote), builder.TakeGraphInfo(),
                  std::move(named_inputs));

  GetGlobalFuzzEnvironment().GetWebNNTestEnvironment().RunUntilIdle();
}

WEBNN_FUZZ_TEST_F(SingleOpConv2d,
                  .WithDomains(AnyConv2dParams(),
                               fuzztest::Arbitrary<uint8_t>())
                      .WithSeeds({{Conv2dParams{
                                       OperandDataType::kFloat16,
                                       mojom::Conv2d::Kind::kDirect,
                                       /*batch=*/1,
                                       /*input_channels=*/3,
                                       /*input_height=*/224,
                                       /*input_width=*/224,
                                       /*output_channels=*/64,
                                       /*filter_height=*/7,
                                       /*filter_width=*/7,
                                       /*beginning_pad_height=*/3,
                                       /*beginning_pad_width=*/3,
                                       /*ending_pad_height=*/3,
                                       /*ending_pad_width=*/3,
                                       /*stride_height=*/1,
                                       /*stride_width=*/1,
                                       /*dilation_height=*/1,
                                       /*dilation_width=*/1,
                                       /*output_padding_height=*/0,
                                       /*output_padding_width=*/0,
                                       /*groups=*/1,
                                       /*is_input_constant=*/false,
                                       /*is_filter_constant=*/true,
                                       /*is_bias_constant=*/true,
                                   },
                                   /*seed_for_data=*/1}}));

WEBNN_FUZZ_TEST_F(SingleOpPool2d,
                  .WithDomains(AnyPool2dParams(),
                               fuzztest::Arbitrary<uint8_t>())
                      .WithSeeds({{Pool2dParams{
                                       OperandDataType::kFloat32,
                                       mojom::Pool2d::Kind::kMaxPool2d,
                                       RoundingType::kFloor,
                                       /*batch=*/1,
                                       /*channels=*/3,
                                       /*input_height=*/4,
                                       /*input_width=*/4,
                                       /*window_height=*/2,
                                       /*window_width=*/2,
                                       /*beginning_pad_height=*/0,
                                       /*beginning_pad_width=*/0,
                                       /*ending_pad_height=*/0,
                                       /*ending_pad_width=*/0,
                                       /*stride_height=*/2,
                                       /*stride_width=*/2,
                                       /*dilation_height=*/1,
                                       /*dilation_width=*/1,
                                       /*is_input_constant=*/false,
                                   },
                                   /*seed_for_data=*/2}}));

}  // namespace webnn::test
