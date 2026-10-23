---
name: ios/combine-guard-and-predicate-in-boolean-return
description: When returning boolean results, return boolean expressions directly or combine guards with short-circuit && instead of branching with if or ternary statements
rubric: readability
severity: nit
---

**Rule.** When a function or method returns a boolean result, return boolean
expressions, comparisons, or conditions directly instead of wrapping them in
redundant conditional statements or ternary operators (`? :`) that return
boolean literals (`true`/`false` in C++ and Swift, `YES`/`NO` in Objective-C).
When a boolean return depends on preceding guards (such as container bounds,
emptiness via `empty()`, `count`, `size()`, or `length`, or pointer/optional
validity), combine the guard and dependent predicate into a single return
statement using logical AND (`&&`). Simplify sequential multi-branch conditional
checks returning boolean literals into a direct boolean expression. Rely on
short-circuit evaluation and omit redundant `else` blocks after returns.

**Why.** Directly returning boolean expressions and combining guarded predicates
eliminates redundant control-flow branches, avoids boilerplate and unnecessary
`else` nesting, simplifies multi-branch truth tables, and leverages
short-circuit evaluation to express conditions idiomatically without altering
program semantics.

## Examples

Bad:

```cpp
// Bad: Redundant if/else branching returning boolean literals.
if (!GetBrowserForTabWithCriteria(browser_list, criteria, is_otr_tab)) {
  return NO;
}
return YES;

// Bad: Redundant guard before evaluating element predicate.
if ([suggestions count] < 1) {
  return false;
}
return suggestions[0].type == SuggestionType::kSaveAndFillCreditCard;

// Bad: Multi-branch returns or branching on nil check.
if (!is_hidden && !is_response) {
  return false;
}
if (is_response && is_hidden) {
  return false;
}
return true;

func deleted() -> Bool {
  if account?.gaia != nil { return false }
  return true
}
```

Good:

```cpp
// Good: Return boolean expression directly.
return GetBrowserForTabWithCriteria(browser_list, criteria, is_otr_tab);

// Good: Combine bounds guard and predicate with short-circuit &&.
return [suggestions count] >= 1 &&
       suggestions[0].type == SuggestionType::kSaveAndFillCreditCard;

// Good: Simplify multi-branch boolean logic or return comparison directly.
return is_hidden ? !is_response : is_response;

func deleted() -> Bool {
  return account?.gaia == nil
}
```
