// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_TEST_SEPARATE_PROFILES_UTIL_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_TEST_SEPARATE_PROFILES_UTIL_H_

@class FakeSystemIdentity;
@protocol GREYMatcher;

// Matcher for the identity disc on the NTP.
id<GREYMatcher> IdentityDiscMatcher();

// Matcher for the account menu.
id<GREYMatcher> AccountMenuMatcher();

// Taps the identity disc on the NTP (which must already be visible).
void TapIdentityDisc();

// Opens the account menu on the NTP (which must already be visible).
void OpenAccountMenu();

// Opens the account menu on the NTP (which must already be visible), then
// enters the "Manage accounts on this device" view.
void OpenManageAccountsView();

// Opens the account menu on the NTP (which must already be visible), then
// taps on the sign-out button, and dismisses the sign-out snackbar.
void SignoutFromAccountMenu();

// Waits for the enterprise onboarding screen to become sufficiently visible.
void WaitForEnterpriseOnboardingScreen();

// Confirms the enterprise onboarding screen by tapping the primary button,
// scrolling to the bottom first if needed (required when `scrollToEndMandatory`
// is active on compact screens).
void ConfirmEnterpriseOnboardingScreen();

// Opens the browsing data management screen by scrolling to the browsing data
// button if needed and tapping it.
void OpenBrowsingDataManagementScreen();

id<GREYMatcher> SigninScreenMatcher();
id<GREYMatcher> ManagedProfileCreationScreenMatcher();
// Matcher for the browsing data button on the enterprise onboarding screen.
id<GREYMatcher> ManagedProfileCreationBrowsingDataButtonMatcher();
id<GREYMatcher> BrowsingDataManagementScreenMatcher();
id<GREYMatcher> HistoryScreenMatcher();
id<GREYMatcher> DefaultBrowserScreenMatcher();

// Matcher for the secondary account button(s) in the account menu.
// Note: This will only uniquely identify a button if there are exactly two
// accounts (i.e. a single non-primary one).
id<GREYMatcher> AccountMenuSecondaryAccountsButtonMatcher();

// Matcher for the "Continue as <identity>" button on the sign-in screen.
id<GREYMatcher> ContinueButtonWithIdentityMatcher(
    FakeSystemIdentity* fakeIdentity);

void ClearHistorySyncPrefs();

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_TEST_SEPARATE_PROFILES_UTIL_H_
