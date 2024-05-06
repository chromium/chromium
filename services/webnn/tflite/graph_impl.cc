// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/tflite/graph_impl.h"

#include "base/location.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected_macros.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "services/webnn/buildflags.h"
#include "services/webnn/error.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/tflite/context_impl.h"
#include "services/webnn/tflite/graph_builder.h"
#include "services/webnn/tflite/op_resolver.h"
#include "services/webnn/webnn_graph_impl.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
#include "third_party/tflite/src/tensorflow/lite/interpreter_builder.h"
#include "third_party/tflite/src/tensorflow/lite/stderr_reporter.h"

#if BUILDFLAG(WEBNN_ENABLE_TFLITE_PROFILER)
#include "third_party/tflite/src/tensorflow/lite/profiling/buffered_profiler.h"
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

base::span<uint8_t> SpanFromTensor(TfLiteTensor* tensor) {
  // SAFETY: TFLite guarantees that it has allocated enough memory to
  // store `tensor`.
  return UNSAFE_BUFFERS(
      base::span(static_cast<uint8_t*>(tensor->data.data), tensor->bytes));
}

}  // namespace

// Represents the thread-safe collection of graph resources which are shared
// among all interpreters. Since this class is reference counted it MUST be
// safe to destroy on any thread.
class GraphImpl::GraphResources
    : public base::RefCountedThreadSafe<GraphResources> {
 public:
  static base::expected<scoped_refptr<GraphResources>, mojom::ErrorPtr> Create(
      const mojom::GraphInfo& graph_info) {
    auto self = base::MakeRefCounted<GraphResources>();

    ASSIGN_OR_RETURN(
        self->model_content_, GraphBuilder::CreateAndBuild(graph_info),
        [](std::string error) {
          return mojom::Error::New(mojom::Error::Code::kNotSupportedError,
                                   std::move(error));
        });

    self->model_ = ::tflite::FlatBufferModel::BuildFromBuffer(
        reinterpret_cast<const char*>(self->model_content_.data()),
        self->model_content_.size(), ::tflite::DefaultErrorReporter());
    if (!self->model_) {
      return base::unexpected(
          mojom::Error::New(mojom::Error::Code::kUnknownError,
                            "Unable to build flatbuffer model"));
    }

    return self;
  }

  GraphResources() = default;

  const ::tflite::FlatBufferModel& model() { return *model_; }

 private:
  friend class base::RefCountedThreadSafe<GraphResources>;

  ~GraphResources() = default;

  // `model_` depends on `model_content_` outliving it.
  flatbuffers::DetachedBuffer model_content_;
  std::unique_ptr<::tflite::FlatBufferModel> model_;
};

// Represents the non-thread-safe collection of graph resources associated with
// a particular compute context (i.e. a TFLite interpreter).
class GraphImpl::ComputeResources {
 public:
  static base::expected<std::unique_ptr<ComputeResources>, mojom::ErrorPtr>
  Create(scoped_refptr<GraphResources> graph_resources, ContextImpl* context) {
    auto self = std::make_unique<ComputeResources>();

    int num_threads =
        context->options().thread_count_hint != 0
            ? static_cast<int>(context->options().thread_count_hint)
            : -1;  // Let the TFLite runtime decide.

    OpResolver op_resolver(context->options());
    ::tflite::InterpreterBuilder builder(graph_resources->model(), op_resolver);
    builder.SetNumThreads(num_threads);
    TfLiteStatus status = builder(&self->interpreter_);
    if (status != kTfLiteOk) {
      return base::unexpected(
          mojom::Error::New(mojom::Error::Code::kUnknownError,
                            base::StrCat({"Unable to build TFLite intepreter: ",
                                          TfLiteStatusToString(status)})));
    }

    // The profiler (if enabled) must be initialized before tensors are
    // allocated.
#if BUILDFLAG(WEBNN_ENABLE_TFLITE_PROFILER)
    self->interpreter_->SetProfiler(&self->profiler_);
#endif

    status = self->interpreter_->AllocateTensors();
    if (status != kTfLiteOk) {
      return base::unexpected(
          mojom::Error::New(mojom::Error::Code::kUnknownError,
                            base::StrCat({"Unable to allocate tensors: ",
                                          TfLiteStatusToString(status)})));
    }

    return self;
  }

  ComputeResources() = default;

  ~ComputeResources() {
#if BUILDFLAG(WEBNN_ENABLE_TFLITE_PROFILER)
    if (interpreter_) {
      ::tflite::profiling::ProfileSummarizer profile_summarizer;
      auto profile_events = profiler_.GetProfileEvents();
      profile_summarizer.ProcessProfiles(profile_events, *interpreter_);
      LOG(INFO) << profile_summarizer.GetOutputString();
      interpreter_->SetProfiler(nullptr);
    }
#endif
  }

