---
name: ios/avoid-static-cast-between-optional-types
description: When converting between distinct std::optional template instantiations, unwrap and construct the target type explicitly instead of using static_cast on the optional
rubric: readability
severity: nit
---

**Rule.** Do not use `static_cast` to convert between distinct template
instantiations of `std::optional` (or `base::expected`). Instead, unwrap the
contained value and explicitly construct the target type (e.g.,
`opt ? std::make_optional(TargetType(*opt)) : std::nullopt` or
`opt.transform([](const auto& val) { return TargetType(val); })`).

**Why.** Using `static_cast` on class templates like `std::optional<T>` to
invoke `std::optional<U>`'s converting constructor obscures whether the cast is
converting the wrapper or the inner value. Unwrapping the source optional (or
using `transform()`) and explicitly constructing the inner type makes presence
handling and the inner type conversion clear at the call site.

## Examples

Bad:

```cpp
// Bad: Using static_cast to convert between distinct std::optional instantiations.
std::optional<autofill::LocalFrameToken> DeserializeFrameIdAsLocalFrameToken(
    const std::string& serialized_id) {
  return static_cast<std::optional<autofill::LocalFrameToken>>(
      autofill::DeserializeJavaScriptFrameId(serialized_id));
}
```

Good:

```cpp
// Good: Unwrap the optional and explicitly construct the target type.
std::optional<autofill::LocalFrameToken> DeserializeFrameIdAsLocalFrameToken(
    const std::string& serialized_id) {
  std::optional<base::UnguessableToken> token =
      autofill::DeserializeJavaScriptFrameId(serialized_id);
  return token ? std::make_optional(autofill::LocalFrameToken(*token))
               : std::nullopt;
}
```
