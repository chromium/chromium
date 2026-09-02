// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_tab_card_label_data.h"

#import "base/strings/utf_string_conversions.h"
#import "components/send_tab_to_self/features.h"
#import "components/send_tab_to_self/metrics_util.h"
#import "components/send_tab_to_self/send_tab_to_self_model.h"
#import "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The duration after which the tab card label expires and is no longer shown.
constexpr base::TimeDelta kLabelExpirationDelay = base::Days(5);

// The maximum acceptable delay between a sync entry being marked as opened
// and the corresponding WebState being created.
constexpr base::TimeDelta kCreationTolerance = base::Seconds(2);

// Returns true if `timestamp` is older than `kLabelExpirationDelay`.
bool IsTimestampExpired(base::Time timestamp) {
  return base::Time::Now() - timestamp > kLabelExpirationDelay;
}

// Returns the SendTabToSelfModel for the given `web_state` if it exists.
// Returns nullptr otherwise.
send_tab_to_self::SendTabToSelfModel* GetSendTabToSelfModel(
    web::WebState* web_state) {
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  send_tab_to_self::SendTabToSelfSyncService* service =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile);
  if (!service) {
    return nullptr;
  }
  send_tab_to_self::SendTabToSelfModel* model =
      service->GetSendTabToSelfModel();
  return model;
}

// Returns the SendTabToSelfEntry corresponding to `web_state` using URL-based
// matching if it is considered to have never been viewed by the user. Returns
// nullptr if there exists no such entry.
const send_tab_to_self::SendTabToSelfEntry* GetUnviewedMatchingEntry(
    web::WebState* web_state,
    send_tab_to_self::SendTabToSelfModel* model) {
  // TODO(crbug.com/488072250): This current logic to match tabs with STTS
  // entries will fail if the tab URL doesn't exactly match the entry URL (e.g.
  // if a redirect happened). Maybe attach the GUID to the persisted webstate.
  const GURL& url = web_state->GetLastCommittedURL();
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return nullptr;
  }

  const base::Time creation_time = web_state->GetCreationTime();
  const base::Time last_active_time = web_state->GetLastActiveTime();

  if (last_active_time > creation_time) {
    // Implies the tab has been viewed by the user.
    // Note that `creation_time` can be greater than `last_active_time` since
    // creation time is reset to the current time upon browser restart.
    return nullptr;
  }

  // Find the sync entry (if any) that matches this tab's URL and creation time.
  for (const send_tab_to_self::SendTabToSelfEntry* entry :
       model->GetOpenedEntriesTargetedToLocalDevice()) {
    if (entry->GetURL() != url) {
      continue;
    }

    const base::Time opened_time = entry->GetOpenedTime();
    if (!opened_time.is_null() &&
        // Note: `last_active_time` is used here since `creation_time` is reset
        // upon browser restart. Edge case: The user switches the newly opened
        // tab within `kCreationTolerance` and does a restart. Upon browser
        // startup, if the tab switcher gets opened, the label will still be
        // visible.
        (last_active_time - opened_time).magnitude() <= kCreationTolerance) {
      return entry;
    }
  }
  return nullptr;
}

}  // namespace

SendTabToSelfTabCardLabelData::SendTabToSelfTabCardLabelData(
    web::WebState* web_state,
    const std::string& entry_guid,
    const std::string& sender_device_name,
    base::Time creation_time)
    : entry_guid_(entry_guid),
      sender_device_name_(base::UTF8ToUTF16(sender_device_name)),
      creation_time_(creation_time) {
  // Registers as an observer to detect when the tab is shown to the user.
  web_state_observation_.Observe(web_state);
}

SendTabToSelfTabCardLabelData::~SendTabToSelfTabCardLabelData() = default;

// static
SendTabToSelfTabCardLabelData* SendTabToSelfTabCardLabelData::FromWebState(
    web::WebState* web_state) {
  if (!web_state) {
    return nullptr;
  }
  SendTabToSelfTabCardLabelData* data =
      static_cast<SendTabToSelfTabCardLabelData*>(
          web_state->GetUserData(UserDataKey()));
  if (data && IsTimestampExpired(data->creation_time_)) {
    RemoveFromWebState(web_state);
    return nullptr;
  }
  return data;
}

// static
NSString* SendTabToSelfTabCardLabelData::GetLabelTextForWebState(
    web::WebState* web_state) {
  if (SendTabToSelfTabCardLabelData* label_data = FromWebState(web_state)) {
    return GetLabelText(label_data->sender_device_name_);
  }

  if (!base::FeatureList::IsEnabled(send_tab_to_self::kSendTabToSelfAutoOpen)) {
    return nil;
  }

  send_tab_to_self::SendTabToSelfModel* model =
      GetSendTabToSelfModel(web_state);
  if (!model || !model->IsReady()) {
    return nil;
  }

  const send_tab_to_self::SendTabToSelfEntry* entry =
      GetUnviewedMatchingEntry(web_state, model);
  if (!entry || IsTimestampExpired(entry->GetOpenedTime())) {
    return nil;
  }

  // If the WebState is realized, attach the UserData to observe `WasShown` and
  // optimize future calls.
  if (web_state->IsRealized()) {
    CreateForWebState(web_state, entry->GetGUID(), entry->GetDeviceName(),
                      entry->GetOpenedTime());
  }

  return GetLabelText(base::UTF8ToUTF16(entry->GetDeviceName()));
}

// static
NSString* SendTabToSelfTabCardLabelData::GetLabelText(
    const std::u16string& device_name) {
  return l10n_util::GetNSStringF(
      IDS_SEND_TAB_TO_SELF_INFOBAR_AUTO_OPEN_SUBTITLE, device_name);
}

void SendTabToSelfTabCardLabelData::WebStateClosedByUser(
    web::WebState* web_state) {
  // Logs abandonment because the user closed the tab without activating it.
  MarkEntryActivated(web_state, send_tab_to_self::ShareActivatedEntryPoint::
                                    kTabOrBrowserClosedWithoutActivation);
}

void SendTabToSelfTabCardLabelData::MarkEntryActivated(
    web::WebState* web_state,
    send_tab_to_self::ShareActivatedEntryPoint entry_point) {
  // Retrieves the model and marks the entry as activated/interacted with,
  // which will trigger the appropriate telemetry logging.
  send_tab_to_self::SendTabToSelfModel* model =
      GetSendTabToSelfModel(web_state);
  if (model) {
    model->MarkEntryActivated(entry_guid_, entry_point);
  }
}

#pragma mark - web::WebStateObserver

void SendTabToSelfTabCardLabelData::WasShown(web::WebState* web_state) {
  MarkEntryActivated(web_state,
                     send_tab_to_self::ShareActivatedEntryPoint::kTabStrip);
  // Deletes itself since the tab is now viewed.
  RemoveFromWebState(web_state);
}

void SendTabToSelfTabCardLabelData::WebStateDestroyed(
    web::WebState* web_state) {
  // Must reset the observation when the WebState is destroyed, to prevent the
  // destructor from attempting to remove the observer from a dying WebState.
  web_state_observation_.Reset();
}
