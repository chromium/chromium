---
name: ios/extract-repeated-condition-checks-into-helper
description: When evaluating compound boolean conditions, member readiness, category membership, or complementary branches, extract or reuse named predicate helpers instead of duplicating inline logic
rubric: design
severity: should_fix
---

**Rule.** Extract recurring compound boolean expressions, multi-flag checks,
multi-property accesses, or semantic state queries into a dedicated named
predicate helper function, accessor, or property instead of inlining or
duplicating conditional logic across call sites, methods, or sibling classes.
Name predicate helpers with indicative query phrasing (such as `Is...`,
`Has...`, `Can...`, or `Should...`) using positive return logic, and invoke them
consistently. When a state query is needed by subclasses, categories, or
external callers, declare it in the public or class-extension header as the
single source of truth. A helper is justified when it meets any of these
criteria: it has two or more callers; it names a non-obvious condition or
computation (one whose meaning is not evident from its operands); or it hosts a
block body, that is, it is the single method called on `weakSelf` from inside a
block. Do not extract a single-caller wrapper that merely renames an expression
already clear at its call site (such as `IsCacheInitialized()` wrapping a lone
`page_content_cache_ != nullptr`); inline it instead. Additionally:

- Complementary branch checks: When conditions are evaluated in complementary
  forms (such as `A || B` vs. `!A && !B`) across branches, extract the condition
  into a dedicated helper or named local variable.
- Component readiness: When readiness is checked in more than one place or
  combines several members, query it through a dedicated helper method (e.g.,
  `IsInitialized()`) rather than repeating raw inspections of backing member
  variables or pointers (e.g., `member_ != nullptr && ready_`).
- Existing helpers: When an existing helper encapsulates capability, readiness,
  or presentation prerequisites (e.g., `CanShow...()`), invoke the shared helper
  directly rather than open-coding constituent state queries.
- Enum category membership: When checking whether an enum, item type, or state
  belongs to a subset, extract the comparison chain into a named predicate
  helper or prefer an exhaustive `switch` statement so the compiler enforces
  exhaustiveness.

**Why.** Inlining multi-clause boolean expressions, multi-part property checks,
raw member readiness inspections, or chained comparisons clutters branch and
loop control flow, couples call sites to low-level state details, and obscures
semantic intent. Duplicating condition checks across methods, branches, or
sibling classes introduces maintenance hazards and risks subtle logic drift as
domain criteria evolve. Centralizing state evaluations into dedicated,
well-named predicate helpers or properties establishes an authoritative single
source of truth, guarantees uniform evaluation, provides clean extension points
for subclasses, keeps control flow concise and testable, and enables compiler
exhaustiveness enforcement for enum classifications. The same criteria keep the
rule from overshooting: a single-caller wrapper that only renames an already
clear expression adds indirection without meaning and is inlined instead.

## Examples

Bad:

```cpp
// Variant 1: Duplicated multi-flag/state queries across methods:
- (void)updateLayout {
  if (_primaryButton.hidden && _secondaryButton.hidden) self.margin = 0;
}
- (void)updatePriority {
  if (!(_primaryButton.hidden && _secondaryButton.hidden)) [self boostPriority];
}

// Variant 2: Inlined enum category check & compound readiness inspection
// repeated across methods:
if (item_type != ItemType::kAddress && item_type != ItemType::kIdentityDoc) {
  return;
}
void CacheBridge::RemoveTab(int64_t id) {
  if (page_content_cache_ && cache_ready_) page_content_cache_->RemoveTab(id);
}
void CacheBridge::ClearAll() {
  if (page_content_cache_ && cache_ready_) page_content_cache_->Clear();
}
```

Good:

```cpp
// Variant 1: Extracted into a named predicate helper/property:
- (BOOL)hasVisibleButtons {
  return !_primaryButton.hidden || !_secondaryButton.hidden;
}
- (void)updateLayout { if (![self hasVisibleButtons]) self.margin = 0; }
- (void)updatePriority { if ([self hasVisibleButtons]) [self boostPriority]; }

// Variant 2: Named predicate helper (names a non-obvious condition) &
// readiness helper shared by both callers:
bool CanDeleteItemType(ItemType type) {
  return type == ItemType::kAddress || type == ItemType::kIdentityDoc;
}
bool CacheBridge::IsCacheInitialized() const {
  return page_content_cache_ && cache_ready_;
}
void CacheBridge::RemoveTab(int64_t id) {
  if (IsCacheInitialized()) page_content_cache_->RemoveTab(id);
}
void CacheBridge::ClearAll() {
  if (IsCacheInitialized()) page_content_cache_->Clear();
}
```
