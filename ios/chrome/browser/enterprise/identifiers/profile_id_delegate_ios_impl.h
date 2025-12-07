// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_IDENTIFIERS_PROFILE_ID_DELEGATE_IOS_IMPL_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_IDENTIFIERS_PROFILE_ID_DELEGATE_IOS_IMPL_H_

#import "base/memory/raw_ptr.h"
#import "base/supports_user_data.h"
#import "components/enterprise/browser/identifiers/profile_id_delegate.h"

class ProfileIOS;

namespace enterprise {

// This class manages the collection of data needed for profile management,
// before the new profile is fully initialized. For now this class only contains
// the preset profile GUID for a newly created profile.
class PresetProfileManagementDataIOS : public base::SupportsUserData::Data {
 public:
  explicit PresetProfileManagementDataIOS(std::string preset_guid);
  ~PresetProfileManagementDataIOS() override = default;

  PresetProfileManagementDataIOS(const PresetProfileManagementDataIOS&) =
      delete;
  PresetProfileManagementDataIOS& operator=(
      const PresetProfileManagementDataIOS&) = delete;

  static PresetProfileManagementDataIOS* Get(ProfileIOS* profile);
  void SetGuid(std::string guid);
  std::string GetGuid();
  void ClearGuid();

  // The preset GUID will be used instead of a new random GUID when a profile is
  // first created. This does not overwrite if a GUID has already been set for a
  // profile.
  std::string guid() { return guid_; }

 private:
  std::string guid_;
};

// Implementation of the profile Id delegate IOS.
class ProfileIdDelegateIOSImpl : public ProfileIdDelegate {
 public:
  explicit ProfileIdDelegateIOSImpl(ProfileIOS* profile);
  ~ProfileIdDelegateIOSImpl() override;

  // ProfileIdDelegate override.
  std::string GetDeviceId() override;

  static std::string GetId();
};

}  // namespace enterprise

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_IDENTIFIERS_PROFILE_ID_DELEGATE_IOS_IMPL_H_
