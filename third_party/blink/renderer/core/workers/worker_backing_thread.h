// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKER_BACKING_THREAD_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKER_BACKING_THREAD_H_

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"
#include "v8/include/v8.h"

namespace blink {

struct WorkerBackingThreadStartupData;

// WorkerBackingThread represents a WebThread with Oilpan and V8. A client of
// WorkerBackingThread (i.e., WorkerThread) needs to call
// InitializeOnBackingThread() to use V8 and Oilpan functionalities, and call
// ShutdownOnBackingThread() when it no longer needs the thread.
class CORE_EXPORT WorkerBackingThread final {
  USING_FAST_MALLOC(WorkerBackingThread);

 public:
  explicit WorkerBackingThread(const ThreadCreationParams&);
  ~WorkerBackingThread();

  // InitializeOnBackingThread() and ShutdownOnBackingThread() attaches and
  // detaches Oilpan and V8 to / from the caller worker script, respectively.
  // A worker script must not call any function after calling
  // ShutdownOnBackingThread(). They should be called from |this| thread.
  void InitializeOnBackingThread(const WorkerBackingThreadStartupData&);
  void ShutdownOnBackingThread();

  blink::Thread& BackingThread() {
    DCHECK(backing_thread_);
    return *backing_thread_;
  }

  v8::Isolate* GetIsolate() { return isolate_; }

  static void MemoryPressureNotificationToWorkerThreadIsolates(
      v8::MemoryPressureLevel);

 private:
  std::unique_ptr<blink::Thread> backing_thread_;
  v8::Isolate* isolate_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_WORKER_BACKING_THREAD_H_
