// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/graph_impl_ort.h"

#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/notimplemented.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected_macros.h"
#include "services/webnn/error.h"
#include "services/webnn/ort/context_impl_ort.h"
#include "services/webnn/ort/error_ort.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "services/webnn/ort/tensor_impl_ort.h"
#include "services/webnn/ort/utils_ort.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/resource_task.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn::ort {

namespace {

struct Session {
  Session(ScopedOrtEnv env,
          std::unique_ptr<OrtModelEditor::WeightsDeleter> weights_deleter,
          ScopedOrtSession session)
      : env(std::move(env)),
        weights_deleter(std::move(weights_deleter)),
        session(std::move(session)) {}
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
  ~Session() = default;

  OrtSession* GetSession() { return session.get(); }

  // `env` should be prior to `session`. That ensures releasing `env` after
  // releasing the session. This avoids unloading the providers DLLs being
  // used during `session` destruction.
  ScopedOrtEnv env;

  // `weights_deleter` should be prior to `session` since `weights_deleter` will
  // be used for deleting the weights when `session` is destroyed.
  std::unique_ptr<OrtModelEditor::WeightsDeleter> weights_deleter;

  ScopedOrtSession session;
};

base::flat_map<std::string,
               scoped_refptr<QueueableResourceState<BufferContentOrt>>>
ToNamedBufferStateMap(
    const base::flat_map<std::string_view, WebNNTensorImpl*>& named_tensors) {
  base::flat_map<std::string,
                 scoped_refptr<QueueableResourceState<BufferContentOrt>>>
      buffer_states;
  buffer_states.reserve(named_tensors.size());

  for (const auto& [name, tensor] : named_tensors) {
    buffer_states.emplace(
        name, static_cast<TensorImplOrt*>(tensor)->GetBufferState());
  }

  return buffer_states;
}

}  // namespace

// Represents the collection of resources associated with a particular graph.
// These resources may outlive their associated `GraphImplOrt` instance while
// executing the graph.
class GraphImplOrt::ComputeResources {
 public:
  ComputeResources(std::unique_ptr<Session> session,
                   base::flat_map<std::string, std::string>
                       operand_input_name_to_onnx_input_name,
                   base::flat_map<std::string, std::string>
                       operand_output_name_to_onnx_output_name)
      : operand_input_name_to_onnx_input_name_(
            std::move(operand_input_name_to_onnx_input_name)),
        operand_output_name_to_onnx_output_name_(
            std::move(operand_output_name_to_onnx_output_name)),
        session_(std::move(session)) {}

  ~ComputeResources() = default;

  // Run the model asynchronously in `base::ThreadPool`.
  void OrtRunSync(
      std::vector<std::pair<std::string, const OrtValue*>> named_input_tensors,
      std::vector<std::pair<std::string, OrtValue*>> named_output_tensors) {
    TRACE_EVENT0("gpu", "ort::GraphImplOrt::ComputeResources::OrtRunSync");

    std::vector<const char*> input_names;
    std::vector<const OrtValue*> input_tensors;
    input_names.reserve(named_input_tensors.size());
    input_tensors.reserve(named_input_tensors.size());
    for (const auto& [name, tensor] : named_input_tensors) {
      input_names.push_back(
          operand_input_name_to_onnx_input_name_.at(name).data());
      input_tensors.push_back(tensor);
    }

    std::vector<const char*> output_names;
    std::vector<OrtValue*> output_tensors;
    output_names.reserve(named_output_tensors.size());
    output_tensors.reserve(named_output_tensors.size());
    for (const auto& [name, tensor] : named_output_tensors) {
      output_names.push_back(
          operand_output_name_to_onnx_output_name_.at(name).data());
      output_tensors.push_back(tensor);
    }

    const OrtApi* ort_api = GetOrtApi();
    CHECK(IsSuccess(ort_api->Run(session_->GetSession(), nullptr,
                                 input_names.data(), input_tensors.data(),
                                 input_names.size(), output_names.data(),
                                 output_names.size(), output_tensors.data())));
  }

 private:
  base::flat_map<std::string, std::string>
      operand_input_name_to_onnx_input_name_;
  base::flat_map<std::string, std::string>
      operand_output_name_to_onnx_output_name_;
  std::unique_ptr<Session> session_;
};

