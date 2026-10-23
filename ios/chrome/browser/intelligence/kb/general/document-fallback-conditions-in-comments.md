---
name: ios/document-fallback-conditions-in-comments
description: When commenting conditional branches, compound guards, or boolean properties and returns, accurately describe all prerequisite conditions rather than omitting criteria or inverting logic
rubric: readability
severity: should_fix
---

**Rule.** When documenting conditional execution, compound guard clauses, early
returns, fallback cascades, or boolean properties and return values, explicitly
and completely describe all evaluated prerequisites, secondary state checks, and
configuration flags governing evaluation. Do not summarize compound checks or
multi-condition guards by mentioning only a subset of criteria (such as checking
pointer availability while omitting initialization or readiness checks like
`!tracker->IsInitialized()`, or checking a destination type while omitting
authentication state). Do not omit required configuration or redaction flags
(such as `redactFlag`), and do not phrase fallback operations as unconditional.
Additionally, for boolean properties, getters, or return values, verify that doc
comments accurately reflect the conditions under which they evaluate to
`true`/`YES` (or `false`/`NO`) and do not describe inverted logic or contradict
the identifier's polarity (such as describing an `isExpandable` property with
conditions where it does not expand).

**Why.** Incomplete, partial, or inverted comments mislead callers and
maintainers about an API's contract or a branch's execution boundaries.
Describing only a subset of compound conditions causes callers to make incorrect
assumptions about required preconditions, while misleading readers into assuming
an early exit or fallback branch executes unconditionally or whenever only a
partial condition is met. Accurately documenting all evaluated criteria,
initialization checks, and state flags prevents subtle logic bugs at call sites,
avoids false assumptions during refactoring, and ensures secondary guards or
redaction requirements are not accidentally removed.

## Examples

Bad:

```cpp
// Variant 1: Inverted logic or omitted prerequisite in boolean property/return doc comment
// YES if the text is fully visible with only one line.
@property(nonatomic, assign, readonly) BOOL isExpandable;

// Returns true if the selected file destination requires the user to be signed in.
- (BOOL)selectedFileDestinationRequiresSignin;

// Variant 2: Omitting secondary initialization check or configuration flag in guard comment
// If the tracker is unavailable, we cannot verify eligibility.
if (!tracker || !tracker->IsInitialized()) {
  return IOSDockingPromoEligibility::kIneligible;
}

// Process element nodes when in actionable mode or if it contains sensitive payments.
if (!actionableMode && !shouldExtractGeometryForSensitivePayment(el, redactFlag)) {
  return null;
}
```

Good:

```cpp
// Variant 1: Accurately documenting property polarity and all prerequisite conditions
// YES if the text doesn't fit with only one line.
@property(nonatomic, assign, readonly) BOOL isExpandable;

// Returns true if the destination requires signin and the user is not signed in.
- (BOOL)selectedFileDestinationRequiresSignin;

// Variant 2: Documenting all compound conditions, initialization checks, and flags
// If the tracker is unavailable or not initialized, we cannot verify eligibility.
if (!tracker || !tracker->IsInitialized()) {
  return IOSDockingPromoEligibility::kIneligible;
}

// Process element nodes when in actionable mode, or if sensitive payments need redaction.
if (!actionableMode && !shouldExtractGeometryForSensitivePayment(el, redactFlag)) {
  return null;
}
```
