---
name: ios/app-configuration-for-egtest-feature-flags
description: In EarlGrey tests, configure features, variations, IPH, and language via dedicated AppLaunchConfiguration fields instead of ScopedFeatureList, raw --enable-features/--disable-features in additional_args, or test-runner FeatureList checks
rubric: correctness
severity: should_fix
citations:
  - path: docs/ios/testing.md
    heading: Relaunch app with different flags
    quote: In EG2 tests, the test process launches the host app process at the beginning, then runs UI actions/assertions in the app. To pass args or feature flags to the app at initial launching, or relaunch the app in the middle of your test, see [this AppLaunchManager API].
---

**Rule.** In EarlGrey tests (`*_egtest.mm`), configure host-app features,
variations, IPH, and language via `AppLaunchConfiguration`'s dedicated fields
(`features_enabled`, `features_enabled_and_params`, `features_disabled`,
`variations_enabled`, `trigger_variations_enabled`, `iph_feature_enabled`, and
`language`) in `-appConfigurationForTestCase` (or via `AppLaunchManager`). Do
not use `base::test::ScopedFeatureList` in test methods or pass raw
`--enable-features=` / `--disable-features=` strings via
`config.additional_args`; reserve `config.additional_args` for app switches
without a dedicated field (such as policy or custom command-line switches, test
hooks, or `-Key value` defaults). Do not call `base::FeatureList::IsEnabled()`
or feature helpers in test-runner code or UI matchers; query host-app state via
an `AppInterface` when needed. If referencing a feature causes test-runner
linker errors from app-side dependencies, factor the feature definition into an
isolated `:features` GN target. For test-specific flags, branch in
`-appConfigurationForTestCase` with `[self isRunningTest:@selector(testFoo)]`
instead of relaunching mid-test via `ensureAppLaunchedWithConfiguration:`,
reserving mid-test relaunches for restart or session-restoration tests.

**Why.** EarlGrey 2 runs tests in a separate process from the host app, so
`ScopedFeatureList` and `FeatureList::IsEnabled()` in the test runner do not
affect or observe the app under test. Passing `base::Feature` references via
`config.features_enabled` and `config.features_disabled` configures the host app
at launch with compile-time type safety instead of raw feature-flag strings in
`additional_args`. Isolating feature flags in `:features` GN targets lets the
test runner reference them without linking app code. See
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
