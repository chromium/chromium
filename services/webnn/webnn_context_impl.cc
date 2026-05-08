// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_context_impl.h"

#include <memory>
#include <set>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/memory_dump_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "services/webnn/error.h"
#include "services/webnn/public/cpp/data_type_limits.h"
#include "services/webnn/public/cpp/graph_validation_utils.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/cpp/supported_tensors.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/mojom/webnn_context.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "services/webnn/scoped_gpu_sequence.h"
#include "services/webnn/webnn_context_provider_impl.h"
#include "services/webnn/webnn_graph_builder_impl.h"
#include "services/webnn/webnn_graph_impl.h"
#include "services/webnn/webnn_tensor_impl.h"
#include "third_party/tflite/buildflags.h"

#if BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(WEBNN_USE_LITERT)
#include "services/webnn/tflite/context_provider_tflite.h"  // nogncheck
#endif  // BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(WEBNN_USE_LITERT)

#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
#include "third_party/xnnpack/src/include/xnnpack.h"  // nogncheck
#endif  // BUILD_TFLITE_WITH_XNNPACK

namespace {
// Generates process-unique IDs to use for tracing resources.
base::AtomicSequenceNumber g_next_webnn_context_tracing_id;

// Return false if the named tensors for dispatch don't match the built
// graph's expectation.
bool ValidateWebNNTensors(
    const base::flat_map<std::string, scoped_refptr<webnn::WebNNTensorImpl>>&
        named_tensors,
    const base::flat_map<std::string, webnn::OperandDescriptor>&
        names_to_descriptors) {
  return std::ranges::equal(
      named_tensors, names_to_descriptors,
      [](const auto& named_tensor, const auto& tensor_spec) {
        const auto& [tensor_name, tensor_impl] = named_tensor;
        const auto& [tensor_spec_name, tensor_spec_descriptor] = tensor_spec;
        return tensor_name == tensor_spec_name &&
               tensor_impl->data_type() == tensor_spec_descriptor.data_type() &&
               tensor_impl->shape() == tensor_spec_descriptor.shape();
      });
}

// Return false if the same tensor was specified in inputs and outputs.
bool ValidateWebNNTensorsUsage(
    const base::flat_map<std::string, blink::WebNNTensorToken>& named_inputs,
    const base::flat_map<std::string, blink::WebNNTensorToken>& named_outputs) {
  // Validate that output tensors are unique.
  std::set<blink::WebNNTensorToken> output_tensors;
  for (const auto& named_output : named_outputs) {
    output_tensors.insert(named_output.second);
  }

  if (output_tensors.size() != named_outputs.size()) {
    return false;
  }

  // Validate tensors used for input and output are unique.
  for (const auto& named_input : named_inputs) {
    if (output_tensors.contains(named_input.second)) {
      return false;
    }
  }

  return true;
}

}  // namespace

