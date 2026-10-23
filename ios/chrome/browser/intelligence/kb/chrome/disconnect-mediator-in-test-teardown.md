---
name: ios/disconnect-mediator-in-test-teardown
description: When tearing down test fixtures instantiating iOS Chrome mediators, invoke [mediator disconnect] in TearDown before nil'ing references to clear observers
rubric: safety
severity: should_fix
---

**Rule.** In unit test fixtures that instantiate an iOS Chrome mediator,
explicitly invoke `[mediator disconnect]` during `TearDown` (or in the fixture
destructor) before resetting the mediator reference to `nil`. Do not rely solely
on ARC deallocation or pointer resetting to clean up mediator resources.

**Why.** Mediators observe C++ model objects such as `WebState` and register
observers. Resetting the pointer to `nil` without calling `-disconnect` can
leave active observers attached or fail to release model references cleanly,
leading to dangling observer pointers, use-after-free crashes, or leaks across
test runs.

## Examples

Bad:

```cpp
void TearDown() override {
  mediator_ = nil;
  PlatformTest::TearDown();
}
```

Good:

```cpp
void TearDown() override {
  [mediator_ disconnect];
  mediator_ = nil;
  PlatformTest::TearDown();
}
```
