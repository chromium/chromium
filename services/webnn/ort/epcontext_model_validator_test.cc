// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/epcontext_model_validator.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/webnn/ort/epcontext_model_validator_test_support.h"
#include "services/webnn/public/cpp/execution_providers_info.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_model_loader.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace webnn::ort {
namespace {

constexpr std::string_view kHistogramName = "WebNN.ORT.EpContextModelResult";

// The validator only checks the ONNX graph shape: exactly one node, and that
// node is an EPContext (com.microsoft) node. The node's blob is left to the
// execution provider to validate, so its contents are irrelevant here.

TEST(WebNNEpContextModelValidatorTest, AcceptsEpContextNodeWithBlob) {
  // The ep_cache_context attribute is skipped, not inspected, so its bytes are
  // arbitrary; the node is accepted regardless.
  const Bytes blob = {1, 2, 3, 4};
  EXPECT_EQ(ValidateEpContextModel(BuildModel({BuildEpContextNode(blob)})),
            EpContextModelResult::kAccepted);
}

TEST(WebNNEpContextModelValidatorTest, AcceptsEpContextNodeWithoutBlob) {
  EXPECT_EQ(ValidateEpContextModel(BuildModel({BuildEpContextNode()})),
            EpContextModelResult::kAccepted);
}

TEST(WebNNEpContextModelValidatorTest, RejectsEmptyGraph) {
  EXPECT_EQ(ValidateEpContextModel(BuildModel({})),
            EpContextModelResult::kWrongNodeCount);
}

TEST(WebNNEpContextModelValidatorTest, RejectsMultipleEpContextNodes) {
  EXPECT_EQ(ValidateEpContextModel(
                BuildModel({BuildEpContextNode(), BuildEpContextNode()})),
            EpContextModelResult::kWrongNodeCount);
}

TEST(WebNNEpContextModelValidatorTest, RejectsExtraNonEpContextNode) {
  // A single stray node alongside the EPContext node is rejected: the graph
  // must contain the EPContext node and nothing else.
  EXPECT_EQ(ValidateEpContextModel(BuildModel(
                {BuildEpContextNode(), BuildNode("Add", "ai.onnx")})),
            EpContextModelResult::kNonEpContextNode);
}

TEST(WebNNEpContextModelValidatorTest, RejectsSingleNonEpContextNode) {
  EXPECT_EQ(ValidateEpContextModel(BuildModel({BuildNode("Add", "ai.onnx")})),
            EpContextModelResult::kNonEpContextNode);
}

TEST(WebNNEpContextModelValidatorTest, RejectsWrongDomain) {
  // op_type is EPContext but the domain is not com.microsoft.
  EXPECT_EQ(
      ValidateEpContextModel(BuildModel({BuildNode("EPContext", "ai.onnx")})),
      EpContextModelResult::kNonEpContextNode);
}

TEST(WebNNEpContextModelValidatorTest, RejectsContribOpInMicrosoftDomain) {
  // A com.microsoft contrib op (right domain, wrong op_type) is rejected: the
  // node must be EPContext, not merely in the com.microsoft domain.
  EXPECT_EQ(ValidateEpContextModel(
                BuildModel({BuildNode("MatMulNBits", "com.microsoft")})),
            EpContextModelResult::kNonEpContextNode);
}

TEST(WebNNEpContextModelValidatorTest, RejectsDuplicateGraph) {
  const Bytes graph = BuildGraph({BuildEpContextNode()});
  EXPECT_EQ(ValidateEpContextModel(BuildModelFromGraphs({graph, graph})),
            EpContextModelResult::kDuplicateGraph);
}

TEST(WebNNEpContextModelValidatorTest, RejectsMissingGraph) {
  // A ModelProto carrying only `ir_version` (field 1, varint).
  const Bytes model = {0x08, 0x0A};
  EXPECT_EQ(ValidateEpContextModel(model), EpContextModelResult::kNoGraph);
}

TEST(WebNNEpContextModelValidatorTest, RejectsEmptyInput) {
  EXPECT_EQ(ValidateEpContextModel(Bytes{}), EpContextModelResult::kNoGraph);
}

TEST(WebNNEpContextModelValidatorTest, RejectsLengthBeyondRemainingBytes) {
  // A `graph` field (field 7, wire type 2) declaring 100 bytes of payload but
  // carrying only 2.
  const Bytes model = {0x3A, 100, 0x0A, 0x00};
  EXPECT_EQ(ValidateEpContextModel(model), EpContextModelResult::kMalformed);
}

TEST(WebNNEpContextModelValidatorTest, RejectsTruncatedModel) {
  Bytes model = BuildModel({BuildEpContextNode()});
  model.resize(model.size() / 2);  // Truncate mid-field.
  EXPECT_EQ(ValidateEpContextModel(model), EpContextModelResult::kMalformed);
}

// Loads compiled models the way the GPU process does, so tests can observe the
// pipe and the reply.
class FakeModelLoader : public mojom::WebNNModelLoader {
 public:
  FakeModelLoader(std::string_view ep_name,
                  mojo::PendingReceiver<mojom::WebNNModelLoader> receiver)
      : ep_name_(ep_name), receiver_(this, std::move(receiver)) {}

