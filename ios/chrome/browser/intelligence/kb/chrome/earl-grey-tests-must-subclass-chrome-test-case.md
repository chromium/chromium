---
name: ios/earl-grey-tests-must-subclass-chrome-test-case
rubric: testing
severity: should_fix
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
