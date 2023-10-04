// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_USER_POPULATION_HELPER_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_USER_POPULATION_HELPER_H_

#include "components/safe_browsing/core/common/proto/csd.pb.h"

class ChromeBrowserState;

// Creates a ChromeUserPopulation proto for the given `browser_state`.
safe_browsing::ChromeUserPopulation GetUserPopulationForBrowserState(
    ChromeBrowserState* browser_state);

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_USER_POPULATION_HELPER_H_
