---
name: ios/use-repeating-callback-for-multiple-invocations
rubric: correctness
severity: should_fix
---

**Rule.** Use `base::RepeatingCallback` (or `base::RepeatingClosure`) and
`base::BindRepeating` instead of `base::OnceCallback` and `base::BindOnce` when
declaring API parameters, storing callbacks, or creating closures for operations
that may execute more than once. Specifically, when implementing timeout
fallbacks, retry loops, or multi-stage notifications where an asynchronous task
or completion handler remains active after a fallback path executes, declare and
retain the callback as a repeating callback so subsequent completions do not
invoke an already-run single-use callback.

**Why.** `base::OnceCallback` is a move-only type designed strictly for a single
invocation; attempting to invoke it a second time or after it has already run
(for example, by sharing a block generated from `base::CallbackToBlock` across a
timeout fallback and the eventual completion) triggers a runtime crash or
assertion failure. `base::RepeatingCallback`, created via `base::BindRepeating`,
is copyable and ref-counted, allowing safe multiple invocations across fallback,
retry, and eventual completion paths.

## Examples

Bad:

```cpp
// Bad: Block wrapping OnceCallback crashes if invoked by both timeout and completion.
void FetchDataWithTimeout(
    base::TimeDelta timeout,
    base::OnceCallback<void(const Response&)> completion_callback) {
  void (^completion_block)(const Response&) =
      base::CallbackToBlock(std::move(completion_callback));
  StartAsyncTask(base::BindOnce(completion_block));
  // Schedule a timeout fallback if the async fetch does not finish in time.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(completion_block, Response::TimeoutError()),
      timeout);
}
```

Good:

```cpp
// Good: RepeatingCallback can safely be invoked across timeout and completion paths.
void FetchDataWithTimeout(
    base::TimeDelta timeout,
    base::RepeatingCallback<void(const Response&)> completion_callback) {
  StartAsyncTask(completion_callback);
  // Schedule a timeout fallback if the async fetch does not finish in time.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(completion_callback, Response::TimeoutError()),
      timeout);
}
```
