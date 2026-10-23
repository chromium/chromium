---
name: ios/invoke-callback-on-early-return
description: When accepting or retaining completion callbacks or Objective-C blocks, invoke (checking nil) or forward them along all early returns, switch cases, and teardowns
rubric: correctness
severity: blocking
citations:
  - path: docs/callback.md
    heading: OnceCallback<> And RepeatingCallback<>
    quote: base::OnceCallback<> is created by base::BindOnce(). This is a callback variant that is a move-only type and can be run only once.
---

**Rule.** When implementing a function, coordinator, mediator, delegate handler
(such as WebKit delegate methods like RunOpenPanel), or script message handler
that accepts or retains a completion callback (such as base::OnceCallback or
base::OnceClosure) or an Objective-C completion block, ensure it is invoked or
forwarded along every control-flow path and teardown lifecycle. Do not allow the
callback, block, or enclosing struct to be silently dropped in early returns,
validation bailouts, switch default cases, or component teardowns (such as
disconnect or stop). For Objective-C completion blocks, verify that the block is
non-nil before invoking it (e.g., if (completion) { completion(fallback_value);
}). For base::OnceCallback, run the callback via std::move(callback).Run(...).
Pass appropriate fallback, error, cancel, or default values (such as nil, false,
or NO), or forward the callback to a downstream asynchronous task.

**Why.** Callers awaiting completion handlers—such as framework delegates,
WebKit processes, UI coordinators, tasks releasing resources (e.g.,
security-scoped access), and JavaScript callers awaiting Promises from script
message handlers—rely on completion callbacks and blocks to finalize operations,
sequence subsequent actions, update UI state, unblock state machines, and
release waiting resources. Dropping a callback or block unexecuted along any
branch—such as an early return, validation bailout, missing switch case, or
component teardown—permanently stalls the caller, causing unfulfilled Promises,
navigation hangs, background task watchdog timeouts, hung UI, or leaked pending
state.

## Examples

Bad:

```cpp
// 1. Objective-C completion block dropped on early return:
- (void)showAlertWithTitle:(NSString*)title completion:(void (^)(BOOL))completion {
  if (_isShowingAlert) {
    return;  // Bad: leaves caller waiting for completion hanging.
  }
  [self presentAlert:title completion:completion];
}

// 2. base::OnceCallback dropped on delegate early bailout:
void Helper::RunOpenPanel(base::OnceCallback<void(NSArray*)> completion) {
  if (!web_state || web_state->IsBeingDestroyed()) {
    return;  // Bad: drops completion callback, stalling WebKit caller.
  }
  ShowPanel(std::move(completion));
}

// 3. Switch statement default branch dropping callback:
switch (result.final_result) {
  case FinalContentAnalysisResult::SUCCESS:
    std::move(download_proceed).Run(true);
    break;
  default:
    break;  // Bad: drops download_proceed without invoking fallback.
}

// 4. Coordinator or mediator teardown dropping retained callback:
- (void)disconnect {
  _consumer = nil;
  // Bad: pending _params->callback dropped uninvoked on teardown.
}
```

Good:

```cpp
// 1. Verify non-nil and invoke completion block before early return:
- (void)showAlertWithTitle:(NSString*)title completion:(void (^)(BOOL))completion {
  if (_isShowingAlert) {
    if (completion) {
      completion(NO);
    }
    return;
  }
  [self presentAlert:title completion:completion];
}

// 2. Invoke OnceCallback with fallback value on bailout:
void Helper::RunOpenPanel(base::OnceCallback<void(NSArray*)> completion) {
  if (!web_state || web_state->IsBeingDestroyed()) {
    std::move(completion).Run(nil);
    return;
  }
  ShowPanel(std::move(completion));
}

// 3. Invoke fallback callback in switch default branch:
switch (result.final_result) {
  case FinalContentAnalysisResult::SUCCESS:
    std::move(download_proceed).Run(true);
    break;
  default:
    std::move(download_proceed).Run(false);
    break;
}

// 4. Safely execute pending callback during teardown:
- (void)disconnect {
  if (_params && !_params->callback.is_null()) {
    std::move(_params->callback).Run(Result::kUnknown);
  }
  _params = std::nullopt;
  _consumer = nil;
}
```
