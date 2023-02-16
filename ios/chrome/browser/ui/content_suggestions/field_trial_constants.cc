// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/content_suggestions/field_trial_constants.h"

namespace field_trial_constants {

const char kTileAblationMVTAndShortcutsFieldTrialName[] =
    "TileAblationMVTAndShortcutsForNewUser";

const variations::VariationID kTileAblationMVTOnlyID = 3360855;
const variations::VariationID kTileAblationMVTAndShortcutsID = 3360856;
const variations::VariationID kShowMVTAndShortcutsControlID = 3360857;

const char kTileAblationMVTOnlyGroup[] = "TileAblationMVTOnly-V1";
const char kTileAblationMVTAndShortcutsGroup[] =
    "TileAblationMVTAndShortcuts-V1";
const char kTileAblationMVTAndShortcutsControlGroup[] = "Control-V1";
const char kTileAblationMVTAndShortcutsDefaultGroup[] = "Default";

}  // namespace field_trial_constants
