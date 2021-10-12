// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_USER_POPULATION_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_USER_POPULATION_H_

#include "components/safe_browsing/core/common/proto/csd.pb.h"

class ChromeBrowserState;

// Returns the UserPopulation enum for the given |browser_state|.
safe_browsing::ChromeUserPopulation::UserPopulation GetUserPopulationPref(
    ChromeBrowserState* browser_state);

// Creates a ChromeUserPopulation proto for the given |browser_state|.
safe_browsing::ChromeUserPopulation GetUserPopulation(
    ChromeBrowserState* browser_state);

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_USER_POPULATION_H_
