---
name: ios/prefer-enum-modes-and-switch-over-ad-hoc-ladders
description: When branching on enum values or operational modes, prefer exhaustive switch statements over if-else ladders or inequality checks, but use direct equality for single enumerators
rubric: readability
severity: should_fix
citations:
  - path: styleguide/c++/checks.md
    heading: Examples
    quote: 'This switch handles all cases, but enums can technically hold any integer value (even if all enum members are enumerated), so the compiler must try to handle other cases too. We can avoid dealing with values outside enums by using NOTREACHED() while also making sure we hear about it. switch (my_enum) { case A: return 1; case B: return 5; case C: return 3; } NOTREACHED();'
---

**Rule.** When branching across multiple variants or discrete operational modes
of an enum (such as mode, type, status, reason, or item/section identifiers),
prefer a `switch` statement over if/else chains, sequential if checks, or
negative inequality checks (`if (val != kCase)`). Unify distinct operational
modes into explicit enum enumerators dispatched via switch rather than mixing
ad-hoc member, pointer, or feature-flag checks with enum values in conditional
ladders. Conversely, when checking an isolated single enumerator, use a direct
equality check (`if (val == kCase)`) instead of a switch with default:.
Likewise, when evaluating a single enum variant in a boolean predicate and
returning false or NO for all others, use a direct boolean expression (such as
`return val == kCase && condition;`) rather than a verbose multi-case switch.

**Why.** Chaining if/else statements or using negative inequality checks (`!=`)
silently directs newly introduced enum enumerators into fallback branches or
skips them, causing silent behavioral regressions without compiler warnings,
whereas a `switch` over the enum makes every variant an explicit case.
Furthermore, mixing ad-hoc variable, pointer, or feature-flag checks with enum
modes produces asymmetric control flow and obscures state transitions.
Conversely, wrapping an isolated single-case check in a switch with default: or
listing every enumerator just to return NO creates unnecessary boilerplate and
obscures intent when a direct equality check or boolean expression is cleaner
and idiomatic.

## Examples

Bad:

```cpp
// Bad: Sequential or chained if checks silently route future enum cases into fallbacks:
if (result == IOSGeminiCameraPickerResult::kFinished) {
  RecordAction("PickerFinished");
} else {
  RecordAction("PickerCancelled");
}

// Bad: Negative inequality and mixing ad-hoc feature flags with enum modes:
if (base::FeatureList::IsEnabled(kIOSMiniMapUniversalLink) && _url) {
  [self.miniMapController presentMapsNativePreview];
} else if (self.mode == MiniMapMode::kDirections) {
  [self.miniMapController presentDirections];
}

// Bad: Multi-case switch returning NO for other cases when checking one value:
- (BOOL)isToolDisabled:(ComposeboxMode)mode {
  switch (mode) {
    case ComposeboxMode::kImageGeneration:
      return self.items.hasTabOrFile;
    case ComposeboxMode::kCanvas:
    case ComposeboxMode::kDeepSearch:
      return NO;
  }
}
```

Good:

```cpp
// Good: Switch over the enum makes every variant an explicit case:
switch (result) {
  case IOSGeminiCameraPickerResult::kCancelled:
    RecordAction("PickerCancelled");
    break;
  case IOSGeminiCameraPickerResult::kFinished:
    RecordAction("PickerFinished");
    break;
}

// Good: Distinct operational modes unified into explicit enum switch dispatch:
switch (self.mode) {
  case MiniMapMode::kMapNativePreviewURL:
    [self.miniMapController presentMapsNativePreview];
    break;
  case MiniMapMode::kDirections:
    [self.miniMapController presentDirections];
    break;
}

// Good: Direct boolean expression for a single enum variant predicate:
- (BOOL)isToolDisabled:(ComposeboxMode)mode {
  return mode == ComposeboxMode::kImageGeneration && self.items.hasTabOrFile;
}
```
