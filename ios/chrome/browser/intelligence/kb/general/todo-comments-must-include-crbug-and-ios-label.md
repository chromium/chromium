---
name: ios/todo-comments-must-include-crbug-and-ios-label
description: When documenting pending work or writing TODO comments in iOS code, use scheme-less TODO(crbug.com/<id>) without spaces, followed by a capitalized, period-terminated action description
rubric: readability
severity: should_fix
citations:
  - path: ios/best_practices.md
    heading: Comments, `TODO`s, and Bug Hygiene
    quote: 'Every `TODO` comment must reference an open issue using `TODO(crbug.com/123456): Description`. Do not reference umbrella tracking bugs in `TODO`s; file and reference a specific blocking bug instead.'
  - path: docs/ios/style.md
    heading: TODO comments
    quote: Every TODO must have a bug
---

**Rule.** When documenting pending work, temporary workarounds, compiler
fallbacks, or deferred cleanups in iOS code, use a formal TODO comment rather
than untracked prose. Format every TODO as `// TODO(crbug.com/<id>): <Action>.`
with a scheme-less Chromium tracking bug (labeled with `Hotlist-TODO-iOS`) in
parentheses immediately following `TODO` without spaces, followed by a colon and
a capitalized, actionable description ending with a period. Reference public
bugs via `crbug.com/<id>` rather than internal trackers (internal bug IDs),
record external IDs (Radars/FB) parenthetically, and never leave bare
`// TODO:`, scratch placeholders (`// todo`), untracked prose, or split
references.

**Why.** Untracked prose describing pending tasks, temporary fallbacks, or
deferred cleanups easily becomes forgotten technical debt invisible to issue
tracking. Standardizing on scheme-less `crbug.com/<id>` labeled with
`Hotlist-TODO-iOS` without spaces ensures deferred work is visible to
contributors and tooling, while parenthetical notes preserve external radar
context without breaking issue trackers. See `ios/best_practices.md` ("Comments,
TODOs, and Bug Hygiene") and `docs/ios/style.md#todo-comments`.

## Examples

Bad:

```cpp
// Bad: Untracked prose for pending work, temporary fallbacks, or cleanups.
// Add call to Mini Map once method is added.
// Added to make compiler happy and should be removed when guards are cleaned up.

// Bad: Improper spacing, missing issue ID, or external tracker as primary.
// TODO (crbug.com/123456): Wait for page stability based on this result.
// TODO(FB22646087): Remove once UIKit propagates trait changes.
// TODO: Change dialogType based on whether saving or sharing.

// Bad: Scratch placeholder, missing colon, split lines, or username-only.
// todo after clicking on Explain
// TODO(crbug.com/483004001) update prompt once api is updated
```

Good:

```cpp
// Good: Pending work tracked with a formal bug-linked TODO.
// TODO(crbug.com/505402932): Add call to Mini Map once method is added.

// Good: Primary crbug with external tracker noted parenthetically.
// TODO(crbug.com/507265316): Remove once UIKit propagates trait changes
// (tracked with Apple as FB22646087).

// Good: Actionable task tracked with scheme-less bug and proper formatting.
// TODO(crbug.com/441137558): Replace controller with real controller.
// TODO(crbug.com/1418068): Added to make the compiler happy and should be
// removed when compile and available guards are cleaned up.
```
