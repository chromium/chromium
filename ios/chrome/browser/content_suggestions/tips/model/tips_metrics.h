// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TIPS_MODEL_TIPS_METRICS_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TIPS_MODEL_TIPS_METRICS_H_

// UMA histogram names.
// Logged when Tips (Magic Stack) is displayed for a user. The enumeration
// indicates which tip was displayed.
extern const char kTipsMagicStackModuleDisplayedTypeHistogram[];
// Logged when the user taps on the Tips (Magic Stack) module. The enumeration
// indicates which tip was interacted with.
extern const char kTipsMagicStackModuleTappedTypeHistogram[];
// Logged when the user interacts with the actionable NTP Customization promo
// sheet or its Undo snackbar.
extern const char kTipsNTPCustomizationPromoActionHistogram[];
// Logged when the user performs an inline action on the actionable NTP Theme
// tip in the Magic Stack.
extern const char kTipsNTPThemeActionHistogram[];
// Logged when the user toggles the inline switch on the actionable Tips
// Notifications tip in the Magic Stack.
extern const char kTipsNotificationsActionHistogram[];

// Enum for the IOS.TipsMagicStack.NTPCustomizationPromo.Action histogram.
// Entries should not be renumbered and numeric values should never be reused.
//
// LINT.IfChange(IOSTipsNTPCustomizationPromoAction)
enum class IOSTipsNTPCustomizationPromoAction {
  kAccepted = 0,
  kCancelled = 1,
  kUndone = 2,
  kMaxValue = kUndone,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSTipsNTPCustomizationPromoAction)

// Enum for the IOS.TipsMagicStack.NTPThemeTip.Action histogram.
// Entries should not be renumbered and numeric values should never be reused.
//
// LINT.IfChange(IOSTipsNTPThemeActionableAction)
enum class IOSTipsNTPThemeActionableAction {
  kDefaultSelected = 0,
  kColorSelected = 1,
  kPresetImageSelected = 2,
  kPhotoPickerOpened = 3,
  kSeeMoreTapped = 4,
  kMaxValue = kSeeMoreTapped,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSTipsNTPThemeActionableAction)

// User action names.
// Logged when the Save Passwords promo is dismissed.
extern const char kSavePasswordsPromoDismissedAction[];
// Logged when the Autofill Passwords promo is dismissed.
extern const char kAutofillPasswordsPromoDismissedAction[];

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_TIPS_MODEL_TIPS_METRICS_H_
