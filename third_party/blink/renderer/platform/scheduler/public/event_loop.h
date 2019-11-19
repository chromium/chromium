// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_EVENT_LOOP_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_EVENT_LOOP_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace v8 {
class Isolate;
class MicrotaskQueue;
}  // namespace v8

namespace blink {

class Agent;
class FrameOrWorkerScheduler;

namespace scheduler {

// Represents an event loop. The instance is held by ExecutionContexts.
// https://html.spec.whatwg.org/C#event-loop
//
// Browsing contexts must share the same EventLoop if they have a chance to
// access each other synchronously.
// That is:
//  - Two Documents must share the same EventLoop if they are scriptable with
//    each other.
//  - Workers and Worklets can have its own EventLoop, as no other browsing
//    context can access it synchronously.
//
// The specification says an event loop has (non-micro) task queues. However,
// we process regular tasks in a different granularity; in our implementation,
// a frame has task queues. This is an intentional violation of the
// specification.
//
// Therefore, currently, EventLoop is a unit that just manages a microtask
// queue: <https://html.spec.whatwg.org/C#microtask-queue>
//
// Microtasks queued during a task are executed at the end of the task or
// after a user script is executed (for the exact timings, refer to the
// specification). Some web platform features require this functionality.
//
// Implementation notes: Originally, microtask queues were created in V8
// for JavaScript promises. V8 allocates a default microtask queue per isolate,
// and it still uses the default queue, not the one in this EventLoop class.
// This is not correct in terms of the standards conformance, and we'll
// eventually merge the queues so both Blink and V8 can use the microtask queue
// allocated in the correct granularity.
class PLATFORM_EXPORT EventLoop final : public WTF::RefCounted<EventLoop> {
  USING_FAST_MALLOC(EventLoop);

 public:
  // Queues |cb| to the backing v8::MicrotaskQueue.
  void EnqueueMicrotask(base::OnceClosure cb);

  // Runs pending microtasks until the queue is empty.
  void PerformMicrotaskCheckpoint();

  // Runs pending microtasks on the isolate's default MicrotaskQueue until it's
  // empty.
  static void PerformIsolateGlobalMicrotasksCheckpoint(v8::Isolate* isolate);

  // Disables or enables all controlled frames.
  void Disable();
  void Enable();

  void AttachScheduler(FrameOrWorkerScheduler*);
  void DetachScheduler(FrameOrWorkerScheduler*);

  // Returns the MicrotaskQueue instance to be associated to v8::Context. Pass
  // it to v8::Context::New().
  v8::MicrotaskQueue* microtask_queue() const { return microtask_queue_.get(); }

  bool IsSchedulerAttachedForTest(FrameOrWorkerScheduler*);

 private:
  friend class WTF::RefCounted<EventLoop>;
  friend blink::Agent;

  EventLoop(v8::Isolate* isolate,
            std::unique_ptr<v8::MicrotaskQueue> microtask_queue = nullptr);
  ~EventLoop();

  static void RunPendingMicrotask(void* data);

  v8::Isolate* isolate_;
  bool loop_enabled_ = true;
  Deque<base::OnceClosure> pending_microtasks_;
  std::unique_ptr<v8::MicrotaskQueue> microtask_queue_;
  HashSet<FrameOrWorkerScheduler*> schedulers_;

  DISALLOW_COPY_AND_ASSIGN(EventLoop);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_EVENT_LOOP_H_
