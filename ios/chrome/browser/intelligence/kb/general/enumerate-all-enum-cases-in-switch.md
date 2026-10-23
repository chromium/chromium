---
name: ios/enumerate-all-enum-cases-in-switch
description: When switching over enums, omit default to preserve exhaustiveness warnings, handle all enumerators explicitly, use NOTREACHED() or NOTIMPLEMENTED(), and document intentional no-ops
rubric: readability
severity: should_fix
citations:
  - path: docs/ios/style.md
    heading: NOTIMPLEMENTED and NOTREACHED logging macros
    quote: "`NOTIMPLEMENTED`: This isn't implemented because we don't use it yet. If it's called, then we need to figure out what it should do."
  - path: styleguide/c++/checks.md
    heading: Examples
    quote: This switch handles all cases, but enums can technically hold any integer value (even if all enum members are enumerated), so the compiler must try to handle other cases too. We can avoid dealing with values outside enums by using NOTREACHED() while also making sure we hear about it.
---

**Rule.** When writing `switch` statements over enums or enum classes, omit the
`default:` label and handle all enumerators explicitly to preserve compiler
exhaustiveness diagnostics (`-Wswitch`). If certain enumerators are unsupported,
invalid, or unreachable, handle them under explicit `case` labels calling
`NOTREACHED();` rather than silently ignoring them with `break;`. In
value-returning switches where all enumerators return, place `NOTREACHED();`
immediately after the switch block to handle potential out-of-range integer
values without suppressing `-Wswitch`. Stub in-progress or unimplemented feature
cases with `NOTIMPLEMENTED();`. If cases intentionally require no action or
return `nil` or `nullptr`, document why the omission is safe with an inline
comment and use an explicit `break;`. When consecutive cases fall through to
share non-obvious logic, add an inline comment explaining the intentional
fallthrough.

**Why.** A `default:` label silences compiler exhaustiveness checks
(`-Wswitch`), allowing unhandled enum values to go undetected at compile time
when new enumerators are added. Silently ignoring unsupported or unimplemented
enum values with an empty `break;` masks invariant violations and incomplete
features. Handling unreachable cases with `NOTREACHED();` fails fast, while
`NOTIMPLEMENTED();` logs incomplete features without crashing. Placing
`NOTREACHED();` after an exhaustive switch block ensures runtime protection
against invalid out-of-range integer values while retaining `-Wswitch`.
Documenting intentional no-ops, `nil` returns, and fallthrough prevents
intentional omissions from being mistaken for bugs during refactoring.

## Examples

Bad:

```cpp
// Bad: 'default:' inside switch silences -Wswitch when new enum values are added.
switch (doc_type) {
  case DocType::kPassport:
    ShowPassportUI();
    break;
  default:
    break;
}

// Bad: Placing 'default: NOTREACHED()' inside switch suppresses exhaustiveness warnings.
UIImage* IconForModel(ComposeboxModelOption option) {
  switch (option) {
    case ComposeboxModelOption::kRegular:
      return SymbolWithPointSize(SymbolBolt, kSymbolPointSize);
    default:
      NOTREACHED();
  }
}

// Bad: Silent break for unimplemented or unreachable cases; unexplained nil return.
switch (item_type) {
  case ItemType::kDownloadWarn:
    break;  // Unimplemented feature fails silently instead of NOTIMPLEMENTED().
  case ItemType::kReaderMode:
    return nil;  // Unexplained nil return.
}
```

Good:

```cpp
// Good: Enumerate all cases without default; explicit NOTREACHED() for impossible cases.
switch (doc_type) {
  case DocType::kPassport:
    ShowPassportUI();
    break;
  case DocType::kVehicle:
  case DocType::kOrder:
    NOTREACHED();  // Invariant: only identity doc types reach this mediator.
}

// Good: Exhaustive switch with NOTREACHED() placed after the block for out-of-range protection.
UIImage* IconForModel(ComposeboxModelOption option) {
  switch (option) {
    case ComposeboxModelOption::kRegular:
      return SymbolWithPointSize(SymbolBolt, kSymbolPointSize);
    case ComposeboxModelOption::kNone:
      return nil;
  }
  NOTREACHED();
}

// Good: NOTIMPLEMENTED() for stubs, and documented nil return or intentional no-op.
switch (item_type) {
  case ItemType::kDownloadWarn:
    NOTIMPLEMENTED();
    break;
  case ItemType::kReaderMode:
    // Reader mode only uses the entry point and does not require a modulator.
    return nil;
}
```
