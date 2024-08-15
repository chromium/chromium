// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/coreml/buffer_impl_coreml.h"

#import <CoreML/CoreML.h>

#include <optional>

#include "mojo/public/cpp/base/big_buffer.h"
#include "services/webnn/coreml/context_impl_coreml.h"
#include "services/webnn/coreml/utils_coreml.h"
#include "services/webnn/public/cpp/operand_descriptor.h"

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
      // Unsupported data types for MLMultiArrays in CoreML.
      NOTREACHED();
  }
}

}  // namespace

// static
base::expected<std::unique_ptr<WebNNBufferImpl>, mojom::ErrorPtr>
BufferImplCoreml::Create(
    mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
    WebNNContextImpl* context,
    mojom::BufferInfoPtr buffer_info) {
  // TODO(crbug.com/343638938): Check `MLBufferUsageFlags` and use an
  // IOSurface to facilitate zero-copy buffer sharing with WebGPU when possible.

  // TODO(crbug.com/329482489): Move this check to the renderer and throw a
  // TypeError.
  if (buffer_info->descriptor.Rank() > 5) {
    LOG(ERROR) << "[WebNN] Buffer rank is too large.";
    return base::unexpected(mojom::Error::New(
        mojom::Error::Code::kNotSupportedError, "Buffer rank is too large."));
  }

  // Limit to INT_MAX for security reasons (similar to PartitionAlloc).
  //
  // TODO(crbug.com/356670455): Consider relaxing this restriction, especially
  // if partial reads and writes of an MLBuffer are supported.
  //
  // TODO(crbug.com/356670455): Consider moving this check to the renderer and
  // throwing a TypeError.
  if (!base::IsValueInRangeForNumericType<int>(
          buffer_info->descriptor.PackedByteLength())) {
    LOG(ERROR) << "[WebNN] Buffer is too large to create.";
    return base::unexpected(mojom::Error::New(
        mojom::Error::Code::kUnknownError, "Buffer is too large to create."));
  }

  NSMutableArray<NSNumber*>* ns_shape = [[NSMutableArray alloc] init];
  for (uint32_t dimension : buffer_info->descriptor.shape()) {
    [ns_shape addObject:[[NSNumber alloc] initWithUnsignedLong:dimension]];
  }

  NSError* error = nil;
  MLMultiArray* multi_array = [[MLMultiArray alloc]
      initWithShape:ns_shape
           dataType:ToMLMultiArrayDataType(buffer_info->descriptor.data_type())
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
    memset(mutable_bytes, 0, size);
  }];
  block_executing_synchronously = false;

  return base::WrapUnique(
      new BufferImplCoreml(std::move(receiver), context, std::move(buffer_info),
                           multi_array, base::PassKey<BufferImplCoreml>()));
}

BufferImplCoreml::BufferImplCoreml(
    mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
    WebNNContextImpl* context,
    mojom::BufferInfoPtr buffer_info,
    MLMultiArray* multi_array,
    base::PassKey<BufferImplCoreml> /*pass_key*/)
    : WebNNBufferImpl(std::move(receiver), context, std::move(buffer_info)),
      multi_array_(multi_array) {}

BufferImplCoreml::~BufferImplCoreml() = default;

void BufferImplCoreml::ReadBufferImpl(
    mojom::WebNNBuffer::ReadBufferCallback callback) {
  mojo_base::BigBuffer output_buffer(PackedByteLength());
  ReadFromMLMultiArray(multi_array_, output_buffer);

  std::move(callback).Run(
      mojom::ReadBufferResult::NewBuffer(std::move(output_buffer)));
}

void BufferImplCoreml::WriteBufferImpl(mojo_base::BigBuffer src_buffer) {
  WriteToMLMultiArray(multi_array_, src_buffer);
}

MLFeatureValue* BufferImplCoreml::AsFeatureValue() {
  return [MLFeatureValue featureValueWithMultiArray:multi_array_];
}

}  // namespace webnn::coreml
