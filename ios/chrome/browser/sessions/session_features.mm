// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sessions/session_features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace sessions {

const base::Feature kSaveSessionTabsToSeparateFiles{
    "SaveSessionTabsToSeparateFiles", base::FEATURE_DISABLED_BY_DEFAULT};

bool ShouldSaveSessionTabsToSeparateFiles() {
  return base::FeatureList::IsEnabled(kSaveSessionTabsToSeparateFiles);
}

}  // namespace sessions
