---
name: ios/phrase-declaration-comments-in-third-person-indicative
description: When writing comments, use third-person indicative phrasing for declarations and protocol methods, and imperative mood for inline implementation or test steps
rubric: readability
severity: should_fix
citations:
  - path: styleguide/objective-c/objective-c.md
    heading: Chromium Objective-C and Objective-C++ style guide
    quote: Chromium follows the Google Objective-C style guide unless an exception is listed below.
  - path: styleguide/objective-c/objective-c.md
    heading: Chromium C++ style
    quote: Where appropriate, the [Chromium C++ style](../c++/c++.md) style guide applies to Chromium Objective-C and (especially) Objective-C++
---

**Rule.** When documenting functions, methods, properties, interfaces,
protocols, or categories, use descriptive third-person singular present tense in
the indicative mood (e.g., `// Checks ...`, `// Updates ...`,
`// Dismisses ...`, `// Sets ...`) rather than the imperative mood
(`// Check ...`, `// Update ...`, `// Dismiss ...`, `// Set ...`). Additionally,
avoid boilerplate command prefixes such as `Command ... to be ...` in protocol
method comments (e.g., use `// Dismisses picture-in-picture.` instead of
`// Command picture in picture to be dismissed.`). Conversely, when writing
inline implementation comments or test execution steps inside a function,
method, or unit test body, use the imperative mood (e.g., `// Open ...`,
`// Check ...`, `// Set ...`, `// Try to modify ...`, `// Verify that ...`)
rather than the third-person indicative mood (e.g., `// Opens ...`,
`// Checks ...`, `// Sets ...`, `// Tries to modify ...`).

**Why.** Chromium and Google C++ and Objective-C style guides distinguish API
contracts from implementation steps: declaration doc comments describe what an
interface, method, or property does from the perspective of an implied subject
('This method checks...', 'This method dismisses...'), whereas inline
implementation and test execution steps describe sequential instructions and
actions within a function body where direct imperative phrasing ('Set the
delegate', 'Open the page') is concise and standard. Maintaining this
distinction prevents confusing internal execution steps with public API
contracts and avoids awkward boilerplate command phrasing or subjectless
third-person phrasing in step-by-step code.

## Examples

Bad:

```cpp
@protocol PictureInPictureCommands <NSObject>
// Command picture in picture to be dismissed.
- (void)dismissPictureInPicture;
@end

// Check if the pending sorting criteria and filter are different.
- (void)applyPendingFilterAndSorting {
  // Bad: Using third-person indicative mood for inline implementation steps:
  // Sets the subtitle when the default search engine is null.
  if (!default_search_engine) {
    SetSubtitle(kDefaultSubtitle);
  }
}

// Bad: Using third-person indicative mood for test action steps:
// Opens quick delete browsing data page.
[self openQuickDeleteBrowsingDataPage];
```

Good:

```cpp
@protocol PictureInPictureCommands <NSObject>
// Dismisses picture-in-picture.
- (void)dismissPictureInPicture;
@end

// Checks if the pending sorting criteria and filter are different.
- (void)applyPendingFilterAndSorting {
  // Good: Using imperative mood for inline implementation steps:
  // Set the subtitle when the default search engine is null.
  if (!default_search_engine) {
    SetSubtitle(kDefaultSubtitle);
  }
}

// Good: Using imperative mood for test action steps:
// Open the Quick Delete Browsing Data page.
[self openQuickDeleteBrowsingDataPage];
```
