---
name: ios/app-configuration-for-egtest-feature-flags
description: In EarlGrey tests, configure feature flags via AppLaunchConfiguration features_enabled instead of ScopedFeatureList, additional_args, or test-runner FeatureList checks
rubric: correctness
severity: should_fix
citations:
  - path: docs/ios/testing.md
    heading: Relaunch app with different flags
    quote: In EG2 tests, the test process launches the host app process at the beginning, then runs UI actions/assertions in the app. To pass args or feature flags to the app at initial launching, or relaunch the app in the middle of your test, see [this AppLaunchManager API].
---

**Rule.** In EarlGrey integration tests (`*_egtest.mm`), configure feature flags
for the app under test via `AppLaunchConfiguration` by appending `base::Feature`
references to `config.features_enabled` or `config.features_disabled` in
`-appConfigurationForTestCase` (or via `AppLaunchManager`). Do not use
`base::test::ScopedFeatureList` in test methods, do not pass
`--enable-features=` or `--disable-features=` via `config.additional_args`, and
do not evaluate `base::FeatureList::IsEnabled()` or feature helper functions
directly in test-runner code or UI matchers (query host app state via an
`AppInterface` if needed). If referencing a feature pulls app-side dependencies
causing test runner linker errors, factor the feature definition into an
isolated `:features` GN target. Do not relaunch the app from inside a test
method
(`[[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:]`)
merely to apply test-specific flags; gate them in `-appConfigurationForTestCase`
with `[self isRunningTest:@selector(testFoo)]`, and reserve relaunches for tests
that verify an actual restart or session restoration.

**Why.** EarlGrey 2 runs tests out-of-process from the host application, so
in-process `base::test::ScopedFeatureList` and direct
`base::FeatureList::IsEnabled()` queries in the test runner or UI matchers only
affect or observe test-runner state rather than the app under test. Passing
features via `config.features_enabled` and `config.features_disabled` ensures
flags reach the host app at launch with compile-time type safety, avoiding
brittle command-line strings in `additional_args`. Separating feature
definitions into dedicated `:features` GN targets prevents linking full app-side
implementations into the test runner binary. See
`docs/ios/testing.md#relaunch-app-with-different-flags`.

## Examples

Bad:

```cpp
// Bad: ScopedFeatureList only modifies the test runner process.
- (void)testFeatureEnabled {
  base::test::ScopedFeatureList feature_list(
      autofill::features::kAutofillEnableWalletBrandingV2);
  [self showVirtualCardEnrollmentBottomSheetAfterSaveCardBottomSheet:YES];
}

// Bad: Querying FeatureList::IsEnabled or feature helpers in matchers/test runner.
id<GREYMatcher> VirtualCardEnrollmentTitle() {
  return grey_accessibilityLabel(l10n_util::GetNSString(
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableWalletBrandingV2)
          ? IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_DIALOG_TITLE_LABEL_V2
          : IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_DIALOG_TITLE_LABEL));
}

// Bad: Passing raw flag strings via additional_args instead of features_enabled.
- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.additional_args.push_back(
      "--enable-features=SendTabToSelfPropagateScrollPosition");
  return config;
}
```

Good:

```cpp
// Good: Configure features via features_enabled/features_disabled in AppLaunchConfiguration.
- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(
      autofill::features::kAutofillEnableWalletBrandingV2);
  config.features_enabled.push_back(
      send_tab_to_self::kSendTabToSelfPropagateScrollPosition);
  return config;
}

// Good: Write deterministic matchers for the configured feature state without runner-side checks.
id<GREYMatcher> VirtualCardEnrollmentTitle() {
  return grey_accessibilityLabel(l10n_util::GetNSString(
      IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_DIALOG_TITLE_LABEL_V2));
}

- (void)testFeatureEnabled {
  [self showVirtualCardEnrollmentBottomSheetAfterSaveCardBottomSheet:YES];
}
```
