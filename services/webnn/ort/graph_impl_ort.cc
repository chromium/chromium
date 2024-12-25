// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/graph_impl_ort.h"

#include "base/memory/scoped_refptr.h"
#include "base/notimplemented.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/types/expected_macros.h"
#include "services/webnn/error.h"
#include "services/webnn/ort/context_impl_ort.h"
#include "services/webnn/ort/error_ort.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/ort/tensor_impl_ort.h"
#include "services/webnn/ort/utils_ort.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/resource_task.h"
#include "services/webnn/webnn_constant_operand.h"
#include "services/webnn/webnn_graph_impl.h"

namespace webnn::ort {

namespace {

base::flat_map<std::string, raw_ref<const BufferContentOrt>>
GetSharedLockedBuffers(
    const base::flat_map<
        std::string,
        scoped_refptr<QueueableResourceState<BufferContentOrt>>>& buffers) {
  std::vector<std::pair<std::string, raw_ref<const BufferContentOrt>>>
      queueable_buffers;
  for (const auto& [name, buffer] : buffers) {
    queueable_buffers.emplace_back(name, buffer->GetSharedLockedResource());
  }
  return queueable_buffers;
}

base::flat_map<std::string, raw_ref<const BufferContentOrt>>
GetExclusivelyLockedBuffers(
    const base::flat_map<
        std::string,
        scoped_refptr<QueueableResourceState<BufferContentOrt>>>& buffers) {
  std::vector<std::pair<std::string, raw_ref<const BufferContentOrt>>>
      queueable_buffers;
  for (const auto& [name, buffer] : buffers) {
    queueable_buffers.emplace_back(name,
                                   *buffer->GetExclusivelyLockedResource());
  }
  return queueable_buffers;
}

}  // namespace

// static
void GraphImplOrt::CreateAndBuild(
    mojom::GraphInfoPtr graph_info,
    ComputeResourceInfo compute_resource_info,
    base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    ContextImplOrt* context,
    WebNNContextImpl::CreateGraphImplCallback callback) {
  auto wrapped_callback = base::BindPostTaskToCurrentDefault(
      base::BindOnce(&GraphImplOrt::DidCreateAndBuild, context->AsWeakPtr(),
                     std::move(compute_resource_info),
                     std::move(callback)));

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()},
      base::BindOnce(&GraphImplOrt::CreateAndBuildOnBackgroundThread,
                     std::move(graph_info), context->options().Clone(),
                     context->properties(), std::move(constant_operands),
                     context->allocator()),
      std::move(wrapped_callback));
}

// static
base::expected<OrtSession*, mojom::ErrorPtr>
GraphImplOrt::CreateAndBuildOnBackgroundThread(
    mojom::GraphInfoPtr graph_info,
    mojom::CreateContextOptionsPtr context_options,
    ContextProperties context_properties,
    base::flat_map<uint64_t, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    scoped_refptr<AllocatorOrt> allocator) {
  auto result = GraphBuilderOrt::CreateAndBuild(
      *graph_info, std::move(context_properties), std::move(constant_operands),
      allocator);
  if (!result.has_value()) {
    return base::unexpected(std::move(result.error()));
  }

  OrtSessionOptions* session_options;
  const OrtApi* ort_api = GetOrtApi();
  CHECK_STATUS(ort_api->CreateSessionOptions(&session_options));

  CHECK_STATUS(ort_api->SetSessionGraphOptimizationLevel(
      session_options, GraphOptimizationLevel::ORT_ENABLE_ALL));

  if (context_options->device == mojom::CreateContextOptions::Device::kGpu ||
      context_options->device == mojom::CreateContextOptions::Device::kNpu) {
    const OrtDmlApi* ort_dml_api;
    CHECK_STATUS(ort_api->GetExecutionProviderApi(
        "DML", 10, reinterpret_cast<const void**>(&ort_dml_api)));

    OrtDmlDeviceOptions options;
    if (context_options->device == mojom::CreateContextOptions::Device::kGpu) {
      options = {OrtDmlPerformancePreference::MinimumPower, OrtDmlDeviceFilter::Gpu};
    } else {
      options = {OrtDmlPerformancePreference::MinimumPower, OrtDmlDeviceFilter::Gpu};
      // NPU is available only when ENABLE_NPU_ADAPTER_ENUMERATION
      // options = {OrtDmlPerformancePreference::MinimumPower, OrtDmlDeviceFilter::Npu};
    }

    ort_dml_api->SessionOptionsAppendExecutionProvider_DML2(session_options, &options);
  }

  OrtSession* session;
  const OrtEnv* env = allocator->env();
  OrtStatus* status = GetOrtGraphApi()->CreateSessionFromModel(
      env, result.value()->model.get_ptr(), session_options, &session);
  ort_api->ReleaseSessionOptions(session_options);

  if (status != NULL) {
    std::string msg = ort_api->GetErrorMessage(status);
    ort_api->ReleaseStatus(status);
    LOG(ERROR) << "[WebNN] Ort Status " << msg;
    return base::unexpected(mojom::Error::New(
        mojom::Error::Code::kUnknownError, "Failed to create ORT session."));
  }

  return session;
}

