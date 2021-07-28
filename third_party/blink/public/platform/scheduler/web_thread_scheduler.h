// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_SCHEDULER_WEB_THREAD_SCHEDULER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_SCHEDULER_WEB_THREAD_SCHEDULER_H_

#include <memory>

#include "base/message_loop/message_pump.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/scheduler/web_rail_mode_observer.h"
#include "third_party/blink/public/platform/scheduler/web_render_widget_scheduling_state.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_input_event_result.h"

namespace base {
namespace trace_event {
class BlameContext;
}  // namespace trace_event
}  // namespace base

namespace blink {
class Thread;
class WebInputEventAttribution;
}  // namespace blink

namespace viz {
struct BeginFrameArgs;
}  // namespace viz

namespace blink {
namespace scheduler {

enum class WebRendererProcessType;
class WebWidgetScheduler;

class BLINK_PLATFORM_EXPORT WebThreadScheduler {
 public:
  WebThreadScheduler(const WebThreadScheduler&) = delete;
  WebThreadScheduler& operator=(const WebThreadScheduler&) = delete;
  virtual ~WebThreadScheduler();

  // ==== Functions for any scheduler =========================================
  //
  // Functions below work on a scheduler instance on any thread.

  // Shuts down the scheduler by dropping any remaining pending work in the work
  // queues. After this call any work posted to the task runners will be
  // silently dropped.
  virtual void Shutdown() = 0;

  // ==== Functions for the main thread scheduler  ============================
  //
  // Virtual functions below should only be called against the scheduler on
  // the main thread. They have default implementation that only does
  // NOTREACHED(), and are overridden only by the main thread scheduler.

  // If |message_pump| is null caller must have registered one using
  // base::MessageLoop.
  static std::unique_ptr<WebThreadScheduler> CreateMainThreadScheduler(
      std::unique_ptr<base::MessagePump> message_pump = nullptr);

  // Returns compositor thread scheduler for the compositor thread
  // of the current process.
  static WebThreadScheduler* CompositorThreadScheduler();

  // Returns main thread scheduler for the main thread of the current process.
  static WebThreadScheduler* MainThreadScheduler();

  // Returns the default task runner.
  virtual scoped_refptr<base::SingleThreadTaskRunner> DefaultTaskRunner();

  // Returns a task runner for input-blocking tasks on the compositor thread.
  // (For input tasks on the main thread, use WebWidgetScheduler instead.)
  virtual scoped_refptr<base::SingleThreadTaskRunner> InputTaskRunner();

  // Returns the compositor task runner.
  virtual scoped_refptr<base::SingleThreadTaskRunner> CompositorTaskRunner();

  // Returns a default task runner. This is basically same as the default task
  // runner, but is explicitly allowed to run JavaScript. For the detail, see
  // the comment at blink::ThreadScheduler::DeprecatedDefaultTaskRunner.
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  DeprecatedDefaultTaskRunner();

  // Creates a WebThread implementation for the renderer main thread.
  virtual std::unique_ptr<Thread> CreateMainThread();

  // Creates a WebAgentGroupScheduler implementation. Must be called from the
  // main thread.
  virtual std::unique_ptr<WebAgentGroupScheduler>
  CreateAgentGroupScheduler() = 0;

  // Creates a WebWidgetScheduler implementation. Must be called from the main
  // thread.
  virtual std::unique_ptr<WebWidgetScheduler> CreateWidgetScheduler();

  // Return the current active AgentGroupScheduler.
  // When a task which belongs to a specific AgentGroupScheduler is going to be
  // run, this AgentGroupScheduler becomes the current active
  // AgentGroupScheduler. And when the task is finished, the current active
  // AgentGroupScheduler becomes nullptr. So if there is no active
  // AgentGroupScheduler, this function returns nullptr. This behaviour is
  // implemented by MainThreadSchedulerImpl’s OnTaskStarted and OnTaskCompleted
  // hook points. So you can’t use this functionality in task observers.
  virtual WebAgentGroupScheduler* GetCurrentAgentGroupScheduler() = 0;

  // Returns a new WebRenderWidgetSchedulingState.  The signals from this will
  // be used to make scheduling decisions.
  virtual std::unique_ptr<WebRenderWidgetSchedulingState>
  NewRenderWidgetSchedulingState();

  // Called to notify about the start of an extended period where no frames
  // need to be drawn. Must be called from the main thread.
  virtual void BeginFrameNotExpectedSoon();

  // Called to notify about the start of a period where main frames are not
  // scheduled and so short idle work can be scheduled. This will precede
  // BeginFrameNotExpectedSoon and is also called when the compositor may be
  // busy but the main thread is not.
  virtual void BeginMainFrameNotExpectedUntil(base::TimeTicks time);

  // Called to notify about the start of a new frame.  Must be called from the
  // main thread.
  virtual void WillBeginFrame(const viz::BeginFrameArgs& args);

