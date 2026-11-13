---
name: ios/localize-user-visible-strings
description: Load user-visible text in iOS UI, alerts, and accessibility labels from string resources via l10n_util::GetNSString or GetNSStringF instead of hardcoded NSString literals
rubric: correctness
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: UI and Assets Best Practices
    quote: '**Localize every user-visible string:** Load user-visible text (labels, buttons, alerts, and accessibility labels) from string resources with `l10n_util::GetNSString(IDS_...)` or `l10n_util::GetNSStringF(...)` from [`//ui/base/l10n/l10n_util_mac.h`](/ui/base/l10n/l10n_util_mac.h); never hardcode user-visible `NSString` literals.'
---

**Rule.** Set user-visible text (`text`, `title`, `placeholder`,
`accessibilityLabel`, `accessibilityHint`, button titles, alert titles and
messages, and similar) from string resources with
`l10n_util::GetNSString(IDS_...)` or `l10n_util::GetNSStringF(IDS_..., ...)`.
Never assign a hardcoded `NSString` literal (`@"..."`) to user-visible text.
This does not apply to accessibility identifiers, internal debug-only UI (which
must not use `.grd` strings), or test code.

**Why.** Hardcoded literals are never translated, so users in other locales see
English text, and VoiceOver users hear untranslated labels. String resources
also give translators the context they need through their descriptions. See
`ios/best_practices.md#ui-and-assets-best-practices`.

## Examples

Bad:

```cpp
self.titleLabel.text = @"Saved passwords";  // BAD: Never translated.
button.accessibilityLabel = @"Close";       // BAD: Never translated.
```

Good:

```cpp
self.titleLabel.text = l10n_util::GetNSString(IDS_IOS_FOO_TITLE);
button.accessibilityLabel =
    l10n_util::GetNSString(IDS_IOS_FOO_CLOSE_BUTTON_ACCESSIBILITY_LABEL);
```
