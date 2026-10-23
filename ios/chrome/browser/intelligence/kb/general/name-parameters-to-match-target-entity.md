---
name: ios/name-parameters-to-match-target-entity
description: When declaring function parameters, properties, or selectors, name them after the specific target entity, component, or state rather than generic terms like active or value
rubric: readability
severity: should_fix
---

**Rule.** When declaring or modifying parameters, properties, or Objective-C
selector segments, name each after the specific entity, attribute, or state it
represents. Do not use generic terms like `active`, `enabled`, `flag`, or
`value` when the function or selector name does not specify what state is being
toggled; name the parameter after the concrete condition or layout (e.g.,
`isSidePanelLayout`). Qualify multi-component parameters (e.g.,
`sidePanelProgress` instead of `progress`), name child component parameters
after intrinsic attributes rather than parent containers (e.g., `highlighted`
instead of `rowIsHighlighted`), align names with parameter types, reflect
attribute identifiers (e.g., `...WithIDs:`), and replace stale copied names. In
Objective-C selectors, the parameter name matches the noun of its selector
keyword (`acceptBlock:(...)acceptBlock`, `highlighted:(BOOL)highlighted`); only
when the keyword is a preposition or qualifier phrase (`withProgress:`,
`animated:`) may the parameter carry a more specific noun that still contains
the keyword's noun (`applyLayoutWithProgress:(CGFloat)sidePanelProgress`).

**Why.** Generic parameter names like active, flag, or value obscure what state
is being toggled when the enclosing method does not name the entity, and bare
names like progress hide whether state is local or global. Mismatched
terminology between parameter names, selector keywords, and declared types
creates ambiguity and makes APIs confusing. Referencing parent containers in
child components leaks enclosing context, while naming parameters after model
objects when only IDs are passed misleadingly implies full model objects are
received.

## Examples

Bad:

```cpp
// Bad: Ambiguous bare parameter in method coordinating multiple components.
- (void)applyLayoutWithProgress:(CGFloat)progress;

// Bad: Generic boolean parameter when selector does not specify the state.
- (void)updateAssistantVisualStyling:(BOOL)active;

// Bad: Leaks parent container into subview; mismatched parameter type.
- (void)updateWithRowIsHighlighted:(BOOL)rowIsHighlighted
                trailingActionType:(TrailingIconType)trailingActionType;

// Bad: Selector implies model objects; parameter is an identifier. The block
// parameter also drops the noun of its keyword (`block` vs. `acceptBlock`).
- (void)downloadRecordsWereRemoved:(NSArray<NSString*>*)downloadIDs;
- (void)showOptions:(NSArray<Option*>*)options
        acceptBlock:(void (^)(NSString* option))block;

// Bad: Generic placeholder for boolean state; stale copied parameter.
void RecordCameraPermissionInitialValue(bool value);
void SetBookmarkNodesCount(int tab_count);
```

Good:

```cpp
// Good: Qualifies parameter with the specific component it affects
// (`withProgress:` is a preposition keyword, so a more specific noun that
// still contains "progress" is fine).
- (void)applyLayoutWithProgress:(CGFloat)sidePanelProgress;

// Good: Names concrete layout condition instead of generic active.
- (void)updateAssistantVisualStyling:(BOOL)isSidePanelLayout;

// Good: Names component's own state and matches parameter type.
- (void)updateForHighlighted:(BOOL)highlighted
                    iconType:(TrailingIconType)iconType;

// Good: Selector and parameters explicitly reflect identifiers/attributes, and
// the block parameter matches its keyword noun.
- (void)downloadsWereRemovedWithIDs:(NSArray<NSString*>*)downloadIDs;
- (void)showOptions:(NSArray<Option*>*)options
        acceptBlock:(void (^)(NSString* selectedOptionID))acceptBlock;

// Good: Descriptive boolean parameter name; accurate entity name.
void RecordCameraPermissionInitialValue(bool enabled);
void SetBookmarkNodesCount(int bookmarks_count);
```
