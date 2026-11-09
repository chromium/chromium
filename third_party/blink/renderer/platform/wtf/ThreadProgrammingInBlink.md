# Thread Programming in Blink

This document provides an overview of thread programming, concurrency models,
and coding rules in Blink.

[TOC]

## Overview & Concurrency Model

Chromium uses a multi-process architecture where sandboxed renderer processes
execute Blink to render web pages. Within each renderer process, Blink employs
a specialized threading model designed to maximize performance, responsiveness,
and security while minimizing the pitfalls of concurrent programming.

### Renderer Threads

A renderer process hosts several types of execution contexts and threads:

* **Main Thread (1 per renderer process)**:
  Almost all core browser engine operations happen on the main thread. This
  includes non-worker JavaScript execution, the DOM tree, CSS styling, layout,
  and painting orchestration. Blink is highly optimized around the assumption of
  this mostly single-threaded architecture.
* **Worker & Worklet Threads**:
  Threads backing Web Workers (`DedicatedWorker`, `SharedWorker`),
  `ServiceWorker`, and `Worklet`s (`AudioWorklet`, `PaintWorklet`,
  `AnimationWorklet`). While each worker or worklet has an isolated global
  scope, they do not necessarily map 1:1 to dedicated OS threads; Blink shares
  or pools backing threads where possible (for example,
  `AnimationAndPaintWorkletThread` shares a single backing thread between
  Animation and off-thread Paint worklets, and Audio Worklets use shared backing
  thread pools) to minimize resource overhead.
* **Specialized Dedicated Threads**:
  Dedicated OS threads are created only when strictly necessary:
  * The **Chrome Compositor thread** (`cc`), handling scrolling, pinch-zoom, and
    smooth compositor animations independently from main-thread jank.
  * The **IO thread** (`Chrome_ChildIOThread`), handling IPC/Mojo messaging
    and inter-process communication.
* **Shared Thread Pool (`base::ThreadPool`)**:
  In general, `base::ThreadPool` should always be preferred over spawning
  dedicated OS threads. Asynchronous background workloads—such as V8 background
  compilation and garbage collection jobs, rasterization, font loading, media
  decoding, and general off-thread computation—execute on Chromium's shared
  thread pool. In Blink, off-thread tasks should be posted using
  `worker_pool::PostTask`, which wraps `base::ThreadPool` to enforce
  `CrossThreadBindOnce` and prevent passing thread-unsafe objects. (Real-time
  WebAudio output rendering callbacks are driven directly by the platform/OS
  audio subsystem rather than Blink-managed threads).

### The Message Passing Rule

In Blink, **sharing mutable state across threads is strongly discouraged**.
Primitives like mutexes (`base::Lock`), read-write locks, condition variables,
and atomic operations (`std::atomic`) should almost never be introduced in
ordinary Blink engine code.

Instead, Blink mandates a **message-passing model**: threads communicate by
posting asynchronous tasks containing owned or copied data across thread-safe
task runners.

#### Why is sharing state across threads discouraged?

1. **Complexity and Fragility**:
   Fine-grained locks (`base::Lock`) are notoriously error-prone and
   susceptible to deadlocks and priority inversions. Furthermore, ad-hoc
   locking and atomic operations (`std::atomic`) easily introduce subtle data
   races and memory-ordering bugs that are difficult to diagnose and reproduce.
2. **Performance Degradation & Priority Inversions**:
   Acquiring a lock on the main thread can cause priority inversions or jank if
   a lower-priority background thread holds the lock, stalling user input or
   frame rendering. Furthermore, concurrent access to shared cache lines across
   CPU cores triggers expensive cache coherency invalidations.
3. **Local Reasoning**:
   In a message-passing architecture, code running on a thread has exclusive
   access to its thread-bound state. Developers can reason locally about object
   lifetimes and invariants without fearing concurrent mutation.

Except for rare, strictly reviewed, performance-critical paths (e.g., real-time
audio processing buffers or low-level allocator internals in `PartitionAlloc`),
cross-thread operations in Blink must be accomplished via tasks.

---

## Cross-Thread Communication (Tasks)

Asynchronous tasks are the primary mechanism for thread-to-thread communication.
For a complete guide to task schedulers and task runners, see
[Task scheduling in Blink](../scheduler/TaskSchedulingInBlink.md).

### Posting Cross-Thread Tasks

