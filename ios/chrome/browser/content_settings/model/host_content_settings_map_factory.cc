// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace ios {

HostContentSettingsMapFactory::HostContentSettingsMapFactory(PassKey key)
    : TypedRefcountedProfileKeyedServiceFactoryIOS(
          std::move(key),
          "HostContentSettingsMap",
          ProfileSelection::kOwnInstanceInIncognito) {}

bool HostContentSettingsMapFactory::ServiceIsRequiredForContextInitialization()
    const {
  // HostContentSettingsMap is required to initialize the PrefService of
  // the ProfileIOS as it is part of the implementation of the
  // SupervisedUserPrefStore.
  return true;
}

scoped_refptr<RefcountedKeyedService>
HostContentSettingsMapFactory::BuildServiceInstanceFor(
    ProfileIOS* profile) const {
  // TODO(crbug.com/40130635): Set restore_session to whether or not the phone
  // has been reset, which would mirror iOS's cookie store.
  const bool is_off_the_record = profile->IsOffTheRecord();
  const bool should_record_metrics = !is_off_the_record;
  return base::MakeRefCounted<HostContentSettingsMap>(
      profile->GetPrefs(), is_off_the_record, /*store_last_modified=*/false,
      /*restore_session=*/false, should_record_metrics);
}

}  // namespace ios
