// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_FRE_FIELD_TRIAL_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_FRE_FIELD_TRIAL_H_

// Version of the new MICE FRE to show.
enum class NewMobileIdentityConsistencyFRE {
  // New MICE FRE with tangible sync (welcome with sign-in + tangible sync
  // screens).
  // Strings in TangibleSyncViewController are set according to the A, B or C
  // variants.
  kTangibleSyncA = 0,
  kTangibleSyncB,
  kTangibleSyncC,
  // New MICE FRE with 2 steps (welcome with sign-in + sync screens).
  kTwoSteps,
  // Old FRE.
  kOld,
  // New MICE FRE with tangible sync (welcome with sign-in + tangible sync
  // screens).
  // Strings in TangibleSyncViewController are set according to the D, E or F
  // variants.
  kTangibleSyncD,
  kTangibleSyncE,
  kTangibleSyncF,
};

namespace fre_field_trial {

// Returns the FRE to display according to the feature flag and experiment.
// See NewMobileIdentityConsistencyFRE.
NewMobileIdentityConsistencyFRE GetNewMobileIdentityConsistencyFRE();

}  // namespace fre_field_trial

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_FRE_FIELD_TRIAL_H_
