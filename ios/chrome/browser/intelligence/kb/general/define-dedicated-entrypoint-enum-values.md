---
name: ios/define-dedicated-entrypoint-enum-values
description: When instrumenting feature entry points, sign-in (signin_metrics::AccessPoint), or AccountMenuAccessPoint, define dedicated, surface-named enum values in enums.xml instead of reusing existing values
rubric: testing
severity: should_fix
citations:
  - path: tools/metrics/histograms/review_guidelines.md
    heading: Histogram is setup correctly.
    quote: When modifying enums it is better to add new values to the enum instead of re-purposing existing enum values.
  - path: tools/metrics/histograms/README.md
    heading: Directly Measure What You Want
    quote: Usually it's best to measure exactly what you want.
  - path: agents/skills/histograms/SKILL.md
    heading: Workflow
    quote: If the histogram uses an enum, ensure the corresponding <enum> and its <int> values are defined in enums.xml.
  - path: docs/supported_platforms.md
    heading: iOS WebView
    quote: It bundles select Chromium features to enhance the web browsing experience beyond what WKWebView provides out of the box.
---

**Rule.** When triggering, presenting, or instrumenting a feature, sign-in flow,
or action from a new entry point, interaction source, or platform-specific UI
surface (such as in-product promos, notifications, widgets, or mobile account
menus vs. desktop profile menus), define and record a dedicated enumerator in
the corresponding entry point or source enum and register it in `enums.xml`
(e.g., `tools/metrics/histograms/metadata/enums.xml` or
`metadata/signin/enums.xml`). Do not reuse an existing enum value from a generic
surface (such as `kSettings` or `kMenu`) or another platform with a different UI
or interaction flow. Additionally:

- When initiating a sign-in flow via `SigninCoordinator`, `ShowSigninCommand`,
  or `IdentityManager`, define a dedicated `signin_metrics::AccessPoint` named
  after the feature, embedder, or product surface rather than internal test
  harnesses or shell controllers (e.g., avoid `kShellViewController`), and
  explicitly qualify ChromeWebView (e.g., `kIosChromeWebView`) on iOS to avoid
  ambiguity with Apple's platform `WKWebView`.
- When presenting the Account Menu via `AccountMenuCoordinator` or
  `showAccountMenuWithAccessPoint:` from a new UI entry point or flow, define
  and pass a dedicated `AccountMenuAccessPoint` enumerator (registered in
  `enums.xml`) rather than reusing an existing or unrelated access point.
- When launching or resuming a feature flow from an intermediate step or
  secondary flow (such as resuming after an authentication sheet, promo, or
  confirmation prompt), define and pass a dedicated entry point enum value (such
  as `gemini::EntryPoint::AIHubSignInSheet`) rather than reusing the initial
  parent UI entry point.

**Why.** Reusing existing entry point or access point enum values across
distinct invocation pathways, generic entry points (`kSettings`, `kMenu`), or
differing platform UI surfaces conflates telemetry data, preventing accurate
attribution of user interactions, feature adoption, promo conversion rates, and
platform-specific usage patterns. In intermediate or continuation flows, reusing
parent entry points obscures funnel conversion and drop-off analysis in UMA
dashboards. For sign-in funnels, generic access points skew baseline
`Signin.SignIn.*` metrics, while naming access points after test harnesses (like
`ShellViewController`) obscures the real product surface. Furthermore, access
point enums such as `AccountMenuAccessPoint` not only log to histograms (such as
`Signin.IOSAccountMenu.Opened`) but also route continuation behaviors in
`AccountMenuMediator`; reusing an existing access point skews entry-point
telemetry and risks unintended continuation side effects.

## Examples

Bad:

```cpp
// Bad: Reusing an existing generic access point for a new sign-in or test shell.
_signinCoordinator = [SigninCoordinator
    consistencyPromoSigninCoordinatorWithBaseViewController:self.viewController
                                                    browser:self.browser
                                               contextStyle:SigninContextStyle::kDefault
                                                accessPoint:signin_metrics::AccessPoint::kSettings
                                       prepareChangeProfile:nil
                                       continuationProvider:DoNothingContinuationProvider()];
_identityManager->GetPrimaryAccountMutator()->SetPrimaryAccount(
    accountId, signin::ConsentLevel::kSignin,
    signin_metrics::AccessPoint::kShellViewController);

// Bad: Reusing an existing entry point for promo/AccountMenu or intermediate flow.
[self.NTPMetricsRecorder recordHomeCustomizationMenuOpenedFromEntrypoint:
                             HomeCustomizationEntrypoint::kMain];
_accountMenuCoordinator = [[AccountMenuCoordinator alloc]
    initWithBaseViewController:self.baseViewController
                       browser:self.browser
                    anchorView:self.baseViewController.view
                   accessPoint:AccountMenuAccessPoint::kPageActionMenu
                           URL:GURL()];
[geminiHandler startGeminiFlowWithStartupState:
    [[GeminiStartupState alloc]
        initWithEntryPoint:gemini::EntryPoint::AIHub]];
```

Good:

```cpp
// Good: Defining dedicated access points named after the feature/embedder surface.
_signinCoordinator = [SigninCoordinator
    consistencyPromoSigninCoordinatorWithBaseViewController:self.viewController
                                                    browser:self.browser
                                               contextStyle:SigninContextStyle::kDefault
                                                accessPoint:signin_metrics::AccessPoint::kCredentialExchangeImport
                                       prepareChangeProfile:nil
                                       continuationProvider:DoNothingContinuationProvider()];
_identityManager->GetPrimaryAccountMutator()->SetPrimaryAccount(
    accountId, signin::ConsentLevel::kSignin,
    signin_metrics::AccessPoint::kIosChromeWebView);

// Good: Defining dedicated enum values for promo, AccountMenu, and resumption.
[self.NTPMetricsRecorder recordHomeCustomizationMenuOpenedFromEntrypoint:
                             HomeCustomizationEntrypoint::kPromo];
_accountMenuCoordinator = [[AccountMenuCoordinator alloc]
    initWithBaseViewController:self.baseViewController
                       browser:self.browser
                    anchorView:self.baseViewController.view
                   accessPoint:AccountMenuAccessPoint::kGeminiEntryFlow
                           URL:GURL()];
[geminiHandler startGeminiFlowWithStartupState:
    [[GeminiStartupState alloc]
        initWithEntryPoint:gemini::EntryPoint::AIHubSignInSheet]];
```
