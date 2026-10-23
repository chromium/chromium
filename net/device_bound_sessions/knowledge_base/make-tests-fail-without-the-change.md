---
name: net/device_bound_sessions/make-tests-fail-without-the-change
rubric: testing
severity: should_fix
---

**Rule.** For every new or changed branch in DBSC code, find the test that runs
it and ask whether it would fail on the code before the CL. Flag: (1) plumbing
tests whose expected values could come from defaults, constants or the request
URL; use a same-site but different-origin context (such as
`https://sub.example.com`) for isolation info, site for cookies and initiator,
and a non-default priority, asserted where it is forwarded; (2) tests that
silently take another path, such as SSO tests whose `provider_params` is empty
(they run standard registration) or tests that use
`ScopedTestRegistrationFetcher` and bypass the real request; (3) negative tests
that are invalid in several ways at once, instead of varying only the input
under test; (4) branches nothing exercises, including multi-session operations
where sessions get different outcomes. Ask for the test, or for the follow-up
that adds it.

**Why.** A test that still passes with the change reverted doesn't protect the
change. Expected values that match defaults, constants or the request URL hide
plumbing bugs, a test that silently takes another code path checks nothing, and
a negative test that is invalid in several ways keeps passing even if the check
under test is removed. Many DBSC branches run only for specific server responses
or flags, so a branch without a test can regress unnoticed.

## Examples

Bad:

```cpp
// Passes even if the code ignores the triggering request: the expected values
// equal the defaults.
EXPECT_EQ(fetcher_params.isolation_info, IsolationInfo());
EXPECT_EQ(restore_priority, BackgroundTaskPriority::kUserVisible);
```

Good:

```cpp
// A distinct initiator and a non-default priority only match if the code
// actually forwards them.
const url::Origin initiator =
    url::Origin::Create(GURL("https://sub.example.com"));
...
EXPECT_EQ(fetcher_params.initiator, initiator);
EXPECT_EQ(restore_priority, BackgroundTaskPriority::kUserBlocking);
```
