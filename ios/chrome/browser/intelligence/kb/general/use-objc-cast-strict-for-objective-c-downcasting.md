---
name: ios/use-objc-cast-strict-for-objective-c-downcasting
description: When casting Objective-C objects, use ObjCCastStrict for invariant-governed collections or trait stores, and ObjCCast for conditional checks, avoiding raw casts and redundant isKindOfClass
rubric: readability
severity: should_fix
citations:
  - path: docs/ios/style.md
    heading: Use ObjCCast<T> and ObjCCastStrict<T>
    quote: 'for Objective-C casts we have two specific casts: `base::apple::ObjCCast<T>arg` is similar to `dynamic_cast<T>`, and `ObjCCastStrict` `DCHECKs` against that class.'
  - path: base/apple/foundation_util.h
    heading: ObjCCast<>() and ObjCCastStrict<>()
    quote: ObjCCast<>() and ObjCCastStrict<>() cast a basic id to a more specific type, ObjCCast<>() returns nil and ObjCCastStrict<>() will CHECK.
---

**Rule.** When downcasting or converting Objective-C object pointers in
Objective-C++ code, use `base::apple::ObjCCastStrict<T>` or
`base::apple::ObjCCast<T>` instead of unchecked `static_cast<T*>`, C-style casts
(such as `(Type*)object`), or manual `isKindOfClass:` checks followed by raw
casts:

- Use `base::apple::ObjCCastStrict<T>(object)` when program logic or invariants
  guarantee the object is of type `T` (or nil), such as values retrieved from
  internal collections, dictionaries, or trait stores via `objectForTrait:`,
  `objectForKey:`, or `objectForKeyedSubscript:`, or within an already verified
  type guard.
- Use `base::apple::ObjCCast<T>(object)` for conditional downcasting based on
  dynamic type (checking the result for `nil`), eliminating redundant
  `[object isKindOfClass:[T class]]` guards. Reserve `ObjCCast<T>` for
  unvalidated external, polymorphic, or untrusted inputs (such as IPC data,
  JavaScript execution results, parsed plists, or deserialized payloads) where
  unexpected types are expected and safely handled by callers degrading to
  `nil`.
- Never use `ObjCCastStrict<T>` on untrusted external inputs like IPC data or
  JavaScript execution results where mismatch should not terminate the process.

**Why.** In Objective-C++, C-style casts and `static_cast` bypass Objective-C
runtime type checking, risking undefined behavior or unrecognized selector
crashes. `base::apple::ObjCCastStrict<T>` enforces expected class invariants at
runtime via `CHECK`, failing fast on programmer errors, invalid state, or API
misuse when reading from internal collections and trait stores while cleanly
tolerating valid `nil` values. Conversely, using `base::apple::ObjCCast<T>` for
internal invariants silently returns `nil` on type mismatches and swallows bugs.
However, `ObjCCast<T>` is essential for dynamic or untrusted external data (such
as IPC payloads or JS results), where it safely verifies the runtime type in a
single expression and returns `nil` on mismatch without terminating the process
or requiring verbose manual `isKindOfClass:` boilerplate.

## Examples

Bad:

```cpp
// C-style cast or static_cast bypassing runtime verification:
auto* label = static_cast<UILabel*>(subview);
TableViewCellContentView* cell =
    (TableViewCellContentView*)[config makeContentView];

// Silent ObjCCast swallowing bugs on internal trait store or collection:
NewTabPageColorPalette* palette =
    base::apple::ObjCCast<NewTabPageColorPalette>(
        [self.mutableTraits objectForTrait:[NewTabPageTrait class]]);

// Redundant isKindOfClass before downcasting instead of conditional ObjCCast:
if ([item isKindOfClass:[CustomItem class]]) {
  CustomItem* customItem = (CustomItem*)item;
  [customItem performAction];
}

// ObjCCastStrict crashes process via CHECK on untrusted JS / IPC input:
std::move(handler).Run(base::apple::ObjCCastStrict<NSString>(result), error);
```

Good:

```cpp
// Strict downcast asserting expected type for internal objects and traits:
UILabel* label = base::apple::ObjCCastStrict<UILabel>(subview);
NewTabPageColorPalette* palette =
    base::apple::ObjCCastStrict<NewTabPageColorPalette>(
        [self.mutableTraits objectForTrait:[NewTabPageTrait class]]);

// Conditional downcast safely combined in a single ObjCCast expression:
if (CustomItem* customItem = base::apple::ObjCCast<CustomItem>(item)) {
  [customItem performAction];
}

// Safe dynamic cast returning nil on mismatch for external or untrusted inputs:
std::move(handler).Run(base::apple::ObjCCast<NSString>(result), error);
```
