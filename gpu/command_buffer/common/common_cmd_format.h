// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the command buffer commands.

#ifndef GPU_COMMAND_BUFFER_COMMON_COMMON_CMD_FORMAT_H_
#define GPU_COMMAND_BUFFER_COMMON_COMMON_CMD_FORMAT_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "base/atomicops.h"
#include "base/numerics/checked_math.h"
#include "gpu/command_buffer/common/cmd_buffer_common.h"

namespace gpu {

// Command buffer is GPU_COMMAND_BUFFER_ENTRY_ALIGNMENT byte aligned.
#pragma pack(push, 4)
static_assert(GPU_COMMAND_BUFFER_ENTRY_ALIGNMENT == 4,
              "pragma pack alignment must be equal to "
              "GPU_COMMAND_BUFFER_ENTRY_ALIGNMENT");

// Used for some glGetXXX commands that return a result through a pointer. We
// need to know if the command succeeded or not and the size of the result. If
// the command failed its result size will 0.
template <typename T>
struct SizedResult {
  typedef T Type;

  T* GetData() { return static_cast<T*>(static_cast<void*>(&data)); }

  // Returns the total size in bytes of the SizedResult for a given number of
  // results including the size field.
  static base::CheckedNumeric<uint32_t> ComputeSize(uint32_t num_results) {
    base::CheckedNumeric<uint32_t> size = num_results;
    size *= sizeof(T);
    size += sizeof(uint32_t);
    return size;
  }

  // Returns the maximum number of results for a given buffer size.
  static uint32_t ComputeMaxResults(size_t size_of_buffer) {
    base::CheckedNumeric<uint32_t> max_results = 0;
    if (size_of_buffer >= sizeof(uint32_t)) {
      max_results = size_of_buffer;
      max_results -= sizeof(uint32_t);
      max_results /= sizeof(T);
    }
    return max_results.ValueOrDefault(0);
  }

  // Set the size for a given number of results.
  void SetNumResults(base::CheckedNumeric<uint32_t> num_results) {
    base::CheckedNumeric<uint32_t> bytes = num_results;
    bytes *= sizeof(T);
    size = bytes.ValueOrDie();
  }

  // Get the number of elements in the result
  int32_t GetNumResults() const {
    // TODO(piman): return uint32_t here to remove the need for checked
    // numerics.
    base::CheckedNumeric<uint32_t> num_results = size;
    num_results /= sizeof(T);
    return num_results.ValueOrDie<int32_t>();
  }

  // Copy the result.
  void CopyResult(void* dst) const { memcpy(dst, &data, size); }

  uint32_t size;  // in bytes.
  int32_t data;   // this is just here to get an offset.
};

static_assert(sizeof(SizedResult<int8_t>) == 8,
              "size of SizedResult<int8_t> should be 8");
static_assert(offsetof(SizedResult<int8_t>, size) == 0,
              "offset of SizedResult<int8_t>.size should be 0");
static_assert(offsetof(SizedResult<int8_t>, data) == 4,
              "offset of SizedResult<int8_t>.data should be 4");

// The format of QuerySync used by EXT_occlusion_query_boolean
struct QuerySync {
  void Reset() {
    process_count = 0;
    result = 0;
  }

  base::subtle::Atomic32 process_count;
  uint64_t result;
};

static_assert(sizeof(QuerySync) == 12, "size of QuerySync should be 12");
static_assert(offsetof(QuerySync, process_count) == 0,
              "offset of QuerySync.process_count should be 0");
static_assert(offsetof(QuerySync, result) == 4,
              "offset of QuerySync.result should be 4");

#pragma pack(pop)

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_COMMON_CMD_FORMAT_H_
