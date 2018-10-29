// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_STREAMER_THREAD_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_STREAMER_THREAD_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"
#include "v8/include/v8.h"

namespace blink {

class ScriptStreamer;

// A singleton thread for running background tasks for script streaming.
class CORE_EXPORT ScriptStreamerThread {
  USING_FAST_MALLOC(ScriptStreamerThread);

 public:
  static void Init();
  static ScriptStreamerThread* Shared();

  void PostTask(CrossThreadClosure);

  bool IsRunningTask() const {
    MutexLocker locker(mutex_);
    return running_task_;
  }

  void TaskDone();

  static void RunScriptStreamingTask(
      std::unique_ptr<v8::ScriptCompiler::ScriptStreamingTask>,
      ScriptStreamer*);

 private:
  ScriptStreamerThread() : running_task_(false) {}

  bool IsRunning() const { return !!thread_; }

  Thread& PlatformThread();

  // At the moment, we only use one thread, so we can only stream one script
  // at a time. FIXME: Use a thread pool and stream multiple scripts.
  std::unique_ptr<Thread> thread_;
  bool running_task_;
  mutable Mutex mutex_;  // Guards m_runningTask.

  DISALLOW_COPY_AND_ASSIGN(ScriptStreamerThread);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_STREAMER_THREAD_H_
