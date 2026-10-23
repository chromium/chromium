---
name: ios/avoid-naming-local-helpers-identically-to-imported-functions
description: When defining file-local helper functions in implementation files, avoid reusing names of imported header functions; give helpers distinct names or convert directly at call sites
rubric: readability
severity: should_fix
---

**Rule.** Do not define a file-local or anonymous-namespace helper function with
the exact same name as an imported function or constant from an included header.
When adapting, converting, or wrapping an imported API (such as converting units
or extracting fields), give the helper a distinct, descriptive name reflecting
the transformation (e.g., indicating the unit, type, or specific purpose) or
perform the conversion directly at the call sites.

**Why.** Defining a file-local helper with the same name as an imported symbol
causes name shadowing, lookup ambiguity, and potential invalid overload errors
(such as functions differing solely by return type). In addition, calling the
function unqualified inside the helper body can trigger unintentional infinite
recursion or compilation failure.

## Examples

Bad:

```cpp
// In an anonymous namespace:
// 'InactiveThreshold()' is already imported returning base::TimeDelta.
double InactiveThreshold() {
  const base::TimeDelta threshold =
      InactiveThreshold();  // Unintentional recursion!
  return threshold.InSecondsF();
}
```

Good:

```cpp
// Option 1: Give the helper a distinct name indicating the conversion:
double InactiveThresholdInSeconds() {
  return InactiveThreshold().InSecondsF();
}

// Option 2: Invoke the conversion directly at call sites:
[trigger setTimeInterval:InactiveThreshold().InSecondsF()];
```
