// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_tab_helper.h"

#import <memory>

#import "base/feature_list.h"
#import "components/send_tab_to_self/features.h"
#import "components/send_tab_to_self/send_tab_to_self_entry.h"
#import "components/send_tab_to_self/send_tab_to_self_model.h"
#import "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_load_navigation_user_data.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_text_fragment_selector_generator.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_util.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "url/origin.h"

namespace {

// Returns the SendTabToSelfEntry with the given `guid`, or nullptr if not
// found.
const send_tab_to_self::SendTabToSelfEntry* GetEntry(web::WebState* web_state,
                                                     const std::string& guid) {
  if (guid.empty()) {
    return nullptr;
  }

  web::BrowserState* browser_state = web_state->GetBrowserState();
  if (!browser_state) {
    return nullptr;
  }

  ProfileIOS* profile = ProfileIOS::FromBrowserState(browser_state);
  send_tab_to_self::SendTabToSelfSyncService* sync_service =
      profile ? SendTabToSelfSyncServiceFactory::GetForProfile(profile)
              : nullptr;
  send_tab_to_self::SendTabToSelfModel* model =
      sync_service ? sync_service->GetSendTabToSelfModel() : nullptr;
  return model ? model->GetEntryByGUID(guid) : nullptr;
}

// Attempts to perform scroll restoration for the given `item` if a text
// fragment is present.
// TODO(crbug.com/485145029): Fetch the text fragment directly from the STTS
// entry in the model rather than attaching it to the navigation item.
void MaybeRestoreScrollPosition(web::WebState* web_state,
                                web::NavigationItem* item) {
  if (!base::FeatureList::IsEnabled(
          send_tab_to_self::kSendTabToSelfPropagateScrollPosition)) {
    return;
  }

  const std::optional<std::string>& fragment =
      item->GetInternalScrollToTextFragment();
  if (fragment.has_value() && !fragment.value().empty()) {
    SendTabToSelfTextFragmentSelectorGenerator::GetInstance()
        ->ScrollToTextFragment(web_state, fragment.value());
  }
}

// Attempts to perform form filling for the given `entry` if it contains
// form data and the feature is enabled.
void MaybePerformFormFilling(
    web::WebState* web_state,
    const send_tab_to_self::SendTabToSelfEntry* entry) {
  if (base::FeatureList::IsEnabled(
          send_tab_to_self::kSendTabToSelfPropagateFormFields)) {
    send_tab_to_self::FillWebState(web_state,
                                   url::Origin::Create(entry->GetURL()),
                                   entry->GetPageContext());
  }
}

}  // namespace

SendTabToSelfTabHelper::SendTabToSelfTabHelper(web::WebState* web_state) {
  CHECK(base::FeatureList::IsEnabled(
            send_tab_to_self::kSendTabToSelfPropagateScrollPosition) ||
        base::FeatureList::IsEnabled(
            send_tab_to_self::kSendTabToSelfPropagateFormFields));
  web_state_observation_.Observe(web_state);
}

SendTabToSelfTabHelper::~SendTabToSelfTabHelper() = default;

void SendTabToSelfTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  SendTabToSelfLoadNavigationUserData* user_data =
      SendTabToSelfLoadNavigationUserData::FromWebState(web_state);
  if (!user_data) {
    return;
  }

  switch (load_completion_status) {
    case web::PageLoadCompletionStatus::SUCCESS:
      break;
    case web::PageLoadCompletionStatus::FAILURE:
      // Do not remove the STTS user data on failure so that it can be retried
      // on subsequent reloads or successful navigations.
      return;
  }

  std::string guid = user_data->entry_guid();

  // Remove the tag immediately so it doesn't trigger again on subsequent
  // reloads of the same page, even if subsequent steps fail or return early.
  SendTabToSelfLoadNavigationUserData::RemoveFromWebState(web_state);

  web::NavigationManager* navigation_manager =
      web_state->GetNavigationManager();
  web::NavigationItem* item = navigation_manager->GetLastCommittedItem();
  if (!item) {
    return;
  }

  MaybeRestoreScrollPosition(web_state, item);

  const send_tab_to_self::SendTabToSelfEntry* entry = GetEntry(web_state, guid);
  if (entry) {
    MaybePerformFormFilling(web_state, entry);
  }

  // Clear the fragment so that reloading the page doesn't trigger the
  // scroll logic again.
  item->SetInternalScrollToTextFragment(std::nullopt);
}

void SendTabToSelfTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state_observation_.Reset();
}