To post a task across threads, use `PostCrossThreadTask` (defined in
[`platform/scheduler/public/post_cross_thread_task.h`](../scheduler/public/post_cross_thread_task.h))
along with `blink::CrossThreadBindOnce`:

```cpp
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

// Posting a task to another thread's task runner:
PostCrossThreadTask(*destination_task_runner, FROM_HERE,
                    CrossThreadBindOnce(&TargetClass::MethodOnTargetThread,
                                        target_arg1, target_arg2));
```

To receive a reply on the originating thread upon task completion, pass an
attributed reply task runner (such as `ExecutionContext::GetTaskRunner(...)`)
as a parameter into the cross-thread task, and have the background task post back
using `PostCrossThreadTask` (see the
[Oilpan example below](#passing-gc-references-across-threads)). Do not use
`base::PostTaskAndReply` or `PostCrossThreadTaskAndReply` on the main thread
because they implicitly post replies to the unattributed default task runner
(`base::SequencedTaskRunner::GetCurrentDefault()`), bypassing scheduler frame
attribution.

### Binding Tasks: `blink::BindOnce` vs `blink::CrossThreadBindOnce`

In standard Chromium code outside Blink, `base::BindOnce` and
`base::BindRepeating` are common. However:

> **Important**: Direct usage of `base::BindOnce` and `base::BindRepeating` is
> **banned** within `//third_party/blink/`.

Many Blink data structures are strictly bound to a single thread (such as
garbage-collected objects, DOM nodes, and the few remaining thread-hostile
types like `SecurityOrigin`). Passing thread-bound types across threads causes
memory corruption and concurrency bugs. Blink enforces safety at compile-time
and runtime using two specialized binders:

1. **`blink::BindOnce` / `blink::BindRepeating`**
   (from [`platform/wtf/functional.h`](functional.h)):
   * **Purpose**: For callbacks invoked and destroyed on the **same thread**.
   * **Protection**: In debug builds, the returned closure verifies (via
     `ThreadCheckingCallbackWrapper`) that it is both executed and destroyed on
     the sequence where `BindOnce` was called. Additionally, compile-time
     checks (`CheckGCedTypeRestriction`) disallow all raw pointers (requiring
     explicit `blink::Unretained()` for off-heap pointers), stack-allocated
     types, and raw `Member<T>` / GC objects.
2. **`blink::CrossThreadBindOnce` / `blink::CrossThreadBindRepeating`**
   (from [`platform/wtf/cross_thread_functional.h`](cross_thread_functional.h)):
   * **Purpose**: For callbacks that may be invoked or destroyed on a
     **different thread**.
   * **Protection**: Enforces the same `CheckGCedTypeRestriction` compile-time
     checks (disallowing all raw pointers without `blink::CrossThreadUnretained()`,
     stack types, and raw GC references), plus verifies that no thread-unsafe
     ref-counted types (`base::RefCounted`) are bound.
     *Note*: It cannot detect transitive thread unsafety (such as passing a
     `Persistent<T>` or custom structures that internally hold thread-bound
     state), so developers must ensure all bound arguments are genuinely safe to
     transfer.

### Task Runners and Attribution

Main-thread tasks should almost never be posted to an un-attributed, generic
task runner.

* Usage of `base::SingleThreadTaskRunner::GetCurrentDefault()` and
  `base::SequencedTaskRunner::GetCurrentDefault()` is **strictly banned** in
  `blink/` because the scheduler cannot determine which frame or document owns
  the task.
* Instead, always retrieve a task runner associated with a specific frame or
  execution context (e.g., `ExecutionContext::GetTaskRunner(TaskType)`) so that
  the scheduler can throttle, defer, pause (via `ScopedPagePauser`), or freeze
  tasks when appropriate. See
  [TaskSchedulingInBlink.md](../scheduler/TaskSchedulingInBlink.md) for details.

### Off-Main-Thread Background Work

If you need to perform heavy computation that does not touch DOM or V8 objects,
do **not** create a new dedicated OS thread. Instead, dispatch tasks to
Chromium's shared thread pool via `worker_pool::PostTask`:

```cpp
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

worker_pool::PostTask(
    FROM_HERE,
    CrossThreadBindOnce(&DoHeavyBackgroundComputation, input_data));
```

---

## Memory Management Across Threads

Blink relies on multiple memory allocators. For an overview of allocation
rules, see [Memory management in Blink](Allocator.md). The interaction between
memory allocators and threading is critical.

### Oilpan (`GarbageCollected<T>`) Thread Affinity

Oilpan is Blink's C++ garbage collection system. Every `GarbageCollected<T>`
object belongs to a specific thread's heap isolate (such as the main thread
heap or a worker thread heap).

* **Strict Thread Isolation**: A `GarbageCollected` object must **never** be
  dereferenced, manipulated, or traversed from any thread other than its owning
  thread.
* **Prohibited Graph Traversals**: Traversing object graphs (following
  `Member<T>` pointers) from another thread is undefined behavior because the
  owning thread's garbage collector may be running, sweeping, or moving objects
  concurrently.

#### Passing GC References Across Threads

When a method or task on a background thread needs to reference an
Oilpan-managed object located on another thread:

1. **`CrossThreadHandle<T>` / `CrossThreadWeakHandle<T>` (Recommended)**
   Defined in
   [`platform/heap/cross_thread_handle.h`](../heap/cross_thread_handle.h),
   handles safely hold an Oilpan object from another thread and allow
   thread-safe copying, moving, and destruction without allowing unsafe
   off-thread dereferencing.

   * Use `MakeCrossThreadHandle` and `MakeUnwrappingCrossThreadHandle` when the
     target object must remain alive until the task runs.
   * Use `MakeCrossThreadWeakHandle` and `MakeUnwrappingCrossThreadWeakHandle`
     when dispatching back to the owning thread weakly, ensuring that if the
     target object is garbage-collected or destroyed before the task executes,
     the callback is safely dropped.

   ```cpp
   class ComputeController : public GarbageCollected<ComputeController> {
    public:
     explicit ComputeController(
         scoped_refptr<base::SingleThreadTaskRunner> task_runner)
         : task_runner_(std::move(task_runner)) {}

     void StartBackgroundJob() {
       CHECK(IsMainThread());
       worker_pool::PostTask(
           FROM_HERE,
           CrossThreadBindOnce(&ComputeController::ProcessOnBackground,
                               MakeCrossThreadWeakHandle(this),
                               task_runner_));
     }

     void OnJobFinished(int result) {
       CHECK(IsMainThread());
       // Safe: Executing on the main thread where 'this' lives.
     }

     void Trace(Visitor* visitor) const {}

    private:
     scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

     static void ProcessOnBackground(
         CrossThreadWeakHandle<ComputeController> controller,
         scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
       CHECK(!IsMainThread());
       // Do background work...
       int result = 42;

       // Post back using MakeUnwrappingCrossThreadWeakHandle to invoke an
       // instance method safely on the original thread:
       PostCrossThreadTask(
           *task_runner, FROM_HERE,
           CrossThreadBindOnce(&ComputeController::OnJobFinished,
                               MakeUnwrappingCrossThreadWeakHandle(
                                   std::move(controller)),
                               result));
     }
   };
   ```

2. **`CrossThreadPersistent<T>` / `CrossThreadWeakPersistent<T>` (Legacy)**
   Defined in
   [`platform/heap/cross_thread_persistent.h`](../heap/cross_thread_persistent.h),
   these provide cross-thread GC roots via `WrapCrossThreadPersistent`.
   * *Caveats*: Neither `CrossThreadPersistent` nor `CrossThreadHandle` keeps
     the owning thread or its heap alive (if the owning thread terminates,
     accessing the pointer results in a use-after-free). However,
     `CrossThreadPersistent` exposes raw pointer access across threads, making
     accidental off-thread dereferencing easy. `CrossThreadHandle` strictly
     prevents off-thread access (crashing on incorrect thread access) and
     integrates with task binding to safely unwrap only on the creation thread.
     Strongly prefer `CrossThreadHandle` for modern code.

### Reference Counting: `blink::RefCounted` vs `blink::ThreadSafeRefCounted`

* **`blink::RefCounted<T>`** (from
  [`platform/wtf/ref_counted.h`](ref_counted.h)):
  Uses non-atomic reference counts. It is **thread-hostile** and must only be
  referenced, copied, and destroyed on a single thread.
* **`blink::ThreadSafeRefCounted<T>`** (from
  [`platform/wtf/thread_safe_ref_counted.h`](thread_safe_ref_counted.h)):
  Uses atomic operations (`base::RefCountedThreadSafe`) for its reference count.
  Instances may be retained across multiple threads via `scoped_refptr<T>`.
  * *Guideline*: Even with `ThreadSafeRefCounted`, shared ownership across
    threads easily obscures object lifetimes and can hide races. Prefer clear,
    single ownership (e.g., transferring `std::unique_ptr` via `std::move`)
    wherever possible.

### Strings and Containers

(Note: All core WTF types live in `namespace blink`.)

* **`blink::String`**:
  `blink::String` instances are safe to transfer across threads via move
  semantics or copy. `StringImpl` utilizes atomic reference counting, making
  `String` free-threaded. However, passing `String` by `const String&` across
  functions on the same thread avoids unnecessary atomic reference count
  increments.
* **`blink::AtomicString`**:
  Unlike early versions of WebKit/Blink where `AtomicString` was strictly
  thread-hostile, modern Blink uses a single, process-wide, thread-safe
  `AtomicStringTable`. While `AtomicString` can be safely passed and read across
  threads, inserting or removing strings from the table acquires a global lock.
  To avoid lock contention across threads, minimize unnecessary `AtomicString`
  allocation on background worker threads.
* **Containers (`Vector`, `HashMap`, `HashSet`, `Deque`)**:
  None of Blink's collection types provide internal thread synchronization.
  They cannot be modified concurrently and should be transferred across
  threads only via move semantics (e.g. `std::move(vector)`) inside a
  cross-thread task.

---

## Threading Primitives in WTF

WTF provides basic utilities for querying and handling threads:

### Thread Queries and Assertions

In accordance with Chromium's
[styleguide](https://chromium.googlesource.com/chromium/src/+/main/styleguide/c++/checks.md#check_dcheck_and-notreached),
prefer `CHECK()` by default to enforce thread invariants. Use `DCHECK()` only in
performance-sensitive hot loops where the thread-local storage access overhead
of `IsMainThread()` is a measured concern:

```cpp
#include "third_party/blink/renderer/platform/wtf/wtf.h"

void Document::UpdateStyle() {
  // Assert that code executes exclusively on the main thread:
  CHECK(IsMainThread());
  ...
}
```

* `blink::IsMainThread()`: Returns true if called from the renderer's main
  thread.
* `blink::CurrentThread()` (from [`platform/wtf/threading.h`](threading.h)):
  Returns the current `base::PlatformThreadId`.
* For non-main thread sequences (e.g., worker threads), prefer Chromium's
  `SEQUENCE_CHECKER`:
  ```cpp
  #include "base/sequence_checker.h"

  class WorkerSubsystem {
   public:
    void DoWork() {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      ...
    }
   private:
    SEQUENCE_CHECKER(sequence_checker_);
  };
  ```

### Thread-Local Storage: `ThreadSpecific<T>`

When state must be stored on a per-thread basis without creating global lock
contention, use `blink::ThreadSpecific<T>` (defined in
[`platform/wtf/thread_specific.h`](thread_specific.h)):

```cpp
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

struct ThreadLocalContext {
  int counter = 0;
};

ThreadSpecific<ThreadLocalContext>& GetThreadLocalContext() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<ThreadLocalContext>, context, ());
  return context;
}

void IncrementLocal() {
  GetThreadLocalContext()->counter++;
}
```

* Instances of `ThreadSpecific<T>` allocate memory using `PartitionAlloc`
  (`Partitions::FastZeroedMalloc`) and leverage platform TLS.
* Destructors of objects in `ThreadSpecific` are executed when non-main threads
  terminate, but are bypassed on main thread exit during fast process shutdown.

---

## Best Practices & Summary Checklist

| Rule | Explanation |
| :--- | :--- |
| **Avoid `base::Lock` / Mutexes** | Prefer message passing via `PostCrossThreadTask` over shared mutable state. |
| **Use `blink::BindOnce` for same-thread** | `base::Bind*` is banned; `blink::BindOnce` ensures thread affinity is checked. |
| **Use `CrossThreadBindOnce` for cross-thread** | Validates parameter safety across thread boundaries at compile-time. |
| **Never access Oilpan objects off-thread** | Oilpan heaps belong to a single thread; use `CrossThreadHandle` to reference them from background tasks. |
| **Minimize `AtomicString` allocation off-thread** | `AtomicStringTable` is thread-safe but internally locked; minimize off-thread allocations to avoid lock contention. |
| **Use `worker_pool::PostTask` for background tasks** | Do not spawn dedicated OS threads for asynchronous background computations. |
| **Assert thread affinity** | Use `CHECK(IsMainThread())` (or `DCHECK` on hot paths) or `SEQUENCE_CHECKER` to enforce execution thread expectations. |
