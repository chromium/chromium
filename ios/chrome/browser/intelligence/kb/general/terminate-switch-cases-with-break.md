---
name: ios/terminate-switch-cases-with-break
description: When writing switch statement case or default blocks, terminate each non-empty branch with break, return, or [[fallthrough]], including braced and final cases
rubric: readability
severity: nit
citations:
  - path: https://google.github.io/styleguide/cppguide.html#Switch_Statements
    heading: Switch Statements
    quote: Terminate each non-empty case block with a break, return, [[fallthrough]], or other control flow statement, even the last one. (The last one needs it so that subsequent additions of case blocks don't fall through accidentally.)
---

**Rule.** In `switch` statements, terminate every non-empty `case` and `default`
block with an explicit control-flow statement (`break;`, `return;`, or
`[[fallthrough]];`), including braced case blocks and the final case in the
switch. Never omit `break;` in the last case even when fallthrough appears
functionally benign or when the case body is enclosed in braces.

**Why.** Omitting `break;` at the end of a switch statement or within a braced
case block does not trigger compiler fallthrough warnings
(`-Wimplicit-fallthrough`) because execution falls off the switch rather than
into a subsequent label. However, this creates a latent control-flow defect:
when developers append additional `case` labels in future edits, execution will
unexpectedly fall through from the previously unterminated case into the new
case. Terminating every non-empty case adheres to style conventions and prevents
accidental fallthrough defects.

## Examples

Bad:

```cpp
// Variant 1: C++ braced case block omitting break in final case
switch (status) {
  case Status::kRunning: {
    UpdateMetrics(GetActiveTasks());
  }
}

// Variant 2: Objective-C++ final case omitting break
switch (step) {
  case GuidedTourStep::kTabGridLongPress:
    [tabGridHandler hideTabGridGuidedTour];
}
```

Good:

```cpp
// Variant 1: Terminate braced case with break
switch (status) {
  case Status::kRunning: {
    UpdateMetrics(GetActiveTasks());
    break;
  }
}

// Variant 2: Terminate final case with break
switch (step) {
  case GuidedTourStep::kTabGridLongPress:
    [tabGridHandler hideTabGridGuidedTour];
    break;
}
```
