// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_AVAILABILITY_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_AVAILABILITY_H_

class PrefService;

// Returns whether the lens overlay is enabled.
bool IsLensOverlayAvailable(const PrefService* prefs);

// Whether the landscape orientation is allowed for Lens Overlay.
bool IsLensOverlayLandscapeOrientationEnabled(const PrefService* prefs);

// Returns whether the lens overlay should open navigation in the same tab
// instead of new tab.
bool IsLensOverlaySameTabNavigationEnabled(const PrefService* prefs);

// Returns whether LVF unified experience is enabled.
bool IsLVFUnifiedExperienceEnabled(const PrefService* prefs);

// Returns whether the escape hatch to LVF is enabled.
bool IsLVFEscapeHatchEnabled(const PrefService* prefs);

// Represents the possible onboarding treatments of Lens Overlay.
enum class LensOverlayOnboardingTreatment {
  // The default onboarding experience.
  kDefaultOnboardingExperience = 0,
  // The speedbump menu replaces the location bar button.
  kSpeedbumpMenu = 1,
  // The onboarding strings are updated.
  kUpdatedOnboardingStrings = 2,
  // The onboarding is presented with updated strings and graphics.
  kUpdatedOnboardingStringsAndVisuals = 3,
  kMaxValue = kUpdatedOnboardingStringsAndVisuals,
};

LensOverlayOnboardingTreatment GetLensOverlayOnboardingTreatment();

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_AVAILABILITY_H_
