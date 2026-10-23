---
name: ios/prefer-assert-for-test-preconditions
description: In tests, use fatal ASSERT_* for preconditions, async waits, and non-null guards before dependent execution, and prefer non-fatal EXPECT_* for verifying outcomes
rubric: correctness
severity: should_fix
---

**Rule.** In unit and integration tests, use fatal assertions (`ASSERT_*`, such
as `ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_EQ`, or `ASSERT_NE`) rather than
non-fatal assertions (`EXPECT_*`) to validate setup preconditions, initial
fixture state, baseline state before mutations, and non-null pointer or object
guards before dependent code executes (omitting redundant defensive guard
returns like `if (!ptr) return;`). Additionally, use fatal assertions (such as
`ASSERT_TRUE`) when checking asynchronous wait operations (such as
`base::test::ios::WaitUntilConditionOrTimeout` or `WaitFor*` helpers) whose
completion is required for subsequent operations to proceed. Conversely, prefer
non-fatal `EXPECT_*` macros (such as `EXPECT_FALSE` or `EXPECT_EQ`) for outcome
checks and terminal verifications when subsequent execution is safe, allowing
remaining checks and teardown cleanup to run.

**Why.** Fatal `ASSERT_*` aborts the test method immediately upon failure,
halting execution at the root cause, preventing undefined behavior or cascading
crashes from dereferencing null pointers or operating on uninitialized state,
and satisfying `[[nodiscard]]` on wait helpers like
`base::test::ios::WaitUntilConditionOrTimeout`. Conversely, non-fatal `EXPECT_*`
logs failures without aborting the test, allowing subsequent assertions to
execute, reporting comprehensive diagnostics, and ensuring fixture teardown and
cleanup run.

## Examples

Bad:

```cpp
// Bad: Using non-fatal checks or defensive bailout before dereference.
EXPECT_NE(date, nil);
if (!date) return;
[calendar components:kUnits fromDate:date];

// Bad: Non-fatal check on async wait or setup precondition.
EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
    kTimeout, ^{ return manager->GetMainWebFrame() != nullptr; }));
manager->GetMainWebFrame()->ExecuteJavaScript(...);

// Bad: Using fatal ASSERT_* for safe terminal outcome verifications.
ASSERT_FALSE(IOSSharingServiceFactory::GetForProfile(profile_.get()));
```

Good:

```cpp
// Good: Fatal ASSERT_* halts on null, eliminating defensive guard returns.
ASSERT_NE(date, nil);
[calendar components:kUnits fromDate:date];

// Good: Fatal ASSERT_* for async wait prevents cascading crashes on timeout.
ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
    kTimeout, ^{ return manager->GetMainWebFrame() != nullptr; }));
manager->GetMainWebFrame()->ExecuteJavaScript(...);

// Good: Non-fatal EXPECT_* allows remaining checks and teardown to run.
EXPECT_FALSE(IOSSharingServiceFactory::GetForProfile(profile_.get()));
```
