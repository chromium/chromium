// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/graph_impl.h"

#include "base/ranges/algorithm.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "services/webnn/error.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/tflite/graph_builder.h"
#include "services/webnn/tflite/op_resolver.h"
#include "services/webnn/webnn_graph_impl.h"
#include "third_party/tflite/src/tensorflow/lite/interpreter_builder.h"
#include "third_party/tflite/src/tensorflow/lite/stderr_reporter.h"

#if BUILDFLAG(WEBNN_ENABLE_TFLITE_PROFILER)
#include "third_party/tflite/src/tensorflow/lite/profiling/profile_summarizer.h"
#endif

namespace webnn::tflite {

namespace {

std::string_view TfLiteStatusToString(TfLiteStatus status) {
  switch (status) {
    case kTfLiteOk:
      return "ok";
    case kTfLiteError:
      return "error";
    case kTfLiteDelegateError:
      return "delegate error";
    case kTfLiteApplicationError:
      return "application error";
    case kTfLiteDelegateDataNotFound:
      return "delegate data not found";
    case kTfLiteDelegateDataWriteError:
      return "delegate data write error";
    case kTfLiteDelegateDataReadError:
      return "delegate data read error";
    case kTfLiteUnresolvedOps:
      return "unresolved ops";
    case kTfLiteCancelled:
      return "cancelled";
  }
}

}  // namespace

#if BUILDFLAG(WEBNN_ENABLE_TFLITE_PROFILER)
ScopedTfLiteProfiler::ScopedTfLiteProfiler(::tflite::Interpreter& interpreter)
    : interpreter_(interpreter) {
  // `profiler_` must be a `std::unique_ptr` because this object is movable and
  // `&profiler_` may change after construction.
  profiler_ = std::make_unique<::tflite::profiling::BufferedProfiler>(
      /*max_num_entries=*/1024);
  interpreter_->SetProfiler(profiler_.get());
}

ScopedTfLiteProfiler::~ScopedTfLiteProfiler() {
  // `profiler_` is nullptr if this object was moved.
  if (profiler_) {
    ::tflite::profiling::ProfileSummarizer profile_summarizer;
    auto profile_events = profiler_->GetProfileEvents();
    profile_summarizer.ProcessProfiles(profile_events, *interpreter_);
    LOG(INFO) << profile_summarizer.GetOutputString();
    interpreter_->SetProfiler(nullptr);
  }
}

void ScopedTfLiteProfiler::Start() {
  profiler_->StartProfiling();
}

void ScopedTfLiteProfiler::Stop() {
  profiler_->StopProfiling();
}
#else
ScopedTfLiteProfiler::ScopedTfLiteProfiler(::tflite::Interpreter&) {}
ScopedTfLiteProfiler::~ScopedTfLiteProfiler() = default;

void ScopedTfLiteProfiler::Start() {}
void ScopedTfLiteProfiler::Stop() {}
#endif

ScopedTfLiteProfiler::ScopedTfLiteProfiler(ScopedTfLiteProfiler&& other) =
    default;

ScopedTfLiteProfiler& ScopedTfLiteProfiler::operator=(
    ScopedTfLiteProfiler&& other) = default;

// static
void GraphImpl::CreateAndBuild(
    mojom::GraphInfoPtr graph_info,
    mojom::WebNNContext::CreateGraphCallback callback) {
  base::expected<flatbuffers::DetachedBuffer, std::string> conversion_result =
      GraphBuilder::CreateAndBuild(*graph_info);
  if (!conversion_result.has_value()) {
    std::move(callback).Run(ToError<mojom::CreateGraphResult>(
        mojom::Error::Code::kNotSupportedError, conversion_result.error()));
    return;
  }
  flatbuffers::DetachedBuffer model_content =
      std::move(conversion_result).value();

  std::unique_ptr<::tflite::FlatBufferModel> model =
      ::tflite::FlatBufferModel::BuildFromBuffer(
          reinterpret_cast<const char*>(model_content.data()),
          model_content.size(), ::tflite::DefaultErrorReporter());
  if (!model) {
    std::move(callback).Run(ToError<mojom::CreateGraphResult>(
        mojom::Error::Code::kUnknownError, "Unable to build flatbuffer model"));
    return;
  }

  OpResolver op_resolver;
  std::unique_ptr<::tflite::Interpreter> interpreter;
  TfLiteStatus status =
      ::tflite::InterpreterBuilder(*model, op_resolver)(&interpreter);
  if (status != kTfLiteOk) {
    std::move(callback).Run(ToError<mojom::CreateGraphResult>(
        mojom::Error::Code::kUnknownError,
        base::StrCat({"Unable to build TFLite intepreter: ",
                      TfLiteStatusToString(status)})));
    return;
  }

  // The profiler (if enabled) must be initialized before tensors are allocated.
  ScopedTfLiteProfiler profiler(*interpreter);

  status = interpreter->AllocateTensors();
  if (status != kTfLiteOk) {
    std::move(callback).Run(ToError<mojom::CreateGraphResult>(
        mojom::Error::Code::kUnknownError,
        base::StrCat(
            {"Unable to allocate tensors: ", TfLiteStatusToString(status)})));
    return;
  }

  mojo::PendingAssociatedRemote<mojom::WebNNGraph> graph;
  mojo::MakeSelfOwnedAssociatedReceiver<mojom::WebNNGraph>(
      base::WrapUnique(new GraphImpl(
          ComputeResourceInfo(graph_info), std::move(model_content),
          std::move(model), std::move(interpreter), std::move(profiler))),
      graph.InitWithNewEndpointAndPassReceiver());
  std::move(callback).Run(
      mojom::CreateGraphResult::NewGraphRemote(std::move(graph)));
}

GraphImpl::~GraphImpl() = default;

GraphImpl::GraphImpl(ComputeResourceInfo compute_resource_info,
                     flatbuffers::DetachedBuffer model_content,
                     std::unique_ptr<::tflite::FlatBufferModel> model,
                     std::unique_ptr<::tflite::Interpreter> interpreter,
                     ScopedTfLiteProfiler profiler)
    : WebNNGraphImpl(std::move(compute_resource_info)),
      model_content_(std::move(model_content)),
      model_(std::move(model)),
      interpreter_(std::move(interpreter)),
      profiler_(std::move(profiler)) {}

void GraphImpl::ComputeImpl(
    base::flat_map<std::string, mojo_base::BigBuffer> named_inputs,
    mojom::WebNNGraph::ComputeCallback callback) {
  for (int tensor_idx : interpreter_->inputs()) {
    TfLiteTensor* tensor = interpreter_->tensor(tensor_idx);
    auto it = named_inputs.find(tensor->name);
    // The caller guarantees that all expected tensors have been provided.
    CHECK(it != named_inputs.end());
    std::ranges::copy(base::make_span(it->second), tensor->data.raw);
  }

  profiler_.Start();
  TfLiteStatus status = interpreter_->Invoke();
  profiler_.Stop();
  if (status != kTfLiteOk) {
    std::move(callback).Run(ToError<mojom::ComputeResult>(
        mojom::Error::Code::kUnknownError,
        base::StrCat({"Failed to compute: ", TfLiteStatusToString(status)})));
    return;
  }

  std::vector<std::pair<std::string, mojo_base::BigBuffer>> named_outputs;
  named_outputs.reserve(interpreter_->outputs().size());
  for (int tensor_idx : interpreter_->outputs()) {
    TfLiteTensor* tensor = interpreter_->tensor(tensor_idx);
    auto tensor_span = base::make_span(tensor->data.raw, tensor->bytes);
    named_outputs.emplace_back(
        tensor->name, mojo_base::BigBuffer(base::as_bytes(tensor_span)));
  }

  std::move(callback).Run(
      mojom::ComputeResult::NewNamedOutputs(std::move(named_outputs)));
}

}  // namespace webnn::tflite
