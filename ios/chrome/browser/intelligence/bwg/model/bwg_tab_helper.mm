// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/intelligence/bwg/utils/bwg_constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/web/public/web_state.h"

BwgTabHelper::BwgTabHelper(web::WebState* web_state) : web_state_(web_state) {
  web_state_observation_.Observe(web_state);
}

BwgTabHelper::~BwgTabHelper() {}

void BwgTabHelper::SetBwgSessionActive(bool active) {
  is_bwg_session_active_ = active;
}

void BwgTabHelper::CreateOrUpdateBwgSessionInStorage(std::string server_id) {
  CreateOrUpdateSessionInPrefs(GetClientId(), server_id);
}

void BwgTabHelper::DeleteBwgSessionInStorage() {
  CleanupSessionFromPrefs(GetClientId());
}

std::string BwgTabHelper::GetClientId() {
  return base::NumberToString(web_state_->GetUniqueIdentifier().identifier());
}

std::optional<std::string> BwgTabHelper::GetServerId() {
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  std::string unique_identifier_string = GetClientId();

  const base::Value::Dict& session_map =
      profile->GetPrefs()->GetDict(prefs::kBwgSessionMap);
  const base::Value::Dict* current_session_dict =
      session_map.FindDict(unique_identifier_string);

  if (!current_session_dict) {
    return std::nullopt;
  }

  std::optional<double> creation_timestamp =
      current_session_dict->FindInt(kLastInteractionTimestampDictKey);
  const std::string* server_id =
      current_session_dict->FindString(kServerIDDictKey);
  if (!creation_timestamp || !server_id) {
    return std::nullopt;
  }

  // Return the server ID if it hasn't yet expired, otherwise clean it up.
  // TODO(crbug.com/424264708): Make the expiration time Finchable.
  int64_t latest_valid_timestamp =
      base::Time::Now().InMillisecondsSinceUnixEpoch() -
      base::Minutes(30).InMilliseconds();
  if (*creation_timestamp > latest_valid_timestamp) {
    return *server_id;
  }

  return std::nullopt;
}

void BwgTabHelper::SetBwgCommandsHandler(id<BWGCommands> handler) {
  bwg_commands_handler_ = handler;
}

#pragma mark - WebStateObserver

void BwgTabHelper::WasShown(web::WebState* web_state) {
  if (is_bwg_session_active_) {
    [bwg_commands_handler_ startBWGFlow];
  }
}

void BwgTabHelper::WasHidden(web::WebState* web_state) {
  if (is_bwg_session_active_) {
    [bwg_commands_handler_ dismissBWGFlow];
  }
}

void BwgTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state_observation_.Reset();
  CleanupSessionFromPrefs(GetClientId());
  web_state_ = nullptr;
}

#pragma mark - Private

void BwgTabHelper::CreateOrUpdateSessionInPrefs(std::string client_id,
                                                std::string server_id) {
  base::Value::Dict session_info_dict;
  session_info_dict.Set(kServerIDDictKey, server_id);
  session_info_dict.Set(
      kLastInteractionTimestampDictKey,
      static_cast<double>(base::Time::Now().InMillisecondsSinceUnixEpoch()));

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  ScopedDictPrefUpdate update(profile->GetPrefs(), prefs::kBwgSessionMap);
  update->Set(client_id, std::move(session_info_dict));
}

void BwgTabHelper::CleanupSessionFromPrefs(std::string session_id) {
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  ScopedDictPrefUpdate update(profile->GetPrefs(), prefs::kBwgSessionMap);
  update->Remove(session_id);
}