namespace webnn {

WebNNContextImpl::WebNNContextImpl(
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    base::WeakPtr<WebNNContextProviderImpl> context_provider,
    WebNNContextImpl::ContextBackendUma backend_uma,
    ContextProperties properties,
    mojom::CreateContextOptionsPtr options,
    mojo::ScopedDataPipeConsumerHandle write_tensor_consumer,
    mojo::ScopedDataPipeProducerHandle read_tensor_producer,
    std::unique_ptr<ScopedGpuSequence> gpu_sequence,
    scoped_refptr<gpu::MemoryTracker> memory_tracker,
    scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
    gpu::SharedImageManager* shared_image_manager,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
    : WebNNObjectBase<mojom::WebNNContext,
                      blink::WebNNContextToken,
                      mojo::Receiver<mojom::WebNNContext>>(
          std::move(receiver),
          gpu_sequence->scheduler_task_runner()),
      context_provider_(std::move(context_provider)),
      properties_(IntersectWithBaseProperties(std::move(properties))),
      options_(std::move(options)),
      memory_type_tracker_(std::move(memory_tracker)),
      gpu_sequence_(std::move(gpu_sequence)),
      write_tensor_consumer_(std::move(write_tensor_consumer)),
      read_tensor_producer_(std::move(read_tensor_producer)),
      shared_image_manager_(shared_image_manager),
      main_task_runner_(std::move(main_task_runner)),
      owning_task_runner_(std::move(owning_task_runner)),
      tracing_id_(g_next_webnn_context_tracing_id.GetNext()) {
  InitializeContext(backend_uma);
}

#if BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(WEBNN_USE_LITERT)
WebNNContextImpl::WebNNContextImpl(
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    base::WeakPtr<tflite::ContextProviderTflite> tflite_context_provider,
    WebNNContextImpl::ContextBackendUma backend_uma,
    ContextProperties properties,
    mojom::CreateContextOptionsPtr options,
    scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
    : WebNNObjectBase<mojom::WebNNContext,
                      blink::WebNNContextToken,
                      mojo::Receiver<mojom::WebNNContext>>(std::move(receiver),
                                                           owning_task_runner),
      tflite_context_provider_(std::move(tflite_context_provider)),
      is_tflite_context_provider_(true),
      properties_(IntersectWithBaseProperties(std::move(properties))),
      options_(std::move(options)),
      memory_type_tracker_(base::MakeRefCounted<gpu::MemoryTracker>()),
      main_task_runner_(std::move(main_task_runner)),
      owning_task_runner_(std::move(owning_task_runner)),
      tracing_id_(g_next_webnn_context_tracing_id.GetNext()) {
  InitializeContext(backend_uma);
}
#endif  // BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(WEBNN_USE_LITERT)

void WebNNContextImpl::InitializeContext(ContextBackendUma backend_uma) {
  RecordContextBackendUma(backend_uma);
#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
  const xnn_status status = xnn_initialize(/*allocator=*/nullptr);
  CHECK_EQ(status, xnn_status_success);
#endif  // BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "WebNN", owning_task_runner_);
}

WebNNContextImpl::~WebNNContextImpl() {
  for (auto impl : tensor_impls_) {
    // Delete non-interop tensor instances from the tracker as they can't
    // unregister themselves since they're ref-counted and might outlive the
    // context.
    if (!impl->has_shared_image()) {
      memory_type_tracker_.TrackMemFree(impl->PackedByteLength());
    }
  }

  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);

#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
  // Deinitialize XNNPACK
  const xnn_status status = xnn_deinitialize();
  CHECK_EQ(status, xnn_status_success);
#endif  // BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
}

// static
void WebNNContextImpl::RecordContextBackendUma(ContextBackendUma backend_uma) {
  base::UmaHistogramEnumeration("WebNN.Context.Backend", backend_uma);
}

void WebNNContextImpl::OnDisconnect() {
  // Explicitly reset all tensor and graph receivers before destruction since
  // destroying bound receivers can cause Mojo to DCHECK due to pending
  // callbacks or if destruction occurs on a different runner than the bound
  // runner.
  for (auto impl : tensor_impls_) {
    impl->ResetMojoReceiver();
  }

  for (auto impl : graph_impls_) {
    impl->ResetMojoReceiver();
  }

  ResetMojoReceiver();

  base::OnceClosure remove_task;
#if BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(WEBNN_USE_LITERT)
  if (is_tflite_context_provider_) {
    remove_task =
        base::BindOnce(&tflite::ContextProviderTflite::RemoveWebNNContextImpl,
                       tflite_context_provider_, handle());
  }
#endif  // BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(WEBNN_USE_LITERT)

  if (!remove_task) {
    remove_task =
        base::BindOnce(&WebNNContextProviderImpl::RemoveWebNNContextImpl,
                       context_provider_, handle());
  }

  if (!main_task_runner_->RunsTasksInCurrentSequence()) {
    main_task_runner_->PostTask(FROM_HERE, std::move(remove_task));
  } else {
    std::move(remove_task).Run();
  }
}

#if BUILDFLAG(IS_WIN)
void WebNNContextImpl::DestroyAllContextsAndKillGpuProcess() {
  if (!main_task_runner_->RunsTasksInCurrentSequence()) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &WebNNContextProviderImpl::DestroyAllContextsAndKillGpuProcess,
            context_provider_));
    return;
  }

  context_provider_->DestroyAllContextsAndKillGpuProcess();
}
#endif  // BUILDFLAG(IS_WIN)

