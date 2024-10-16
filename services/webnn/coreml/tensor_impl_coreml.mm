// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/coreml/tensor_impl_coreml.h"

#import <CoreML/CoreML.h>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/webnn/coreml/buffer_content.h"
#include "services/webnn/coreml/context_impl_coreml.h"
#include "services/webnn/coreml/utils_coreml.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/queueable_resource_state.h"
#include "services/webnn/resource_task.h"

namespace webnn::coreml {

namespace {

MLMultiArrayDataType ToMLMultiArrayDataType(OperandDataType data_type) {
  switch (data_type) {
    case OperandDataType::kFloat32:
      return MLMultiArrayDataTypeFloat32;
    case OperandDataType::kFloat16:
      if (__builtin_available(macOS 12, *)) {
        return MLMultiArrayDataTypeFloat16;
      }
      NOTREACHED();
    case OperandDataType::kInt32:
      return MLMultiArrayDataTypeInt32;
    case OperandDataType::kUint32:
    case OperandDataType::kInt64:
    case OperandDataType::kUint64:
    case OperandDataType::kInt8:
    case OperandDataType::kUint8:
    case OperandDataType::kInt4:
    case OperandDataType::kUint4:
      // Unsupported data types for MLMultiArrays in CoreML.
      NOTREACHED();
  }
}

}  // namespace

// static
base::expected<std::unique_ptr<WebNNTensorImpl>, mojom::ErrorPtr>
TensorImplCoreml::Create(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    WebNNContextImpl* context,
    mojom::TensorInfoPtr tensor_info) {
  // TODO(crbug.com/343638938): Check `MLTensorUsageFlags` and use an
  // IOSurface to facilitate zero-copy buffer sharing with WebGPU when possible.

  // TODO(crbug.com/329482489): Move this check to the renderer and throw a
  // TypeError.
  if (tensor_info->descriptor.Rank() > 5) {
    LOG(ERROR) << "[WebNN] Tensor rank is too large.";
    return base::unexpected(mojom::Error::New(
        mojom::Error::Code::kNotSupportedError, "Tensor rank is too large."));
  }

  // Limit to INT_MAX for security reasons (similar to PartitionAlloc).
  //
  // TODO(crbug.com/356670455): Consider relaxing this restriction, especially
  // if partial reads and writes of an MLTensor are supported.
  //
  // TODO(crbug.com/356670455): Consider moving this check to the renderer and
  // throwing a TypeError.
  if (!base::IsValueInRangeForNumericType<int>(
          tensor_info->descriptor.PackedByteLength())) {
    LOG(ERROR) << "[WebNN] Tensor is too large to create.";
    return base::unexpected(mojom::Error::New(
        mojom::Error::Code::kUnknownError, "Tensor is too large to create."));
  }

  NSMutableArray<NSNumber*>* ns_shape = [[NSMutableArray alloc] init];
  for (uint32_t dimension : tensor_info->descriptor.shape()) {
    [ns_shape addObject:[[NSNumber alloc] initWithUnsignedLong:dimension]];
  }

  NSError* error = nil;
  MLMultiArray* multi_array = [[MLMultiArray alloc]
      initWithShape:ns_shape
           dataType:ToMLMultiArrayDataType(tensor_info->descriptor.data_type())
              error:&error];
  if (error) {
    LOG(ERROR) << "[WebNN] Failed to allocate buffer: " << error;
    return base::unexpected(mojom::Error::New(mojom::Error::Code::kUnknownError,
                                              "Failed to allocate buffer."));
  }

  // `MLMultiArray` doesn't initialize its contents.
  __block bool block_executing_synchronously = true;
  [multi_array getMutableBytesWithHandler:^(void* mutable_bytes, NSInteger size,
                                            NSArray<NSNumber*>* strides) {
    // TODO(crbug.com/333392274): Refactor this method to assume the handler may
    // be invoked on some other thread. We should not assume that the block
    // will always run synchronously.
    CHECK(block_executing_synchronously);

    // TODO(crbug.com/333392274): Use the `WriteToMLMultiArray()` function
    // which handles non-contiguous buffers.
    memset(mutable_bytes, 0, size);
  }];
  block_executing_synchronously = false;

  auto buffer_content = std::make_unique<BufferContent>(std::move(multi_array));
  auto buffer_state =
      base::MakeRefCounted<QueueableResourceState<BufferContent>>(
          std::move(buffer_content));
  return base::WrapUnique(new TensorImplCoreml(
      std::move(receiver), context, std::move(tensor_info),
      std::move(buffer_state), base::PassKey<TensorImplCoreml>()));
}

TensorImplCoreml::TensorImplCoreml(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    WebNNContextImpl* context,
    mojom::TensorInfoPtr tensor_info,
    scoped_refptr<QueueableResourceState<BufferContent>> buffer_state,
    base::PassKey<TensorImplCoreml> /*pass_key*/)
    : WebNNTensorImpl(std::move(receiver), context, std::move(tensor_info)),
      buffer_state_(std::move(buffer_state)) {}

TensorImplCoreml::~TensorImplCoreml() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TensorImplCoreml::ReadTensorImpl(
    mojom::WebNNTensor::ReadTensorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Lock the buffer contents as shared/read-only.
  std::vector<scoped_refptr<QueueableResourceStateBase>> shared_resources = {
      buffer_state_};

  auto task = base::MakeRefCounted<ResourceTask>(
      std::move(shared_resources),
      /*exclusive_resources=*/
      std::vector<scoped_refptr<QueueableResourceStateBase>>(),
      base::BindOnce(
          [](size_t bytes_to_read,
             scoped_refptr<QueueableResourceState<BufferContent>> buffer_state,
             ReadTensorCallback read_tensor_result_callback,
             base::OnceClosure completion_closure) {
            mojo_base::BigBuffer output_buffer(bytes_to_read);

            // Read from the underlying buffer contents, which are kept alive
            // until `completion_closure` is run.
            buffer_state->GetSharedLockedResource().Read(base::BindOnce(
                [](base::OnceClosure completion_closure,
                   ReadTensorCallback read_tensor_result_callback,
                   mojo_base::BigBuffer output_buffer) {
                  // Unlock the buffer contents.
                  std::move(completion_closure).Run();

                  std::move(read_tensor_result_callback)
                      .Run(mojom::ReadTensorResult::NewBuffer(
                          std::move(output_buffer)));
                },
                std::move(completion_closure),
                std::move(read_tensor_result_callback)));
          },
          /*bytes_to_read=*/PackedByteLength(), buffer_state_,
          std::move(callback)));
  task->Enqueue();
}

void TensorImplCoreml::WriteTensorImpl(mojo_base::BigBuffer src_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Take an exclusive lock to the buffer contents while writing.
  std::vector<scoped_refptr<QueueableResourceStateBase>> exclusive_resources = {
      buffer_state_};

  auto task = base::MakeRefCounted<ResourceTask>(
      /*shared_resources=*/
      std::vector<scoped_refptr<QueueableResourceStateBase>>(),
      std::move(exclusive_resources),
      base::BindOnce(
          [](scoped_refptr<QueueableResourceState<BufferContent>> buffer_state,
             mojo_base::BigBuffer src_buffer,
             base::OnceClosure completion_closure) {
            // Write to the underlying buffer contents, which are kept alive
            // until `completion_closure` is run.
            buffer_state->GetExclusivelyLockedResource()->Write(
                src_buffer, std::move(completion_closure));
          },
          buffer_state_, std::move(src_buffer)));
  task->Enqueue();
}

const scoped_refptr<QueueableResourceState<BufferContent>>&
TensorImplCoreml::GetBufferState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return buffer_state_;
}

}  // namespace webnn::coreml
