---
name: ios/test-queue-filtering-rejection-and-retention
description: When introducing or modifying queue or collection admission, filtering, or eviction logic, write unit tests covering both incompatible-item rejection and multi-item retention
rubric: testing
severity: should_fix
---

**Rule.** When introducing or modifying queue or collection admission,
filtering, or eviction logic, write unit tests for both the rejection path and
the multi-item retention path. Verify that conflicting or incompatible items are
dropped, and verify that multiple compatible items remain queued together and
are processed without unintended eviction. Do not test single-item admission or
rejection in isolation without asserting behavior when multiple compatible items
are enqueued.

**Why.** Testing only single-item admission or isolated rejection paths allows
subtle collection-clearing bugs, state collisions, or inadvertent evictions in
filtering logic to pass unnoticed. Verifying that multiple compatible items
remain enqueued alongside rejection tests ensures that admission checks
correctly preserve coexisting items and maintain queue integrity.

## Examples

Bad:

```cpp
// Only tests isolated rejection of a conflicting task.
TEST_F(FooTaskQueueTest, DropTaskOnConflict) {
  [queue_ addTaskRequest:task1];
  [queue_ addTaskRequest:conflicting_task];
  EXPECT_FALSE(conflicting_task_executed);
}
// Missing: No test asserting that multiple compatible tasks coexist in the queue.
```

Good:

```cpp
// Verifies that conflicting tasks are dropped.
TEST_F(FooTaskQueueTest, DropTaskOnConflict) {
  [queue_ addTaskRequest:task1];
  [queue_ addTaskRequest:conflicting_task];
  EXPECT_FALSE(conflicting_task_executed);
}

// Verifies that multiple compatible tasks remain enqueued and execute.
TEST_F(FooTaskQueueTest, RetainMultipleCompatibleTasks) {
  [queue_ addTaskRequest:task1];
  [queue_ addTaskRequest:task2];
  [queue_ executeTasks];
  EXPECT_TRUE(task1_executed);
  EXPECT_TRUE(task2_executed);
}
```
