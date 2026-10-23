---
name: ios/avoid-unretained-for-background-tasks
description: When offloading tasks to background runners, pass value copies to static or free functions without base::Unretained, WeakPtr, or self, and avoid secondary detached SEQUENCE_CHECKERs
rubric: safety
severity: blocking
citations:
  - path: ios/best_practices.md
    heading: Concurrency Best Practices
    quote: When running background work, pass owned/copied inputs into a stateless callback that touches no sequence-bound objects (`self` or `this`), and deliver outputs back to the originating sequence via `PostTaskAndReply` or `PostTaskAndReplyWithResult`.
  - path: docs/threading_and_tasks.md
    heading: Posting a Parallel Task
    quote: base::ThreadPool::PostTaskAndReplyWithResult(FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE}, base::BindOnce(&ReadFromDisk, path), base::BindOnce(&OnReadFromDisk, weak_ptr_factory_.GetWeakPtr()));
---

**Rule.** Do not pass `base::Unretained(this)`, dereference `base::WeakPtr`, or
capture Objective-C `self` in tasks dispatched to background task runners or
thread pools (`base::ThreadPool`, `base::SequencedTaskRunner`). Sequence-affine
objects (such as UI components, notification clients, or `BookmarkModel`) must
remain on their owning sequence. When offloading work to background runners,
make background worker operations static member functions, free functions (such
as in an anonymous namespace), or self-contained closures that accept only
required inputs by value copy or `std::move()`, and return results to the
calling sequence using `PostTaskAndReply` (binding `base::WeakPtr` or weak
`self` only in the reply callback). Additionally, do not declare secondary
`SEQUENCE_CHECKER` or `THREAD_CHECKER` members detached with
`DETACH_FROM_SEQUENCE` in sequence-affine classes to validate background
execution; instead, verify sequence execution within the worker function using
`CHECK(task_runner->RunsTasksInCurrentSequence())`.

**Why.** Binding `base::Unretained(this)` or capturing `self` across sequences
introduces severe use-after-free and data-race hazards if the object is
destroyed on the originating sequence while the background task is queued or
executing. Unlike same-sequence tasks where `base::WeakPtr` provides safe
cancellation, `base::WeakPtr` is sequence-affine and invalid to dereference on
background threads. In Chromium and iOS, UI components, clients, and core models
are bound to the UI sequence, so accessing them off-sequence triggers
sequence-checker crashes. Furthermore, `SEQUENCE_CHECKER` is designed to verify
single-sequence affinity; adding secondary sequence checkers detached on
construction encourages dangerous cross-sequence member access via raw pointers.
Extracting self-contained static or free functions that accept value copies and
return results via `PostTaskAndReply` enforces explicit data dependencies and
guarantees lifetime safety without binding cross-sequence instance state.

## Examples

Bad:

```cpp
// Bad: Unretained(this) with detached SEQUENCE_CHECKER for background tasks.
file_runner_->PostTaskAndReply(
    FROM_HERE,
    base::BindOnce(&DownloadService::DoMove, base::Unretained(this), src, dst),
    base::BindOnce(&DownloadService::OnDone, weak_factory_.GetWeakPtr()));

// Bad: Dereferencing sequence-affine WeakPtr or capturing self on background runner.
task_runner_->PostTask(
    FROM_HERE,
    base::BindOnce(&Client::UntrackBookmark, weak_factory_.GetWeakPtr(), url));
task_runner_->PostTask(FROM_HERE, base::BindOnce(^{
  [self processItems:_items];
}));
```

Good:

```cpp
// Good: Static worker with RunsTasksInCurrentSequence and reply on calling sequence.
// Perform file I/O in the background to avoid blocking the main sequence.
file_runner_->PostTaskAndReply(
    FROM_HERE,
    base::BindOnce(&DownloadService::DoMove, file_runner_, src, dst),
    base::BindOnce(&DownloadService::OnDone, weak_factory_.GetWeakPtr()));
static void DownloadService::DoMove(
    scoped_refptr<base::SequencedTaskRunner> runner, base::FilePath s, base::FilePath d) {
  CHECK(runner->RunsTasksInCurrentSequence());
}

// Good: Pass copied values to background worker and return result via reply callback.
__weak __typeof(self) weakSelf = self;
// Offload item processing to the background task runner to keep UI responsive.
task_runner_->PostTaskAndReply(
    FROM_HERE, base::BindOnce(&ProcessData, copied_items),
    base::BindOnce(^{
      [weakSelf didProcessItems];
    }));
```
