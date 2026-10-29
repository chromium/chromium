---
name: blink/content_extraction/no-layout-or-style-updates-during-traversal
description: >-
  Never trigger synchronous style, layout, or AXObjectCache updates during
  content extraction traversal; advance lifecycles upfront and use non-updating
  DOM and accessibility queries.
rubric: correctness
severity: blocking
---

**Rule.** Never trigger synchronous style, layout, or accessibility-tree updates
during content extraction traversal:

1. **Use layout-clean read-only DOM queries during traversal**: Never call DOM
   methods that force synchronous style or layout updates during tree traversal,
   such as `Element::innerText()`, `SVGElement::title()`,
   `Element::IsKeyboardFocusableSlow()`, or `Element::IsFocusable()` with its
   default update behavior. Use `Element::GetInnerTextWithoutUpdate()`,
   `Node::textContent()`, and
   `element->IsFocusable(Element::UpdateBehavior::kAssertNoLayoutUpdates)`
   instead.
2. **Never instantiate `AXObjectCache` during extraction**: Never call
   `Document::GetOrCreateAXObjectCache()` or construct `AXObject` instances.
   Only query `Document::ExistingAXObjectCache()` when reading already-active
   accessibility state, and use stateless static helpers on `AXObject` /
   `AXNodeObject` (`DetermineRawAriaRole`, `IsAriaAttributeTrue`,
   `GetCSSAltText`).
3. **Perform all lifecycle and display-lock updates before traversal begins**:
   Advance lifecycle phases (`UpdateAllLifecyclePhasesExceptPaint` /
   `UpdateLifecycleToLayoutClean`) for the main document, subframes, and any
   open `WebPagePopup` in `UpdateLifecycle()` before walking the tree, and guard
   `ScopedForceActivatableDisplayLocks` with
   `!document.InvalidationDisallowed()`.

**Why.** Content extraction frequently runs inside
`DidFinishPostLifecycleSteps()` where `document.InvalidationDisallowed()` is
true; calling any API that dirties style or layout hits an immediate
`DocumentLifecycle` `CHECK` crash, and updating a popup or child frame's
lifecycle mid-walk can dispatch synchronous events that invalidate
`LayoutObject`s already on the call stack. Constructing `AXObjectCache` forces
Blink to build the full accessibility tree, causing severe 99th-percentile CPU
regressions.

## Examples

Bad:

```cpp
// BUG: `innerText()`, default `IsFocusable()`, and `GetOrCreateAXObjectCache()`
// dirty layout or construct the full AXTree during a read-only traversal!
String label = element->innerText();
bool focusable = element->IsFocusable();
AXObjectCache* ax_cache = document.GetOrCreateAXObjectCache();
```

Good:

```cpp
// Use non-updating DOM/AX queries during traversal:
String label = element->GetInnerTextWithoutUpdate();
bool focusable =
    element->IsFocusable(Element::UpdateBehavior::kAssertNoLayoutUpdates);
AXObjectCache* ax_cache = document.ExistingAXObjectCache();
```
