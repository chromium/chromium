// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_ANNOTATIONS_ANNOTATIONS_UTIL_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_ANNOTATIONS_ANNOTATIONS_UTIL_H_

class PrefService;

// Types for the WebAnnotations.
// VALUES MUST COINCIDE WITH THE WebAnnotations POLICY DEFINITION.
enum class WebAnnotationType {
  kAddresses,
  kCalendar,
  kEMailAddresses,
  kPackage,
  kPhoneNumbers,
  kUnits,
};

// Values for the WebAnnotations policy.
// VALUES MUST COINCIDE WITH THE WebAnnotations POLICY
// DEFINITION.
enum class WebAnnotationPolicyValue {
  // Enabled. Phone numbers are detected and underlined. Actions can be trigger
  // using one tap or long press.
  kEnabled,
  // Phone numbers are detected but not underlined. Actions can only be trigger
  // using long press.
  kLongPressOnly,
  // Disabled. No detection, no possible actions.
  kDisabled,
};

// Helper function to get the policy value for a given `type`.
WebAnnotationPolicyValue GetPolicyForType(PrefService* prefs,
                                          WebAnnotationType type);

// Returns whether the address detection feature is enabled.
bool IsAddressDetectionEnabled();

// Returns whether the automatic detection settings is enabled.
// Note: If one-tap-address_detection is disabled, the setting
// is not present and default to true.
// TODO(crbug.com/40942126): remove once internal classes use new API.
bool IsAddressAutomaticDetectionEnabled(PrefService* prefs);

// Whether the user accepted the address detection one tap interstitial.
// TODO(crbug.com/40942126): remove once internal classes use new API.
bool IsAddressAutomaticDetectionAccepted(PrefService* prefs);

// Whether the consent screen should be presented to the user.
bool ShouldPresentConsentScreen(PrefService* prefs);

// Whether the IPH screen for consent should be presented to the user.
bool ShouldPresentConsentIPH(PrefService* prefs);

// Returns whether the long press detection is enabled.
// Note: If one-tap-address is disabled, the setting
// is not present and default to true.
// TODO(crbug.com/40942126): remove once internal classes use new API.
bool IsAddressLongPressDetectionEnabled(PrefService* prefs);

// Returns whether the automatic unit detection settings is enabled.
// TODO(crbug.com/40942126): remove once internal classes use new API.
bool IsUnitAutomaticDetectionEnabled(PrefService* prefs);

// Whether the annotations for type `type` should be detected and trigger
// actions on long press.
bool IsLongPressAnnotationEnabledForType(PrefService* prefs,
                                         WebAnnotationType type);

// Whether the annotations for type `type` should be detected and trigger
// actions on one tap.
bool IsOneTapAnnotationEnabledForType(PrefService* prefs,
                                      WebAnnotationType type);

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_ANNOTATIONS_ANNOTATIONS_UTIL_H_
