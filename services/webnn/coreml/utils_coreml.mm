// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/coreml/utils_coreml.h"

#include <CoreML/CoreML.h>

#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/containers/span_writer.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/task/bind_post_task.h"
#include "mojo/public/cpp/base/big_buffer.h"

namespace webnn::coreml {

namespace {

uint32_t GetDataTypeByteSize(MLMultiArrayDataType data_type) {
  switch (data_type) {
    case MLMultiArrayDataTypeDouble:
      return 8;
    case MLMultiArrayDataTypeFloat32:
    case MLMultiArrayDataTypeInt32:
      return 4;
    case MLMultiArrayDataTypeFloat16:
      return 2;
  }
}

std::vector<uint32_t> ToStdVector(NSArray<NSNumber*>* ns_array) {
  std::vector<uint32_t> std_vector;
  std_vector.reserve(ns_array.count);
  for (NSNumber* number in ns_array) {
    std_vector.push_back(number.unsignedIntegerValue);
  }
  return std_vector;
}

// Extract data from an `MLMultiArray` - which may not be contiguous - using its
// `shape` and `strides` as appropriate.
void RecursivelyReadFromMLMultiArray(
    base::span<const uint8_t> multi_array_backed_input_buffer,
    uint32_t item_byte_size,
    base::span<const uint32_t> shape,
    base::span<const uint32_t> strides,
    base::span<uint8_t> output_buffer) {
  // Data is packed, copy the whole thing.
  //
  // On the last dimension, the bytes left to read could be more than the bytes
  // left to write because of strides from the previous dimension, but as long
  // as the current stride is 1, we can copy continously.
  if (multi_array_backed_input_buffer.size() == output_buffer.size() ||
      (shape.size() == 1 && strides[0] == 1)) {
    output_buffer.copy_from(
        multi_array_backed_input_buffer.first(output_buffer.size()));
    return;
  }

  CHECK_EQ(output_buffer.size() % shape[0], 0u);
  size_t subspan_size = output_buffer.size() / shape[0];

  base::SpanReader<const uint8_t> reader(multi_array_backed_input_buffer);
  base::SpanWriter<uint8_t> writer(output_buffer);
  for (uint32_t i = 0; i < shape[0]; i++) {
    auto output_subspan = writer.Skip(subspan_size);
    CHECK(output_subspan);
    auto input_subspan = reader.Read(strides[0] * item_byte_size);
    CHECK(input_subspan);
    if (shape.size() == 1) {
      output_subspan->copy_from(input_subspan->first(item_byte_size));
    } else {
      RecursivelyReadFromMLMultiArray(*input_subspan, item_byte_size,
                                      shape.subspan(1u), strides.subspan(1u),
                                      *output_subspan);
    }
  }
}

// Copy data to an `MLMultiArray` - which may not be contiguous - using its
// `shape` and `strides` as appropriate.
void RecursivelyWriteToMLMultiArray(
    base::span<const uint8_t> input_buffer,
    uint32_t item_byte_size,
    base::span<const uint32_t> shape,
    base::span<const uint32_t> strides,
    base::span<uint8_t> multi_array_backed_output_buffer) {
  // Data is packed, copy the whole thing.
  //
  // On the last dimension, the bytes left to write could be more than the bytes
  // left to read because of strides from the previous dimension, but as long as
  // the current stride is 1, we can copy continously.
  if (input_buffer.size() == multi_array_backed_output_buffer.size() ||
      (shape.size() == 1 && strides[0] == 1)) {
    multi_array_backed_output_buffer.copy_prefix_from(input_buffer);
    return;
  }

  CHECK_EQ(input_buffer.size() % shape[0], 0u);
  size_t subspan_size = input_buffer.size() / shape[0];

  base::SpanReader<const uint8_t> reader(input_buffer);
  base::SpanWriter<uint8_t> writer(multi_array_backed_output_buffer);
  for (uint32_t i = 0; i < shape[0]; i++) {
    auto output_subspan = writer.Skip(strides[0] * item_byte_size);
    CHECK(output_subspan);
    auto input_subspan = reader.Read(subspan_size);
    CHECK(input_subspan);
    if (shape.size() == 1) {
      output_subspan->copy_from(input_subspan->first(item_byte_size));
    } else {
      RecursivelyWriteToMLMultiArray(*input_subspan, item_byte_size,
                                     shape.subspan(1u), strides.subspan(1u),
                                     *output_subspan);
    }
  }
}

}  // namespace

void ReadFromMLMultiArray(
    MLMultiArray* multi_array,
    base::OnceCallback<void(mojo_base::BigBuffer)> result_callback) {
  __block auto wrapped_callback =
      base::BindPostTaskToCurrentDefault(std::move(result_callback));

  [multi_array getBytesWithHandler:^(const void* bytes, NSInteger size) {
    std::vector<uint32_t> shape = ToStdVector(multi_array.shape);
    std::vector<uint32_t> strides = ToStdVector(multi_array.strides);
    CHECK_EQ(shape.size(), strides.size());

    // SAFETY: -[MLMultiArray getBytesWithHandler:] guarantees that `bytes`
    // points to at least `size` valid bytes.
    auto multi_array_data = UNSAFE_BUFFERS(base::span(
        static_cast<const uint8_t*>(bytes), base::checked_cast<size_t>(size)));

    mojo_base::BigBuffer output_buffer(multi_array_data.size());

    RecursivelyReadFromMLMultiArray(multi_array_data,
                                    GetDataTypeByteSize(multi_array.dataType),
                                    shape, strides, output_buffer);

    std::move(wrapped_callback).Run(std::move(output_buffer));
  }];
}

void WriteToMLMultiArray(MLMultiArray* multi_array,
                         base::span<const uint8_t> bytes_to_write,
                         base::OnceClosure done_closure) {
  __block auto wrapped_closure =
      base::BindPostTaskToCurrentDefault(std::move(done_closure));

  [multi_array getMutableBytesWithHandler:^(void* mutable_bytes, NSInteger size,
                                            NSArray<NSNumber*>* strides) {
    std::vector<uint32_t> shape = ToStdVector(multi_array.shape);
    std::vector<uint32_t> std_strides = ToStdVector(strides);
    CHECK_EQ(shape.size(), std_strides.size());

    // SAFETY: -[MLMultiArray getMutableBytesWithHandler:] guarantees that
    // `mutable_bytes` points to at least `size` valid bytes.
    auto mutable_multi_array_data =
        UNSAFE_BUFFERS(base::span(static_cast<uint8_t*>(mutable_bytes),
                                  base::checked_cast<size_t>(size)));
    RecursivelyWriteToMLMultiArray(
        bytes_to_write, GetDataTypeByteSize(multi_array.dataType), shape,
        std_strides, mutable_multi_array_data);

    std::move(wrapped_closure).Run();
  }];
}

}  // namespace webnn::coreml
