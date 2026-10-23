---
name: ios/pass-sink-parameters-by-value-and-move
description: When functions, constructors, or initializers unconditionally store, mutate, or sink an argument, pass parameters by value and std::move rather than const references
rubric: safety
severity: should_fix
---

**Rule.** When a function, constructor, Objective-C initializer, or consuming
helper unconditionally copies, mutates, or takes ownership of a non-primitive or
movable C++ object (such as a struct, std::string, or std::vector) to store in a
member variable, ivar, or downstream sink, accept the parameter by value rather
than by const reference ('const T&'), and transfer ownership using std::move().
Reserve const T& strictly for read-only parameters where the caller retains
ownership across the invocation. Additionally, when a function needs a local
mutable copy of an argument, passing by value allows callers with rvalues or
temporaries to transfer storage directly, eliminating explicit local copy
allocations. This includes values unpacked from a `std::optional`
(`std::move(*optional)`, not `optional.value()`) and parameters forwarded into a
child component's initializer; once a value is moved into the child, do not keep
a redundant copy in the parent's ivars.

**Why.** Accepting a sink or mutated parameter by const T& forces callers with
temporaries or expiring rvalues to perform unnecessary deep copies and prevents
them from moving resources. It also forces the callee to allocate an explicit
local copy if it needs to modify or retain the data. Passing by value lets
callers choose between copying lvalues and moving rvalues into the parameter
with zero-copy overhead, while enabling the callee to directly mutate or
std::move the argument into members or ivars.

## Examples

Bad:

```cpp
// Bad: Accepting const T& forces explicit local copies when mutating or sinking into ivars.
void TabHelper::MaybeShowPayments(const autofill::FormActivityParams& params) {
  autofill::FormActivityParams params_copy = params;
  params_copy.has_user_gesture = true;
  ShowPaymentsBottomSheet(params_copy, /*detach=*/true);
}

- (instancetype)initWithParams:(const autofill::FormActivityParams&)params {
  if ((self = [super init])) {
    _params = params;  // Redundant copy from const reference.
  }
  return self;
}
```

Good:

```cpp
// Good: Pass by value and std::move into mutations, sinks, or ivars.
void TabHelper::MaybeShowPayments(autofill::FormActivityParams params) {
  params.has_user_gesture = true;
  ShowPaymentsBottomSheet(std::move(params), /*detach=*/true);
}

- (instancetype)initWithParams:(autofill::FormActivityParams)params {
  if ((self = [super init])) {
    _params = std::move(params);
  }
  return self;
}
```
