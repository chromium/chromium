// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/profile_internals/profile_internals_handler.h"

#import <utility>

#import "base/functional/bind.h"
#import "base/i18n/time_formatting.h"
#import "base/values.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/web/public/webui/web_ui_ios.h"

namespace {

base::Value::List GaiaIdSetToValue(
    const ProfileAttributesIOS::GaiaIdSet& gaia_ids) {
  base::Value::List list;
  for (const GaiaId& gaia_id : gaia_ids) {
    list.Append(gaia_id.ToString());
  }
  return list;
}

base::Value::Dict CreateProfileEntry(const ProfileAttributesIOS& attr,
                                     bool is_personal_profile,
                                     bool is_current_profile,
                                     bool is_loaded) {
  base::Value::Dict profile_entry;
  profile_entry.Set("profileName", attr.GetProfileName());
  profile_entry.Set("isPersonalProfile", is_personal_profile);
  profile_entry.Set("isCurrentProfile", is_current_profile);
  profile_entry.Set("isLoaded", is_loaded);
  profile_entry.Set("isNewProfile", attr.IsNewProfile());
  profile_entry.Set("isFullyInitialized", attr.IsFullyInitialized());

  base::Value::Dict attributes;
  attributes.Set("gaiaId", attr.GetGaiaId().ToString());
  attributes.Set("userName", attr.GetUserName());
  attributes.Set("hasAuthenticationError", attr.HasAuthenticationError());
  attributes.Set("attachedGaiaIds",
                 GaiaIdSetToValue(attr.GetAttachedGaiaIds()));
  attributes.Set("lastActiveTime",
                 base::TimeFormatAsIso8601(attr.GetLastActiveTime()));
  profile_entry.Set("attributes", std::move(attributes));

  return profile_entry;
}

base::Value::List GetProfilesList(ProfileIOS* current_profile) {
  ProfileManagerIOS* profile_manager =
      GetApplicationContext()->GetProfileManager();
  ProfileAttributesStorageIOS* attributes_storage =
      profile_manager->GetProfileAttributesStorage();
  base::Value::List profiles_list;
  attributes_storage->IterateOverProfileAttributes(base::BindRepeating(
      [](base::Value::List& profiles_list, ProfileManagerIOS* profile_manager,
         std::string_view personal_profile_name,
         std::string_view current_profile_name,
         const ProfileAttributesIOS& attr) {
        bool is_personal_profile =
            attr.GetProfileName() == personal_profile_name;
        bool is_current_profile = attr.GetProfileName() == current_profile_name;
        bool is_loaded =
            profile_manager->GetProfileWithName(attr.GetProfileName());
        profiles_list.Append(CreateProfileEntry(attr, is_personal_profile,
                                                is_current_profile, is_loaded));
      },
      std::ref(profiles_list), profile_manager,
      attributes_storage->GetPersonalProfileName(),
      current_profile->GetProfileName()));
  return profiles_list;
}

}  // namespace

ProfileInternalsHandler::ProfileInternalsHandler() = default;

ProfileInternalsHandler::~ProfileInternalsHandler() = default;

void ProfileInternalsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getProfilesList",
      base::BindRepeating(&ProfileInternalsHandler::HandleGetProfilesList,
                          base::Unretained(this)));
}

void ProfileInternalsHandler::HandleGetProfilesList(
    const base::Value::List& args) {
  CHECK_EQ(1u, args.size());
  std::string callback_id = args[0].GetString();

  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui());
  web_ui()->ResolveJavascriptCallback(callback_id, GetProfilesList(profile));
}
