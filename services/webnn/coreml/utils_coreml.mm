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
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "mojo/public/cpp/base/big_buffer.h"

namespace webnn::coreml {

namespace {

size_t GetDataTypeByteSize(MLMultiArrayDataType data_type) {
  // MLMultiArrayDataType values encode a format in the high bits (float =
  // 0x10000, int = 0x20000) and the size (in bits) in the lower 16 bits.
  //
  // To determine the byte size of the type, mask off the format and divide. For
  // example:
  //
  // MLMultiArrayDataTypeFloat64 (0x10040) -> 0x40 (64 bits) / 8 = 8 bytes.
  // MLMultiArrayDataTypeInt32 (0x20020) -> 0x20 (32 bits) / 8 = 4 bytes.
  return (data_type & 0xFFFF) / 8;
}

std::vector<size_t> ToStdVector(NSArray<NSNumber*>* ns_array) {
  std::vector<size_t> std_vector;
  std_vector.reserve(ns_array.count);
  for (NSNumber* number in ns_array) {
    std_vector.push_back(
        base::checked_cast<size_t>(number.unsignedIntegerValue));
  }
  return std_vector;
}

// Extract data from an `MLMultiArray` - which may not be contiguous - using its
// `shape` and `strides` as appropriate.
void RecursivelyReadFromMLMultiArray(
    base::span<const uint8_t> multi_array_backed_input_buffer,
    size_t item_byte_size,
    base::span<const size_t> shape,
    base::span<const size_t> strides,
    base::span<uint8_t> output_buffer) {
  // Data is packed, copy the whole thing.
  //
  // On the last dimension, the bytes left to read could be more than the bytes
  // left to write because of strides from the previous dimension, but as long
  // as the current stride is 1, we can copy continuously.
  if (multi_array_backed_input_buffer.size() == output_buffer.size() ||
      (shape.size() == 1 && strides[0] == 1)) {
    output_buffer.copy_from(
        multi_array_backed_input_buffer.first(output_buffer.size()));
    return;
  }

  CHECK_EQ(output_buffer.size() % shape[0], 0u);
  size_t subspan_size = output_buffer.size() / shape[0];
  size_t stride_byte_size =
      base::CheckMul(strides[0], item_byte_size).ValueOrDie();

  base::SpanReader<const uint8_t> reader(multi_array_backed_input_buffer);
  base::SpanWriter<uint8_t> writer(output_buffer);
  for (size_t i = 0; i < shape[0]; i++) {
    auto output_subspan = writer.Skip(subspan_size);
    CHECK(output_subspan);
    auto input_subspan = reader.Read(stride_byte_size);
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
    size_t item_byte_size,
    base::span<const size_t> shape,
    base::span<const size_t> strides,
    base::span<uint8_t> multi_array_backed_output_buffer) {
  // Data is packed, copy the whole thing.
  //
  // On the last dimension, the bytes left to write could be more than the bytes
  // left to read because of strides from the previous dimension, but as long as
  // the current stride is 1, we can copy continuously.
  if (input_buffer.size() == multi_array_backed_output_buffer.size() ||
      (shape.size() == 1 && strides[0] == 1)) {
    multi_array_backed_output_buffer.copy_prefix_from(input_buffer);
    return;
  }

  CHECK_EQ(input_buffer.size() % shape[0], 0u);
  size_t subspan_size = input_buffer.size() / shape[0];
  size_t stride_byte_size =
      base::CheckMul(strides[0], item_byte_size).ValueOrDie();

  base::SpanReader<const uint8_t> reader(input_buffer);
  base::SpanWriter<uint8_t> writer(multi_array_backed_output_buffer);
  for (size_t i = 0; i < shape[0]; i++) {
    auto output_subspan = writer.Skip(stride_byte_size);
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

mojo_base::BigBuffer ReadFromMLMultiArray(MLMultiArray* multi_array) {
  size_t packed_size =
      base::CheckMul(base::checked_cast<size_t>(multi_array.count),
                     GetDataTypeByteSize(multi_array.dataType))
          .ValueOrDie();
  __block mojo_base::BigBuffer output_buffer(packed_size);
  __block bool block_executing_synchronously = true;

  [multi_array getBytesWithHandler:^(const void* bytes, NSInteger size) {
    CHECK(block_executing_synchronously);
    std::vector<size_t> shape = ToStdVector(multi_array.shape);
    std::vector<size_t> strides = ToStdVector(multi_array.strides);
    CHECK_EQ(shape.size(), strides.size());

    // SAFETY: -[MLMultiArray getBytesWithHandler:] guarantees that `bytes`
    // points to at least `size` valid bytes.
    auto multi_array_data = UNSAFE_BUFFERS(base::span(
        static_cast<const uint8_t*>(bytes), base::checked_cast<size_t>(size)));

    RecursivelyReadFromMLMultiArray(multi_array_data,
                                    GetDataTypeByteSize(multi_array.dataType),
                                    shape, strides, output_buffer);
  }];
  block_executing_synchronously = false;
  return std::move(output_buffer);
}

void WriteToMLMultiArray(MLMultiArray* multi_array,
                         base::span<const uint8_t> bytes_to_write) {
  __block bool block_executing_synchronously = true;

  [multi_array getMutableBytesWithHandler:^(void* mutable_bytes, NSInteger size,
                                            NSArray<NSNumber*>* strides) {
    CHECK(block_executing_synchronously);
    std::vector<size_t> shape = ToStdVector(multi_array.shape);
    std::vector<size_t> std_strides = ToStdVector(strides);
    CHECK_EQ(shape.size(), std_strides.size());

    // SAFETY: -[MLMultiArray getMutableBytesWithHandler:] guarantees that
    // `mutable_bytes` points to at least `size` valid bytes.
    auto mutable_multi_array_data =
        UNSAFE_BUFFERS(base::span(static_cast<uint8_t*>(mutable_bytes),
                                  base::checked_cast<size_t>(size)));
    RecursivelyWriteToMLMultiArray(
        bytes_to_write, GetDataTypeByteSize(multi_array.dataType), shape,
        std_strides, mutable_multi_array_data);
  }];
  block_executing_synchronously = false;
}

}  // namespace webnn::coreml
