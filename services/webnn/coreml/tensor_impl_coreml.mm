// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/coreml/tensor_impl_coreml.h"

#import <CoreFoundation/CoreFoundation.h>
#import <CoreML/CoreML.h>
#import <CoreVideo/CVPixelBuffer.h>
#import <IOSurface/IOSurfaceRef.h>

#include "base/apple/bridging.h"
#include "base/compiler_specific.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/webnn/coreml/buffer_content_coreml.h"
#include "services/webnn/coreml/context_impl_coreml.h"
#include "services/webnn/coreml/utils_coreml.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "services/webnn/queueable_resource_state.h"
#include "services/webnn/resource_task.h"

namespace webnn::coreml {

namespace {

MLMultiArrayDataType ToMLMultiArrayDataType(OperandDataType data_type) {
  switch (data_type) {
    case OperandDataType::kFloat32:
      return MLMultiArrayDataTypeFloat32;
    case OperandDataType::kFloat16:
      return MLMultiArrayDataTypeFloat16;
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

NSArray<NSNumber*>* ShapeToNSArray(base::span<const uint32_t> shape) {
  NSMutableArray<NSNumber*>* ns_shape = [[NSMutableArray alloc] init];
  if (shape.empty()) {
    // Allocate a one-element array to hold the value of a scalar tensor.
    [ns_shape addObject:[[NSNumber alloc] initWithUnsignedLong:1ul]];
  } else {
    for (uint32_t dimension : shape) {
      [ns_shape addObject:[[NSNumber alloc] initWithUnsignedLong:dimension]];
    }
  }

  return ns_shape;
}

// Creates an MLMultiArray given a data type and shape. See documentation here:
// https://developer.apple.com/documentation/coreml/mlmultiarray/init(shape:datatype:)
API_AVAILABLE(macos(12.3))
MLMultiArray* CreateMultiArrayFromDescriptor(OperandDescriptor descriptor) {
  NSArray<NSNumber*>* shape = ShapeToNSArray(descriptor.shape());

  NSError* error = nil;
  MLMultiArray* multi_array = [[MLMultiArray alloc]
      initWithShape:shape
           dataType:ToMLMultiArrayDataType(descriptor.data_type())
              error:&error];
  if (error) {
    LOG(ERROR) << "[WebNN] Failed to allocate tensor: " << error;
    return nil;
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
    UNSAFE_TODO(memset(mutable_bytes, 0, size));
  }];
  block_executing_synchronously = false;

  return multi_array;
}

// Creates an MLMultiArray by wrapping an IOSurface wrapped by a CVPixelBuffer.
// This is only supported for float16 tensors. See the documentation here:
// https://developer.apple.com/documentation/coreml/mlmultiarray/init(pixelbuffer:shape:)
API_AVAILABLE(macos(12.0))
MLMultiArray* CreateMultiArrayBackedByIOSurface(OperandDescriptor descriptor) {
  CHECK_EQ(descriptor.data_type(), OperandDataType::kFloat16);

  // The pixel buffer's width must match the last dimension of the tensor.
  NSArray<NSNumber*>* shape = ShapeToNSArray(descriptor.shape());
  NSNumber* width = shape.lastObject;
  NSNumber* height =
      @(descriptor.NumberOfElements() / static_cast<size_t>(width.intValue));

  NSDictionary* iosurface_properties = @{
    (NSString*)kIOSurfaceWidth : width,
    (NSString*)kIOSurfaceHeight : height,
    (NSString*)kIOSurfaceBytesPerElement : @(2),
    // This is the only supported data type for importing an MLMultiArray from a
    // CVPixelBuffer.
    (NSString*)kIOSurfacePixelFormat : @(kCVPixelFormatType_OneComponent16Half),
  };

  IOSurfaceRef surface =
      IOSurfaceCreate(base::apple::NSToCFPtrCast(iosurface_properties));

  CVPixelBufferRef pixel_buffer = nil;
  CVReturn pixel_buffer_result = CVPixelBufferCreateWithIOSurface(
      kCFAllocatorDefault, surface,
      /*pixelBufferAttributes=*/nil, &pixel_buffer);
  if (pixel_buffer_result != kCVReturnSuccess) {
    LOG(ERROR) << "[WebNN] Failed to allocate tensor: " << pixel_buffer_result;
    return nil;
  }

  return [[MLMultiArray alloc] initWithPixelBuffer:pixel_buffer shape:shape];
}

}  // namespace

// static
base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr>
TensorImplCoreml::Create(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    base::WeakPtr<WebNNContextImpl> context,
    mojom::TensorInfoPtr tensor_info) {
  // TODO(crbug.com/329482489): Move this check to the renderer and throw a
  // TypeError.
  if (tensor_info->descriptor.Rank() > 5) {
    LOG(ERROR) << "[WebNN] Tensor rank is too large.";
    return base::unexpected(mojom::Error::New(
        mojom::Error::Code::kNotSupportedError, "Tensor rank is too large."));
  }

  CHECK(base::IsValueInRangeForNumericType<int>(
      tensor_info->descriptor.PackedByteLength()));

  MLMultiArray* multi_array = nil;
  if (tensor_info->descriptor.data_type() == OperandDataType::kFloat16) {
    // TODO(https://crbug.com/333392274): Consider not using IOSurface when
    // WebGPU interop is not requested.
    multi_array = CreateMultiArrayBackedByIOSurface(tensor_info->descriptor);
  } else if (tensor_info->usage.Has(MLTensorUsageFlags::kWebGpuInterop)) {
    // TODO(https://crbug.com/333392274): Support WebGPU interop with more
    // than just float16 tensors.
    return base::unexpected(
        mojom::Error::New(mojom::Error::Code::kUnknownError,
                          "Interoperability with WebGPU is only supported "
                          "when using float16 tensors."));
  } else {
    multi_array = CreateMultiArrayFromDescriptor(tensor_info->descriptor);
  }
  if (!multi_array) {
    return base::unexpected(mojom::Error::New(mojom::Error::Code::kUnknownError,
                                              "Failed to allocate tensor."));
  }

  auto buffer_content = std::make_unique<BufferContent>(std::move(multi_array));
  auto buffer_state =
      base::MakeRefCounted<QueueableResourceState<BufferContent>>(
          std::move(buffer_content));
  return base::MakeRefCounted<TensorImplCoreml>(
      std::move(receiver), std::move(context), std::move(tensor_info),
      std::move(buffer_state),
      /*representation=*/
      RepresentationPtr{nullptr, OnTaskRunnerDeleter(nullptr)},
      base::PassKey<TensorImplCoreml>());
}

// static
base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr>
TensorImplCoreml::Create(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    base::WeakPtr<WebNNContextImpl> context,
    mojom::TensorInfoPtr tensor_info,
    RepresentationPtr representation) {
  if (tensor_info->descriptor.data_type() != OperandDataType::kFloat16) {
    return base::unexpected(
        mojom::Error::New(mojom::Error::Code::kUnknownError,
                          "Unsupported data type for WebGPU interop."));
  }
  IOSurfaceRef io_surface = representation->GetIOSurface();

  if (IOSurfaceGetBytesPerElement(io_surface) != 2) {
    return base::unexpected(
        mojom::Error::New(mojom::Error::Code::kUnknownError,
                          "Invalid IOSurface: bytes per element is not 2."));
  }
  if (IOSurfaceGetPixelFormat(io_surface) !=
      kCVPixelFormatType_OneComponent16Half) {
    return base::unexpected(mojom::Error::New(
        mojom::Error::Code::kUnknownError,
        "Invalid IOSurface: pixel format is not OneComponent16Half."));
  }
  size_t width = 1ul;
  const std::vector<uint32_t>& shape = tensor_info->descriptor.shape();
  if (!shape.empty()) {
    width = shape.back();
  }
  size_t height = tensor_info->descriptor.NumberOfElements() / width;
  if (height != IOSurfaceGetHeight(io_surface) ||
      width != IOSurfaceGetWidth(io_surface)) {
    return base::unexpected(mojom::Error::New(
        mojom::Error::Code::kUnknownError,
        "Invalid IOSurface: width and height doesn't match with tensor."));
  }

  CVPixelBufferRef pixel_buffer = nullptr;
  CVReturn pixel_buffer_result = CVPixelBufferCreateWithIOSurface(
      kCFAllocatorDefault, io_surface,
      /*pixelBufferAttributes=*/nil, &pixel_buffer);
  if (pixel_buffer_result != kCVReturnSuccess) {
    LOG(ERROR) << "[WebNN] Failed to create pixel buffer from IOSurface: "
               << pixel_buffer_result;
    return base::unexpected(
        mojom::Error::New(mojom::Error::Code::kUnknownError,
                          "Failed to create pixel buffer from IOSurface."));
  }

  MLMultiArray* multi_array =
      [[MLMultiArray alloc] initWithPixelBuffer:pixel_buffer
                                          shape:ShapeToNSArray(shape)];
  if (!multi_array) {
    return base::unexpected(mojom::Error::New(mojom::Error::Code::kUnknownError,
                                              "Failed to allocate tensor."));
  }

  auto buffer_content = std::make_unique<BufferContent>(std::move(multi_array));
  auto buffer_state =
      base::MakeRefCounted<QueueableResourceState<BufferContent>>(
          std::move(buffer_content));
  return base::MakeRefCounted<TensorImplCoreml>(
      std::move(receiver), std::move(context), std::move(tensor_info),
      std::move(buffer_state), std::move(representation),
      base::PassKey<TensorImplCoreml>());
}

TensorImplCoreml::TensorImplCoreml(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    base::WeakPtr<WebNNContextImpl> context,
    mojom::TensorInfoPtr tensor_info,
    scoped_refptr<QueueableResourceState<BufferContent>> buffer_state,
    RepresentationPtr representation,
    base::PassKey<TensorImplCoreml> /*pass_key*/)
    : WebNNTensorImpl(std::move(receiver),
                      std::move(context),
                      std::move(tensor_info),
                      std::move(representation)),
      buffer_state_(std::move(buffer_state)) {}

TensorImplCoreml::~TensorImplCoreml() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
}

void TensorImplCoreml::ReadTensorImpl(
    mojom::WebNNTensor::ReadTensorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  ScopedTrace scoped_trace("TensorImplCoreml::ReadTensorImpl");

  // Lock the buffer contents as shared/read-only.
  std::vector<scoped_refptr<QueueableResourceStateBase>> shared_resources = {
      buffer_state_};

  scoped_trace.AddStep("Wait for tensor");
  auto task = base::MakeRefCounted<ResourceTask>(
      std::move(shared_resources),
      /*exclusive_resources=*/
      std::vector<scoped_refptr<QueueableResourceStateBase>>(),
      base::BindOnce(
          [](scoped_refptr<QueueableResourceState<BufferContent>> buffer_state,
             ReadTensorCallback read_tensor_result_callback,
             ScopedTrace scoped_trace, base::OnceClosure completion_closure) {
            scoped_trace.AddStep("Begin read");

            // Read from the underlying buffer contents, which are kept alive
            // until `completion_closure` is run.
            buffer_state->GetSharedLockedResource().Read(base::BindOnce(
                [](base::OnceClosure completion_closure,
                   ReadTensorCallback read_tensor_result_callback,
                   ScopedTrace scoped_trace,
                   mojo_base::BigBuffer output_buffer) {
                  scoped_trace.AddStep("End read");
                  // Unlock the buffer contents.
                  std::move(completion_closure).Run();

                  std::move(read_tensor_result_callback)
                      .Run(mojom::ReadTensorResult::NewBuffer(
                          std::move(output_buffer)));
                },
                std::move(completion_closure),
                std::move(read_tensor_result_callback),
                std::move(scoped_trace)));
          },
          buffer_state_, std::move(callback), std::move(scoped_trace)));
  task->Enqueue();
}

void TensorImplCoreml::WriteTensorImpl(mojo_base::BigBuffer src_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  ScopedTrace scoped_trace("TensorImplCoreml::WriteTensorImpl");

  // Take an exclusive lock to the buffer contents while writing.
  std::vector<scoped_refptr<QueueableResourceStateBase>> exclusive_resources = {
      buffer_state_};

  scoped_trace.AddStep("Wait for tensor");
  auto task = base::MakeRefCounted<ResourceTask>(
      /*shared_resources=*/
      std::vector<scoped_refptr<QueueableResourceStateBase>>(),
      std::move(exclusive_resources),
      base::BindOnce(
          [](scoped_refptr<QueueableResourceState<BufferContent>> buffer_state,
             mojo_base::BigBuffer src_buffer, ScopedTrace scoped_trace,
             base::OnceClosure completion_closure) {
            scoped_trace.AddStep("Begin write");
            // Write to the underlying buffer contents, which are kept alive
            // until `completion_closure` is run.
            buffer_state->GetExclusivelyLockedResource()->Write(
                src_buffer,
                base::BindOnce(
                    [](base::OnceClosure completion_closure,
                       ScopedTrace scoped_trace) {
                      scoped_trace.AddStep("End write");
                      std::move(completion_closure).Run();
                    },
                    std::move(completion_closure), std::move(scoped_trace)));
          },
          buffer_state_, std::move(src_buffer), std::move(scoped_trace)));
  task->Enqueue();
}

const scoped_refptr<QueueableResourceState<BufferContent>>&
TensorImplCoreml::GetBufferState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  return buffer_state_;
}

