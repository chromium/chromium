---
name: ios/retain-preference-constants-until-callers-migrated
rubric: change_scope
severity: nit
---

**Rule.** When removing or renaming a shared constant, preference name, or API
that is still referenced by code outside the current CL's build (such as
downstream embedders or two-sided cross-repository migrations), retain the old
symbol alongside a `// TODO(crbug.com/123456): Remove once <callers> migrate.`
comment until all external callers have migrated, then remove the old symbol in
a follow-up CL.

**Why.** Deleting or renaming a symbol in one step while downstream targets or
external repositories still reference it breaks two-sided rolls and downstream
builds. Keeping a deprecated alias or constant with a tracking `TODO` allows
callers to migrate cleanly before the old declaration is removed.

## Examples

Bad:

```cpp
// Deleting a shared constant while downstream callers still reference it:
namespace prefs {
inline constexpr char kNewFooPref[] = "ios.foo.new";
}  // namespace prefs
```

Good:

```cpp
// Retain the old constant with a tracking `TODO` until all callers migrate:
namespace prefs {
inline constexpr char kNewFooPref[] = "ios.foo.new";
// TODO(crbug.com/123456): Remove once downstream callers migrate to
// `kNewFooPref`.
inline constexpr char kOldFooPref[] = "ios.foo.old";
}  // namespace prefs
```
