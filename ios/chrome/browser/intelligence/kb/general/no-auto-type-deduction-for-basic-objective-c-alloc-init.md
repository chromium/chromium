---
name: ios/no-auto-type-deduction-for-basic-objective-c-alloc-init
description: When initializing Objective-C objects with alloc init, declare the explicit class pointer type rather than using auto.
rubric: readability
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: Type Deduction (`auto`), `id`, and Literals
    quote: Never use `auto` for Objective-C `alloc`/`init` expressions or to hide return types that aren't visible in the statement.
---

**Rule.** Do not use `auto` when initializing Objective-C objects with
`[[Class alloc] init]`. Declare the explicit type
`ClassName* var = [[ClassName alloc] init]`.

**Why.** In Objective-C++, `[[Foo alloc] init]` returns `id` (or
`instancetype`), causing `auto` to deduce `id` rather than `Foo*` and losing
static type safety. See `ios/best_practices.md#type-deduction-and-literals`.

## Examples

Bad:

```cpp
auto item = [[MyCustomItem alloc] init];
```

Good:

```cpp
MyCustomItem* item = [[MyCustomItem alloc] init];
```
