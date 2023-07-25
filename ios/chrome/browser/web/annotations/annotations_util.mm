// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/annotations/annotations_util.h"

#import "base/feature_list.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/web/common/features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

bool IsAddressDetectionEnabled() {
  return base::FeatureList::IsEnabled(web::features::kOneTapForMaps);
}

bool IsAddressAutomaticDetectionEnabled(PrefService* prefs) {
  return IsAddressDetectionEnabled() &&
         prefs->GetBoolean(prefs::kDetectAddressesEnabled);
}

bool IsAddressAutomaticDetectionAccepted(PrefService* prefs) {
  return IsAddressDetectionEnabled() &&
         prefs->GetBoolean(prefs::kDetectAddressesAccepted);
}

bool IsAddressLongPressDetectionEnabled(PrefService* prefs) {
  return !IsAddressDetectionEnabled() ||
         prefs->GetBoolean(prefs::kDetectAddressesEnabled);
}
