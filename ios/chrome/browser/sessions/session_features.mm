// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace sessions {

BASE_FEATURE(kSaveSessionTabsToSeparateFiles,
             "SaveSessionTabsToSeparateFiles",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool ShouldSaveSessionTabsToSeparateFiles() {
  return base::FeatureList::IsEnabled(kSaveSessionTabsToSeparateFiles);
}

}  // namespace sessions