// static
void GraphImplOrt::CreateAndBuild(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
    mojom::GraphInfoPtr graph_info,
    ComputeResourceInfo compute_resource_info,
    base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    ContextImplOrt* context,
    WebNNContextImpl::CreateGraphImplCallback callback) {
  ScopedTrace scoped_trace("GraphImplOrt::CreateAndBuild");

  auto wrapped_callback = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&GraphImplOrt::DidCreateAndBuild, std::move(receiver),
                     context->AsWeakPtr(), std::move(compute_resource_info),
                     std::move(callback)));

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()},
      base::BindOnce(&GraphImplOrt::CreateAndBuildOnBackgroundThread,
                     std::move(graph_info), context->session_options(),
                     context->properties(), std::move(constant_operands),
                     std::move(scoped_trace)),
      std::move(wrapped_callback));
}

// static
base::expected<std::unique_ptr<GraphImplOrt::ComputeResources>, mojom::ErrorPtr>
GraphImplOrt::CreateAndBuildOnBackgroundThread(
    mojom::GraphInfoPtr graph_info,
    scoped_refptr<SessionOptions> session_options,
    ContextProperties context_properties,
    base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    ScopedTrace scoped_trace) {
  scoped_trace.AddStep("Create model info");
  ASSIGN_OR_RETURN(std::unique_ptr<OrtModelEditor::ModelInfo> model_info,
                   GraphBuilderOrt::CreateAndBuild(
                       *graph_info, std::move(context_properties),
                       std::move(constant_operands)));

  // `CreateEnv()` will increase the reference count and return the reference of
  // the existing `OrtEnv` instance that is created by context provider. `env`
  // will be owned by `GraphImplOrt::Session` that ensures releasing `OrtEnv`
  // reference after releasing `OrtSession`.
  const OrtApi* ort_api = GetOrtApi();
  ScopedOrtEnv env;
  if (ORT_CALL_FAILED(ort_api->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "WebNN",
                                         ScopedOrtEnv::Receiver(env).get()))) {
    return base::unexpected(mojom::Error::New(mojom::Error::Code::kUnknownError,
                                              "Failed to create graph."));
  }

  scoped_trace.AddStep("Create session from model");
  ScopedOrtSession session;
  if (ORT_CALL_FAILED(GetOrtModelEditorApi()->CreateSessionFromModel(
          env.get(), model_info->model.get(), session_options->get(),
          ScopedOrtSession::Receiver(session).get()))) {
    return base::unexpected(mojom::Error::New(mojom::Error::Code::kUnknownError,
                                              "Failed to build graph."));
  }

  scoped_trace.AddStep("Create compute resources");
  auto compute_session = base::WrapUnique(
      new Session(std::move(env), std::move(model_info->weights_deleter),
                  std::move(session)));

  base::flat_map<std::string, std::string>
      operand_input_name_to_onnx_input_name;
  for (uint64_t id : graph_info->input_operands) {
    std::string_view operand_name =
        graph_info->id_to_operand_map.at(id)->name.value();
    operand_input_name_to_onnx_input_name.emplace(
        operand_name, GetOperandName(operand_name, id));
  }
  base::flat_map<std::string, std::string>
      operand_output_name_to_onnx_output_name;
  for (uint64_t id : graph_info->output_operands) {
    std::string_view operand_name =
        graph_info->id_to_operand_map.at(id)->name.value();
    operand_output_name_to_onnx_output_name.emplace(
        operand_name, GetOperandName(operand_name, id));
  }

  // TODO: remove this log which is for temporary debugging purpose.
  LOG(ERROR) << "========= Running on ORT =============";

  return base::WrapUnique(new GraphImplOrt::ComputeResources(
      std::move(compute_session),
      std::move(operand_input_name_to_onnx_input_name),
      std::move(operand_output_name_to_onnx_output_name)));
}

// static
void GraphImplOrt::DidCreateAndBuild(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
    base::WeakPtr<WebNNContextImpl> context,
    ComputeResourceInfo compute_resource_info,
    WebNNContextImpl::CreateGraphImplCallback callback,
    base::expected<std::unique_ptr<GraphImplOrt::ComputeResources>,
                   mojom::ErrorPtr> result) {
  if (!result.has_value()) {
    std::move(callback).Run(base::unexpected(std::move(result.error())));
    return;
  }

  if (!context) {
    std::move(callback).Run(base::unexpected(mojom::Error::New(
        mojom::Error::Code::kUnknownError, "Context was destroyed.")));
    return;
  }

  std::move(callback).Run(base::WrapUnique(new GraphImplOrt(
      std::move(receiver), std::move(compute_resource_info),
      std::move(result.value()), static_cast<ContextImplOrt*>(context.get()))));
}

