---
name: ios/use-delegating-constructor-syntax
description: 'When delegating initialization between C++ constructors, use member initializer list syntax (`: ClassName(...)`) instead of invoking the constructor inside the constructor body'
rubric: correctness
severity: should_fix
---

**Rule.** When a constructor delegates initialization to another constructor of
the same class, invoke the target constructor in the member initializer list
(`: ClassName(...)`). Never call the constructor by name as an expression
statement inside the constructor body, as this creates a temporary instance that
is destroyed immediately rather than initializing `*this`.

**Why.** In C++, invoking `ClassName(...)` as an expression statement inside a
constructor body constructs an unnamed temporary object that is destroyed at the
end of the statement, leaving `*this` uninitialized or default-initialized.
Using delegating constructor syntax in the member initializer list ensures that
the target constructor executes directly on `*this` before the delegating
constructor's body runs.

## Examples

Bad:

```cpp
// BAD: Calling the constructor inside the body creates an unnamed temporary
// object that is immediately destroyed; `*this` remains uninitialized.
DataSharingUIDelegateIOS::DataSharingUIDelegateIOS(
    ShareKitService* share_kit_service,
    collaboration::CollaborationService* collaboration_service,
    TabGroupService* tab_group_service) {
  DataSharingUIDelegateIOS(share_kit_service, collaboration_service);
}
```

Good:

```cpp
// GOOD: Delegate initialization to the target constructor in the member
// initializer list.
DataSharingUIDelegateIOS::DataSharingUIDelegateIOS(
    ShareKitService* share_kit_service,
    collaboration::CollaborationService* collaboration_service,
    TabGroupService* tab_group_service)
    : DataSharingUIDelegateIOS(share_kit_service, collaboration_service) {}
```
