---
name: ios/phrase-declaration-comments-in-third-person-indicative
description: When writing declaration doc comments for functions, methods, properties, interfaces, protocols, or categories, use third-person indicative phrasing; inline implementation and test-body comments are out of scope
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
`// Command picture in picture to be dismissed.`). This rule applies only to
declaration doc comments. Do not flag inline implementation comments or test
execution steps inside a function, method, or unit test body; the imperative
mood (e.g., `// Open ...`, `// Check ...`, `// Verify that ...`) is fine there.

**Why.** Chromium and Google C++ and Objective-C style guides distinguish API
contracts from implementation steps: declaration doc comments describe what an
interface, method, or property does from the perspective of an implied subject
('This method checks...', 'This method dismisses...'), whereas inline
implementation and test execution steps describe sequential instructions and
actions within a function body where direct imperative phrasing ('Set the
delegate', 'Open the page') is concise and standard. Maintaining this
distinction keeps public API contracts consistent and avoids awkward boilerplate
command phrasing in declaration comments.

## Examples

Bad:

```cpp
@protocol PictureInPictureCommands <NSObject>
// Command picture in picture to be dismissed.
- (void)dismissPictureInPicture;
@end

// Check if the pending sorting criteria and filter are different.
- (void)applyPendingFilterAndSorting;
```

Good:

```cpp
@protocol PictureInPictureCommands <NSObject>
// Dismisses picture-in-picture.
- (void)dismissPictureInPicture;
@end

// Checks if the pending sorting criteria and filter are different.
- (void)applyPendingFilterAndSorting {
  // Not flagged: inline implementation comments may use the imperative mood.
  // Set the subtitle when the default search engine is null.
  if (!default_search_engine) {
    SetSubtitle(kDefaultSubtitle);
  }
}
```