  void LoadCompiledGraph(mojom::CompiledGraphPtr compiled_graph,
                         LoadCompiledGraphCallback callback) override {
    if (!PrepareCompiledModel(compiled_graph->compiled_model_data, ep_name_,
                              receiver_.GetBadMessageCallback())) {
      return;
    }
    std::move(callback).Run(mojom::LoadedGraphInfo::New(
        blink::WebNNGraphToken(), std::vector<mojom::Device>()));
  }

 private:
  std::string_view ep_name_;
  mojo::Receiver<mojom::WebNNModelLoader> receiver_;
};

class WebNNCompiledModelLoadingTest : public testing::Test {
 protected:
  // Sends `model` to a FakeModelLoader for `ep_name` and returns whether the
  // load succeeded, or std::nullopt if no reply arrived because the pipe was
  // closed.
  std::optional<bool> Load(std::string_view ep_name, const Bytes& model) {
    mojo::Remote<mojom::WebNNModelLoader> remote;
    FakeModelLoader loader(ep_name, remote.BindNewPipeAndPassReceiver());

    auto compiled_graph = mojom::CompiledGraph::New();
    compiled_graph->compiled_model_data =
        mojo_base::BigBuffer(base::span<const uint8_t>(model));

    std::optional<bool> succeeded;
    remote->LoadCompiledGraph(
        std::move(compiled_graph),
        base::BindLambdaForTesting(
            [&](base::expected<mojom::LoadedGraphInfoPtr, mojom::ErrorPtr>
                    result) { succeeded = result.has_value(); }));
    EXPECT_TRUE(base::test::RunUntil(
        [&] { return succeeded.has_value() || !remote.is_connected(); }));
    return succeeded;
  }

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
};

TEST_F(WebNNCompiledModelLoadingTest, AcceptsEpContextModel) {
  EXPECT_EQ(
      Load(kOpenVINOExecutionProvider, BuildModel({BuildEpContextNode()})),
      true);
  histogram_tester_.ExpectUniqueSample(kHistogramName,
                                       EpContextModelResult::kAccepted, 1);
}

TEST_F(WebNNCompiledModelLoadingTest, RejectsInvalidEpContextModel) {
  mojo::test::BadMessageObserver bad_message_observer;

  // No reply arrives because the pipe is closed, so the load fails.
  EXPECT_EQ(
      Load(kOpenVINOExecutionProvider,
           BuildModel({BuildEpContextNode(), BuildNode("Add", "ai.onnx")})),
      std::nullopt);
  EXPECT_THAT(bad_message_observer.WaitForBadMessage(),
              testing::StartsWith("Rejected compiled model: "));
  histogram_tester_.ExpectUniqueSample(
      kHistogramName, EpContextModelResult::kNonEpContextNode, 1);
}

TEST_F(WebNNCompiledModelLoadingTest, SkipsValidationForFullGraphEp) {
  // An EP that compiles into an ordinary ONNX model has no EPContext invariant
  // to check, so the model is loaded as produced.
  EXPECT_EQ(
      Load(kWebGpuExecutionProvider, BuildModel({BuildNode("Add", "ai.onnx")})),
      true);
  histogram_tester_.ExpectTotalCount(kHistogramName, 0);
}

}  // namespace
}  // namespace webnn::ort
