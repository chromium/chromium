// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_USER_POPULATION_HELPER_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_USER_POPULATION_HELPER_H_

#import "components/safe_browsing/core/common/proto/csd.pb.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

// Creates a ChromeUserPopulation proto for the given `profile`.
safe_browsing::ChromeUserPopulation GetUserPopulationForProfile(
    ProfileIOS* profile);

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_USER_POPULATION_HELPER_H_
