// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_FIELD_TRIAL_IDS_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_FIELD_TRIAL_IDS_H_

#import "components/variations/variations_associated_data.h"

// Experiment IDs defined for the above field trial groups.
extern const variations::VariationID kControlTrialID;
extern const variations::VariationID kTangibleSyncAFRETrialID;
extern const variations::VariationID kTangibleSyncDFRETrialID;
extern const variations::VariationID kTangibleSyncEFRETrialID;
extern const variations::VariationID kTangibleSyncFFRETrialID;
extern const variations::VariationID kTwoStepsMICEFRETrialID;

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_FIELD_TRIAL_IDS_H_