bool TensorImplCoreml::ImportTensorImpl() {
  CHECK(representation_);
  // Tensor will own the access.
  representation_access_ =
      ScopedAccessPtr(representation_->BeginScopedAccess().release(),
                      OnTaskRunnerDeleter(context_->main_task_runner()));
  if (!representation_access_) {
    return false;
  }

  // Always true since CoreML requires no device synchronization.
  return true;
}

void TensorImplCoreml::ExportTensorImpl(ScopedAccessPtr access,
                                        ExportTensorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);
  // Take an exclusive lock to the buffer contents to wait for all existing
  // operations to finish.
  std::vector<scoped_refptr<QueueableResourceStateBase>> exclusive_resources = {
      buffer_state_};

  auto task = base::MakeRefCounted<ResourceTask>(
      /*shared_resources=*/
      std::vector<scoped_refptr<QueueableResourceStateBase>>(),
      std::move(exclusive_resources),
      base::BindOnce(
          [](base::WeakPtr<WebNNContextImpl> context,
             ExportTensorCallback callback,
             base::OnceClosure completion_closure) {
            std::move(completion_closure).Run();
            if (!context) {
              return;
            }

            context->scheduler_task_runner()->PostTask(
                FROM_HERE,
                base::BindOnce(
                    [](base::WeakPtr<WebNNContextImpl> context,
                       ExportTensorCallback callback) {
                      if (!context) {
                        return;
                      }
                      std::move(callback).Run(context->GenVerifiedSyncToken());
                    },
                    context, std::move(callback)));
          },
          context_, std::move(callback)));
  task->Enqueue();
}

}  // namespace webnn::coreml
