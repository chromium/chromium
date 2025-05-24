// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_COMMAND_BUFFER_TRACE_UTILS_H_
#define GPU_IPC_COMMON_COMMAND_BUFFER_TRACE_UTILS_H_

namespace gpu {

inline uint64_t GlobalFlushTracingId(int channel_id, uint32_t local_flush_id) {
  return (static_cast<uint64_t>(channel_id) << 32) | local_flush_id;
}

}  // namespace gpu

#endif  // GPU_IPC_COMMON_COMMAND_BUFFER_TRACE_UTILS_H_