  mojom::ComputeResultPtr InvokeInterpreter(NamedBuffers named_inputs) {
    for (int tensor_idx : interpreter_->inputs()) {
      TfLiteTensor* tensor = interpreter_->tensor(tensor_idx);
      auto it = named_inputs.find(tensor->name);
      // The caller guarantees that all expected tensors have been provided.
      CHECK(it != named_inputs.end());
      SpanFromTensor(tensor).copy_from(base::make_span(it->second));
    }

#if BUILDFLAG(WEBNN_ENABLE_TFLITE_PROFILER)
    profiler_.StartProfiling();
#endif
    TfLiteStatus status = interpreter_->Invoke();
#if BUILDFLAG(WEBNN_ENABLE_TFLITE_PROFILER)
    profiler_.StopProfiling();
#endif
    if (status != kTfLiteOk) {
      return ToError<mojom::ComputeResult>(
          mojom::Error::Code::kUnknownError,
          base::StrCat({"Failed to compute: ", TfLiteStatusToString(status)}));
    }

    std::vector<std::pair<std::string, mojo_base::BigBuffer>> named_outputs;
    named_outputs.reserve(interpreter_->outputs().size());
    for (int tensor_idx : interpreter_->outputs()) {
      TfLiteTensor* tensor = interpreter_->tensor(tensor_idx);
      named_outputs.emplace_back(tensor->name,
                                 mojo_base::BigBuffer(SpanFromTensor(tensor)));
    }

    return mojom::ComputeResult::NewNamedOutputs(std::move(named_outputs));
  }

 private:
  // `interpreter_` depends on the `FlatBufferModel` owned by `graph_resources_`
  // outliving it.
  scoped_refptr<GraphResources> graph_resources_;
  std::unique_ptr<::tflite::Interpreter> interpreter_;

#if BUILDFLAG(WEBNN_ENABLE_TFLITE_PROFILER)
  ::tflite::profiling::BufferedProfiler profiler_{/*max_num_entries=*/1024};
#endif
};

// static
base::expected<std::unique_ptr<GraphImpl>, mojom::ErrorPtr>
GraphImpl::CreateAndBuild(mojom::GraphInfoPtr graph_info,
                          ContextImpl* context) {
  ASSIGN_OR_RETURN(scoped_refptr<GraphResources> graph_resources,
                   GraphResources::Create(*graph_info));

  ASSIGN_OR_RETURN(std::unique_ptr<ComputeResources> compute_resources,
                   ComputeResources::Create(graph_resources, context));

  return base::WrapUnique(new GraphImpl(ComputeResourceInfo(graph_info),
                                        std::move(graph_resources),
                                        std::move(compute_resources), context));
}

GraphImpl::~GraphImpl() = default;

GraphImpl::GraphImpl(ComputeResourceInfo compute_resource_info,
                     scoped_refptr<GraphResources> graph_resources,
                     std::unique_ptr<ComputeResources> compute_resources,
                     ContextImpl* context)
    : WebNNGraphImpl(std::move(compute_resource_info)),
      context_(context),
      graph_resources_(std::move(graph_resources)),
      compute_resources_(std::move(compute_resources)) {}

void GraphImpl::ComputeImpl(NamedBuffers named_inputs,
                            ComputeCallback callback) {
  // Borrow `compute_resources_` for the current invocation, creating a new one
  // if necessary.
  auto compute_resources = std::move(compute_resources_);
  if (!compute_resources) {
    ASSIGN_OR_RETURN(compute_resources,
                     ComputeResources::Create(graph_resources_, context_),
                     [&callback](mojom::ErrorPtr error) {
                       std::move(callback).Run(
                           mojom::ComputeResult::NewError(std::move(error)));
                     });
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(
          [](NamedBuffers named_inputs,
             std::unique_ptr<ComputeResources> compute_resources)
              -> AsyncComputeResult {
            mojom::ComputeResultPtr result =
                compute_resources->InvokeInterpreter(std::move(named_inputs));
            return {std::move(result), std::move(compute_resources)};
          },
          std::move(named_inputs), std::move(compute_resources)),
      base::BindOnce(&GraphImpl::OnComputeComplete, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void GraphImpl::OnComputeComplete(ComputeCallback callback,
                                  AsyncComputeResult result) {
  // Returns the borrowed `compute_resources_` if another task hasn't already.
  if (!compute_resources_) {
    compute_resources_ = std::move(result.second);
  }

  std::move(callback).Run(std::move(result.first));
}

void GraphImpl::DispatchImpl(
    const base::flat_map<std::string_view, WebNNBufferImpl*>& named_inputs,
    const base::flat_map<std::string_view, WebNNBufferImpl*>& named_outputs) {
  // TODO(crbug.com/1472888): Implement MLBuffer for TFLite. Involve
  // an IPC security reviewer.
  NOTIMPLEMENTED();
}

}  // namespace webnn::tflite
