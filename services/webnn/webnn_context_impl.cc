// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_context_impl.h"

#include <memory>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "services/webnn/error.h"
#include "services/webnn/public/cpp/data_type_limits.h"
#include "services/webnn/public/cpp/graph_validation_utils.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/supported_data_types.h"
#include "services/webnn/public/cpp/supported_tensors.h"
#include "services/webnn/public/mojom/webnn_context.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/public/mojom/webnn_graph_builder.mojom.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "services/webnn/scoped_sequence.h"
#include "services/webnn/webnn_context_provider_impl.h"
#include "services/webnn/webnn_graph_builder_impl.h"
#include "services/webnn/webnn_graph_impl.h"
#include "services/webnn/webnn_tensor_impl.h"
#include "third_party/tflite/buildflags.h"

#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
#include "third_party/xnnpack/src/include/xnnpack.h"  // nogncheck
#endif  // BUILD_TFLITE_WITH_XNNPACK

namespace webnn {

WebNNContextImpl::WebNNContextImpl(
    mojo::PendingReceiver<mojom::WebNNContext> receiver,
    base::WeakPtr<WebNNContextProviderImpl> context_provider,
    ContextProperties properties,
    mojom::CreateContextOptionsPtr options,
    mojo::ScopedDataPipeConsumerHandle write_tensor_consumer,
    mojo::ScopedDataPipeProducerHandle read_tensor_producer,
    gpu::CommandBufferId command_buffer_id,
    std::unique_ptr<ScopedSequence> sequence,
    scoped_refptr<gpu::MemoryTracker> memory_tracker,
    scoped_refptr<base::SingleThreadTaskRunner> owning_task_runner,
    gpu::SharedImageManager* shared_image_manager,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
    : WebNNObjectBase<mojom::WebNNContext,
                      blink::WebNNContextToken,
                      mojo::Receiver<mojom::WebNNContext>>(
          std::move(receiver),
          sequence->scheduler_task_runner()),
      context_provider_(std::move(context_provider)),
      properties_(IntersectWithBaseProperties(std::move(properties))),
      options_(std::move(options)),
      command_buffer_id_(command_buffer_id),
      sequence_(std::move(sequence)),
      write_tensor_consumer_(std::move(write_tensor_consumer)),
      read_tensor_producer_(std::move(read_tensor_producer)),
      memory_type_tracker_(std::move(memory_tracker)),
      shared_image_manager_(shared_image_manager),
      main_task_runner_(std::move(main_task_runner)),
      owning_task_runner_(std::move(owning_task_runner)) {
#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
  // Initialize XNNPACK
  const xnn_status status = xnn_initialize(/*allocator=*/nullptr);
  CHECK_EQ(status, xnn_status_success);
#endif  // BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
}

WebNNContextImpl::~WebNNContextImpl() {
  // Close all tensor pipes explicitly so no response callbacks are pending as
  // Mojo forbids callbacks that are pending during destruction.
  for (auto impl : tensor_impls_) {
    impl->ResetMojoReceiver();
  }

#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
  // Deinitialize XNNPACK
  const xnn_status status = xnn_deinitialize();
  CHECK_EQ(status, xnn_status_success);
#endif  // BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
}

void WebNNContextImpl::OnDisconnect() {
  if (!main_task_runner_->RunsTasksInCurrentSequence()) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WebNNContextProviderImpl::RemoveWebNNContextImpl,
                       context_provider_, handle()));
    return;
  }

  context_provider_->RemoveWebNNContextImpl(handle());
}

#if BUILDFLAG(IS_WIN)
void WebNNContextImpl::DestroyAllContextsAndKillGpuProcess(
    const std::string& reason) {
  if (!main_task_runner_->RunsTasksInCurrentSequence()) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &WebNNContextProviderImpl::DestroyAllContextsAndKillGpuProcess,
            context_provider_, reason));
    return;
  }

  context_provider_->DestroyAllContextsAndKillGpuProcess(reason);
}
#endif  // BUILDFLAG(IS_WIN)

void WebNNContextImpl::ReportBadGraphBuilderMessage(
    const std::string& message,
    base::PassKey<WebNNGraphBuilderImpl> pass_key) {
  graph_builder_impls_.ReportBadMessage(message);
}

void WebNNContextImpl::TakeGraph(
    scoped_refptr<WebNNGraphImpl> graph_impl,
    base::PassKey<WebNNGraphBuilderImpl> pass_key) {
  graph_impls_.emplace(std::move(graph_impl));
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

  graph_builder_ptr->SetId(id, base::PassKey<WebNNContextImpl>());
}

