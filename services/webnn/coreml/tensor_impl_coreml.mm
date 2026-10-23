// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/coreml/tensor_impl_coreml.h"

#import <CoreFoundation/CoreFoundation.h>
#import <CoreML/CoreML.h>
#import <CoreVideo/CVPixelBuffer.h>
#import <IOSurface/IOSurfaceRef.h>

#include "base/apple/bridging.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/types/expected.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/webnn/coreml/context_impl_coreml.h"
#include "services/webnn/coreml/utils_coreml.h"
#include "services/webnn/error.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"

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

  if (!surface) {
    LOG(ERROR) << "[WebNN] Failed to allocate IOSurface.";
    return nil;
  }

  // Zero-initialize the IOSurface. Calling IOSurfaceLock/IOSurfaceUnlock
  // appears to be sufficient. https://crbug.com/40455843#comment18
  CHECK_EQ(IOSurfaceLock(surface, 0, NULL), KERN_SUCCESS);
  CHECK_EQ(IOSurfaceUnlock(surface, 0, NULL), KERN_SUCCESS);

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
    WebNNContextImpl& context,
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
  if (tensor_info->usage.Has(MLTensorUsageFlags::kWebGpuInterop)) {
    if (tensor_info->descriptor.data_type() == OperandDataType::kFloat16) {
      multi_array = CreateMultiArrayBackedByIOSurface(tensor_info->descriptor);
    } else {
      // TODO(https://crbug.com/333392274): Support WebGPU interop with more
      // than just float16 tensors.
      return base::unexpected(
          mojom::Error::New(mojom::Error::Code::kUnknownError,
                            "Interoperability with WebGPU is only supported "
                            "when using float16 tensors."));
    }
  } else {
    multi_array = CreateMultiArrayFromDescriptor(tensor_info->descriptor);
  }
  if (!multi_array) {
    return base::unexpected(mojom::Error::New(mojom::Error::Code::kUnknownError,
                                              "Failed to allocate tensor."));
  }

  return base::MakeRefCounted<TensorImplCoreml>(
      std::move(receiver), context, std::move(tensor_info),
      std::move(multi_array),
      /*representation=*/
      RepresentationPtr{nullptr, OnTaskRunnerDeleterWithWait(nullptr)},
      base::PassKey<TensorImplCoreml>());
}

// static
base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr>
TensorImplCoreml::Create(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    WebNNContextImpl& context,
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

  return base::MakeRefCounted<TensorImplCoreml>(
      std::move(receiver), context, std::move(tensor_info),
      std::move(multi_array), std::move(representation),
      base::PassKey<TensorImplCoreml>());
}

TensorImplCoreml::TensorImplCoreml(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    WebNNContextImpl& context,
    mojom::TensorInfoPtr tensor_info,
    MLMultiArray* multi_array,
    RepresentationPtr representation,
    base::PassKey<TensorImplCoreml> /*pass_key*/)
    : WebNNTensorImpl(std::move(receiver),
                      context,
                      std::move(tensor_info),
                      std::move(representation)),
      multi_array_(std::move(multi_array)) {}

TensorImplCoreml::~TensorImplCoreml() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

MLFeatureValue* TensorImplCoreml::AsFeatureValue() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return [MLFeatureValue featureValueWithMultiArray:multi_array_];
}

void TensorImplCoreml::ReadTensorImpl(
    mojom::WebNNTensor::ReadTensorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ScopedTrace scoped_trace("TensorImplCoreml::ReadTensorImpl");
  scoped_trace.AddStep("Begin read");
  mojo_base::BigBuffer output_buffer = ReadFromMLMultiArray(multi_array_);
  scoped_trace.AddStep("End read");

  std::move(callback).Run(
      mojom::ReadTensorResult::NewBuffer(std::move(output_buffer)));
}

void TensorImplCoreml::WriteTensorImpl(mojo_base::BigBuffer src_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ScopedTrace scoped_trace("TensorImplCoreml::WriteTensorImpl");
  scoped_trace.AddStep("Begin write");
  WriteToMLMultiArray(multi_array_, src_buffer);
  scoped_trace.AddStep("End write");
}

bool TensorImplCoreml::ImportTensorImpl(ScopedAccessPtr access) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  representation_access_ = std::move(access);

  // Always true since CoreML requires no device synchronization.
  return true;
}

void TensorImplCoreml::ExportTensorImpl(ScopedAccessPtr access) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Since we wait for all WebNN operations to complete, we only need to release
  // the ScopedAccess to end WebNN access.
}

}  // namespace webnn::coreml
