---
name: ios/guard-cancelable-callback-pointer-before-cancel
description: When cancelling asynchronous tasks via a pointer or smart pointer to a cancelable callback, verify the pointer is non-null before calling Cancel()
rubric: safety
severity: should_fix
---

**Rule.** When cancelling asynchronous operations or racing tasks via a raw
pointer or smart pointer to a cancelable callback (such as
`base::CancelableOnceCallback` or `base::CancelableRepeatingCallback`), verify
that the pointer is non-null before invoking `->Cancel()`. Do not assume the
pointer remains valid or engaged inside asynchronous completion callbacks where
sibling tasks or teardown may have already run or reset the callback.

**Why.** In concurrent or racing asynchronous workflows, completion or failure
of one task often resets the cancelable callback pointer. Invoking `->Cancel()`
through an unverified pointer risks a null pointer dereference crash if the
referenced task already completed or was cleaned up.

## Examples

Bad:

```cpp
void FooFetcher::OnImageDataFetched(
    const std::string& data,
    const image_fetcher::RequestMetadata& metadata) {
  // Unconditionally dereferencing can crash if thumbnail fetch completed first.
  thumbnail_callback_->Cancel();
  thumbnail_callback_.reset();
}
```

Good:

```cpp
void FooFetcher::OnImageDataFetched(
    const std::string& data,
    const image_fetcher::RequestMetadata& metadata) {
  if (thumbnail_callback_) {
    thumbnail_callback_->Cancel();
    thumbnail_callback_.reset();
  }
}
```
