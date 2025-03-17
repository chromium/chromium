// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/identifiers/profile_id_delegate_ios_impl.h"

#import "base/check.h"
#import "base/files/file_path.h"
#import "base/uuid.h"
#import "components/enterprise/browser/controller/browser_dm_token_storage.h"
#import "components/enterprise/browser/identifiers/identifiers_prefs.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace enterprise {

namespace {

// Address to this variable used as the user data key.
const void* const kPresetProfileManagementDataIOS =
    &kPresetProfileManagementDataIOS;

// Create the guid for the given profile if absent.
void CreateProfileGUID(ProfileIOS* profile) {
  auto* prefs = profile->GetPrefs();
  if (!prefs->GetString(kProfileGUIDPref).empty()) {
    return;
  }

  auto* preset_profile_management_data =
      PresetProfileManagmentDataIOS::Get(profile);
  std::string preset_profile_guid = preset_profile_management_data->GetGuid();

  std::string new_profile_guid =
      (preset_profile_guid.empty())
          ? base::Uuid::GenerateRandomV4().AsLowercaseString()
          : std::move(preset_profile_guid);

  prefs->SetString(kProfileGUIDPref, new_profile_guid);
  preset_profile_management_data->ClearGuid();
}

}  // namespace

PresetProfileManagmentDataIOS* PresetProfileManagmentDataIOS::Get(
    ProfileIOS* profile) {
  CHECK(profile);

  if (!profile->GetUserData(kPresetProfileManagementDataIOS)) {
    profile->SetUserData(
        kPresetProfileManagementDataIOS,
        std::make_unique<PresetProfileManagmentDataIOS>(std::string()));
  }

  return static_cast<PresetProfileManagmentDataIOS*>(
      profile->GetUserData(kPresetProfileManagementDataIOS));
}

void PresetProfileManagmentDataIOS::SetGuid(std::string guid) {
  CHECK(!guid.empty());
  CHECK(guid_.empty());

  guid_ = std::move(guid);
}

std::string PresetProfileManagmentDataIOS::GetGuid() {
  return guid_;
}

void PresetProfileManagmentDataIOS::ClearGuid() {
  guid_.clear();
}

PresetProfileManagmentDataIOS::PresetProfileManagmentDataIOS(
    std::string preset_guid)
    : guid_(std::move(preset_guid)) {}

ProfileIdDelegateIOSImpl::ProfileIdDelegateIOSImpl(ProfileIOS* profile) {
  CHECK(profile);
  CreateProfileGUID(profile);
}

ProfileIdDelegateIOSImpl::~ProfileIdDelegateIOSImpl() = default;

std::string ProfileIdDelegateIOSImpl::GetDeviceId() {
  return ProfileIdDelegateIOSImpl::GetId();
}

// Gets the device ID from the BrowserDMTokenStorage.
std::string ProfileIdDelegateIOSImpl::GetId() {
  std::string device_id =
      policy::BrowserDMTokenStorage::Get()->RetrieveClientId();

  return device_id;
}

}  // namespace enterprise
