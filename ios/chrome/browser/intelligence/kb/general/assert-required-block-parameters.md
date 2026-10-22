---
name: ios/assert-required-block-parameters
rubric: correctness
severity: should_fix
---

**Rule.** When implementing Objective-C methods that accept required block
parameters (such as `ProceduralBlock`, callback, completion, or resolver
blocks), assert non-nullness using `CHECK(block)` at API entry or initialization
(using a bare `CHECK(block)` when continuing cannot be made safe, or
`CHECK(block, base::NotFatalUntil::M<n>)` with `M<n>` set to `MAJOR` in
`//chrome/VERSION` plus 3 or 4 and a graceful fallback when execution can safely
recover) rather than returning silent fallbacks. When directly executing
optional block parameters—including in early-return, fallback, or synchronous
execution branches where callers may pass `nil`—always guard the invocation with
`if (block)` before invoking it to prevent null-pointer dereferences.
Conversely, when a callback or confirmation block is optional and `nil` has
well-defined semantics (such as auto-approving or continuing a flow), do not
assert non-nil with `CHECK`; handle `nil` explicitly and execute the default
behavior.

**Why.** In Objective-C and Objective-C++, block invocations are direct C-style
function pointer calls rather than runtime message dispatches. Unlike sending an
Objective-C message to `nil` (which safely evaluates to a no-op), directly
invoking a `nil` block pointer dereferences null and triggers an immediate fatal
crash (`EXC_BAD_ACCESS` / `SIGSEGV`). Because Chromium iOS style prohibits
nullability annotations (`nonnull`, `nullable`) in favor of runtime checks,
required blocks must fail fast at entry with `CHECK()` rather than masking
contract errors with fallbacks. Conversely, asserting `CHECK()` on an optional
block with defined `nil` semantics creates false invariants and crashes valid
callers instead of executing the expected default flow.

## Examples

Bad:

```cpp
// 1. Required block not asserted at entry:
- (LensOverlayBottomSheetDetentProxy*)detentWithResolver:(CGFloat (^)())heightResolver {
  return [[LensOverlayBottomSheetDetentProxy alloc] initWithResolver:heightResolver];
}

// 2. Optional block invoked without nil guard in fallback branch:
if (presentingViewController) {
  [presentingViewController dismissViewControllerAnimated:animated completion:completion];
} else {
  completion();  // Crashes with EXC_BAD_ACCESS if caller passed nil.
}

// 3. Optional block with default semantics asserted with CHECK:
- (void)confirmChangeProfile:(SigninChangeProfileConfirmationBlock)confirmBlock {
  CHECK(confirmBlock);
  confirmBlock(^(BOOL canProceed) { [_delegate didConfirmChangeProfileCanProceed:canProceed]; });
}
```

Good:

```cpp
// 1. Assert required block at entry:
- (LensOverlayBottomSheetDetentProxy*)detentWithResolver:(CGFloat (^)())heightResolver {
  CHECK(heightResolver);
  return [[LensOverlayBottomSheetDetentProxy alloc] initWithResolver:heightResolver];
}

// 2. Guard optional block before invocation:
if (presentingViewController) {
  [presentingViewController dismissViewControllerAnimated:animated completion:completion];
} else if (completion) {
  completion();
}

// 3. Handle nil optional block with default behavior:
- (void)confirmChangeProfile:(SigninChangeProfileConfirmationBlock)confirmBlock {
  if (!confirmBlock) {
    [_delegate didConfirmChangeProfileCanProceed:YES];
    return;
  }
  confirmBlock(^(BOOL canProceed) { [_delegate didConfirmChangeProfileCanProceed:canProceed]; });
}
```
