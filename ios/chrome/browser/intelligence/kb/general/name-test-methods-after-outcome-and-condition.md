---
name: ios/name-test-methods-after-outcome-and-condition
description: When naming or commenting test methods, describe the verified outcome, observable contract, and functional condition rather than internal counters, tracking mechanics, or implementation side effects
rubric: readability
severity: should_fix
---

**Rule.** Name test methods (GoogleTest `TEST...` or EarlGrey `- (void)test...`)
and leading doc comments after the verified outcome, observable behavior, and
functional condition or interaction mechanism under test (e.g.,
`test<Outcome>When<Condition>`, `test<Outcome>Via<Mechanism>`,
`...DisabledByFlag`, `...DuringLookups`). Do not name tests or write
descriptions based on internal implementation mechanics, such as internal
counters, private fields, tracking data structures, or internal side effects
(such as a helper class being missing instead of a feature flag being disabled).
Avoid naming tests after state toggles alone (`test<State>Enabled`) or generic
outcomes when multiple interaction pathways exist. When assertions change, keep
names and comments aligned with the behavior the test now verifies.

**Why.** Test names and leading comments appear in test runners, CI logs, and
failure reports, serving as executable specifications and living documentation.
Naming tests after both expected outcomes and functional conditions or
interaction mechanisms makes failures immediately diagnosable. Conversely, names
and comments that expose internal tracking mechanics (like counters or private
fields), reference internal side effects rather than functional flags, or retain
obsolete assertions tightly couple tests to private data structures and mislead
developers about verified system behavior.

## Examples

Bad:

```cpp
// Variant 1: State toggle alone obscures expected outcome
- (void)testSignedOutEnabled { ... }

// Variant 2: Generic outcome when multiple interaction pathways exist
- (void)testPageDismissal { [self swipeDownToDismiss]; }

// Variant 3: Named after internal side effects instead of functional scenario
TEST_F(ScreenshotDelegateTest, ScreenshotNotBlockedWhenTabHelperMissing) { ... }

// Variant 4: Test comment and name describe internal counters and tracking mechanics
// Tests that multiple lookups are tracked via a counter and protection remains enabled.
TEST_F(DataProtectionTabHelperTest, MultipleLookupsTracked) { ... }
```

Good:

```cpp
// Variant 1: Describes both expected outcome and condition under test
- (void)testSaveToDriveDisplayedWhenSignedOut { ... }

// Variant 2: Disambiguates interaction mechanism across sibling tests
- (void)testPageDismissalViaDownSwipe { [self swipeDownToDismiss]; }

// Variant 3: Named after functional scenario and feature configuration
TEST_F(ScreenshotDelegateTest, ScreenshotProtectionDisabledByFlag) { ... }

// Variant 4: Describes observable contract and external conditions
// Tests that screenshot protection remains enabled during pending lookups.
TEST_F(DataProtectionTabHelperTest, EnableProtectionDuringLookups) { ... }
```
