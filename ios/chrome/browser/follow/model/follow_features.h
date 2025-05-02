// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_FEATURES_H_
#define IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_FEATURES_H_

#import "base/feature_list.h"
#import "base/time/time.h"

// TODO(crbug.com/380460820): Clean up the these methods.

// Returns the minimum number of visiting the website during a day.
double GetDailyVisitMin();

// Returns the minimum number of visiting the website in the history.
double GetNumVisitMin();

// Returns the duration for querying the visit history.
base::TimeDelta GetVisitHistoryDuration();

// Returns the duration to be excluded when query the visit history.
base::TimeDelta GetVisitHistoryExclusiveDuration();

// Returns the delay between two IPHs.
base::TimeDelta GetShowFollowIPHAfterLoaded();

#endif  // IOS_CHROME_BROWSER_FOLLOW_MODEL_FOLLOW_FEATURES_H_
