---
name: ios/use-uikit-test-util-for-view-lookup
description: In //ios/chrome unit tests, find views with the chrome_test_util helpers in uikit_test_util.h instead of local recursive subview searches, and add missing generic helpers to that header
rubric: testing
severity: nit
citations:
  - path: ios/best_practices.md
    heading: Testing Best Practices
    quote: '**Use shared view lookup helpers in unit tests:** To find views in a hierarchy, use the `chrome_test_util` helpers in [`//ios/chrome/test/app/uikit_test_util.h`](/ios/chrome/test/app/uikit_test_util.h) (`FindViewById()`, `FindViewByClass()`, `FindViewsByClass()`, `FindLabelWithText()`, `FindViewWithPredicate()`) instead of writing local recursive subview searches.'
---

**Rule.** In `//ios/chrome` unit tests that inspect a `UIView` hierarchy, find
views with the `chrome_test_util` helpers in
`ios/chrome/test/app/uikit_test_util.h` (`FindViewById()`, `FindViewByClass()`,
`FindViewsByClass()`, `FindLabelWithText()`, `FindViewWithPredicate()`, and
`FindViewsWithPredicate()`). Do not write local recursive functions or loops
over `subviews` to search the hierarchy. If a generic `UIView` helper is
missing, add it to `uikit_test_util.h` instead of defining it in the test file.
This does not apply to `//ios/web`, `//ios/components`, or `//ios/web_view`
tests, whose `DEPS` do not allow `//ios/chrome/test/app` includes.

**Why.** Shared helpers avoid duplicated, subtly different search code across
tests, and they check the type of every match and can report unexpected
duplicates, which local loops usually skip. See
`ios/best_practices.md#testing-best-practices`.

## Examples

Bad:

```cpp
// BAD: Local recursive search duplicated in a test file.
UILabel* FindLabel(UIView* view) {
  for (UIView* subview in view.subviews) {
    if ([subview isKindOfClass:[UILabel class]]) {
      return static_cast<UILabel*>(subview);
    }
    if (UILabel* label = FindLabel(subview)) {
      return label;
    }
  }
  return nil;
}
```

Good:

```cpp
UILabel* label = chrome_test_util::FindViewByClass<UILabel>(view);
```