void WebNNContextImpl::CreateWeightsFile(
    base::OnceCallback<void(base::File)> callback) {
  base::OnceClosure create_task;
#if BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(WEBNN_USE_LITERT)
  if (is_tflite_context_provider_) {
    create_task =
        base::BindOnce(&tflite::ContextProviderTflite::CreateWeightsFile,
                       tflite_context_provider_,
                       base::BindPostTaskToCurrentDefault(std::move(callback)));
  }
#endif  // BUILDFLAG(WEBNN_USE_TFLITE) || BUILDFLAG(WEBNN_USE_LITERT)

  if (!create_task) {
    create_task = base::BindOnce(
        &WebNNContextProviderImpl::CreateWeightsFile, context_provider_,
        base::BindPostTaskToCurrentDefault(std::move(callback)));
  }

  if (!main_task_runner_->RunsTasksInCurrentSequence()) {
    main_task_runner_->PostTask(FROM_HERE, std::move(create_task));
  } else {
    std::move(create_task).Run();
  }
}

void WebNNContextImpl::ReportBadGraphBuilderMessage(
    const std::string& message,
    base::PassKey<WebNNGraphBuilderImpl> pass_key) {
  graph_builder_impls_.ReportBadMessage(message);
}

void WebNNContextImpl::BuildGraph(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraph> receiver,
    mojom::GraphInfoPtr graph_info,
    WebNNGraphImpl::ComputeResourceInfo compute_resource_info,
    base::flat_map<OperandId, std::unique_ptr<WebNNConstantOperand>>
        constant_operands,
    base::flat_map<OperandId, scoped_refptr<WebNNTensorImpl>>
        constant_tensor_operands,
    BuildGraphCallback callback) {
  CreateGraphImpl(std::move(receiver), std::move(graph_info),
                  std::move(compute_resource_info),
                  std::move(constant_operands),
                  std::move(constant_tensor_operands),
                  base::BindOnce(&WebNNContextImpl::OnGraphBuilt, AsWeakPtr(),
                                 std::move(callback)));
}

void WebNNContextImpl::OnGraphBuilt(
    BuildGraphCallback callback,
    base::expected<scoped_refptr<WebNNGraphImpl>, mojom::ErrorPtr> result) {
  if (!result.has_value()) {
    std::move(callback).Run(base::unexpected(std::move(result.error())));
    return;
  }

  GraphCreationResult creation_result;
  creation_result.graph_token = result.value()->handle();
  creation_result.devices = result.value()->devices();

  graph_impls_.emplace(std::move(result.value()));

  std::move(callback).Run(std::move(creation_result));
}

void WebNNContextImpl::RemoveGraphBuilder(
    mojo::ReceiverId graph_builder_id,
    base::PassKey<WebNNGraphBuilderImpl> /*pass_key*/) {
  graph_builder_impls_.Remove(graph_builder_id);
}

void WebNNContextImpl::CreateGraphBuilder(
    mojo::PendingAssociatedReceiver<mojom::WebNNGraphBuilder> receiver) {
  auto graph_builder = std::make_unique<WebNNGraphBuilderImpl>(*this);
  WebNNGraphBuilderImpl* graph_builder_ptr = graph_builder.get();

  mojo::ReceiverId id =
      graph_builder_impls_.Add(std::move(graph_builder), std::move(receiver));

  graph_builder_ptr->SetId(id, GraphBuilderContext::GetPassKey());
}

void WebNNContextImpl::CreateTensor(
    mojom::TensorInfoPtr tensor_info,
    mojo_base::BigBuffer tensor_data,
    mojom::WebNNContext::CreateTensorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ScopedTrace scoped_trace("WebNNContextImpl::CreateTensor");

  if (!ValidateTensor(properties_, tensor_info->descriptor).has_value()) {
    GetMojoReceiver().ReportBadMessage(kBadMessageInvalidTensor);
    return;
  }

  if (tensor_info->usage.Has(MLTensorUsageFlags::kGraphConstant)) {
    const base::expected<OperandDescriptor, std::string> validated_descriptor =
        webnn::OperandDescriptor::Create(
            properties_, tensor_info->descriptor.data_type(),
            tensor_info->descriptor.shape(), "WebNNGraphConstant");
    if (!validated_descriptor.has_value()) {
      GetMojoReceiver().ReportBadMessage(kBadMessageInvalidTensor);
      return;
    }

    if (!properties_.data_type_limits.constant.Supports(
            validated_descriptor.value())) {
      GetMojoReceiver().ReportBadMessage(kBadMessageInvalidTensor);
      return;
    }

    if (tensor_data.size() != validated_descriptor->PackedByteLength()) {
      GetMojoReceiver().ReportBadMessage(kBadMessageInvalidTensor);
      return;
    }
  }

  mojo::PendingAssociatedRemote<mojom::WebNNTensor> remote;
  auto receiver = remote.InitWithNewEndpointAndPassReceiver();

  auto result = CreateTensorImpl(std::move(receiver), std::move(tensor_info));
  if (!result.has_value()) {
    std::move(callback).Run(
        mojom::CreateTensorResult::NewError(std::move(result.error())));
    return;
  }

  // Write the specified values into the tensor. If `tensor_data` is empty,
  // the tensor should be left initialized to zero. The `tensor_data` size
  // should of been already validated in CreateTensor().
  if (tensor_data.size() > 0) {
    result.value()->WriteTensorImpl(std::move(tensor_data));
  }

  auto success = mojom::CreateTensorSuccess::New(std::move(remote),
                                                 result.value()->handle());
  std::move(callback).Run(
      mojom::CreateTensorResult::NewSuccess(std::move(success)));

  memory_type_tracker_.TrackMemAlloc(result.value()->PackedByteLength());

  // Associates a `WebNNTensor` instance with this context so the WebNN service
  // can access the implementation.
  tensor_impls_.emplace(*std::move(result));
}

