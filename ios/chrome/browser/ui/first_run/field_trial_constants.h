// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_FIELD_TRIAL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_FIELD_TRIAL_CONSTANTS_H_

// Name of current experiment.
extern const char kIOSMICeAndDefaultBrowserTrialName[];

// Indicates which FRE default browser promo variant to use.
extern const char kFREDefaultBrowserPromoParam[];

// Indicates if the FRE default browser promo variant "Wait 14 days after FRE
// default browser promo" is enabled.
extern const char kFREDefaultBrowserPromoDefaultDelayParam[];

// Indicates if the FRE default browser promo variant "FRE default browser
// promo only" is enabled.
extern const char kFREDefaultBrowserPromoFirstRunOnlyParam[];

// Indicates if the FRE default browser promo variant "Wait 3 days after FRE
// default promo" is enabled.
extern const char kFREDefaultBrowserPromoShortDelayParam[];

// Indicates which variant of the new MICE FRE to use.
extern const char kNewMobileIdentityConsistencyFREParam[];
extern const char kNewMobileIdentityConsistencyFREParamTangibleSyncA[];
extern const char kNewMobileIdentityConsistencyFREParamTangibleSyncB[];
extern const char kNewMobileIdentityConsistencyFREParamTangibleSyncC[];
extern const char kNewMobileIdentityConsistencyFREParamTangibleSyncD[];
extern const char kNewMobileIdentityConsistencyFREParamTangibleSyncE[];
extern const char kNewMobileIdentityConsistencyFREParamTangibleSyncF[];
extern const char kNewMobileIdentityConsistencyFREParamTwoSteps[];

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_FIELD_TRIAL_CONSTANTS_H_
