---
name: ios/remove-unused-file-scope-constants
description: When refactoring, inlining, or removing feature code, delete unreferenced constants, accessibility identifiers, metrics, and keys from headers and file scope, avoiding speculative declarations without consumers.
rubric: readability
severity: should_fix
citations:
  - path: agents/skills/feature-flag-removal/SKILL.md
    heading: 08. **Remove dead code**
    quote: In all removed lines, check for functions, classes, and constants that are no longer referenced and delete their declarations and definitions. You may need to delete entire source files.
  - path: styleguide/c++/defining_compile_time_const.md
    heading: Guidelines
    quote: Define constants in the narrowest scope possible, just like with other variables. This minimizes name conflicts, symbol duplication, and dead code. Don’t put things at namespace scope or in headers unless you actually need to refer to them from multiple places.
  - path: agents/projects/code-health/histogram-cleanup/references/automated_review.md
    heading: Review Prompt
    quote: Verify that there are no leftover references to the removed histogram in code or comments. Use rg or cs to verify.
---

**Rule.** Do not declare speculative constants, identifiers, or helper symbols
lacking active consumers, and do not retain unreferenced constants, static
variables, or dead definitions in anonymous namespaces or at file scope when
values are inlined, refactored, or superseded. Remove obsolete constant
definitions and any empty anonymous namespace blocks. Additionally, when
removing feature code, UI components, retired feature flags, or dead code paths,
audit companion headers and implementation files to cleanly delete associated
constants (such as accessibility identifiers, layout metrics, dictionary keys,
pref keys, histogram names, or string keys) that become unreferenced. Cleanly
remove both source file definitions and their corresponding `extern` or
`FOUNDATION_EXPORT` declarations and explanatory doc comments from shared
headers rather than leaving orphaned symbols behind.

**Why.** Constants declared in shared headers have external linkage, preventing
compilers from detecting or warning when all call sites are deleted. Leaving
unreferenced constants, orphaned `extern` declarations, or unused file-scope
symbols in anonymous namespaces bloats symbol tables, clutters compilation units
and public interfaces, and increases cognitive overhead. Furthermore, retaining
obsolete accessibility identifiers, layout metrics, or configuration keys
misleads maintainers into believing deprecated UI elements, feature flags, or
active data dependencies are still supported.

## Examples

Bad:

```cpp
// Variant 1: Unused file-scope constant in anonymous namespace after inlining:
namespace {
const CGFloat kDotWidthPercentage = 0.5;
NSString* const kDriveActionIdentifier = @"chromeAction.chooseFromDrive";
}  // namespace

// Variant 2: Orphaned extern header declaration after removing metric definition:
// Name of the histogram logging docking promo actions.
extern const char kIOSDockingPromoActionHistogram[];

// Variant 3: Orphaned UI identifier and pref key after deleting component:
// feature_constants.h / .mm:
extern NSString* const kNTPLensQuickActionIdentifier;
NSString* const kNTPLensQuickActionIdentifier = @"LensQuickActionIdentifier";
extern const char kObsoleteFeaturePrefKey[];
```

Good:

```cpp
// Variant 1: Remove unreferenced constants and empty anonymous namespaces:
namespace {
const CGFloat kDotWidthPercentage = 0.5;
}  // namespace

// Variant 2: Remove orphaned extern declaration and comment from header:
// (kIOSDockingPromoActionHistogram declaration and doc comment deleted)

// Variant 3: Cleanly delete declarations and definitions when component is removed:
// (kNTPLensQuickActionIdentifier and kObsoleteFeaturePrefKey removed from
// feature_constants.h and .mm)
```
