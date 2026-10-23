---
name: ios/extract-explanatory-boolean-variables-for-conditions
description: When conditionals or ternary expressions contain nested sub-expressions, container searches, verbose queries, or non-obvious checks, extract them into descriptively named intermediate boolean variables
rubric: readability
severity: should_fix
citations:
  - path: docs/documentation_guidelines.md
    heading: Implementation documentation
    quote: Code should be as self documenting as possible, but sometimes that isn't enough.
---

**Rule.** Extract complex, verbose, or non-obvious conditional logic from
compound `if` statements, ternary expressions (`? :`), and boolean assignments
into descriptively named local boolean variables (`const bool` / `BOOL`). This
applies to:

- Nested sub-expressions or bypass clauses (such as nested disjunctions or
  feature flags combined with UI state).
- Container searches and range queries (such as `std::find`,
  `std::ranges::find`, or `.contains()`), especially when pairing allowlist
  checks with inverted denylist checks.
- Single domain queries whose contextual meaning is not immediately obvious
  (e.g., `localOptionValid` or `hasAvailableCapacity`), where a named variable
  serves as inline documentation.

Ensure that simple, self-explanatory conditions remain inline, and take care not
to precompute variables in a way that unintentionally bypasses short-circuit
evaluation of expensive operations.

**Why.** Inlining complex expressions with nested negations, disjunctions, or
verbose container lookups directly into multi-clause conditionals and ternary
expressions increases cognitive load and obscures branching intent. Decomposing
these checks into descriptive intermediate boolean constants documents their
semantic purpose, simplifies the final decision logic, and prevents subtle
precedence or logical errors without sacrificing readability.

## Examples

Bad:

```cpp
// Bad: Convoluted container lookups and allowlist/denylist checks inlined.
can_select = base::Contains(allowed, model) && !base::Contains(disabled, model) &&
             model.size_bytes < kMaxModelSizeBytes;

// Bad: Nested feature bypass and non-obvious identity query in ternary.
if (_historyReady && (IsPasswordRemovalEnabled() || _passwordsReady)) {
  return _identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin)
             ? kSignedInSubtitle : kSignedOutSubtitle;
}
```

Good:

```cpp
// Good: Decomposed into descriptive intermediate boolean variables.
const bool model_allowed =
    base::Contains(allowed, model) && !base::Contains(disabled, model);
const bool model_fits = model.size_bytes < kMaxModelSizeBytes;
can_select = model_allowed && model_fits;

// Good: Extracted explanatory variables name each condition's meaning.
const BOOL passwordsReady = _passwordsReady || IsPasswordRemovalEnabled();
const BOOL isSignedIn =
    _identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
if (_historyReady && passwordsReady) {
  return isSignedIn ? kSignedInSubtitle : kSignedOutSubtitle;
}
```
