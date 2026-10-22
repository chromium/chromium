---
name: ios/no-auto-type-deduction-for-basic-objective-c-alloc-init
rubric: readability
severity: should_fix
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
