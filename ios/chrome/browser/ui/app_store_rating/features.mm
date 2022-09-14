// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/app_store_rating/features.h"

#import "base/feature_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const base::Feature kAppStoreRating{"AppStoreRating",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

bool IsAppStoreRatingEnabled() {
  return base::FeatureList::IsEnabled(kAppStoreRating);
}
