---
name: ios/earl-grey-tests-must-subclass-chrome-test-case
description: Subclass ChromeTestCase (or a ChromeTestCase subclass), never XCTestCase directly, for EarlGrey tests in //ios/chrome/
rubric: testing
severity: should_fix
citations:
  - path: docs/ios/testing.md
    heading: Writing EarlGrey tests
    quote: Create `SomeGreatTestCase` as a subclass of `ChromeTestCase`.
  - path: ios/best_practices.md
    heading: Testing Best Practices
    quote: '**EarlGrey tests (`*_egtest.mm`):** Use for end-to-end integration and user journeys across UI and model layers.'
---

**Rule.** EarlGrey test classes in `//ios/chrome/` `*_egtest.mm` files must
subclass `ChromeTestCase` (or a `ChromeTestCase` subclass), never `XCTestCase`
directly.

**Why.** `ChromeTestCase` sets up the test environment, manages the host app
lifecycle, resets state between tests, and integrates with Chrome's EarlGrey
test runner. See `docs/ios/testing.md#writing-earlgrey-tests` and
`ios/best_practices.md#testing-best-practices`.

## Examples

Bad:

```cpp
// ios/chrome/browser/feature/ui/my_feature_egtest.mm
#import <XCTest/XCTest.h>

@interface MyFeatureTestCase : XCTestCase
@end
```

Good:

```cpp
// ios/chrome/browser/feature/ui/my_feature_egtest.mm
#import "ios/chrome/test/earl_grey/chrome_test_case.h"

@interface MyFeatureTestCase : ChromeTestCase
@end
```
