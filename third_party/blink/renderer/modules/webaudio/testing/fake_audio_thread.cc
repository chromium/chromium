// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/testing/fake_audio_thread.h"

#include "base/check.h"
#include "base/synchronization/waitable_event.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/deferred_task_handler.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_persistent.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

FakeAudioThread::FakeAudioThread(ThreadType thread_type) {
  thread_ = NonMainThread::CreateThread(
      ThreadCreationParams(thread_type));
}

FakeAudioThread::~FakeAudioThread() {
  thread_.reset();
}

void FakeAudioThread::RunOnAudioThread(CrossThreadOnceClosure closure) {
  base::WaitableEvent event;
  PostCrossThreadTask(
      *thread_->GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(
          [](CrossThreadOnceClosure closure, base::WaitableEvent* event) {
            if (closure) {
              std::move(closure).Run();
            }
            event->Signal();
          },
          std::move(closure), CrossThreadUnretained(&event)));
  event.Wait();
}

void FakeAudioThread::RunOnAudioThreadWithContext(
    BaseAudioContext* context,
    CrossThreadOnceClosure closure) {
  DCHECK(context);
  RunOnAudioThread(CrossThreadBindOnce(
      [](BaseAudioContext* context, CrossThreadOnceClosure closure) {
        context->GetDeferredTaskHandler().SetAudioThreadToCurrentThread();
        if (closure) {
          std::move(closure).Run();
        }
      },
      WrapCrossThreadPersistent(context), std::move(closure)));
}

}  // namespace blink
