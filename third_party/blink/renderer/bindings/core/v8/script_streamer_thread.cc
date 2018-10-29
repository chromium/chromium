// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_streamer_thread.h"

#include <memory>
#include "base/location.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_streamer.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"

namespace blink {

static ScriptStreamerThread* g_shared_thread = nullptr;
// Guards s_sharedThread. s_sharedThread is initialized and deleted in the main
// thread, but also used by the streamer thread. Races can occur during
// shutdown.
static Mutex* g_mutex = nullptr;

void ScriptStreamerThread::Init() {
  // Only enabled when ScheduledScriptStreaming is disabled.
  if (!RuntimeEnabledFeatures::ScheduledScriptStreamingEnabled()) {
    DCHECK(!g_shared_thread);
    DCHECK(IsMainThread());
    // This is called in the main thread before any tasks are created, so no
    // locking is needed.
    g_mutex = new Mutex();
    g_shared_thread = new ScriptStreamerThread();
  }
}

ScriptStreamerThread* ScriptStreamerThread::Shared() {
  DCHECK(!RuntimeEnabledFeatures::ScheduledScriptStreamingEnabled());
  return g_shared_thread;
}

void ScriptStreamerThread::PostTask(CrossThreadClosure task) {
  DCHECK(!RuntimeEnabledFeatures::ScheduledScriptStreamingEnabled());
  DCHECK(IsMainThread());
  MutexLocker locker(mutex_);
  DCHECK(!running_task_);
  running_task_ = true;
  PostCrossThreadTask(*PlatformThread().GetTaskRunner(), FROM_HERE,
                      std::move(task));
}

void ScriptStreamerThread::TaskDone() {
  MutexLocker locker(mutex_);
  DCHECK(running_task_);
  running_task_ = false;
}

Thread& ScriptStreamerThread::PlatformThread() {
  if (!IsRunning()) {
    thread_ = Platform::Current()->CreateThread(
        ThreadCreationParams(WebThreadType::kScriptStreamerThread));
  }
  return *thread_;
}

void ScriptStreamerThread::RunScriptStreamingTask(
    std::unique_ptr<v8::ScriptCompiler::ScriptStreamingTask> task,
    ScriptStreamer* streamer) {
  DCHECK(!RuntimeEnabledFeatures::ScheduledScriptStreamingEnabled());
  TRACE_EVENT1(
      "v8,devtools.timeline", "v8.parseOnBackground", "data",
      InspectorParseScriptEvent::Data(streamer->ScriptResourceIdentifier(),
                                      streamer->ScriptURLString()));
  // Running the task can and will block: SourceStream::GetSomeData will get
  // called and it will block and wait for data from the network.
  task->Run();
  streamer->StreamingCompleteOnBackgroundThread();
  MutexLocker locker(*g_mutex);
  ScriptStreamerThread* thread = Shared();
  if (thread)
    thread->TaskDone();
  // If thread is 0, we're shutting down.
}

}  // namespace blink
