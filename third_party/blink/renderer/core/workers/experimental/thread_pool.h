// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_EXPERIMENTAL_THREAD_POOL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_EXPERIMENTAL_THREAD_POOL_H_

#include "third_party/blink/renderer/core/workers/experimental/thread_pool_thread.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {
class Document;
class ThreadPoolMessagingProxy;

class ThreadPool final : public GarbageCollectedFinalized<ThreadPool>,
                         public Supplement<Document>,
                         public ContextLifecycleObserver,
                         public ThreadPoolThreadProvider {
  USING_GARBAGE_COLLECTED_MIXIN(ThreadPool);
  EAGERLY_FINALIZE();

 public:
  static const char kSupplementName[];
  static ThreadPool* From(Document&);
  ~ThreadPool();

  ThreadPoolThread* GetLeastBusyThread() override;
  void ContextDestroyed(ExecutionContext*) final;
  void Trace(blink::Visitor*) final;

 private:
  ThreadPool(Document&);

  ThreadPoolThread* CreateNewThread();

  HeapHashSet<Member<ThreadPoolMessagingProxy>> thread_proxies_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_EXPERIMENTAL_THREAD_POOL_H_
