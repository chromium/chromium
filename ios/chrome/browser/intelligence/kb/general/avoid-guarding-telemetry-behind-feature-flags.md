---
name: ios/avoid-guarding-telemetry-behind-feature-flags
description: When instrumenting telemetry for experiments or rollouts, record baseline and outcome metrics unconditionally across treatment and control paths rather than gating behind feature flags
rubric: testing
severity: should_fix
citations:
  - path: docs/flag_guarding_guidelines.md
    heading: What type of flag rollout to use?
    quote: If a change has the potential to affect performance or memory, or you want to analyze the impact of the launch on feature-specific metrics, use a disabled-by-default base::Feature flag and run an A/B experiment.
  - path: tools/metrics/histograms/README.md
    heading: Enum Histograms
    quote: Enumerated histogram are most appropriate when you have a list of connected / related states that should be analyzed jointly.
---

**Rule.** When instrumenting telemetry for feature experiments, migrations, or
ablation studies, emit baseline status and outcome metrics unconditionally
across both treatment and control/fallback paths. Do not gate telemetry helpers
(such as `base::UmaHistogram*`, `RecordAction`, or `UserMetricsAction`) behind
rollout feature flag checks (`base::FeatureList::IsEnabled(...)` or
`Is*Enabled()`) when pre-existing state or baseline behavior can be observed in
the control group. Ensure existing baseline entry points or fallback UI are
instrumented with matching impression and interaction metrics (`.Shown` and
`.Used`), even if slated for deprecation. Additionally, when recording boolean
histograms across feature configurations, do not log negative (false) outcomes
solely in the feature-disabled branch; record them under both paths so the
enabled population maintains an accurate denominator.

**Why.** In feature experiments, migrations, and ablation studies, telemetry
must be collected across both control and treatment groups to measure relative
impact, baseline distributions, conversion rates, and shifting engagement.
Gating baseline or outcome metrics behind experimental feature flags
(`base::FeatureList::IsEnabled(...)`) blinds dashboards and A/B analysis to
control group behavior. Similarly, omitting telemetry from baseline entry points
or fallback paths hides pre-migration comparison data, while logging negative
boolean samples only when a flag is disabled deprives the enabled population of
a denominator, artificially inflating its event incidence to 100%.

## Examples

Bad:

```cpp
// Variant 1: Gating baseline status or outcome metrics behind a feature flag
if (base::FeatureList::IsEnabled(kExperimentalFeature)) {
  if (destinationRequiresSignin) {
    return;
  }
  if (userSignedIn) {
    base::UmaHistogramEnumeration(kSignInStatusMetric, SignInStatus::kSignedIn);
  }
}

// Variant 2: Gating telemetry helper behind feature flag check
- (void)recordSigninPromoEvent:(SigninPromoEvents)event {
  if (!IsSigninPromoMigrationEnabled()) return;
  base::UmaHistogramEnumeration("IOS.SignInPromo.PromoEvents", event);
}

// Variant 3: Omitting baseline entry point or fallback UI telemetry
- (void)shareButtonTapped {
  RecordAction(UserMetricsAction("MobileToolbarShareMenu"));
  [self.delegate recordShareButtonPressed];
}

// Variant 4: Logging negative baseline only in feature-disabled branch
if (base::FeatureList::IsEnabled(kMyFeature)) {
  if (!pending_query) base::UmaHistogramBoolean("MyFeature.Event", true);
} else {
  base::UmaHistogramBoolean("MyFeature.Event", false);
}
```

Good:

```cpp
// Variant 1: Record baseline status unconditionally across both groups
if (base::FeatureList::IsEnabled(kExperimentalFeature)) {
  if (destinationRequiresSignin) {
    return;
  }
}
if (userSignedIn) {
  base::UmaHistogramEnumeration(kSignInStatusMetric, SignInStatus::kSignedIn);
}

// Variant 2: Log metric across both control and treatment arms
- (void)recordSigninPromoEvent:(SigninPromoEvents)event {
  base::UmaHistogramEnumeration("IOS.SignInPromo.PromoEvents", event);
}

// Variant 3: Instrument baseline entry point or fallback UI with matching metrics
- (void)shareButtonTapped {
  RecordAction(UserMetricsAction("MobileToolbarShareMenu"));
  base::UmaHistogramEnumeration("Mobile.ShareThisPage.Used",
                                ShareThisPageLocation::kLocationBar);
  [self.delegate recordShareButtonPressed];
}

// Variant 4: Record the outcome exactly once per call under both feature arms
bool event_fired = base::FeatureList::IsEnabled(kMyFeature) && !pending_query;
base::UmaHistogramBoolean("MyFeature.Event", event_fired);
```