GraphImplOrt::~GraphImplOrt() = default;

GraphImplOrt::GraphImplOrt(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
    ComputeResourceInfo compute_resource_info,
    std::unique_ptr<GraphImplOrt::ComputeResources> compute_resources,
    ContextImplOrt* context)
    : WebNNGraphImpl(std::move(receiver),
                     context,
                     std::move(compute_resource_info)) {
  compute_resources_state_ =
      base::MakeRefCounted<QueueableResourceState<ComputeResources>>(
          std::move(compute_resources));
}

void GraphImplOrt::DispatchImpl(
    const base::flat_map<std::string_view, WebNNTensorImpl*>&
        named_input_tensors,
    const base::flat_map<std::string_view, WebNNTensorImpl*>&
        named_output_tensors) {
  TRACE_EVENT0("gpu", "ort::GraphImplOrt::DispatchImpl");

  base::flat_map<std::string,
                 scoped_refptr<QueueableResourceState<BufferContentOrt>>>
      named_input_buffer_states = ToNamedBufferStateMap(named_input_tensors);
  base::flat_map<std::string,
                 scoped_refptr<QueueableResourceState<BufferContentOrt>>>
      named_output_buffer_states = ToNamedBufferStateMap(named_output_tensors);

  // Input tensors will be read from while the graph is executing, so lock them
  // them as shared/read-only.
  std::vector<scoped_refptr<QueueableResourceStateBase>> shared_resources;
  shared_resources.reserve(named_input_tensors.size());
  for (const auto& [_, buffer_state] : named_input_buffer_states) {
    shared_resources.push_back(buffer_state);
  }

  // Exclusively reserve all output tensors, which will be written to.
  std::vector<scoped_refptr<QueueableResourceStateBase>> exclusive_resources;
  // Extra +1 is for the compute resources.
  exclusive_resources.reserve(1 + named_output_tensors.size());
  exclusive_resources.push_back(compute_resources_state_);
  for (const auto& [_, buffer_state] : named_output_buffer_states) {
    exclusive_resources.push_back(buffer_state);
  }

  auto task = base::MakeRefCounted<ResourceTask>(
      std::move(shared_resources), std::move(exclusive_resources),
      base::BindOnce(
          [](scoped_refptr<QueueableResourceState<ComputeResources>>
                 compute_resources_state,
             base::flat_map<
                 std::string,
                 scoped_refptr<QueueableResourceState<BufferContentOrt>>>
                 named_input_buffer_states,
             base::flat_map<
                 std::string,
                 scoped_refptr<QueueableResourceState<BufferContentOrt>>>
                 named_output_buffer_states,
             base::OnceClosure completion_closure) {
            ComputeResources* raw_compute_resources =
                compute_resources_state->GetExclusivelyLockedResource();

            std::vector<std::pair<std::string, const OrtValue*>>
                named_input_tensors;
            named_input_tensors.reserve(named_input_buffer_states.size());
            std::vector<std::pair<std::string, OrtValue*>> named_output_tensors;
            named_output_tensors.reserve(named_output_buffer_states.size());

            for (const auto& [name, buffer] : named_input_buffer_states) {
              named_input_tensors.emplace_back(
                  name, buffer->GetSharedLockedResource().tensor());
            }
            for (const auto& [name, buffer] : named_output_buffer_states) {
              named_output_tensors.emplace_back(
                  name, buffer->GetExclusivelyLockedResource()->tensor());
            }

            // Compute tasks can take a significant amount of time, use the
            // thread pool to avoid blocking the main thread.
            base::ThreadPool::PostTaskAndReply(
                FROM_HERE,
                base::BindOnce(&ComputeResources::OrtRunSync,
                               base::Unretained(raw_compute_resources),
                               std::move(named_input_tensors),
                               std::move(named_output_tensors)),
                std::move(completion_closure));
          },
          compute_resources_state_, std::move(named_input_buffer_states),
          std::move(named_output_buffer_states)));

  task->Enqueue();
}

}  // namespace webnn::ort
