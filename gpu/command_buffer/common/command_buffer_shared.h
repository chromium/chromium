// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_SHARED_H_
#define GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_SHARED_H_

#include <array>
#include <atomic>

#include "base/atomicops.h"
#include "command_buffer.h"

namespace gpu {

// This is a standard 4-slot asynchronous communication mechanism, used to
// ensure that the reader gets a consistent copy of what the writer wrote.
template<typename T>
class SharedState {
  std::array<std::array<T, 2>, 2> states_;
  std::atomic<int32_t> reading_;
  std::atomic<int32_t> latest_;
  std::array<std::atomic<int32_t>, 2> slots_;

 public:
  void Initialize() {
    states_ = {};
    reading_.store(0, std::memory_order_relaxed);
    latest_.store(0, std::memory_order_relaxed);
    slots_[0].store(0, std::memory_order_relaxed);
    slots_[1].store(0, std::memory_order_release);
    // TODO(crbug.com/40175832): Merge fence into release store
    std::atomic_thread_fence(std::memory_order_seq_cst);
  }

  void Write(const T& state) {
    int towrite = !reading_.load(std::memory_order_acquire);
    int index = !slots_[towrite].load(std::memory_order_acquire);
    states_[towrite][index] = state;
    slots_[towrite].store(index, std::memory_order_release);
    latest_.store(towrite, std::memory_order_release);
    // TODO(crbug.com/40175832): Merge fence into release store
    std::atomic_thread_fence(std::memory_order_seq_cst);
  }

  // Attempt to update the state, updating only if the generation counter is
  // newer.
  void Read(T* state) {
    // TODO(crbug.com/40175832): Merge fence into subsequent load
    std::atomic_thread_fence(std::memory_order_seq_cst);
    int toread = !!latest_.load(std::memory_order_acquire);
    reading_.store(toread, std::memory_order_release);
    // TODO(crbug.com/40175832): Merge fence into release store above
    std::atomic_thread_fence(std::memory_order_seq_cst);
    int index = !!slots_[toread].load(std::memory_order_acquire);
    if (states_[toread][index].generation - state->generation < 0x80000000U)
      *state = states_[toread][index];
  }
};

typedef SharedState<CommandBuffer::State> CommandBufferSharedState;

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_SHARED_H_
