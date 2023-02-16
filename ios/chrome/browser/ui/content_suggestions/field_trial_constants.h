// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_FIELD_TRIAL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_FIELD_TRIAL_CONSTANTS_H_

#import "components/variations/variations_associated_data.h"

namespace field_trial_constants {

// Name of the field trial to configure improved default suggestions experiment
// for popular sites.
extern const char kTileAblationMVTAndShortcutsFieldTrialName[];

// Variation IDs for the improved popular sites default suggestions experiment
// arms.
extern const variations::VariationID kTileAblationMVTOnlyID;
extern const variations::VariationID kTileAblationMVTAndShortcutsID;
extern const variations::VariationID kShowMVTAndShortcutsControlID;

// Group names for the improved popular sites default suggestions experiment.
extern const char kTileAblationMVTOnlyGroup[];
extern const char kTileAblationMVTAndShortcutsGroup[];
// Group that shows the default NTP in the same % as the two above.
extern const char kTileAblationMVTAndShortcutsControlGroup[];
// Group that shows the default NTP for everyone else.
extern const char kTileAblationMVTAndShortcutsDefaultGroup[];

}  // namespace field_trial_constants

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_FIELD_TRIAL_CONSTANTS_H_
