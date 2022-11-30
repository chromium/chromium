// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar_container/toolbar_container_features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace toolbar_container {

BASE_FEATURE(kToolbarContainerEnabled,
             "ToolbarContainerEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace toolbar_container