void WebNNContextImpl::CreateTensor(
    mojom::TensorInfoPtr tensor_info,
    mojo_base::BigBuffer tensor_data,
    mojom::WebNNContext::CreateTensorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
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

  // Associates a `WebNNTensor` instance with this context so the WebNN service
  // can access the implementation.
  tensor_impls_.emplace(*std::move(result));
}

const scoped_refptr<gpu::SchedulerTaskRunner>&
WebNNContextImpl::scheduler_task_runner() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  return sequence_->scheduler_task_runner();
}

void WebNNContextImpl::WaitSyncToken(const gpu::SyncToken& fence) {
  // Prevent WebNN from performing further operations until the specified
  // SyncToken fence has been released.
  base::OnceClosure nop_task = base::DoNothing();
  sequence_->scheduler().ScheduleTask(gpu::Scheduler::Task(
      sequence_->sequence_id(), std::move(nop_task), {fence}));
}

gpu::SyncToken WebNNContextImpl::GenVerifiedSyncToken() {
  gpu::SyncToken verified_release(
      gpu::CommandBufferNamespace::WEBNN_CONTEXT_INTERFACE, command_buffer_id_,
      ++last_sync_token_release_id_);

  // Release the sync token once the sequence has completed execution by
  // appending a no-op task - the sync token will be automatically signaled
  // by the scheduler after this task executes.
  base::OnceClosure nop_task = base::DoNothing();
  sequence_->scheduler().ScheduleTask(gpu::Scheduler::Task(
      sequence_->sequence_id(), std::move(nop_task), {}, verified_release));

  // Verify the release since the sync token could be passed to another Mojo
  // interface which requires verification. The release token was verified by
  // returning it to the renderer only after ScheduleTask was called.
  verified_release.SetVerifyFlush();
  return verified_release;
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

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

  // Wait for the SharedImage to be created.
  WaitSyncToken(fence);

  // Must be a scheduled task since this depends on shared image creation task.
  scheduler_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](WebNNContextImpl* self, mojom::TensorInfoPtr tensor_info,
             const gpu::Mailbox& mailbox, CreateTensorCallback callback) {
            CHECK(self->shared_image_manager_);

            constexpr char kWebNNCreateTensorErrorMessage[] =
                "Failed to create tensor.";

            // Tensor will own the representation.
            WebNNTensorImpl::RepresentationPtr representation(
                self->shared_image_manager_
                    ->ProduceWebNNTensor(mailbox, &self->memory_type_tracker_)
                    .release(),
                OnTaskRunnerDeleter(self->main_task_runner()));
            if (!representation) {
              std::move(callback).Run(ToError<mojom::CreateTensorResult>(
                  mojom::Error::Code::kUnknownError,
                  kWebNNCreateTensorErrorMessage));
              return;
            }

            mojo::PendingAssociatedRemote<mojom::WebNNTensor> remote;
            auto receiver = remote.InitWithNewEndpointAndPassReceiver();

            auto result = self->CreateTensorFromSharedImageImpl(
                std::move(receiver), std::move(tensor_info),
                std::move(representation));
            if (!result.has_value()) {
              std::move(callback).Run(mojom::CreateTensorResult::NewError(
                  std::move(result.error())));
              return;
            }

            if (!result.value()->ImportTensorImpl()) {
              std::move(callback).Run(ToError<mojom::CreateTensorResult>(
                  mojom::Error::Code::kUnknownError,
                  kWebNNCreateTensorErrorMessage));
              return;
            }

            auto success = mojom::CreateTensorSuccess::New(
                std::move(remote), result.value()->handle());
            std::move(callback).Run(
                mojom::CreateTensorResult::NewSuccess(std::move(success)));
            self->tensor_impls_.emplace(*std::move(result));
          },
          // Safe to use base::Unretained because this context owns the sequence
          // used by the task runner to run this task.
          base::Unretained(this), std::move(tensor_info), mailbox,
          std::move(callback)));
}

void WebNNContextImpl::RemoveWebNNTensorImpl(
    const blink::WebNNTensorToken& handle) {
  const auto it = tensor_impls_.find(handle);
  CHECK(it != tensor_impls_.end());
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

void WebNNContextImpl::OnLost(const std::string& reason) {
  // Safe to use base::Unretained because `this` is sequence-bound to
  // scheduler_task_runner_. Deletion occurs via Shutdown(), which drops all
  // pending tasks - including this one - before the object is destroyed.
  scheduler_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](WebNNContextImpl* self, const std::string& reason) {
                       self->GetMojoReceiver().ResetWithReason(
                           /*custom_reason=*/0, reason);
                       self->OnDisconnect();
                     },
                     base::Unretained(this), reason));
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
  backend_context_properties.data_type_limits.dequantize_linear_zero_point
      .data_types.RetainAll(DataTypeConstraint::kInts4Ints8Ints32);
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