ScopedGpuSequence* WebNNContextImpl::gpu_sequence() const {
  return gpu_sequence_.get();
}

bool WebNNContextImpl::HasValidWriteTensorConsumer() const {
  return write_tensor_consumer_.is_valid();
}

bool WebNNContextImpl::HasValidReadTensorProducer() const {
  return read_tensor_producer_.is_valid();
}

void WebNNContextImpl::ReadDataFromBigBufferOrDataPipe(
    mojo_base::BigBuffer src_buffer,
    base::span<uint8_t> dst_span) {
  if (src_buffer.size() == 0) {
    CHECK(write_tensor_consumer_);
    size_t bytes_read = 0;
    if (write_tensor_consumer_->ReadData(MOJO_READ_DATA_FLAG_ALL_OR_NONE,
                                         dst_span,
                                         bytes_read) != MOJO_RESULT_OK) {
      OnLost("WriteTensor(): Failed to read tensor data from data pipe.");
    }
  } else {
    dst_span.copy_from(src_buffer);
  }
}

mojo_base::BigBuffer WebNNContextImpl::WriteDataToDataPipeOrBigBuffer(
    base::span<const uint8_t> src_span) {
  if (read_tensor_producer_ &&
      src_span.size() > mojo_base::BigBuffer::kMaxInlineBytes &&
      read_tensor_producer_->WriteAllData(src_span) == MOJO_RESULT_OK) {
    return mojo_base::BigBuffer();
  }
  return mojo_base::BigBuffer(src_span);
}

