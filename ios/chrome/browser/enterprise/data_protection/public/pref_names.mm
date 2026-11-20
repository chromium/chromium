// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/public/pref_names.h"

#import "components/prefs/pref_registry_simple.h"

namespace enterprise_data_protection {

const char kDataProtectionWatermarkedTabs[] =
    "enterprise_data_protection.watermarked_tabs";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kDataProtectionWatermarkedTabs);
}

}  // namespace enterprise_data_protection
