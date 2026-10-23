---
name: ios/declare-subclass-hooks-in-subclassing-header
description: When exposing Objective-C subclass hooks or base properties, isolate them in a (Subclassing) category or +subclassing.h header instead of public headers or private extensions
rubric: readability
severity: should_fix
---

**Rule.** Isolate protected subclass hooks, initializers, and base class
properties intended for subclasses in an explicit `(Subclassing)` category
declared in a separate companion header named `foo+subclassing.h` (a
`(Protected)` category in `foo+protected.h` is the only other sanctioned form),
rather than exposing them in the public header, in a `_private.h` header,
trapping them in `.mm` private class extensions, or re-declaring properties in
subclasses. Document each hook with comments explaining purpose and override
expectations. Additionally, do not add subclass-specific helper methods to a
base class or its `(Subclassing)` category when only a single subclass needs
that logic; implement the helper directly in the subclass.

**Why.** Objective-C private class extensions in `.mm` files are inaccessible
across translation units, while exposing protected hooks in public headers
breaks encapsulation and re-declaring properties in subclasses creates duplicate
state and shadowing bugs. Categories are declared in their own `foo+bar.h`
files, so isolating the protected interface in a dedicated `+subclassing.h` (or
`+protected.h`) companion header establishes an explicit protected contract
without polluting public APIs or superclasses with subclass-specific helpers.

## Examples

Bad:

```cpp
// In base_grid_mediator.mm
@interface BaseGridMediator ()
// Inaccessible to subclasses in other translation units.
@property(nonatomic, assign) BOOL closingAllItems;
@end

// In sub_grid_mediator.mm
// Anti-pattern: re-declaring base property in subclass due to private base extension.
@property(nonatomic, assign) BOOL closingAllItems;
```

Good:

```cpp
// In base_grid_mediator+subclassing.h (a separate header; not the public
// base_grid_mediator.h and not a base_grid_mediator_private.h).
@interface BaseGridMediator (Subclassing)
// Protected state exposed specifically for subclass access and override hooks.
@property(nonatomic, assign) BOOL closingAllItems;
@end
```
