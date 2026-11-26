---
name: ios/no-auto-type-deduction-for-basic-objective-c-alloc-init
description: Declare the explicit class pointer type rather than auto when initializing Objective-C objects with alloc/init
rubric: readability
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: Type Deduction (`auto`), `id`, and Literals
    quote: Never use `auto` for Objective-C `alloc`/`init` expressions or to hide return types that aren't visible in the statement
---

**Rule.** Do not use `auto` when initializing Objective-C objects with
`[[Class alloc] init]`. Declare the explicit class pointer type instead:
`ClassName* var = [[ClassName alloc] init]`.

**Why.** Writing the explicit class pointer type keeps Objective-C object
declarations scannable and consistent across `//ios` code. See
`ios/best_practices.md#type-deduction-auto-id-and-literals`.

## Examples

Bad:

```cpp
auto item = [[MyCustomItem alloc] init];
```

Good:

```cpp
MyCustomItem* item = [[MyCustomItem alloc] init];
```
