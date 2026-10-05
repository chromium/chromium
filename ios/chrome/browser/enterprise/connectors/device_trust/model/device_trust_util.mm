// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_util.h"

#import "base/check.h"
#import "base/feature_list.h"
#import "ios/chrome/browser/enterprise/connectors/device_trust/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

bool ShouldRegisterDeviceTrust(const ProfileIOS* profile) {
  CHECK(profile);
  return base::FeatureList::IsEnabled(
             enterprise_connectors::features::kEnableIOSDeviceTrustConnector) &&
         !profile->IsOffTheRecord();
}
