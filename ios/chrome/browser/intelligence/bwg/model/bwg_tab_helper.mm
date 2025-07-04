// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/google/core/common/google_util.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/utils/bwg_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "ios/web/util/content_type_util.h"
#import "url/gurl.h"

namespace {

// Gets the session dictionary for `cliend_id` from `profile`'s prefs, if the
// session is not expired.
std::optional<const base::Value::Dict*> GetSessionDictFromPrefs(
    std::string client_id,
    ProfileIOS* profile) {
  const base::Value::Dict& sessions_map =
      profile->GetPrefs()->GetDict(prefs::kBwgSessionMap);
  if (sessions_map.empty()) {
    return std::nullopt;
  }

  const base::Value::Dict* current_session_dict =
      sessions_map.FindDict(client_id);
  if (!current_session_dict) {
    return std::nullopt;
  }

  std::optional<double> creation_timestamp =
      current_session_dict->FindInt(kLastInteractionTimestampDictKey);
  if (!creation_timestamp) {
    return std::nullopt;
  }

  // Return the session dict if it hasn't yet expired.
  int64_t latest_valid_timestamp =
      base::Time::Now().InMillisecondsSinceUnixEpoch() -
      BWGSessionValidityDuration().InMilliseconds();
  if (*creation_timestamp > latest_valid_timestamp) {
    return current_session_dict;
  }

  return std::nullopt;
}

}  // namespace

BwgTabHelper::BwgTabHelper(web::WebState* web_state) : web_state_(web_state) {
  web_state_observation_.Observe(web_state);
}

BwgTabHelper::~BwgTabHelper() {}

void BwgTabHelper::SetBwgUiShowing(bool showing) {
  is_bwg_ui_showing_ = showing;

  if (is_bwg_ui_showing_) {
    is_bwg_session_active_in_background_ = false;
  }
}

bool BwgTabHelper::GetIsBwgSessionActiveInBackground() {
  return is_bwg_session_active_in_background_;
}

bool BwgTabHelper::ShouldShowZeroState() {
  bool is_srp = google_util::IsGoogleSearchUrl(web_state_->GetVisibleURL());

  std::optional<std::string> last_interaction_url = GetURLOnLastInteraction();
  if (!last_interaction_url.has_value()) {
    return !is_srp;
  }

  return !is_srp && !web_state_->GetVisibleURL().EqualsIgnoringRef(
                        GURL(last_interaction_url.value()));
}

void BwgTabHelper::CreateOrUpdateBwgSessionInStorage(std::string server_id) {
  CreateOrUpdateSessionInPrefs(GetClientId(), server_id);
}

void BwgTabHelper::DeleteBwgSessionInStorage() {
  CleanupSessionFromPrefs(GetClientId());
}

bool BwgTabHelper::IsBwgAvailableForWebState() {
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());

  // The BWG Service determines whether the profile is eligible.
  BwgService* bwg_service = BwgServiceFactory::GetForProfile(profile);
  const bool is_profile_eligible =
      !profile->IsOffTheRecord() && bwg_service->IsEligibleForBwg();

  // The web state is eligible for HTML and images that use http/https schemas,
  const GURL& url = web_state_->GetVisibleURL();
  const std::string mime_type = web_state_->GetContentsMimeType();
  const BOOL is_web_state_eligible =
      url.SchemeIsHTTPOrHTTPS() &&
      (web::IsContentTypeHtml(mime_type) || web::IsContentTypeImage(mime_type));

  return is_profile_eligible && is_web_state_eligible;
}

std::string BwgTabHelper::GetClientId() {
  return base::NumberToString(web_state_->GetUniqueIdentifier().identifier());
}

std::optional<std::string> BwgTabHelper::GetServerId() {
  std::optional<const base::Value::Dict*> session_dict =
      GetSessionDictFromPrefs(
          GetClientId(),
          ProfileIOS::FromBrowserState(web_state_->GetBrowserState()));
  if (!session_dict.has_value()) {
    return std::nullopt;
  }

  const std::string* server_id =
      session_dict.value()->FindString(kServerIDDictKey);
  if (server_id) {
    return *server_id;
  }

  return std::nullopt;
}

void BwgTabHelper::SetBwgCommandsHandler(id<BWGCommands> handler) {
  bwg_commands_handler_ = handler;
}

#pragma mark - WebStateObserver

void BwgTabHelper::WasShown(web::WebState* web_state) {
  if (is_bwg_session_active_in_background_) {
    [bwg_commands_handler_ startBWGFlow];
  }
}

void BwgTabHelper::WasHidden(web::WebState* web_state) {
  if (is_bwg_ui_showing_) {
    is_bwg_session_active_in_background_ = true;
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
  if (client_id.empty() || server_id.empty()) {
    return;
  }

  base::Value::Dict session_info_dict;
  session_info_dict.Set(kServerIDDictKey, server_id);
  session_info_dict.Set(
      kLastInteractionTimestampDictKey,
      static_cast<double>(base::Time::Now().InMillisecondsSinceUnixEpoch()));
  session_info_dict.Set(kURLOnLastInteractionDictKey,
                        web_state_->GetVisibleURL().spec());

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  ScopedDictPrefUpdate update(profile->GetPrefs(), prefs::kBwgSessionMap);
  update->Set(client_id, std::move(session_info_dict));
}

void BwgTabHelper::CleanupSessionFromPrefs(std::string session_id) {
  if (session_id.empty()) {
    return;
  }

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  ScopedDictPrefUpdate update(profile->GetPrefs(), prefs::kBwgSessionMap);
  update->Remove(session_id);
}

std::optional<std::string> BwgTabHelper::GetURLOnLastInteraction() {
  std::optional<const base::Value::Dict*> session_dict =
      GetSessionDictFromPrefs(
          GetClientId(),
          ProfileIOS::FromBrowserState(web_state_->GetBrowserState()));
  if (!session_dict.has_value()) {
    return std::nullopt;
  }

  const std::string* last_interaction_url =
      session_dict.value()->FindString(kURLOnLastInteractionDictKey);
  if (last_interaction_url) {
    return *last_interaction_url;
  }

  return std::nullopt;
}