void WebNNContextImpl::CreateTensorFromMailbox(mojom::TensorInfoPtr tensor_info,
                                               const gpu::Mailbox& mailbox,
                                               const gpu::SyncToken& fence,
                                               CreateTensorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ScopedTrace scoped_trace("WebNNContextImpl::CreateTensorFromMailbox");

  if (!tensor_info->usage.Has(MLTensorUsageFlags::kWebGpuInterop)) {
    GetMojoReceiver().ReportBadMessage(kBadMessageInvalidTensor);
    return;
  }

  if (!ValidateTensor(properties_, tensor_info->descriptor).has_value()) {
    GetMojoReceiver().ReportBadMessage(kBadMessageInvalidTensor);
    return;
  }

  // WebNN graph constants cannot be shared since they may not be readable.
  if (tensor_info->usage.Has(MLTensorUsageFlags::kGraphConstant)) {
    GetMojoReceiver().ReportBadMessage(kBadMessageInvalidTensor);
    return;
  }

  // SharedImageManager is not available when running without GPU
  // dependencies. WebGPU interop requires GPU process resources.
  if (!shared_image_manager_) {
    std::move(callback).Run(ToError<mojom::CreateTensorResult>(
        mojom::Error::Code::kNotSupportedError,
        "WebGPU interop is not supported in this context."));
    return;
  }

  if (!gpu_sequence_) {
    std::move(callback).Run(ToError<mojom::CreateTensorResult>(
        mojom::Error::Code::kNotSupportedError,
        "WebGPU interop is not supported without a GPU sequence."));
    return;
  }

  // Ensure the Mojo callback is posted back to the task runner. Running
  // it directly on the GPU sequence can violate Mojo's sequence checks,
  // even if executing on the same thread.
  auto mojo_callback_wrapper =
      base::BindPostTask(mojo_task_runner(), std::move(callback));

  // Must be a scheduled task since this depends on shared image creation task.
  RunOrScheduleTaskWithThisContext(
      base::BindOnce(
          [](mojom::TensorInfoPtr tensor_info, const gpu::Mailbox& mailbox,
             CreateTensorCallback callback, ScopedTrace scoped_trace,
             WebNNContextImpl& self) {
            CHECK(self.shared_image_manager_);

            constexpr char kWebNNCreateTensorErrorMessage[] =
                "Failed to create tensor.";

            // Tensor will own the representation.
            // TODO(https://crbug.com/481747252): When SharedImageBacking memory
            // tracking is fixed memory tracking for interop should work.
            WebNNTensorImpl::RepresentationPtr representation(
                self.shared_image_manager_
                    ->ProduceWebNNTensor(mailbox, &self.memory_type_tracker_)
                    .release(),
                OnTaskRunnerDeleter(self.main_task_runner()));
            if (!representation) {
              std::move(callback).Run(ToError<mojom::CreateTensorResult>(
                  mojom::Error::Code::kUnknownError,
                  kWebNNCreateTensorErrorMessage));
              return;
            }

            mojo::PendingAssociatedRemote<mojom::WebNNTensor> remote;
            auto receiver = remote.InitWithNewEndpointAndPassReceiver();

            auto result = self.CreateTensorFromSharedImageImpl(
                std::move(receiver), std::move(tensor_info),
                std::move(representation));
            if (!result.has_value()) {
              std::move(callback).Run(mojom::CreateTensorResult::NewError(
                  std::move(result.error())));
              return;
            }

            if (!result.value()->ImportTensorInternal()) {
              std::move(callback).Run(ToError<mojom::CreateTensorResult>(
                  mojom::Error::Code::kUnknownError,
                  kWebNNCreateTensorErrorMessage));
              return;
            }

            auto success = mojom::CreateTensorSuccess::New(
                std::move(remote), result.value()->handle());
            std::move(callback).Run(
                mojom::CreateTensorResult::NewSuccess(std::move(success)));
            self.tensor_impls_.emplace(*std::move(result));
          },
          std::move(tensor_info), mailbox, std::move(mojo_callback_wrapper),
          std::move(scoped_trace)),
      fence);
}

void WebNNContextImpl::Dispatch(
    const blink::WebNNGraphToken& graph_token,
    const base::flat_map<std::string, blink::WebNNTensorToken>& named_inputs,
    const base::flat_map<std::string, blink::WebNNTensorToken>& named_outputs) {
  ScopedTrace scoped_trace("WebNNContextImpl::Dispatch");

  if (!ValidateWebNNTensorsUsage(named_inputs, named_outputs)) {
    GetMojoReceiver().ReportBadMessage(kBadMessageInvalidTensor);
    return;
  }

  // Resolve graph token to graph impl.
  auto graph_it = graph_impls_.find(graph_token);
  if (graph_it == graph_impls_.end()) {
    GetMojoReceiver().ReportBadMessage(kBadMessageInvalidGraph);
    return;
  }
  scoped_refptr<WebNNGraphImpl> graph_impl = *graph_it;

  // Resolve the token of an input MLTensor to the corresponding `WebNNTensor`
  // instance.
  std::vector<std::pair<std::string, scoped_refptr<WebNNTensorImpl>>>
      name_to_input_tensors;
  name_to_input_tensors.reserve(named_inputs.size());
  for (const auto& [name, tensor_handle] : named_inputs) {
    scoped_refptr<WebNNTensorImpl> input_tensor =
        GetWebNNTensorImpl(tensor_handle);
    if (!input_tensor) {
      return;
    }

    // Input MLTensor is always dispatchable, which isn't allowed when used as
    // a graph constant.
    if (input_tensor->usage().Has(MLTensorUsageFlags::kGraphConstant)) {
      GetMojoReceiver().ReportBadMessage(kBadMessageInvalidTensor);
      return;
    }

    name_to_input_tensors.emplace_back(name, std::move(input_tensor));
  }
  base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>>
      name_to_input_tensor_map(std::move(name_to_input_tensors));
  if (!ValidateWebNNTensors(
          name_to_input_tensor_map,
          graph_impl->compute_resource_info().input_names_to_descriptors)) {
    GetMojoReceiver().ReportBadMessage(kBadMessageInvalidTensor);
    return;
  }

  // Resolve the token of an output MLTensor to the corresponding `WebNNTensor`
  // instance.
  std::vector<std::pair<std::string, scoped_refptr<WebNNTensorImpl>>>
      name_to_output_tensors;
  name_to_output_tensors.reserve(named_outputs.size());
  for (const auto& [name, tensor_handle] : named_outputs) {
    scoped_refptr<WebNNTensorImpl> output_tensor =
        GetWebNNTensorImpl(tensor_handle);
    if (!output_tensor) {
      return;
    }

    // Output MLTensor is always dispatchable, which isn't allowed when used as
    // a graph constant.
    if (output_tensor->usage().Has(MLTensorUsageFlags::kGraphConstant)) {
      GetMojoReceiver().ReportBadMessage(kBadMessageInvalidTensor);
      return;
    }

    name_to_output_tensors.emplace_back(name, std::move(output_tensor));
  }

  base::flat_map<std::string, scoped_refptr<WebNNTensorImpl>>
      name_to_output_tensor_map(std::move(name_to_output_tensors));
  if (!ValidateWebNNTensors(
          name_to_output_tensor_map,
          graph_impl->compute_resource_info().output_names_to_descriptors)) {
    GetMojoReceiver().ReportBadMessage(kBadMessageInvalidTensor);
    return;
  }

  graph_impl->RunDispatch(
      std::move(name_to_input_tensor_map), std::move(name_to_output_tensor_map),
      std::move(scoped_trace), GetMojoReceiver().GetBadMessageCallback());
}