  // Called to notify that a previously begun frame was committed. Must be
  // called from the main thread.
  virtual void DidCommitFrameToCompositor();

  // Keep InputEventStateToString() in sync with this enum.
  enum class InputEventState {
    EVENT_CONSUMED_BY_COMPOSITOR,
    EVENT_FORWARDED_TO_MAIN_THREAD,
  };
  static const char* InputEventStateToString(InputEventState input_event_state);

  // Tells the scheduler that the system processed an input event. Called by the
  // compositor (impl) thread.  Note it's expected that every call to
  // DidHandleInputEventOnCompositorThread where |event_state| is
  // EVENT_FORWARDED_TO_MAIN_THREAD will be followed by a corresponding call
  // to DidHandleInputEventOnMainThread.
  virtual void DidHandleInputEventOnCompositorThread(
      const WebInputEvent& web_input_event,
      InputEventState event_state);

  // Tells the scheduler that an input event of the given type is about to be
  // posted to the main thread. Must be followed later by a call to
  // WillHandleInputEventOnMainThread. Called by the compositor thread.
  virtual void WillPostInputEventToMainThread(
      WebInputEvent::Type web_input_event_type,
      const WebInputEventAttribution& web_input_event_attribution);

  // Tells the scheduler the input event of the given type is about to be
  // handled. Called on the main thread.
  virtual void WillHandleInputEventOnMainThread(
      WebInputEvent::Type web_input_event_type,
      const WebInputEventAttribution& web_input_event_attribution);

  // Tells the scheduler that the system processed an input event. Must be
  // called from the main thread.
  virtual void DidHandleInputEventOnMainThread(
      const WebInputEvent& web_input_event,
      WebInputEventResult result);

  // Tells the scheduler that the system is displaying an input animation (e.g.
  // a fling). Called by the compositor (impl) thread.
  virtual void DidAnimateForInputOnCompositorThread();

  // Tells the scheduler that the compositor thread queued up a BeginMainFrame
  // task to run on the main thread.
  virtual void DidScheduleBeginMainFrame();

  // Tells the scheduler that the main thread processed a BeginMainFrame task
  // from its queue. Note that DidRunBeginMainFrame will be called
  // unconditionally, even if BeginMainFrame early-returns without committing
  // a frame.
  virtual void DidRunBeginMainFrame();

  // Tells the scheduler about the change of renderer visibility status (e.g.
  // "all widgets are hidden" condition). Used mostly for metric purposes.
  // Must be called on the main thread.
  virtual void SetRendererHidden(bool hidden);

  // Tells the scheduler about the change of renderer background status, i.e.,
  // there are no critical, user facing activities (visual, audio, etc...)
  // driven by this process. A stricter condition than |SetRendererHidden()|,
  // the process is assumed to be foregrounded when the scheduler is
  // constructed. Must be called on the main thread.
  virtual void SetRendererBackgrounded(bool backgrounded);

  // Tells the scheduler about "keep-alive" state which can be due to:
  // service workers, shared workers, or fetch keep-alive.
  // If set to true, then the scheduler should not freeze the renderer.
  virtual void SetSchedulerKeepActive(bool keep_active);

  // Tells the scheduler when a begin main frame is requested due to input
  // handling.
  virtual void OnMainFrameRequestedForInput();

#if defined(OS_ANDROID)
  // Android WebView has very strange WebView.pauseTimers/resumeTimers API.
  // It's very old and very inconsistent. The API promises that this
  // "pauses all layout, parsing, and JavaScript timers for all WebViews".
  // Also CTS tests expect that loading tasks continue to run.
  // We should change it to something consistent (e.g. stop all javascript)
  // but changing WebView and CTS is a slow and painful process, so for
  // the time being we're doing our best.
  // DO NOT USE FOR ANYTHING EXCEPT ANDROID WEBVIEW API IMPLEMENTATION.
  virtual void PauseTimersForAndroidWebView();
  virtual void ResumeTimersForAndroidWebView();
#endif  // defined(OS_ANDROID)

  // RAII handle for pausing the renderer. Renderer is paused while
  // at least one pause handle exists.
  class BLINK_PLATFORM_EXPORT RendererPauseHandle {
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
  virtual std::unique_ptr<RendererPauseHandle> PauseRenderer()
      WARN_UNUSED_RESULT;

  // Returns true if the scheduler has reason to believe that high priority work
  // may soon arrive on the main thread, e.g., if gesture events were observed
  // recently.
  // Must be called from the main thread.
  virtual bool IsHighPriorityWorkAnticipated();

  // Sets the default blame context to which top level work should be
  // attributed in this renderer. |blame_context| must outlive this scheduler.
  virtual void SetTopLevelBlameContext(
      base::trace_event::BlameContext* blame_context);

  // Sets the kind of renderer process. Should be called on the main thread
  // once.
  virtual void SetRendererProcessType(WebRendererProcessType type);

 protected:
  WebThreadScheduler() = default;
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_SCHEDULER_WEB_THREAD_SCHEDULER_H_
