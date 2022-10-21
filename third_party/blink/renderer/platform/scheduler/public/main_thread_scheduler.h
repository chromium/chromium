// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_MAIN_THREAD_SCHEDULER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_MAIN_THREAD_SCHEDULER_H_

#include <memory>

#include "third_party/blink/public/common/input/web_input_event_attribution.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
class AgentGroupScheduler;

namespace scheduler {
class WebThreadScheduler;
}  // namespace scheduler

class RAILModeObserver;

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
  virtual AgentGroupScheduler* CreateAgentGroupScheduler() = 0;

  // The current active AgentGroupScheduler is set when the task gets
  // started (i.e., OnTaskStarted) and unset when the task gets
  // finished (i.e., OnTaskCompleted). GetCurrentAgentGroupScheduler()
  // returns nullptr in task observers.
  virtual AgentGroupScheduler* GetCurrentAgentGroupScheduler() = 0;

  virtual void AddRAILModeObserver(RAILModeObserver* observer) = 0;

  virtual void RemoveRAILModeObserver(RAILModeObserver const* observer) = 0;

  // Returns a list of all unique attributions that are marked for event
  // dispatch. If |include_continuous| is true, include event types from
  // "continuous" sources (see PendingUserInput::IsContinuousEventTypes).
  virtual Vector<WebInputEventAttribution> GetPendingUserInputInfo(
      bool include_continuous) const {
    return {};
  }

 private:
  // For `ToWebMainThreadScheduler`.
  friend class scheduler::WebThreadScheduler;

  // For `Isolate`.
  friend class ScopedMainThreadOverrider;

  // Get the isolate previously set with `SetV8Isolate`. This method is scoped
  // private so only friends can use it. Other users should use
  // `WebAgentGroupScheduler::Isolate` instead.
  virtual v8::Isolate* Isolate() = 0;

  // Return a reference to an underlying main thread WebThreadScheduler object.
  // Can be null if there is no underlying main thread WebThreadScheduler
  // (e.g. worker threads).
  virtual scheduler::WebThreadScheduler* ToWebMainThreadScheduler() {
    return nullptr;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_PUBLIC_MAIN_THREAD_SCHEDULER_H_