void WebNNContextImpl::RemoveWebNNTensorImpl(
    const blink::WebNNTensorToken& handle) {
  const auto it = tensor_impls_.find(handle);
  CHECK(it != tensor_impls_.end());
  if (!it->get()->has_shared_image()) {
    memory_type_tracker_.TrackMemFree(it->get()->PackedByteLength());
  }
  // Upon calling erase, the handle will no longer refer to a valid
  // `WebNNTensorImpl`.
  tensor_impls_.erase(it);
}

void WebNNContextImpl::RemoveWebNNGraphImpl(
    const blink::WebNNGraphToken& handle) {
  const auto it = graph_impls_.find(handle);
  CHECK(it != graph_impls_.end());
  // Upon calling erase, the handle will no longer refer to a valid
  // `WebNNGraphImpl`.
  graph_impls_.erase(it);
}

const ContextProperties& WebNNContextImpl::properties() const {
  return properties_;
}

const mojom::CreateContextOptions& WebNNContextImpl::options() const {
  return *options_;
}

void WebNNContextImpl::OnLost(const std::string& reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ResetMojoReceiver(reason);
  OnDisconnect();
}

void WebNNContextImpl::RunOrScheduleTaskWithThisContext(
    RunOrScheduleTaskCallback task,
    const gpu::SyncToken& fence) {
  // Safe to use std::ref because `this` owns gpu_sequence_ and
  // its deletion drops all pending tasks before the context is destroyed.
  RunOrScheduleTask(base::BindOnce(std::move(task), std::ref(*this)), fence);
}

void WebNNContextImpl::RunOrScheduleTask(base::OnceClosure task,
                                         const gpu::SyncToken& fence,
                                         const gpu::SyncToken& release) {
  if (gpu_sequence_) {
    gpu_sequence_->ScheduleGpuTask(std::move(task), fence, release);
    return;
  }

  DCHECK(!fence.HasData());
  DCHECK(!release.HasData());
  DCHECK(owning_task_runner()->RunsTasksInCurrentSequence());
  std::move(task).Run();
}

scoped_refptr<WebNNTensorImpl> WebNNContextImpl::GetWebNNTensorImpl(
    const blink::WebNNTensorToken& tensor_handle) {
  const auto it = tensor_impls_.find(tensor_handle);
  if (it == tensor_impls_.end()) {
    GetMojoReceiver().ReportBadMessage(kBadMessageInvalidTensor);
    return nullptr;
  }
  return it->get();
}

