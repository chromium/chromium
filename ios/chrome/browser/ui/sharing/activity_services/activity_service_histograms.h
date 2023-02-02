// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_ACTIVITY_SERVICE_HISTOGRAMS_H_
#define IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_ACTIVITY_SERVICE_HISTOGRAMS_H_

#import "ios/chrome/browser/ui/sharing/activity_services/activity_type_util.h"
#import "ios/chrome/browser/ui/sharing/sharing_scenario.h"

// Records a histogram metric for the current scenario.
void RecordScenarioInitiated(SharingScenario scenario);

// Records the given activity `type` for a `scenario`.
void RecordActivityForScenario(activity_type_util::ActivityType type,
                               SharingScenario scenario);

// Records the given activity `type` for a `scenario`.
void RecordCancelledScenario(SharingScenario scenario);

#endif  // IOS_CHROME_BROWSER_UI_SHARING_ACTIVITY_SERVICES_ACTIVITY_SERVICE_HISTOGRAMS_H_
