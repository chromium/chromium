---
name: ios/prefer-value-or-for-default-fallback
description: When extracting or moving values from an std::optional with a default fallback, prefer opt.value_or(...) or std::move(opt).value_or(...) over ternary checks or if-blocks
rubric: readability
severity: nit
---

**Rule.** Prefer `std::optional::value_or(...)` over manual ternary checks (such
as `opt.has_value() ? opt.value() : fallback` or `opt ? *opt : fallback`) or
intermediate conditional checks (`if (opt.has_value())`) when extracting or
assigning a value with a default fallback. When transferring ownership from an
expiring optional, use `std::move(opt).value_or(fallback)` rather than checking
`.has_value()` in a ternary operator. When accessing a member of an optional
struct with a fallback, chain `.value_or(kDefault).member` with a default struct
instance. For expensive, allocating, or side-effecting expressions, evaluate the
fallback lazily via explicit conditional branching instead of passing eager
arguments to `.value_or()`

**Why.** Using `opt.value_or(...)` provides idiomatic and concise unwrapping,
eliminating redundant variable names, ternary duplication, and intermediate
conditional branching. Invoking `std::move(opt).value_or(...)` leverages rvalue
ref-qualified accessors to transfer ownership cleanly without manual ternary
branching. Chaining `.value_or(...)` clarifies intent when defaulting nullable
pointers, scalar values, or struct members. Because arguments to
`.value_or(...)` are evaluated eagerly, explicit conditional checks should be
preserved when the fallback involves non-trivial computation or side effects.

## Examples

Bad:

```cpp
// Verbose ternary expression checking has_value() and calling value():
AssistantContainerDetent detent =
    _activeDetent.has_value() ? _activeDetent.value() : self.detents.front();

// Manual ternary check moving from an expiring optional:
auto info = opt.has_value() ? std::move(opt.value()) : DefaultSettings();

// Intermediate conditional check to assign with fallback:
if (maybe_server.has_value()) {
  remote_debugging_server_ = std::move(*maybe_server);
}

// Manual ternary branching to extract a struct field with fallback:
return font_info.has_value() ? font_info.value().category
                             : IOSContentSizeCategory::kUnspecified;
```

Good:

```cpp
// Concise unwrapping using value_or():
AssistantContainerDetent detent = _activeDetent.value_or(self.detents.front());

// Moving from an expiring optional with fallback:
auto info = std::move(opt).value_or(DefaultSettings());

// Direct assignment with fallback default using value_or():
remote_debugging_server_ = maybe_server.value_or(nullptr);

// Extracting a struct field cleanly via a default struct and value_or():
constexpr FontDescription kDefaultFont = {
    .category = IOSContentSizeCategory::kUnspecified,
};
return font_info.value_or(kDefaultFont).category;
```
