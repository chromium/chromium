# Chrome Remote Desktop Testing Best Practices

When creating or updating tests in `//remoting`, follow these rules to ensure
stability and avoid flakiness:

*   **Do Not Mask Failures:** Never modify a test simply to hide or bypass a
    legitimate underlying Chromium bug.
*   **Print Debugging:** If you are stuck or do not understand why a test is
    failing, do not guess. Add `LOG(ERROR) << "State: " << variable;`
    statements, rebuild, and re-run the test to empirically observe the state.
*   **Unit Tests & TDD:** New functionality should always have unit tests, and
    Test-Driven Development (TDD) should be used when possible. However, CRD is
    very complex and often calls directly into OS APIs, making unit testing
    difficult or sometimes impossible. Apply judgment.
*   **Asserts:** Avoid YODA style test asserts.
*   **TaskEnvironment:** Prefer `base::test::TaskEnvironment` over raw threads.
*   **Avoid RunLoops:** Prefer `base::test::TestFuture` and
    `base::test::RunOnceCallback` (from `base/test/gmock_callback_support.h`)
    over manual `RunLoops`.
*   **RunUntilIdle is Banned:** Do **NOT** use `RunUntilIdle()`. It is
    non-deterministic.
*   **Observing Completion:**
    *   Explicitly quit run loops using `run_loop.Quit()` or
        `run_loop.QuitClosure()` if completion is observable via a callback.
    *   Otherwise, wait for a condition via `base::test::RunUntil()`.
*   **Flakes:** If a test succeeds after a retry, investigate and fix the root
    cause. `remoting_unittests` should run in <10s; longer runs likely indicate
    a hang.

## Technical Reference

### Gmock Callback Support (`base/test/gmock_callback_support.h`)

Provides `RunOnceCallback<N>(args...)` for mocking callbacks that take
`base::OnceCallback`. Use it in `WillOnce()` to provide results asynchronously.

### Test Future (`base/test/test_future.h`)

A modern replacement for `RunLoop`. Use `future.GetCallback()` to pass to async
methods and `future.Get()` or `future.Take()` to wait for and retrieve results.

### Task Environment (`base/test/task_environment.h`)

Enables the use of `base::ThreadPool` and `RunLoop` in tests. Prefer
`SingleThreadTaskEnvironment` when multi-threading is not required.
