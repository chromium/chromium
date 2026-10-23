---
name: ios/scope-enums-to-handled-variants
description: When defining or extending enums, scope enumerators to actively handled states, avoid premature additions without switch handling, and omit unreachable NOTREACHED() cases
rubric: design
severity: should_fix
citations:
  - path: docs/chrome_browser_design_principles.md
    heading: General
    quote: Avoid tight coupling of unrelated features.
  - path: styleguide/c++/checks.md
    heading: Examples
    quote: This switch handles all cases, but enums can technically hold any integer value (even if all enum members are enumerated), so the compiler must try to handle other cases too.
---

**Rule.** When defining or extending a feature-local enum, scope enumerators
strictly to active, concrete states that consuming logic reaches and handles. Do
not introduce enum values prematurely before their feature logic is implemented,
and update all existing exhaustive switch statements in the same changelist when
adding an enumerator. Avoid speculative, unused, or catch-all values (such as
`kAll` or placeholder modes), and remove dead enumerators from a single-consumer
enum rather than keeping unreachable switch branches that only call
`NOTREACHED()`. Also omit variants for presentation styles or actions handled by
separate components or pipelines (such as snackbars vs. dialogs).

**Why.** Every declared enumerator expands the enum's contract and forces
downstream consumers and switch statements to handle that state. Exhaustive
switch statements without default branches rely on the compiler (`-Wswitch`) to
catch unhandled cases; introducing enumerators prematurely or without updating
existing switches breaks the build or creates unhandled states. Defining
speculative, unused, or externally handled values on a single-consumer enum also
introduces dead code and unnecessary `NOTREACHED()` branches.

## Examples

Bad:

```cpp
// Bad: Adding speculative or catch-all values without active use cases.
enum class QueryType {
  kSync,
  kAsync,
  kAll,  // Speculative; forces callers and switches to handle an unused state.
};

// Bad: Prematurely adding enumerators before feature handling is implemented.
enum class BubbleViewType {
  kDefault,
  kRich,
  kRichWithNext,  // Added before switch statements handle it.
};

// Bad: Including variants handled by a separate pipeline (e.g. snackbars).
enum class DialogType {
  kClipboardPasteWarn,
  kClipboardPasteBlock,  // Handled by snackbar, forcing `NOTREACHED()` below.
  kDownloadWarn,
};

WarningDialog GetWarningDialog(DialogType type) {
  switch (type) {
    case DialogType::kClipboardPasteWarn:
      return CreatePasteWarningDialog();
    case DialogType::kDownloadWarn:
      return CreateDownloadWarningDialog();
    case DialogType::kClipboardPasteBlock:
      // Unreachable branch in component that only handles warning dialogs.
      NOTREACHED();
  }
}
```

Good:

```cpp
// Good: Only define enumerators with concrete, active use cases.
enum class QueryType {
  kSync,
  kAsync,
};

// Good: Defer enumerators until the CL where switch handling is implemented.
enum class BubbleViewType {
  kDefault,
  kRich,
};

// Good: Scoped strictly to variants handled by the warning dialog component.
enum class DialogType {
  kClipboardPasteWarn,
  kDownloadWarn,
};

WarningDialog GetWarningDialog(DialogType type) {
  switch (type) {
    case DialogType::kClipboardPasteWarn:
      return CreatePasteWarningDialog();
    case DialogType::kDownloadWarn:
      return CreateDownloadWarningDialog();
  }
}
```
