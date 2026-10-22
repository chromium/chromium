---
name: ios/document-test-case-purpose
rubric: readability
severity: should_fix
---

**Rule.** Precede each test definition (GoogleTest `TEST`, `TEST_F`, `TEST_P`,
`TYPED_TEST`, `IN_PROC_BROWSER_TEST`, or EarlGrey `test*` method) with a concise
explanatory comment written as a complete grammatical sentence in the
third-person indicative mood (e.g., `// Tests that ...` or
`// Verifies that ...`, avoiding imperative phrasing like `// Test ...` or
incomplete clauses). State the specific scenario, invariant, or expected outcome
verified, rather than merely repeating the fixture and test case name. Refer to
UI surfaces by their official capitalized names and expand non-obvious
abbreviations. Focus on the behavior under test: avoid redundantly repeating
obvious setup code or incidental fixture defaults and ambient environment
settings (such as default search engines). However, when tests involve
non-trivial feature flags, mock configurations, parameterized variations, or
subtle edge conditions, explicitly document those preconditions so the test
contract is clear at a glance.

**Why.** Test method comments follow Chromium and Google style conventions by
describing verified behavior in the third-person indicative mood as complete
grammatical sentences, matching method declaration style across the codebase.
Stating the scenario, preconditions, and verified outcome clarifies test intent
and speeds up triage during regression failures or parameterized test debugging
without requiring readers to reverse-engineer setup boilerplate or mock
assertions. Omitting incidental fixture defaults and ambient setup avoids
misleading readers into assuming ambient defaults are required prerequisites,
while documenting non-trivial feature flags or edge conditions ensures the test
contract is immediately understood.

## Examples

Bad:

```cpp
// Bad: Imperative phrasing and restates incidental fixture defaults.
// Test title when 'kPasswordRemoval' is on and DSE is Google.
TEST_F(QuickDeleteMediatorTest, TitleUpdatesOnSignIn) { ... }

// Bad: Imperative mood and lowercase UI surface name in EarlGrey test.
// Test cancel button dismisses the other data page.
- (void)testPageNavigationCancelButton { ... }

// Bad: Parameterized or mock test missing explanatory comment for preconditions.
TEST_P(SaveCardInfobarBannerOverlayMediatorMetricsTest, LogsOfferBannerShown) {
  InitInfobar(GetParam().is_for_upload, GetParam().card_save_type);
  ...
}
```

Good:

```cpp
// Good: Third-person indicative sentence stating verified behavior and preconditions.
// Tests that the consumer receives the title when user sign-in status changes.
TEST_F(QuickDeleteMediatorTest, TitleUpdatesOnSignIn) { ... }

// Good: Complete third-person sentence using capitalized official page name.
// Tests that the cancel button dismisses the Quick Delete Other Data page.
- (void)testPageNavigationCancelButton { ... }

// Good: Documents parameterized scenario and expected outcome using descriptive mood.
// Tests that the "Shown" metric is recorded when the infobar banner is presented.
TEST_P(SaveCardInfobarBannerOverlayMediatorMetricsTest, LogsOfferBannerShown) {
  InitInfobar(GetParam().is_for_upload, GetParam().card_save_type);
  ...
}
```
