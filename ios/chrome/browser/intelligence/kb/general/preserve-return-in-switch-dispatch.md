---
name: ios/preserve-return-in-switch-dispatch
description: When dispatching terminal actions in a switch followed by downstream logic or NOTREACHED(), exit cases with return rather than break
rubric: correctness
severity: should_fix
citations:
  - path: styleguide/c++/checks.md
    heading: Examples
    quote: 'switch (my_enum) { case A: return 1; case B: return 5; case C: return 3; } NOTREACHED();'
---

**Rule.** When refactoring early-return branches into a switch statement or
handling terminal actions in a switch followed by downstream logic or
`NOTREACHED()`, explicitly `return` from handled cases instead of using `break`.
Exiting via `break` only exits the switch block and proceeds to subsequent
statements or assertions, causing unintended logic execution or crashes.

**Why.** In C, C++, and Objective-C++, `break;` only exits the innermost switch
block and continues into downstream statements. If downstream code contains
event handlers, state mutations, or unreachable assertions like `NOTREACHED()`,
falling through via `break` causes unexpected side effects or immediate
assertion failures.

## Examples

Bad:

```cpp
switch (itemIdentifier) {
  case ItemIdentifier::kPasswordsAndPasskeys:
    cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
    return;
  case ItemIdentifier::kSearchHistory:
    if (defaultEngine == SearchEngineType::kGoogle) {
      cell.accessoryView = customView;
    }
    break;
  case ItemIdentifier::kMyActivity:
    cell.accessoryView = customView;
    return;
}
NOTREACHED();
```

Good:

```cpp
switch (itemIdentifier) {
  case ItemIdentifier::kPasswordsAndPasskeys:
    cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
    return;
  case ItemIdentifier::kSearchHistory:
    if (defaultEngine != SearchEngineType::kGoogle) {
      return;
    }
    cell.accessoryView = customView;
    return;
  case ItemIdentifier::kMyActivity:
    cell.accessoryView = customView;
    return;
}
NOTREACHED();
```
