// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/features/features_utils.h"

#import "base/feature_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/public/provider/chrome/browser/raccoon/raccoon_api.h"

bool IsModernTabStripOrRaccoonEnabled() {
  return base::FeatureList::IsEnabled(kModernTabStrip) ||
         ios::provider::IsRaccoonEnabled();
}
