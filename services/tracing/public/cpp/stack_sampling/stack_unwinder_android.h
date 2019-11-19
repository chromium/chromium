// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_STACK_UNWINDER_ANDROID_H_
#define SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_STACK_UNWINDER_ANDROID_H_

#include <ucontext.h>

#include <map>
#include <vector>

#include "base/component_export.h"
#include "base/debug/proc_maps_linux.h"
#include "base/profiler/stack_sampler.h"
#include "base/threading/platform_thread.h"

namespace jni_generator {
struct JniJavaCallContextUnchecked;
}
struct unw_context_t;

namespace tracing {

// Utility to unwind stacks for current thread on ARM devices. Contains ability
// to unwind stacks based on EHABI section in Android libraries and using the
// custom stack unwind information in Chrome. This works on top of
// base::trace_event::CFIBacktraceAndroid, which unwinds Chrome only stacks.
// This class does not provide any thread safety guarantees. It is also unsafe
// to use multiple instances of this class at the same time due to signal
// handling. So, the client must ensure synchronization between multiple
// instances of this class.
class COMPONENT_EXPORT(TRACING_CPP) StackUnwinderAndroid {
 public:
  using JniMarker = jni_generator::JniJavaCallContextUnchecked;

  // Whether to use libunwind for android framework frames.
  static const bool kUseLibunwind;

  StackUnwinderAndroid();
  ~StackUnwinderAndroid();

  // Initializes the unwinder for current process. It finds all loaded libraries
  // in current process and also initializes CFIBacktraceAndroid, with file IO.
  // Can be called multiple times, to update the loaded modules.
  void Initialize();

  // Unwinds stack frames for current thread and stores the program counters in
  // |out_trace|, and returns the number of frames stored.
  size_t TraceStack(const void** out_trace, size_t max_depth) const;

  // Same as above function, but pauses the thread with the given |tid| and then
  // unwinds. |tid| should not be current thread's.
  size_t TraceStack(base::PlatformThreadId tid,
                    base::StackBuffer* stack_buffer,
                    const void** out_trace,
                    size_t max_depth) const;

  // Returns the end address of the memory map with given |addr|.
  uintptr_t GetEndAddressOfRegion(uintptr_t addr) const;

  // Returns true if the given |pc| was part of any mapped segments in the
  // process.
  bool IsAddressMapped(uintptr_t pc) const;

  bool is_initialized() const { return is_initialized_; }

 private:
  // Sends a SIGURG signal to the thread with id |tid| and copies the stack
  // segment of the thread, along with register context. Returns true on
  // success.
  bool SuspendThreadAndRecordStack(base::PlatformThreadId tid,
                                   base::StackBuffer* stack_buffer,
                                   uintptr_t* sp,
                                   size_t* stack_size,
                                   unw_context_t* context,
                                   ucontext_t* signal_context) const;

  // Replaces any pointers to the old stack to point to the new stack segment.
  // Returns the jni markers found on stack while scanning stack for pointers.
  std::vector<const JniMarker*> RewritePointersAndGetMarkers(
      base::StackBuffer* stack_buffer,
      uintptr_t sp,
      size_t stack_size) const;

  bool is_initialized_ = false;

  // Stores all the memory mapped regions in the current process, including all
  // the files mapped and anonymous regions. This data could be stale, but the
  // error caused by changes in library loads would be missing stackframes and
  // is acceptable.
  std::vector<base::debug::MappedMemoryRegion> regions_;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_STACK_UNWINDER_ANDROID_H_