bool WebNNContextImpl::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  std::string dump_name = base::StringPrintf("webnn/context_0x%x", tracing_id_);
  auto* const dump = pmd->CreateAllocatorDump(dump_name);
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  memory_type_tracker_.memory_tracker()->GetSize());
  return true;
}

ContextProperties WebNNContextImpl::IntersectWithBaseProperties(
    ContextProperties backend_context_properties) {
  // A specific maximum rank is still under discussion, but 8 is the highest
  // supported by any backend.
  constexpr SupportedRanks kNonScalarMaxRank = SupportedRanks::NonScalarUpTo(8);
  constexpr SupportedRanks kAtLeast2D{2, 8};

  // Only intersects for ones that have limits defined in the specification.
  // For ones that has no limit, no need to intersect with
  // `SupportedDataTypes::All()`.
  backend_context_properties.data_type_limits.arg_min_max_input.ranks
      .IntersectWith(kNonScalarMaxRank);
  backend_context_properties.data_type_limits.arg_min_max_output.data_types
      .RetainAll(DataTypeConstraint::kInt32To64);
  backend_context_properties.data_type_limits.batch_normalization_input
      .IntersectWith({DataTypeConstraint::kFloat16To32, kNonScalarMaxRank});
  backend_context_properties.data_type_limits.batch_normalization_mean
      .IntersectWith(
          {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(1)});
  backend_context_properties.data_type_limits.concat_inputs.ranks.IntersectWith(
      kNonScalarMaxRank);
  backend_context_properties.data_type_limits.conv2d_input.IntersectWith(
      {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(4)});
  backend_context_properties.data_type_limits.conv2d_bias.IntersectWith(
      {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(1)});
  backend_context_properties.data_type_limits.conv_transpose2d_input
      .IntersectWith(
          {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(4)});
  backend_context_properties.data_type_limits.conv_transpose2d_bias
      .IntersectWith(
          {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(1)});
  backend_context_properties.data_type_limits.cumulative_sum_input
      .IntersectWith(
          {DataTypeConstraint::kFloat16To32Ints32To64, kNonScalarMaxRank});
  backend_context_properties.data_type_limits.dequantize_linear_input.data_types
      .RetainAll(DataTypeConstraint::kInts4Ints8Ints32);
  backend_context_properties.data_type_limits.dequantize_linear_scale.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.logical_and_input.data_types
      .RetainAll(DataTypeConstraint::kUint8);
  backend_context_properties.data_type_limits.logical_or_input.data_types
      .RetainAll(DataTypeConstraint::kUint8);
  backend_context_properties.data_type_limits.logical_xor_input.data_types
      .RetainAll(DataTypeConstraint::kUint8);
  backend_context_properties.data_type_limits.logical_not_input.data_types
      .RetainAll(DataTypeConstraint::kUint8);
  backend_context_properties.data_type_limits.is_nan_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.is_infinite_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.logical_output.RetainAll(
      DataTypeConstraint::kUint8);
  backend_context_properties.data_type_limits.abs_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32Int8To64);
  backend_context_properties.data_type_limits.ceil_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.cos_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.erf_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.exp_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.floor_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.log_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.neg_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32Int8To64);
  backend_context_properties.data_type_limits.reciprocal_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.round_even_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.sign_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32Int8To64);
  backend_context_properties.data_type_limits.sin_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.sqrt_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.tan_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.elu_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.gather_input.ranks.IntersectWith(
      kNonScalarMaxRank);
  backend_context_properties.data_type_limits.gather_indices.data_types
      .RetainAll(DataTypeConstraint::kGatherScatterIndicesSupportedDataTypes);
  backend_context_properties.data_type_limits.gather_elements_input.ranks
      .IntersectWith(kNonScalarMaxRank);
  backend_context_properties.data_type_limits.gather_elements_indices
      .IntersectWith(
          {DataTypeConstraint::kGatherScatterIndicesSupportedDataTypes,
           kNonScalarMaxRank});
  backend_context_properties.data_type_limits.gather_nd_input.ranks
      .IntersectWith(kNonScalarMaxRank);
  backend_context_properties.data_type_limits.gather_nd_indices.IntersectWith(
      {DataTypeConstraint::kGatherScatterIndicesSupportedDataTypes,
       kNonScalarMaxRank});
  backend_context_properties.data_type_limits.gelu_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.gemm_a.IntersectWith(
      {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(2)});
  backend_context_properties.data_type_limits.gemm_c.IntersectWith(
      {DataTypeConstraint::kFloat16To32, SupportedRanks::UpTo(2)});
  backend_context_properties.data_type_limits.gru_input.IntersectWith(
      {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(3)});
  backend_context_properties.data_type_limits.gru_bias.IntersectWith(
      {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(2)});
  backend_context_properties.data_type_limits.gru_output_sequence.IntersectWith(
      {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(4)});
  backend_context_properties.data_type_limits.gru_cell_input.IntersectWith(
      {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(2)});
  backend_context_properties.data_type_limits.gru_cell_bias.IntersectWith(
      {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(1)});
  backend_context_properties.data_type_limits.hard_sigmoid_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.hard_swish_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.instance_normalization_input
      .IntersectWith(
          {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(4)});
  backend_context_properties.data_type_limits.instance_normalization_scale
      .IntersectWith(
          {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(1)});
  backend_context_properties.data_type_limits.layer_normalization_input
      .data_types.RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.leaky_relu_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.linear_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.lstm_input.IntersectWith(
      {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(3)});
  backend_context_properties.data_type_limits.lstm_bias.IntersectWith(
      {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(2)});
  backend_context_properties.data_type_limits.lstm_output_sequence
      .IntersectWith(
          {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(4)});
  backend_context_properties.data_type_limits.lstm_cell_input.IntersectWith(
      {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(2)});
  backend_context_properties.data_type_limits.lstm_cell_bias.IntersectWith(
      {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(1)});
  backend_context_properties.data_type_limits.matmul_input.IntersectWith(
      {DataTypeConstraint::kFloat16To32, kAtLeast2D});
  backend_context_properties.data_type_limits.average_pool2d_input
      .IntersectWith(
          {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(4)});
  backend_context_properties.data_type_limits.l2_pool2d_input.IntersectWith(
      {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(4)});
  backend_context_properties.data_type_limits.max_pool2d_input.ranks
      .IntersectWith(SupportedRanks::Exactly(4));
  backend_context_properties.data_type_limits.prelu_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32Int8To64);
  backend_context_properties.data_type_limits.quantize_linear_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.quantize_linear_zero_point
      .data_types.RetainAll(DataTypeConstraint::kInts4Ints8Ints32);
  backend_context_properties.data_type_limits.reduce_l1_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32Ints32To64);
  backend_context_properties.data_type_limits.reduce_l2_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.reduce_log_sum_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.reduce_log_sum_exp_input
      .data_types.RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.reduce_mean_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.reduce_product_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32Ints32To64);
  backend_context_properties.data_type_limits.reduce_sum_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32Ints32To64);
  backend_context_properties.data_type_limits.reduce_sum_square_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32Ints32To64);
  backend_context_properties.data_type_limits.relu_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32Int8To64);
  backend_context_properties.data_type_limits.resample2d_input.IntersectWith(
      {DataTypeConstraint::kFloat16To32, SupportedRanks::Exactly(4)});
  backend_context_properties.data_type_limits.scatter_elements_input.ranks
      .IntersectWith(kNonScalarMaxRank);
  backend_context_properties.data_type_limits.scatter_elements_indices
      .data_types.RetainAll(
          DataTypeConstraint::kGatherScatterIndicesSupportedDataTypes);
  backend_context_properties.data_type_limits.scatter_nd_input.ranks
      .IntersectWith(kNonScalarMaxRank);
  backend_context_properties.data_type_limits.scatter_nd_indices.IntersectWith(
      {DataTypeConstraint::kGatherScatterIndicesSupportedDataTypes,
       kNonScalarMaxRank});
  backend_context_properties.data_type_limits.sigmoid_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.softmax_input.IntersectWith(
      {DataTypeConstraint::kFloat16To32, kNonScalarMaxRank});
  backend_context_properties.data_type_limits.softplus_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.softsign_input.data_types
      .RetainAll(DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.split_input.ranks.IntersectWith(
      kNonScalarMaxRank);
  backend_context_properties.data_type_limits.tanh_input.data_types.RetainAll(
      DataTypeConstraint::kFloat16To32);
  backend_context_properties.data_type_limits.triangular_input.ranks
      .IntersectWith(kAtLeast2D);
  backend_context_properties.data_type_limits.where_condition.data_types
      .RetainAll(DataTypeConstraint::kUint8);
  return backend_context_properties;
}

}  // namespace webnn
