// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WEB_THREAD_SUPPORTING_GC_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WEB_THREAD_SUPPORTING_GC_H_

#include <memory>
#include "base/threading/thread_checker.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/heap/gc_task_runner.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/noncopyable.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

// WebThreadSupportingGC wraps a WebThread and adds support for attaching
// to and detaching from the Blink GC infrastructure.
//
// The initialize method must be called during initialization on the WebThread
// and before the thread allocates any objects managed by the Blink GC. The
// shutdown method must be called on the WebThread during shutdown when the
// thread no longer needs to access objects managed by the Blink GC.
//
// WebThreadSupportingGC usually internally creates and owns WebThread unless
// an existing WebThread is given via createForThread.
class PLATFORM_EXPORT WebThreadSupportingGC final {
  USING_FAST_MALLOC(WebThreadSupportingGC);
  WTF_MAKE_NONCOPYABLE(WebThreadSupportingGC);

 public:
  static std::unique_ptr<WebThreadSupportingGC> Create(
      const ThreadCreationParams&);
  static std::unique_ptr<WebThreadSupportingGC> CreateForThread(Thread*);
  ~WebThreadSupportingGC();

  void PostTask(const base::Location& location, base::OnceClosure task) {
    thread_->GetTaskRunner()->PostTask(location, std::move(task));
  }

  void PostDelayedTask(const base::Location& location,
                       base::OnceClosure task,
                       TimeDelta delay) {
    thread_->GetTaskRunner()->PostDelayedTask(location, std::move(task), delay);
  }

  void PostTask(const base::Location& location, CrossThreadClosure task) {
    PostCrossThreadTask(*thread_->GetTaskRunner(), location, std::move(task));
  }

  void PostDelayedTask(const base::Location& location,
                       CrossThreadClosure task,
                       TimeDelta delay) {
    PostDelayedCrossThreadTask(*thread_->GetTaskRunner(), location,
                               std::move(task), delay);
  }

  bool IsCurrentThread() const { return thread_->IsCurrentThread(); }

  void AddTaskObserver(Thread::TaskObserver* observer) {
    thread_->AddTaskObserver(observer);
  }

  void RemoveTaskObserver(Thread::TaskObserver* observer) {
    thread_->RemoveTaskObserver(observer);
  }

  // Must be called on the matching blink::Thread.
  void InitializeOnThread();
  void ShutdownOnThread();

  Thread& PlatformThread() const {
    DCHECK(thread_);
    return *thread_;
  }

 private:
  WebThreadSupportingGC(const ThreadCreationParams*, Thread*);

  std::unique_ptr<GCTaskRunner> gc_task_runner_;

  // m_thread is guaranteed to be non-null after this instance is constructed.
  // m_owningThread is non-null unless this instance is constructed for an
  // existing thread via createForThread().
  Thread* thread_ = nullptr;
  std::unique_ptr<Thread> owning_thread_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace blink

#endif
