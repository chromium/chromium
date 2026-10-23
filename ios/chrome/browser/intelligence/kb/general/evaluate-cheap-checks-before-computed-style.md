---
name: ios/evaluate-cheap-checks-before-computed-style
description: When evaluating conditions or classifier predicates in DOM scripts, check cheap precomputed arguments and in-memory flags before performing expensive DOM queries or style computations
rubric: readability
severity: nit
---

**Rule.** When performing conditional checks or implementing predicate functions
in DOM extraction and manipulation scripts, evaluate cheap in-memory arguments,
precomputed flags, and cheap properties before performing expensive operations
such as DOM tree queries, attribute lookups, or style and layout computations
(e.g. `getComputedStyle()`, layout measurements). Return early on fast paths or
use boolean short-circuiting so costly DOM queries and layout recalculations
execute only when earlier checks cannot resolve the condition.

**Why.** Evaluating cheap precomputed arguments, in-memory flags, or simple
properties first allows classifier and extraction functions to short-circuit
immediately. Calling expensive DOM inspection APIs, attribute lookups, or layout
and style queries like `getComputedStyle()` incurs significant runtime overhead
and can trigger unnecessary style resolution and layout recalculations.

## Examples

Bad:

```cpp
// Calling getComputedStyle before checking cheap in-memory argument.
function isGenericContainer(element: HTMLElement, interactionInfo?: PageContentNodeInteractionInfo): boolean {
  const style = windowObj.getComputedStyle(element);
  if (style.position === 'fixed' || style.position === 'sticky') {
    return true;
  }
  if (interactionInfo) {
    return true;
  }
  return false;
}
```

Good:

```cpp
// Check cheap in-memory argument first to avoid computing styles.
function isGenericContainer(element: HTMLElement, interactionInfo?: PageContentNodeInteractionInfo): boolean {
  if (interactionInfo) {
    return true;
  }
  const style = windowObj.getComputedStyle(element);
  if (style.position === 'fixed' || style.position === 'sticky') {
    return true;
  }
  return false;
}
```
