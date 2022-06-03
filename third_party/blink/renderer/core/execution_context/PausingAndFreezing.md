# Pausing and Freezing

There are two main mechanisms to halt execution flow in a frame:

* Pausing
* Freezing

Pausing is generally only used for nested event loops (ie. synchronous print). All other types of halting should use freezing.

# Execution Context States

There are 4 lifecycle states [defined](https://cs.chromium.org/chromium/src/third_party/blink/public/mojom/frame/lifecycle.mojom):

* kRunning - actively running context
* kPaused - Paused state. Does not fire frozen or resumed events on the document.
* kFrozen - Frozen state, pause all media.
* kFrozenAutoResumeMedia - Frozen state, resume media when resuming.

## kPaused
* Used for synchronous mechanisms for a single thread, eg. window.print, V8 inspector debugging.
* Fires [ExecutionContextLifecycleStateObserver][ExecutionContextLifecycleStateObserver] changed for kPaused.
* Some [ExecutionContextLifecycleStateObserver][ExecutionContextLifecycleStateObserver] may drop mojo connections.
* Not visible to page in terms of state transitions.
* Does *not* [freeze](https://wicg.github.io/page-lifecycle/spec.html#freeze-steps) or [resume](https://wicg.github.io/page-lifecycle/spec.html#resume-steps) the document.
* Pauses execution of [pausable task queues][TaskQueues] in MainThreadScheduler.

## kFrozen
* Used iframe feature policies, Resource Coordinator background policies. (Should be used for bfcache in the future)
* Fires [ExecutionContextLifecycleStateObserver][ExecutionContextLifecycleStateObserver] change for kFrozen.
* Executes [freeze](https://wicg.github.io/page-lifecycle/spec.html#freeze-steps) and [resume](https://wicg.github.io/page-lifecycle/spec.html#resume-steps) algorithms.
* [Dedicated workers][DedicatedWorker] freeze via a [ExecutionContextLifecycleStateObserver][ExecutionContextLifecycleStateObserver] callback.
* Freezes execution of [frozen task queues][TaskQueues] in MainThreadScheduler.
* Pauses execution of [pausable task queues][TaskQueues] in MainThreadScheduler. (This is a proposed feature, with it we would remove the definition of frozen
task queues in the scheduler).

# Freezing IPCs

Top level documents are frozen via [PageMsg_SetPageFrozen](https://cs.chromium.org/search/?q=PageMsg_SetPageFrozen&sq=package:chromium&type=cs) message. This
will freeze and entire frame tree.

Individual frames may be frozen via [SetLifecycleState](https://cs.chromium.org/chromium/src/content/common/frame.mojom?g=0) freezing only an individual frame.
Subframes will need to be frozen independently with a separate IPC.

Frame freezing can also be initiated by the [Page Scheduler][#Page Scheduler] under certain conditions.

# Page Scheduler

The [Page Scheduler][PageScheduler] also generates frozen state transitions.

Note: These state transitions do not work correctly for OOPIFs because the information is not propagated to the entire frame tree.

It is desirable to move all of these transitions to the browser side.


# Freezing Workers

In order to freeze a worker/worklet an implementation will pauses execution of all [pausable task queues][# Task Queues] and tne enters a
nested event loop. Only the none pausable tasks will execute in the nested event loop. Explicitly the Internal Worker task queue will
be used to resume the worker.

To freeze workers the Workers themselves are [ExecutionContextLifecycleStateObservers][ExecutionContextLifecycleStateObserver] and they listen to
the kFrozen/kResume state of the owning execution context and then propagate that to their own execution context.


[ExecutionContextLifecycleStateObserver]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/execution_context/execution_context_lifecycle_state_observer.h
[DedicatedWorker]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/workers/dedicated_worker.h
[PageScheduler]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h
[TaskQueues]: https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/public/platform/TaskTypes.md
