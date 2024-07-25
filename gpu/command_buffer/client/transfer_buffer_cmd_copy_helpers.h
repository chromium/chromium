// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef GPU_COMMAND_BUFFER_CLIENT_TRANSFER_BUFFER_CMD_COPY_HELPERS_H_
#define GPU_COMMAND_BUFFER_CLIENT_TRANSFER_BUFFER_CMD_COPY_HELPERS_H_

#include <bit>

#include "base/numerics/safe_math.h"
#include "gpu/command_buffer/client/transfer_buffer.h"

namespace gpu {

// Sum the sizes of the types in Ts as CheckedNumeric<T>.
template <typename T, typename... Ts>
constexpr base::CheckedNumeric<T> CheckedSizeOfPackedTypes() {
  static_assert(sizeof...(Ts) > 0, "");
  base::CheckedNumeric<T> checked_elements_size = 0;
  for (size_t s : {sizeof(Ts)...}) {
    checked_elements_size += s;
  }
  return checked_elements_size;
}

// Compute the number of bytes required for a struct-of-arrays where each array
// of type T has count items. If there is an overflow, this function returns 0.
template <typename... Ts>
constexpr base::CheckedNumeric<uint32_t> ComputeCheckedCombinedCopySize(
    uint32_t count) {
  static_assert(sizeof...(Ts) > 0, "");
  base::CheckedNumeric<uint32_t> checked_combined_size = 0;
  base::CheckedNumeric<uint32_t> checked_count(count);
  for (auto info : {std::make_pair(sizeof(Ts), alignof(Ts))...}) {
    size_t alignment = info.second;
    DCHECK(std::has_single_bit(alignment));

    checked_combined_size =
        (checked_combined_size + alignment - 1) & ~(alignment - 1);
    checked_combined_size += checked_count * info.first;
  }
  return checked_combined_size;
}

// Copy count items from each array in arrays starting at array[offset_count]
// into the address pointed to by buffer
template <typename... Ts>
auto CopyArraysToBuffer(uint32_t count,
                        uint32_t offset_count,
                        void* buffer,
                        Ts*... arrays)
    -> std::array<uint32_t, sizeof...(arrays)> {
  constexpr uint32_t arr_count = sizeof...(arrays);
  static_assert(arr_count > 0, "Requires at least one array");
  DCHECK_GT(count, 0u);
  DCHECK(buffer);

  // Length of each copy
  std::array<size_t, arr_count> copy_lengths{{(count * sizeof(Ts))...}};

  std::array<size_t, arr_count> alignments{{alignof(Ts)...}};

  // Offset to the destination of each copy
  std::array<uint32_t, arr_count> byte_offsets{};
  byte_offsets[0] = 0;
  base::CheckedNumeric<uint32_t> checked_byte_offset = copy_lengths[0];
  for (uint32_t i = 1; i < arr_count; ++i) {
    DCHECK(std::has_single_bit(alignments[i]));
    checked_byte_offset =
        (checked_byte_offset + alignments[i] - 1) & ~(alignments[i] - 1);
    byte_offsets[i] = checked_byte_offset.ValueOrDie();
    checked_byte_offset += copy_lengths[i];
  }

  // Pointers to the copy sources
  std::array<const int8_t*, arr_count> byte_pointers{
      {([](bool b) { DCHECK(b); }(arrays),
        reinterpret_cast<const int8_t*>(arrays + offset_count))...}};

  for (uint32_t i = 0; i < arr_count; ++i) {
    memcpy(static_cast<int8_t*>(buffer) + byte_offsets[i], byte_pointers[i],
           copy_lengths[i]);
  }

  return byte_offsets;
}

// Sum the sizes of the types in Ts. This will fail to compile if the result
// does not fit in T.
template <typename T, typename... Ts>
constexpr T SizeOfPackedTypes() {
  constexpr base::CheckedNumeric<T> checked_elements_size =
      CheckedSizeOfPackedTypes<T, Ts...>();
  static_assert(checked_elements_size.IsValid(), "");
  return checked_elements_size.ValueOrDie();
}

template <typename... Ts>
constexpr uint32_t ComputeCombinedCopySize(uint32_t count) {
  return ComputeCheckedCombinedCopySize<Ts...>(count).ValueOrDefault(
      UINT32_MAX);
}

template <typename... Ts>
constexpr uint32_t ComputeCombinedCopySize(uint32_t count,
                                           const Ts*... arrays) {
  return ComputeCheckedCombinedCopySize<Ts...>(count).ValueOrDefault(
      UINT32_MAX);
}

// Compute the largest array size for a struct-of-arrays that can fit inside
// a buffer
template <typename... Ts>
constexpr uint32_t ComputeMaxCopyCount(uint32_t buffer_size) {
  // Start by tightly packing the elements and decrease copy_count until
  // the total aligned copy size fits
  constexpr uint32_t elements_size = SizeOfPackedTypes<uint32_t, Ts...>();
  uint32_t copy_count = buffer_size / elements_size;

  while (copy_count > 0) {
    base::CheckedNumeric<uint32_t> checked_combined_size =
        ComputeCheckedCombinedCopySize<Ts...>(copy_count);
    uint32_t combined_size = 0;
    if (checked_combined_size.AssignIfValid(&combined_size) &&
        combined_size <= buffer_size) {
      break;
    }
    copy_count--;
  }

  return copy_count;
}

}  // namespace gpu

namespace internal {

// The transfer buffer may not fit all count items from each array in arrays.
// This function copies in equal number of items from each array into the buffer
// and calls a callback function f. It releases the buffer and repeats until
// all items have been consumed.
template <typename F, typename TransferBuffer, typename... Ts>
bool TransferArraysAndExecute(uint32_t count,
                              TransferBuffer* buffer,
                              const F& f,
                              Ts*... arrays) {
  static_assert(sizeof...(arrays) > 0, "Requires at least one array");
  DCHECK(buffer);

  uint32_t offset_count = 0;
  while (count) {
    uint32_t desired_size =
        gpu::ComputeCheckedCombinedCopySize<Ts...>(count).ValueOrDefault(
            UINT32_MAX);
    uint32_t copy_count = gpu::ComputeMaxCopyCount<Ts...>(buffer->size());
    if (!buffer->valid() || copy_count == 0) {
      // Reset the buffer to the desired size
      buffer->Reset(desired_size);
      if (!buffer->valid()) {
        return false;
      }
      // The buffer size may be less than the desired size. Recompute the number
      // of elements to copy.
      copy_count = gpu::ComputeMaxCopyCount<Ts...>(buffer->size());
      if (copy_count == 0) {
        return false;
      }
    }

    std::array<uint32_t, sizeof...(arrays)> byte_offsets =
        gpu::CopyArraysToBuffer(copy_count, offset_count, buffer->address(),
                                arrays...);
    f(byte_offsets, offset_count, copy_count);
    buffer->Release();
    offset_count += copy_count;
    count -= copy_count;
  }
  return true;
}

}  // namespace internal

namespace gpu {
template <typename F, typename... Ts>
bool TransferArraysAndExecute(uint32_t count,
                              ScopedTransferBufferPtr* buffer,
                              const F& f,
                              Ts*... arrays) {
  return internal::TransferArraysAndExecute<F, ScopedTransferBufferPtr, Ts...>(
      count, buffer, f, arrays...);
}

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_TRANSFER_BUFFER_CMD_COPY_HELPERS_H_
