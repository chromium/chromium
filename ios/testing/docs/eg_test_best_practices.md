# Writing robust EarlGrey tests

These rules help keep EarlGrey (EG2) tests stable on slow simulators, physical
devices and new iOS versions. Each example below is adapted from a deflake CL.
For setting up and running tests, see
[Automated testing for Chrome for iOS](/docs/ios/testing.md). For multi-window
tests, see [Adding Multiwindow EG Tests](/docs/ios/multiwindow_eg_tests.md).

## Rules

### Wait for the result of each state-changing action

After an action that presents or dismisses UI or signs in, use the polling wait
helpers `[ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:]` or
`[ChromeEarlGrey waitForUIElementToDisappearWithMatcher:]` instead of a one-shot
`assertWithMatcher:`. EarlGrey waits for the app to be idle, but transitions can
still be in progress, for example on physical devices. Polling waits return as
soon as the condition holds, so they do not slow down passing tests.

```objc
[self openLockdownModeSettings];

// Bad: checks once, while the push animation may still be in progress.
[[EarlGrey
    selectElementWithMatcher:TableViewSwitchCell(kLockdownModeCellId, NO)]
    assertWithMatcher:grey_notNil()];

// Good: polls until the cell is sufficiently visible.
[ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                    TableViewSwitchCell(kLockdownModeCellId, NO)];
```

### Persist state before relaunching the app within a test

When a test relaunches the app mid-test (e.g. to enable a feature or to check
that state survives a restart), call `[ChromeEarlGrey saveSessionImmediately]`
and `[ChromeEarlGrey commitPendingUserPrefsWrite]` first. Session and pref
saving is asynchronous, so otherwise the relaunched app may restore an older
state, e.g. the New Tab Page instead of the page the test loaded.

```objc
[ChromeEarlGrey loadURL:destinationUrl];
[SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

// Without `saveSessionImmediately`, the relaunched app may restore the New Tab
// Page instead of `destinationUrl`.
[ChromeEarlGrey saveSessionImmediately];
[ChromeEarlGrey commitPendingUserPrefsWrite];

[self relaunchWithIPHFeature:@"IPH_SendTabToSelfOmnibox" safariSwitcher:NO];
```

### Put app-side setup in `-setUp`

The app is relaunched with a wiped profile before every test, while
`+setUpForTestCase` runs only once per test class.

```objc
// Bad: only the first test's app process uses the fake responses.
+ (void)setUpForTestCase {
  [super setUpForTestCase];
  [SigninEarlGrey setUseFakeResponsesForProfileSeparationPolicyRequests];
}

// Good: every test's app process uses the fake responses.
- (void)setUp {
  [super setUp];
  [SigninEarlGrey setUseFakeResponsesForProfileSeparationPolicyRequests];
}
```

### Fake slow system APIs

When Chrome only updates the UI after an asynchronous system API calls its
completion handler, and that API is not what the test verifies, swizzle it with
`EarlGreyScopedBlockSwizzler` so that the completion runs right away. The
system's latency is unbounded and EarlGrey does not wait for it, so a fixed
timeout can be too short on slow bots.

```objc
// The app-side block skips the system prompt and calls the completion handler
// without waiting for the system.
EarlGreyScopedBlockSwizzler turnOnRequestSwizzler(
    @"ASSettingsHelper",
    @"requestToTurnOnCredentialProviderExtensionWithCompletionHandler:",
    [PasswordsInOtherAppsAppInterface
        swizzleTurnOnCredentialProviderExtensionRequestWithResult:NO]);
```

### Know what the synchronization scopes do

Tests use `ScopedSynchronizationDisabler` when the app does not become idle in
time for the check, for example while an infinite spinner or a video is shown,
or to catch an infobar before it auto-dismisses. Inside that scope, EarlGrey
does not wait for the app at all, so every check in it must be a polling wait.

`ScopedDisableTimerTracking` only stops EarlGrey from waiting for non-repeating
`NSTimer`s. EarlGrey waits only for those whose interval is within
`kGREYConfigKeyNSTimerMaxTrackableInterval` (an EarlGrey default, currently 1.5
seconds), never for repeating or longer timers. The `ChromeEarlGreyUI` menu and
settings helpers use it around scroll searches, so that EarlGrey does not wait
for the scroll bars to fade out after each scroll (crbug.com/40138424).

## Suppressing failing tests

See [Testing Best Practices](/ios/best_practices.md#testing-best-practices). Do
not use `DISABLED_` or `FLAKY_` prefixes in EarlGrey tests; add an entry to
[`//ios/testing/test_expectations.txt`](/ios/testing/test_expectations.txt)
instead.
