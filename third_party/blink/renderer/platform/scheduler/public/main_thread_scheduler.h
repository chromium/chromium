// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_MAIN_THREAD_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_MAIN_THREAD_SCHEDULER_H_

#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"

namespace blink {

// This class is used to submit tasks and pass other information from Blink to
// the platform's main thread scheduler.
class PLATFORM_EXPORT MainThreadScheduler : public ThreadScheduler {
 public:
  // RAII handle for pausing the renderer. Renderer is paused while
  // at least one pause handle exists.
  class PLATFORM_EXPORT RendererPauseHandle {
   public:
    RendererPauseHandle() = default;
    RendererPauseHandle(const RendererPauseHandle&) = delete;
    RendererPauseHandle& operator=(const RendererPauseHandle&) = delete;
    virtual ~RendererPauseHandle() = default;
  };

  // Tells the scheduler that the renderer process should be paused.
  // Pausing means that all javascript callbacks should not fire.
  // https://html.spec.whatwg.org/#pause
  //
  // Renderer will be resumed when the handle is destroyed.
  // Handle should be destroyed before the renderer.
  [[nodiscard]] virtual std::unique_ptr<RendererPauseHandle>
  PauseScheduler() = 0;

  // Returns a task runner which does not generate system wakeups on its own.
  // This means that if a delayed task is posted to it, it will run when
  // the delay expires AND another task runs.
  virtual scoped_refptr<base::SingleThreadTaskRunner> NonWakingTaskRunner() = 0;

  // Creates a AgentGroupScheduler implementation.
  virtual std::unique_ptr<scheduler::WebAgentGroupScheduler>
  CreateAgentGroupScheduler() = 0;

  // The current active AgentGroupScheduler is set when the task gets
  // started (i.e., OnTaskStarted) and unset when the task gets
  // finished (i.e., OnTaskCompleted). GetCurrentAgentGroupScheduler()
  // returns nullptr in task observers.
  virtual scheduler::WebAgentGroupScheduler*
  GetCurrentAgentGroupScheduler() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_MAIN_THREAD_SCHEDULER_H_
