/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_GC_TASK_RUNNER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_GC_TASK_RUNNER_H_

#include <memory>
#include "base/location.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"

namespace blink {

class GCTaskObserver final : public Thread::TaskObserver {
  USING_FAST_MALLOC(GCTaskObserver);

 public:
  GCTaskObserver() : nesting_(0) {}

  ~GCTaskObserver() override {
    // m_nesting can be 1 if this was unregistered in a task and
    // didProcessTask was not called.
    DCHECK(!nesting_ || nesting_ == 1);
  }

  void WillProcessTask(const base::PendingTask&) override { nesting_++; }

  void DidProcessTask(const base::PendingTask&) override {
    // In the production code WebKit::initialize is called from inside the
    // message loop so we can get didProcessTask() without corresponding
    // willProcessTask once. This is benign.
    if (nesting_)
      nesting_--;

    ThreadState::Current()->SafePoint(nesting_
                                          ? BlinkGC::kHeapPointersOnStack
                                          : BlinkGC::kNoHeapPointersOnStack);
  }

 private:
  int nesting_;
};

class GCTaskRunner final {
  USING_FAST_MALLOC(GCTaskRunner);

 public:
  explicit GCTaskRunner(Thread* thread)
      : gc_task_observer_(std::make_unique<GCTaskObserver>()), thread_(thread) {
    thread_->AddTaskObserver(gc_task_observer_.get());
  }

  ~GCTaskRunner() { thread_->RemoveTaskObserver(gc_task_observer_.get()); }

 private:
  std::unique_ptr<GCTaskObserver> gc_task_observer_;
  Thread* thread_;
};

}  // namespace blink

#endif
