---
name: ios/use-system-accessibility-traits-for-state
description: In iOS UI, make sure interactive elements have an accessibilityLabel and express state with system accessibility traits such as UIAccessibilityTraitSelected instead of encoding it in accessibilityLabel or accessibilityValue strings
rubric: correctness
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: UI and Assets Best Practices
    quote: "**Expose state through accessibility traits:** Make sure every interactive element has an `accessibilityLabel`; set it explicitly only where UIKit cannot derive it from visible text (icon-only buttons and custom controls). Express state with system traits (for example `UIAccessibilityTraitSelected`, `UIAccessibilityTraitNotEnabled`, or `UIAccessibilityTraitButton`) instead of encoding it in `accessibilityLabel` or `accessibilityValue` strings such as \"selected\", so VoiceOver announces it in the user's language and consistently with system controls."
---

**Rule.** Make sure every interactive element in `//ios` UI has an
`accessibilityLabel` describing what it is. Set it explicitly only on icon-only
buttons and custom controls; UIKit derives it from the visible title of standard
controls such as a titled `UIButton`. Express state and role with system traits:
through the `selected` and `enabled` properties on standard `UIControl`
subclasses, which update `UIAccessibilityTraitSelected` and
`UIAccessibilityTraitNotEnabled` automatically, or by adding and removing
`UIAccessibilityTraitSelected`, `UIAccessibilityTraitNotEnabled`,
`UIAccessibilityTraitButton`, and similar on `accessibilityTraits` in custom
views. Do not append or assign state words ("selected", "on", "disabled") to
`accessibilityLabel` or `accessibilityValue`.

**Why.** VoiceOver announces system traits in the user's language and in the
same way as UIKit controls, and assistive technologies such as Switch Control
rely on them. State encoded in strings is usually untranslated, is announced
inconsistently, and is easy to leave stale when the state changes. See
`ios/best_practices.md#ui-and-assets-best-practices`.

## Examples

Bad:

```cpp
- (void)setSelected:(BOOL)selected {
  _selected = selected;
  // BAD: State encoded in a string.
  self.accessibilityValue = selected ? @"selected" : nil;
}
```

Good:

```cpp
- (void)setSelected:(BOOL)selected {
  _selected = selected;
  if (selected) {
    self.accessibilityTraits |= UIAccessibilityTraitSelected;
  } else {
    self.accessibilityTraits &= ~UIAccessibilityTraitSelected;
  }
}
```