// static
void GraphImplOrt::DidCreateAndBuild(
    base::WeakPtr<WebNNContextImpl> context,
    ComputeResourceInfo compute_resource_info,
    WebNNContextImpl::CreateGraphImplCallback callback,
    base::expected<OrtSession*, mojom::ErrorPtr> result) {
  if (!result.has_value()) {
    std::move(callback).Run(base::unexpected(std::move(result).error()));
    return;
  }

  if (!context) {
    std::move(callback).Run(base::unexpected(mojom::Error::New(
        mojom::Error::Code::kUnknownError, "Context was destroyed.")));
    return;
  }

  std::move(callback).Run(base::WrapUnique(
      new GraphImplOrt(std::move(compute_resource_info), result.value(),
                       static_cast<ContextImplOrt*>(context.get()))));
}

// Represents the collection of resources associated with a particular graph.
// These resources may outlive their associated `GraphImplOrt` instance while
// executing the graph.
class GraphImplOrt::ComputeResources {
 public:
  ComputeResources(OrtSession* session) : session_(session) {}

  ~ComputeResources() {
    const OrtApi* ort_api = GetOrtApi();
    ort_api->ReleaseSession(session_);
  }

  void DoDispatch(base::flat_map<std::string, raw_ref<const BufferContentOrt>>
                      named_input_buffers,
                  base::flat_map<std::string, raw_ref<const BufferContentOrt>>
                      named_output_buffers) {
    std::vector<const char*> ort_input_names, ort_output_names;
    ort_input_names.reserve(named_input_buffers.size());
    ort_output_names.reserve(named_output_buffers.size());

    std::vector<const OrtValue*> ort_input_tensors;
    std::vector<OrtValue*> ort_output_tensors;
    ort_input_tensors.reserve(named_input_buffers.size());
    ort_output_tensors.reserve(named_output_buffers.size());

    for (const auto& [name, buffer] : named_input_buffers) {
      ort_input_names.push_back(name.data());
      ort_input_tensors.push_back(buffer->tensor());
    }

    for (const auto& [name, buffer] : named_output_buffers) {
      ort_output_names.push_back(name.data());
      ort_output_tensors.push_back(buffer->tensor());
    }

    const OrtApi* ort_api = GetOrtApi();
    CHECK_STATUS(ort_api->Run(session_, nullptr, ort_input_names.data(),
                              ort_input_tensors.data(), ort_input_names.size(),
                              ort_output_names.data(), ort_output_names.size(),
                              ort_output_tensors.data()));
  }

 private:
  raw_ptr<OrtSession> session_;
};

GraphImplOrt::~GraphImplOrt() = default;

GraphImplOrt::GraphImplOrt(ComputeResourceInfo compute_resource_info,
                           OrtSession* session,
                           ContextImplOrt* context)
    : WebNNGraphImpl(context, std::move(compute_resource_info)) {
  std::unique_ptr<ComputeResources> compute_resources =
      base::WrapUnique(new ComputeResources(session));
  compute_resources_state_ =
      base::MakeRefCounted<QueueableResourceState<ComputeResources>>(
          std::move(compute_resources));
}

// TODO: Consider using OrtApi::RunAsync
void GraphImplOrt::DispatchImpl(
    const base::flat_map<std::string_view, WebNNTensorImpl*>&
        named_input_tensors,
    const base::flat_map<std::string_view, WebNNTensorImpl*>&
        named_output_tensors) {
  std::vector<std::pair<
      std::string, scoped_refptr<QueueableResourceState<BufferContentOrt>>>>
      named_input_buffer_states, named_output_buffer_states;
  named_input_buffer_states.reserve(named_input_tensors.size());
  named_output_buffer_states.reserve(named_output_tensors.size());

  for (const auto& [name, tensor] : named_input_tensors) {
    named_input_buffer_states.emplace_back(
        name, static_cast<TensorImplOrt*>(tensor)->GetBufferState());
  }
  for (const auto& [name, tensor] : named_output_tensors) {
    named_output_buffer_states.emplace_back(
        name, static_cast<TensorImplOrt*>(tensor)->GetBufferState());
  }

  // Input tensors will be read from while the graph is executing, so lock them
  // them as shared/read-only.
  std::vector<scoped_refptr<QueueableResourceStateBase>> shared_resources;
  shared_resources.reserve(named_input_tensors.size());
  for (const auto& [name, buffer_state] : named_input_buffer_states) {
    shared_resources.push_back(buffer_state);
  }

  // Exclusively reserve all output tensors, which will be written to.
  std::vector<scoped_refptr<QueueableResourceStateBase>> exclusive_resources;
  // Extra +1 is for the compute resources.
  exclusive_resources.reserve(1 + named_output_tensors.size());
  exclusive_resources.push_back(compute_resources_state_);
  for (const auto& [name, buffer_state] : named_output_buffer_states) {
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
            // Compute tasks can take a significant amount of time, use the
            // thread pool to avoid blocking the main thread.
            base::ThreadPool::PostTaskAndReply(
                FROM_HERE,
                base::BindOnce(
                    &ComputeResources::DoDispatch,
                    // Unretained is safe here because a reference to
                    // a `QueueableResourceState` corresponding to
                    // `raw_compute_resources` is held by the
                    // `ResourceTask` until `completion_closure` is run below.
                    base::Unretained(compute_resources_state
                                         ->GetExclusivelyLockedResource()),
                    GetSharedLockedBuffers(named_input_buffer_states),
                    GetExclusivelyLockedBuffers(named_output_buffer_states)),
                std::move(completion_closure));
          },
          compute_resources_state_, std::move(named_input_buffer_states),
          std::move(named_output_buffer_states)));

  task->Enqueue();
}

}  // namespace webnn::ort
