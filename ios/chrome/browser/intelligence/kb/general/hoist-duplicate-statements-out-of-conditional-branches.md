---
name: ios/hoist-duplicate-statements-out-of-conditional-branches
description: When conditional branches or switch cases duplicate statements, fallback values, or single active cases, hoist shared logic, simplify to early-return guards, or use [[fallthrough]];
rubric: readability
severity: should_fix
---

**Rule.** When identical statements execute unconditionally across all branches
of an if/else block or switch statement, hoist them outside the conditional
structure. Eliminate redundant conditionals and switch statements entirely when
all branches execute identical code, return the fallback value, or when
non-target cases are no-ops for a dedicated action. When a switch has only a
single active case while all other cases share identical fallback or
early-return exits, simplify it into an early-return guard clause. Where
adjacent switch cases share identical trailing logic, use \[[fallthrough]\];. If
retaining temporary placeholder branching for future divergence, document it
with an inline // TODO(crbug.com/<id>): comment.

**Why.** Duplicating identical statements or retaining multi-case switch
statements when all other cases are no-ops or shared fallbacks clutters code,
introduces unneeded mutable variables, and obscures control flow. Hoisting
shared logic, flattening single-case switches into early-return guards, or using
\[[fallthrough]\]; keeps execution paths flat and self-documenting.

## Examples

Bad:

```cpp
// Redundant conditional returning fallback value:
if (badge_type == BadgeType::kNone) {
  return BadgeType::kNone;
}
return badge_type;

// Single-case switch with shared early-return/fallback cases:
switch (info.price_bucket) {
  case PriceBucket::kLowPrice:
    message = getHighConfidenceMomentsText();
    break;
  case PriceBucket::kHighPrice:
  case PriceBucket::kTypicalPrice:
    config->relevance = Relevance::kLow;
    return;
}
```

Good:

```cpp
// Return common result directly:
return badge_type;

// Flatten single-case switch into an early-return guard:
if (info.price_bucket != PriceBucket::kLowPrice) {
  config->relevance = Relevance::kLow;
  return;
}
std::string message = getHighConfidenceMomentsText();
```
