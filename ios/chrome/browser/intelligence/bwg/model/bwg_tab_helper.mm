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
#import "ios/chrome/browser/intelligence/bwg/model/bwg_snapshot_utils.h"
#import "ios/chrome/browser/intelligence/bwg/utils/bwg_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
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
      current_session_dict->FindDouble(kLastInteractionTimestampDictKey);
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

  // The UI was foregrounded, so it can no longer be active in the background.
  if (is_bwg_ui_showing_) {
    is_bwg_session_active_in_background_ = false;
  }

  // UI was hidden but the session is not active, so update the snapshot to
  // remove the overlay from it.
  if (!is_bwg_ui_showing_ && !is_bwg_session_active_in_background_) {
    cached_snapshot_ = nil;
  }
}

bool BwgTabHelper::GetIsBwgSessionActiveInBackground() {
  return is_bwg_session_active_in_background_;
}

bool BwgTabHelper::ShouldShowZeroState() {
  std::optional<std::string> last_interaction_url = GetURLOnLastInteraction();
  // Show zero-state if no last interaction URL was found.
  if (!last_interaction_url.has_value()) {
    return true;
  }

  // Show zero-state if the last interaction URL is different from the current
  // one.
  return !web_state_->GetVisibleURL().EqualsIgnoringRef(
      GURL(last_interaction_url.value()));
}

bool BwgTabHelper::ShouldShowSuggestionChips() {
  // Show suggestion chips if we should show zero-state and we're not currently
  // on a Search results page.
  return ShouldShowZeroState() &&
         !google_util::IsGoogleSearchUrl(web_state_->GetVisibleURL());
}

void BwgTabHelper::CreateOrUpdateBwgSessionInStorage(std::string server_id) {
  CreateOrUpdateSessionInPrefs(GetClientId(), server_id);
}

void BwgTabHelper::DeleteBwgSessionInStorage() {
  CleanupSessionFromPrefs(GetClientId());
}

void BwgTabHelper::PrepareBwgFreBackgrounding() {
  cached_snapshot_ =
      bwg_snapshot_utils::GetCroppedFullscreenSnapshot(web_state_->GetView());
  is_bwg_session_active_in_background_ = true;
}

std::string BwgTabHelper::GetClientId() {
  return base::NumberToString(web_state_->GetUniqueIdentifier().identifier());
}

std::optional<std::string> BwgTabHelper::GetServerId() {
  if (IsGeminiCrossTabEnabled()) {
    PrefService* pref_service =
        ProfileIOS::FromBrowserState(web_state_->GetBrowserState())->GetPrefs();
    base::Time last_interaction_timestamp =
        pref_service->GetTime(prefs::kLastGeminiInteractionTimestamp);
    const std::string server_id =
        pref_service->GetString(prefs::kGeminiConversationId);
    if (base::Time::Now() - last_interaction_timestamp <
        BWGSessionValidityDuration()) {
      if (!server_id.empty()) {
        return server_id;
      }
    }
  } else {
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
  }
  return std::nullopt;
}

void BwgTabHelper::SetBwgCommandsHandler(id<BWGCommands> handler) {
  bwg_commands_handler_ = handler;
}

#pragma mark - WebStateObserver

void BwgTabHelper::WasShown(web::WebState* web_state) {
  if (is_bwg_session_active_in_background_) {
    [bwg_commands_handler_
        startBWGFlowWithEntryPoint:bwg::EntryPoint::TabReopen];
    cached_snapshot_ = nil;
  }
}

void BwgTabHelper::WasHidden(web::WebState* web_state) {
  if (is_bwg_ui_showing_) {
    cached_snapshot_ =
        bwg_snapshot_utils::GetCroppedFullscreenSnapshot(web_state_->GetView());
    is_bwg_session_active_in_background_ = true;
    [bwg_commands_handler_ dismissBWGFlowWithCompletion:nil];
  }

  UpdateWebStateSnapshotInStorage();
}

void BwgTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  bool floaty_shown = profile->GetPrefs()->GetBoolean(prefs::kIOSBwgConsent);
  bool bwg_promo_shown =
      profile->GetPrefs()->GetInteger(prefs::kIOSBWGPromoImpressionCount) > 0;
  if (!floaty_shown && !bwg_promo_shown) {
    [bwg_commands_handler_ showBWGPromoIfPageIsEligible];
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

  if (IsGeminiCrossTabEnabled()) {
    PrefService* pref_service =
        ProfileIOS::FromBrowserState(web_state_->GetBrowserState())->GetPrefs();
    pref_service->SetTime(prefs::kLastGeminiInteractionTimestamp,
                          base::Time::Now());
    pref_service->SetString(prefs::kGeminiConversationId, server_id);
  } else {
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
}

void BwgTabHelper::CleanupSessionFromPrefs(std::string session_id) {
  if (IsGeminiCrossTabEnabled()) {
    // TODO(crbug.com/436012307): Once this launches, remove `session_id` from
    // method.
    PrefService* pref_service =
        ProfileIOS::FromBrowserState(web_state_->GetBrowserState())->GetPrefs();
    pref_service->ClearPref(prefs::kGeminiConversationId);
    return;
  }

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

void BwgTabHelper::UpdateWebStateSnapshotInStorage() {
  SnapshotTabHelper* snapshot_tab_helper =
      SnapshotTabHelper::FromWebState(web_state_);

  if (!snapshot_tab_helper) {
    return;
  }

  if (cached_snapshot_) {
    snapshot_tab_helper->UpdateSnapshotStorageWithImage(cached_snapshot_);
  } else {
    snapshot_tab_helper->UpdateSnapshotWithCallback(nil);
  }
}
