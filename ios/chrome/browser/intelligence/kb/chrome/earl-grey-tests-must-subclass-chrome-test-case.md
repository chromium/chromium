---
name: ios/earl-grey-tests-must-subclass-chrome-test-case
description: When writing EarlGrey integration tests in *_egtest.mm, inherit from ChromeTestCase and prefix test methods with lowercase test.
rubric: testing
severity: should_fix
citations:
  - path: docs/ios/testing.md
    heading: Writing EarlGrey tests
    quote: Create `SomeGreatTestCase` as a subclass of `ChromeTestCase`. Create test methods, eg `-(void)testMyGreatUIFeature {...}`, and put UI actions within the test methods.
  - path: ios/best_practices.md
    heading: Testing Best Practices
    quote: 'EarlGrey tests (`*_egtest.mm`): Use for end-to-end integration and user journeys across UI and model layers.'
---

**Rule.** EarlGrey test classes in `*_egtest.mm` files must inherit from
`ChromeTestCase`. Test methods must be parameterless and start with lowercase
`test`.

**Why.** `ChromeTestCase` sets up the test environment, manages test app
lifecycle, resets state between tests, and integrates with the test runner
infrastructure. See `docs/ios/testing.md#writing-earlgrey-tests` and
`ios/best_practices.md#testing-best-practices`.

## Examples

Bad:

```cpp
@interface MyFeatureTestCase : XCTestCase
@end
```

Good:

```cpp
#import "ios/chrome/test/earl_grey/chrome_test_case.h"

@interface MyFeatureTestCase : ChromeTestCase
@end
```
