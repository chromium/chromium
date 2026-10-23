---
name: ios/avoid-redundant-semicolons
description: When writing or modifying C++, C, or Objective-C code, omit redundant semicolons after control-flow compound blocks, standalone semicolons, or duplicate double semicolons
rubric: readability
severity: should_fix
---

**Rule.** Do not include redundant semicolons that create superfluous empty
statements in C++, C, Objective-C, or Objective-C++ code. Specifically, remove:
(1) trailing semicolons after compound control-flow closing braces (such as
`if`, `for`, or `while` statements: `};`), (2) stray or isolated semicolons on
their own lines leftover from deleted or replaced statements, expressions, or
macro invocations, and (3) duplicate consecutive semicolons (such as `;;`)
following declarations, expressions, or method and function calls.

**Why.** In C++, C, and Objective-C/C++, compound control-flow statements do not
require a terminating semicolon after their closing braces, unlike class,
struct, or lambda definitions. Stray semicolons on their own lines or duplicate
semicolons (`;;`) introduce useless empty statements (null statements) that add
visual clutter, violate code hygiene, and signal incomplete edits or accidental
merge/refactoring artifacts. Furthermore, automated formatters like
`clang-format` do not eliminate redundant empty statements, and extraneous
semicolons can trigger compiler diagnostics such as `-Wextra-semi` or
`-Wextra-semi-stmt`.

## Examples

Bad:

```cpp
// Redundant semicolon after compound control-flow block:
if (completion) {
  completion();
};

// Stray semicolon on its own line:
[ChromeEarlGreyUI longPressElementOnWebView:kLinkSelector];
;

// Duplicate double semicolon after declaration or call:
int total = CalculateTotal();;
```

Good:

```cpp
// No terminating semicolon after control-flow closing brace:
if (completion) {
  completion();
}

// Stray semicolon removed:
[ChromeEarlGreyUI longPressElementOnWebView:kLinkSelector];

// Single statement-terminating semicolon:
int total = CalculateTotal();
```
