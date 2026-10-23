---
name: ios/chain-callbacks-with-then
description: When sequencing callbacks, chain completion steps using base::OnceCallback::Then() instead of member closures, trampoline methods, or ScopedClosureRunner, reserving ScopedClosureRunner for cancellation-safe cleanup
rubric: readability
severity: should_fix
citations:
  - path: docs/callback.md
    heading: Chaining callbacks
    quote: When you have 2 callbacks that you wish to run in sequence, they can be joined together into a single callback through the use of Then().
---

**Rule.** When sequencing callbacks, running completion closures, or adapting
arguments, chain them using base::OnceCallback::Then() (or
base::RepeatingCallback::Then()) instead of storing follow-up closures in member
variables, writing boilerplate trampoline methods or forwarding closures,
manually invoking Run(), or relying on a local base::ScopedClosureRunner at
scope exit. Conversely, do not use .Then() for cleanup that must execute upon
cancellation or callback destruction; bind a base::ScopedClosureRunner to the
callback instead so RAII guarantees execution on completion and abort, and
document the cleanup invariant. In cross-sequence operations, wrap chained
callbacks with base::BindPostTask.

**Why.** Chaining with .Then() declaratively joins asynchronous steps in
execution order and keeps control flow local, eliminating stateful member
variables, boilerplate trampolines, and fragile scope-exit ScopedClosureRunners.
Conversely, .Then() only runs if the initial callback executes; if an operation
is cancelled or destroyed uninvoked, chained callbacks never run, whereas a
bound base::ScopedClosureRunner executes on both completion and abort via RAII.
Documenting this cleanup invariant prevents refactorings from inadvertently
skipping abort handling.

## Examples

Bad:

```cpp
// Bad: Storing follow-up closures in member variables and writing trampoline methods.
_barrierClosure = base::BarrierClosure(
    count, base::BindOnce(^{ [weakSelf allImportsDidFinish]; }));
_importer->Continue(ids, base::BindOnce(^(Results res) {
  [weakSelf importDidFinish:res];
}));
// ...
- (void)importDidFinish:(Results)res {
  [_delegate importDidFinish:res];
  _barrierClosure.Run();
}

// Bad: Relying on ScopedClosureRunner at scope exit to sequence post-callback actions.
base::ScopedClosureRunner runner(
    base::BindOnce(&CloseWebStateIfEmptyPage, weak_web_state));
std::move(callback).Run(PolicyDecision::Cancel());

// Bad: Binding completion closure as parameter or calling Run() manually.
GetData(base::BindOnce([](base::OnceClosure done, Data data) {
  ProcessData(std::move(data));
  std::move(done).Run();
}, std::move(done)));

// Bad: Using .Then() for cleanup that must run even if aborted.
creditCardAccessManager->FetchCreditCard(
    &card, base::BindOnce(&OnCreditCardFetched).Then(std::move(clearTask)));
```

Good:

```cpp
// Good: Chain sequential follow-up closures directly with .Then().
base::RepeatingClosure barrier = base::BarrierClosure(
    count, base::BindOnce(^{ [weakDelegate allImportsDidFinish]; }));
_importer->Continue(ids, base::BindOnce(^(Results res) {
  [weakDelegate importDidFinish:res];
}).Then(barrier));

// Good: Chain sequential steps with .Then() rather than ScopedClosureRunner.
std::move(callback)
    .Then(base::BindOnce(&CloseWebStateIfEmptyPage, weak_web_state))
    .Run(PolicyDecision::Cancel());

// Good: Chain completion closures directly with .Then().
GetData(base::BindOnce(&ProcessData).Then(std::move(done)));

// Good: Bind ScopedClosureRunner so cleanup runs on both completion and abort.
// Capture runner to guarantee ClearCacheOrigin runs even if cancelled/destroyed.
creditCardAccessManager->FetchCreditCard(
    &card, base::BindOnce(
               [](base::ScopedClosureRunner runner, const CreditCard& card) {},
               base::ScopedClosureRunner(base::BindOnce(&ClearCacheOrigin, weak))));
```
