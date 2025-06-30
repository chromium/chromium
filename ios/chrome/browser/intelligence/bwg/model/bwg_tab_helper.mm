// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"

#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
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

void BwgTabHelper::SetBwgCommandsHandler(id<BWGCommands> handler) {
  bwg_commands_handler_ = handler;
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

  // TODO(crbug.com/419070203): Refactor these pref keys into a constants file.
  std::optional<int> creation_timestamp =
      current_session_dict->FindInt("last_interaction_timestamp");
  const std::string* server_id = current_session_dict->FindString("server_id");
  if (!creation_timestamp || !server_id) {
    return std::nullopt;
  }

  // Return the serverID if it hasn't yet expired, otherwise clean it up.
  // TODO(crbug.com/424264708): Make the expiration time Finchable.
  int64_t latest_valid_timestamp =
      base::Time::Now().InMillisecondsSinceUnixEpoch() -
      base::Minutes(30).InMilliseconds();
  if (*creation_timestamp > latest_valid_timestamp) {
    return *server_id;
  } else {
    ScopedDictPrefUpdate update(profile->GetPrefs(), prefs::kBwgSessionMap);
    update->Remove(unique_identifier_string);
    return std::nullopt;
  }
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
  web_state_ = nullptr;
  // TODO(crbug.com/419070203): Cleanup session from prefs.
}
