---
name: ios/audit-callers-when-relaxing-contract-to-nullptr
description: When relaxing an accessor or method to return nullptr instead of guaranteeing non-null, audit and update all callers to handle null before dereferencing
rubric: correctness
severity: should_fix
---

**Rule.** When modifying a method or accessor to return `nullptr` or
`std::nullopt` (such as replacing an assertion or `CHECK` because an underlying
dependency may be destroyed asynchronously), audit and update all call sites.
Ensure callers explicitly verify that the returned pointer or optional is valid
before dereferencing it.

**Why.** Callers written against an API that previously guaranteed non-null
returns will dereference a null pointer and crash if the method is relaxed to
return `nullptr` without updating call sites. Verifying nullability at every
call site prevents runtime crashes when objects are destroyed asynchronously.

## Examples

Bad:

```cpp
// Accessor modified to return nullptr when underlying WebState is destroyed:
- (autofill::ChildFrameRegistrar*)frameRegistrar {
  if (!_webState) {
    return nullptr;
  }
  return autofill::ChildFrameRegistrar::GetOrCreateForWebState(_webState.get());
}

// Caller assumes registrar is always non-null:
autofill::ChildFrameRegistrar* registrar = [self frameRegistrar];
std::optional<autofill::LocalFrameToken> local =
    registrar->LookupChildFrame(remote);
```

Good:

```cpp
// Accessor modified to return nullptr when underlying WebState is destroyed:
- (autofill::ChildFrameRegistrar*)frameRegistrar {
  if (!_webState) {
    return nullptr;
  }
  return autofill::ChildFrameRegistrar::GetOrCreateForWebState(_webState.get());
}

// Caller explicitly checks for nullptr before dereferencing:
autofill::ChildFrameRegistrar* registrar = [self frameRegistrar];
if (!registrar) {
  return;
}
std::optional<autofill::LocalFrameToken> local =
    registrar->LookupChildFrame(remote);
```
