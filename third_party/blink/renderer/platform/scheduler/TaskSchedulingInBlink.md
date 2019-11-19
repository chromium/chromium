# Task scheduling in Blink

This document provides an overview of task scheduling in Blink and Chrome’s
renderer process and outlines best practises for posting tasks.

## Overview
Most of Blink is essentially single-threaded: most important things happen
on the main thread (including Javascript execution, DOM, CSS, layout calculations),
which means that there are many things which want to run on the main thread
at the same time. Therefore Blink needs a scheduling policy to prioritise
the right thing — for example, to schedule input handling above everything else.

The majority of the scheduling logic deals explicitly with main thread
scheduling and this document assumes that we are talking about the main thread
unless stated otherwise. If you don’t need DOM access, please refer to
[the off main thread scheduling section](#off-main-thread-scheduling)
to find out how to schedule this type of work.

## Tasks

The main scheduling unit in Blink is a task. A task is a base::Closure posted via
TaskRunner::PostTask or TaskRunner::PostDelayedTask interface. The regular method of
creating closures (base::BindOnce/Repeating) [is banned](#binding-tasks).
Blink should use WTF::Bind (for tasks which are posted to the same thread) and
CrossThreadBind
([for tasks which are posted to a different thread](#off-main-thread-scheduling)).

At the moment Blink Scheduler treats tasks as an atomic unit — if a task has started,
it can’t be interrupted until it completes. The scheduler can only choose a new task
to run from the eligible tasks or can elect not to run any task at all.

## Task runners

In order to post a task, a task runner reference is needed. Almost all main
thread tasks should be associated with a frame to allow the scheduler to freeze
or prioritise individual frames. This is a hard requirement backed by a DCHECK
that a task running javascript should have this association
(which is being introduced).

FrameScheduler::GetTaskRunner (or its aliases LocalFrame::GetTaskRunner,
WebLocalFrame::GetTaskRunner, RenderFrame::GetTaskRunner or
ExecutionContext::GetTaskRunner) should be used for that. They return a task runner
which continues to run tasks after the frame is detached.


## Thread-global task runners

Some tasks can’t be associated with a particular frame. One example is garbage
collection which interacts with all javascript heaps in the isolate. For these use-cases
a named per-thread task runner should be used:
- for content/: blink::scheduler::WebThreadScheduler::Current()->SpecificTaskRunner()
- for blink/: blink::scheduler::ThreadScheduler::Current()->SpecificTaskRunner()

Per-thread task runners include:
- Compositor task runner
- GC task runner
- Cleanup task runner
- Default (deprecated)
- Input task runner (semi-deprecated)
- IPC (semi-deprecated)

New task runners might be added in the future; contact scheduler-dev@chromium.org
if you think you need a new one.

## ThreadTaskRunnerHandle::Get

base::ThreadTaskRunnerHandle::Get is a way to get a task runner and
it returns a default task runner for a thread. Because tasks posted to it
lack any attribution, the scheduler can’t properly schedule and prioritise them.

ThreadTaskRunnerHandle::Get usages are banned in blink/ and content/renderer/
directories and strongly discouraged elsewhere in the renderer process.
Please help us to convert them to the appropriate task runner
(usually per-frame one). See
[https://docs.google.com/document/d/1k7EEHQUEujgQ7BAhbmNdeaddwfJPWp7qjLy8mnVTQ9Y/edit](this guideline)
for more details.


## Task types and task sources

In addition to frame association, the scheduler also needs to know the nature
of the task to correctly handle it. blink::TaskType encodes this information.
TaskType is a required parameter of all GetTaskRunner() methods and FrameScheduler
returns an appropriate task runner based on the TaskType.

All tasks mentioned in the spec should have task source explicitly defined
(e.g. see [generic task sources definition](https://html.spec.whatwg.org/C/#generic-task-sources)
in the spec). There are still some places where the task source is not mentioned
explicitly — reach out to domenic@ and garykac@ for advice.

For the non-speced tasks (for example, clean up caches or record metrics.
Note that these tasks shouldn’t run javascript), kInternal\* task types should be used.

If you’re happy with the default scheduling policies, which should happen in the
majority of cases, kInternalDefault task type should be used. Otherwise, reach out to
scheduler-dev@chromium.org to discuss adding new task type for your needs.

## Scheduling policies

### Priorities

The scheduler selects the next task to run based on the priority
(modulo some starvation logic). The tasks with the same priority run in order.

There are following rules to assign priorities:
- Input task runner has the highest priority.
- Compositor task runner has high priority when user gestures are observed.
- There are several ongoing experiments to increase or decrease priorities for individual frames.

The default priority is normal.

### Pausing

During synchronous dialogs (alert() or print()) or inside v8 debugger
breakpoints scheduler enters a nested run loop and stops running pauseable tasks.
Specifically, no javascript can run when a page is paused.

The pausing is triggered by ScopedPagePauser. Almost all tasks can be paused.

### Deferring

Scheduler may elect not to run some tasks when processing user gestures
in order to increase responsiveness.

Scheduler defers tasks for two seconds after a user gesture, as it’s very likely
that another gesture will arrive soon. The majority of tasks can be deferred.

Most of the tasks are deferrable.

### Freezing

Scheduler freezes background pages in order to increase responsiveness of
the foreground tabs and save power.

On mobile all pages are frozen after five minutes in the background.
On desktop only eligible pages are frozen, which is determined by heuristics
based on the APIs page is using.

Most of the tasks are freezable.

### Throttling

Scheduler delays tasks in the background pages and offscreen frames in order
to improve responsiveness of the foreground pages without breaking
useful background page functionality.

At the moment only javascript timers (setTimeout/setInterval) are throttleable.
While there is a general desire to expand this list, this is a low priority effort
as we are focused on making freezing better instead.

## Off-main thread scheduling

If your task doesn’t have to run on the main thread, use
worker_pool::PostTask, which uses a thread pool
behind the scenes.

Do not create your own dedicated thread if you need ordering for your tasks,
use worker_pool::CreateTaskRunner instead —
this creates a sequence (virtual thread which can run tasks in order on
any of the threads in the thread pool).
(Note: this doesn't exist yet because we haven't encountered a use case in Blink
which needs it. If you need one, please task to scheduler-dev@ and we'll add it).

See [threading and tasks](../../../../../docs/threading_and_tasks.md) for
more details.

## Binding tasks

Many data structures in Blink are bound to a particular thread (e.g. Strings,
garbage-collected classes, etc), so it’s not safe to pass a pointer to them to
another thread. To enforce this, base::Bind is banned in Blink and WTF::Bind
and CrossThreadBind are provided as alternatives. WTF::Bind should be used
to post tasks to the same thread and closures returned by it DCHECK that
they run on the same thread. CrossThreadBind applies CrossThreadCopier
to its arguments and creates a deep copy, so the resulting closure can run
on a different thread.


## TODO(altimin): Document idle tasks

