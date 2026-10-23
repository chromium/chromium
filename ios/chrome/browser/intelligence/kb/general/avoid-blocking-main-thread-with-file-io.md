---
name: ios/avoid-blocking-main-thread-with-file-io
description: When performing disk I/O, file reachability, or image loading on the main thread, offload them to base::ThreadPool with base::MayBlock() or background queues
rubric: safety
severity: should_fix
citations:
  - path: docs/threading_and_tasks.md
    heading: Keeping the Browser Responsive
    quote: Do not perform expensive work on the main thread, the IO thread or any sequence that is expected to run tasks with a low latency. Instead, perform expensive work asynchronously using `base::ThreadPool::PostTaskAndReply*()` or `base::SequencedTaskRunner::PostTaskAndReply()`.
---

**Rule.** Do not perform synchronous disk I/O, file reachability checks, or
filesystem existence queries on the UI or main thread. Offload blocking file and
disk operations—such as `base::PathExists`, `base::ReadFileToString`,
`-[NSURL checkResourceIsReachableAndReturnError:]`,
`+[UIImage imageWithContentsOfFile:]`, and `NSData` file reads—to a background
task runner using `base::ThreadPool::PostTaskAndReply()` or
`base::ThreadPool::PostTaskAndReplyWithResult()` with the `{base::MayBlock()}`
task trait.

**Why.** Synchronous disk access and filesystem queries block the calling thread
until I/O completes. Invoking blocking operations (such as `base::PathExists`,
`base::ReadFileToString`, `-[NSURL checkResourceIsReachableAndReturnError:]`,
`+[UIImage imageWithContentsOfFile:]`, or `NSData` file reads) on the main
thread blocks the UI run loop, resulting in dropped frames, UI unresponsiveness,
and iOS watchdog termination crashes (`0x8badf00d`). Offloading blocking I/O to
`base::ThreadPool` keeps the UI responsive while allowing completion callbacks
to run safely on the main sequence.

## Examples

Bad:

```cpp
// Bad: Synchronously querying file reachability on the UI/main thread.
bool IsFileReady(NSURL* file_url) {
  return [file_url checkResourceIsReachableAndReturnError:nil];
}
```

Good:

```cpp
// Check file existence on a background sequence to avoid blocking the main
// thread with disk I/O.
base::ThreadPool::PostTaskAndReplyWithResult(
    FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
    base::BindOnce(&base::PathExists, base::apple::NSURLToFilePath(file_url)),
    std::move(callback));
```
