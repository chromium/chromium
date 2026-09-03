// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/profile/features.h"

#import "base/feature_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"

bool IsMultiProfilePushNotificationHandlingEnabled() {
  return base::FeatureList::IsEnabled(kIOSPushNotificationMultiProfile);
}

BASE_FEATURE(kDestroyOTRProfileEarly, base::FEATURE_ENABLED_BY_DEFAULT);
