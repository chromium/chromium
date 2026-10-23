---
name: ios/avoid-informal-acronyms-in-comments
description: When writing comments, documentation, or identifiers, spell out terms completely; avoid colloquial shorthand ('aka', 'w/o'), non-standard abbreviations ('VC'), and obscure feature acronyms ('PAM', 'DB').
rubric: readability
severity: nit
---

**Rule.** In code comments and interface documentation, spell out terms
completely in full narrative English or canonical symbol names. Avoid informal
abbreviations or colloquial shorthand (such as 'aka' or 'w/o'), common
abbreviation shortcuts (such as 'VC' for 'view controller'), ambiguous
initialisms (such as 'DB' for 'default browser'), and ad-hoc or obscure
feature-specific acronyms (such as 'PAM' for 'Page Action Menu'). Use clear,
grammatical sentences and standard conjunctions (such as 'and' or 'or') instead
of shorthand or sentence fragments, and reference specific types or parameter
names in backticks where appropriate. When a comment refers to a feature, use
its canonical, searchable name (for example 'Page Action Menu', never 'PAM').
Universally understood industry acronyms (such as 'URL', 'UI', or 'HTTP') are
acceptable. This rule covers prose only; abbreviations inside identifiers are
out of its scope.

**Why.** Cryptic abbreviations, conversational shorthand, ad-hoc type
contractions like 'VC', and obscure feature-internal acronyms hinder
readability, look unpolished in documentation, and create ambiguity for
engineers unfamiliar with a specific subsystem. Furthermore, non-standard
acronyms in comments impair full-text searchability of the documentation.
Spelling terms out in full ensures documentation remains professional,
unambiguous, easily searchable, and maintainable across diverse contributors.

## Examples

Bad:

```cpp
// `viewController` is the VC used to present the bottom sheet. Aka. not the
// root.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController;

// Will show the NTP, since Chrome is already set as DB.
void ShowInitialScreen();

// Dismisses the PAM and starts the Gemini flow.
[self dismissPageActionMenuWithCompletion:completion];
```

Good:

```cpp
// `viewController` is the view controller used to present the bottom sheet,
// and is not the root.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController;

// Will show the NTP, since Chrome is already set as default browser.
void ShowInitialScreen();

// Dismisses the Page Action Menu and starts the Gemini flow.
[self dismissPageActionMenuWithCompletion:completion];
```
