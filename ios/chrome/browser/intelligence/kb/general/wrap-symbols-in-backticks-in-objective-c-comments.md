---
name: ios/wrap-symbols-in-backticks-in-objective-c-comments
description: In comments and test matchers, wrap exact code symbols and parameterized selectors in backticks, and quote user-facing UI labels and controls using canonical capitalization
rubric: readability
severity: should_fix
citations:
  - path: styleguide/objective-c/objective-c.md
    heading: Disambiguating Symbols
    quote: Where needed to avoid ambiguity, use backticks to quote variable names and symbols in comments in preference to using quotation marks or naming the symbols inline.
---

**Rule.** In code comments and test matcher descriptions, format code symbols
and user-facing UI text using distinct disambiguation conventions:

1. Code symbols: Wrap code symbols—such as classes, member variables (e.g.,
   `_viewController`), properties, constants, and types—in backticks rather than
   quotation marks, pipe delimiters (`|symbol|`), or unquoted prose. When
   documenting properties, fields, or derived values, refer to the exact source
   identifier in backticks (e.g., `serviceIdentifier`, `itemCache`) rather than
   vague descriptive phrases. For Objective-C method selectors that accept
   arguments, include the trailing colon for each parameter (e.g.,
   `configureCell:`, `handleIdentityUpdated:`) to accurately reflect the
   selector signature.
2. User-facing UI elements: Enclose user-facing labels, buttons, controls,
   dialogs, screens, section titles, and settings cell copy in double quotation
   marks rather than backticks or unquoted plain text (e.g., the "Add" button,
   the "Search history" cell, "Done", "Manage other data"). Preserve the label's
   exact canonical display capitalization and phrasing rather than arbitrary
   title-casing or plain lowercase text to clearly distinguish literal UI labels
   from generic actions. Additionally, when renaming UI strings, actions, or
   localized resource IDs (such as updating to "Delete Browsing Data" or "Edit
   Group"), update all associated comments and test matchers to eliminate
   outdated terminology.

**Why.** Per Chromium's Objective-C style guide ("Disambiguating Symbols"),
wrapping code symbols in backticks distinguishes identifiers from surrounding
prose, replaces legacy pipe delimiters, and ensures markdown doc viewers and
code-search indexing tools properly cross-reference referenced members.
Including trailing colons on parameterized selectors accurately distinguishes
them from parameterless methods. Conversely, reserving backticks strictly for
code identifiers prevents code-indexing tools from misinterpreting user-facing
UI copy as codebase symbols. Enclosing exact UI copy and button labels in
quotation marks with canonical display capitalization clearly signals that the
comment refers to a specific, labeled UI element rather than a generic verb or
action (e.g., tapping "Add" vs. adding an item). Synchronizing comments when
strings are renamed prevents stale terminology from contradicting
implementation.

## Examples

Bad:

```cpp
// GeminiStartupConfiguration holds credentials derived from the service.
@interface GeminiStartupConfiguration : NSObject
// Tests that allowMultilineDetailText is set after configureCell: or "configureCell:".
// `handleIdentityUpdated` invokes callback when |foo_delegate| is ready.
- (void)handleIdentityUpdated:(id)sender;

// Returns either the add button or the select button based on current state.
- (UIBarButtonItem*)addOrSelectButton;

// Returns a matcher for the search history cell or `Manage other data` section.
id<GREYMatcher> SearchHistoryCellMatcher() { ... }

// Called when the "Clear Browsing Data" panel is opened to tap `Done` or "Rename Group".
- (void)clearBrowsingData;
```

Good:

```cpp
// `GeminiStartupConfiguration` holds credentials derived from `serviceIdentifier`.
@interface GeminiStartupConfiguration : NSObject
// Tests that `allowMultilineDetailText` is set after `configureCell:`.
// `handleIdentityUpdated:` invokes callback when `foo_delegate` is ready.
- (void)handleIdentityUpdated:(id)sender;

// Returns either the "Add" button or the "Select" button based on current state.
- (UIBarButtonItem*)addOrSelectButton;

// Returns a matcher for the "Search history" cell or the "Manage other data" section.
id<GREYMatcher> SearchHistoryCellMatcher() { ... }

// Called when the "Delete Browsing Data" panel is opened to tap "Done" or "Edit Group".
- (void)deleteBrowsingData;
```
