---
name: net/device_bound_sessions/complete-async-operations-exactly-once
rubric: correctness
severity: nit
---

**Rule.** In `net/device_bound_sessions/`, every asynchronous operation that
takes a completion callback (`URLFetcher`, `RegistrationFetcher`, multi-step
signing, and `SessionService` APIs such as `PrewarmSessionsForUrl()`) must
complete exactly once on every path: success, network error, watchdog timeout,
early returns, session deletion, and `SessionStore` load success or failure.
Prefer funneling all paths into one idempotent completion helper that returns
early if the callback already ran, stops `watchdog_timer_`, records the result
once, and runs the callback last; this fits best in classes that do a one-time
job and report the result through a callback. Touch no members after a statement
that may run the callback, and comment that `this` may be deleted there. Drain
queued waiters (`deferred_requests_`, `proactive_requests_`, requests waiting
for the store to load) exactly once on every exit, and join parallel steps with
`base::BarrierClosure` plus a guard against a second completion.

**Why.** A completion callback that never runs hangs every request deferred on
the session, and one that runs twice, or a member access after it runs, is a
use-after-free because the callback may delete `this`. Watchdog timeouts, early
returns and parallel steps each add a path that can skip or repeat the
completion, and a single idempotent sink makes every path finish the same way.

## Examples

Bad:

```cpp
void URLFetcher::OnTimeout() {
  std::move(callback_).Run();
  // The callback may have deleted `this`, and a late response can still run
  // its own completion path and report a second result.
  net_error_ = net::ERR_TIMED_OUT;
}
```

Good:

```cpp
void URLFetcher::OnTimeout() {
  Complete(net::ERR_TIMED_OUT);
}

void URLFetcher::Complete(int net_error) {
  if (!callback_) {
    return;  // Already completed, for example by the watchdog.
  }
  watchdog_timer_.Stop();
  net_error_ = net_error;
  // Move `callback_` out immediately to avoid re-entrancy issues.
  base::ScopedClosureRunner runner(std::move(callback_));
  if (net_error != OK && request_->is_pending()) {
    request_->CancelWithError(net_error);
  }
  // `this` may be deleted when `runner` runs at scope exit.
}
```
