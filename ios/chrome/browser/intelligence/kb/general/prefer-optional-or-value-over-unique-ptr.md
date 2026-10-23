---
name: ios/prefer-optional-or-value-over-unique-ptr
description: Prefer stack allocation, direct value members, or std::optional over std::unique_ptr for local objects, test fixtures, and lazily initialized members without polymorphism or ownership transfer
rubric: safety
severity: should_fix
citations:
  - path: docs/transcripts/wuwt-e01-pointers.md
    heading: 06:10 DANA
    quote: std::optional and absl::optional hold an object inside of it by value instead of by pointer. This means that the object is held in that space allocated for the optional. ... And it doesn't require a heap allocation, the way unique_ptr does because it's storing the memory in place. So if you have an optional on the stack, the object inside is also right there on the stack.
---

**Rule.** Prefer value semantics—allocating objects directly on the stack for
local variables and transient traversal contexts, storing direct value members
in classes and test fixtures, or using `std::optional<T>` (initialized via
`.emplace()`) for lazy, deferred, or conditional construction—rather than
wrapping in `std::unique_ptr<T>` or allocating via `std::make_unique<T>`.
Specifically:

- For scope-bound locals and transient context objects (such as tree traversal
  parameters or state bundles), allocate directly on the stack or wrap in
  `std::optional` if conditionally constructed.
- For class members and test fixtures (`testing::Test`, `PlatformTest`), declare
  owned objects, test clients, fakes, helpers, and standard containers (such as
  `std::vector` or `std::queue`) as direct value members instead of allocating
  them on the heap in constructors or `SetUp()`.
- For optional, deferred, or lazily initialized members of C++ classes (such as
  a helper whose constructor arguments are only available after construction),
  use `std::optional<T>` initialized with `.emplace(...)` and cleared via
  `.reset()` or `std::nullopt`. A `base::ScopedObservation` member of a C++
  observer needs neither: declare it as a plain value member (`obs_{this}`) and
  start or stop observing with `.Observe(...)` and `.Reset()`. This bullet does
  not apply to Objective-C ivars: owned C++ objects held by an Objective-C
  object (observer bridges and `base::ScopedObservation` instances) stay
  `std::unique_ptr<T>` ivars that are `.reset()` in `-disconnect` or `-stop`.

Reserve `std::unique_ptr<T>` strictly for dynamic polymorphism, ownership
transfer across lifetime boundaries, breaking header dependency cycles with
forward-declared types, or owned C++ objects held in Objective-C ivars.

**Why.** Stack allocation and direct value members eliminate dynamic heap
allocation overhead, allocator churn, heap fragmentation, and pointer
indirection (`->` and `.get()`) while tying object lifetime directly to
enclosing block or fixture scope. Standard containers already manage dynamic
memory internally, and non-polymorphic types do not require heap indirection
(Abseil Tip 187). Using `std::unique_ptr` purely for optionality or lazy
initialization misleadingly signals ownership handoff or polymorphic lifetime
decoupling, whereas `std::optional<T>` provides inline storage with value
semantics while supporting in-place `.emplace()` initialization. In test
fixtures, direct value members eliminate boilerplate `.get()` calls and
redundant `std::make_unique` allocations in `SetUp()`. Objective-C ivars are the
exception because `-dealloc` timing is non-deterministic: a `std::unique_ptr`
ivar that is `.reset()` in `-disconnect` makes the bridge's teardown explicit
and independent of when the Objective-C object is finally released.

## Examples

Bad:

```cpp
// 1. Unnecessary heap allocation for local or transient context
void Process(Node* node) {
  auto ctx = std::make_unique<ExtractionContext>(depth + 1);
  TraverseChild(node, ctx.get());
}

// 2. Unnecessary unique_ptr for test fixture member
class FeatureTest : public PlatformTest {
  void SetUp() override {
    helper_ = std::make_unique<FakeHelper>();
  }
  std::unique_ptr<FakeHelper> helper_;
};

// 3. Lazy or optional C++ class member using unique_ptr instead of optional
std::unique_ptr<base::ScopedObservation<Source, Observer>> obs_;
void Init(Source* src) {
  obs_ = std::make_unique<base::ScopedObservation<Source, Observer>>(this);
  obs_->Observe(src);
}

// 4. std::optional for an owned C++ bridge held in an Objective-C ivar
@implementation FooMediator {
  std::optional<FooObserverBridge> _observerBridge;
}
@end
```

Good:

```cpp
// 1. Direct stack allocation for local or transient context
void Process(Node* node) {
  ExtractionContext ctx(depth + 1);
  TraverseChild(node, &ctx);
}

// 2. Direct value member in test fixture
class FeatureTest : public PlatformTest {
  FakeHelper helper_;
};

// 3. ScopedObservation as a plain C++ value member; Observe()/Reset() cover
// the deferred start and stop, so neither unique_ptr nor optional is needed.
base::ScopedObservation<Source, Observer> obs_{this};
void Init(Source* src) {
  obs_.Observe(src);
}

// 4. Owned C++ objects in Objective-C ivars stay std::unique_ptr and are reset
// in -disconnect
@implementation FooMediator {
  std::unique_ptr<FooObserverBridge> _observerBridge;
  std::unique_ptr<base::ScopedObservation<FooService, FooServiceObserver>>
      _scopedObservation;
}

- (void)disconnect {
  _scopedObservation.reset();
  _observerBridge.reset();
}
@end
```
