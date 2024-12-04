// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FEATURE_ENGAGEMENT_MODEL_TRACKER_FACTORY_UTIL_H_
#define IOS_CHROME_BROWSER_FEATURE_ENGAGEMENT_MODEL_TRACKER_FACTORY_UTIL_H_

#include <memory>

#include "components/keyed_service/core/keyed_service.h"

class ProfileIOS;

namespace feature_engagement {

// Util method for creating a FeatureEngagementTracker.
std::unique_ptr<KeyedService> CreateFeatureEngagementTracker(
    ProfileIOS* profile);

}  // namespace feature_engagement

#endif  // IOS_CHROME_BROWSER_FEATURE_ENGAGEMENT_MODEL_TRACKER_FACTORY_UTIL_H_
