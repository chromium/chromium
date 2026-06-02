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
#import "components/shared_highlighting/core/common/text_fragment.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_load_navigation_user_data.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_text_fragment_selector_generator.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_util.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
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

// Attempts to perform scroll restoration for the given `entry` if a text
// fragment is present.
void MaybeRestoreScrollPosition(
    web::WebState* web_state,
    const send_tab_to_self::SendTabToSelfEntry* entry) {
  if (!base::FeatureList::IsEnabled(
          send_tab_to_self::kSendTabToSelfPropagateScrollPosition)) {
    return;
  }

  const send_tab_to_self::TextFragmentData& fragment_data =
      entry->GetPageContext().scroll_position.text_fragment;
  if (fragment_data.IsEmpty()) {
    return;
  }

  std::string fragment_string =
      fragment_data.ToSharedHighlightingTextFragment().ToEscapedString(
          shared_highlighting::TextFragment::EscapedStringFormat::
              kWithoutTextDirective);
  if (!fragment_string.empty()) {
    SendTabToSelfTextFragmentSelectorGenerator::GetInstance()
        ->ScrollToTextFragment(web_state, fragment_string);
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

  const send_tab_to_self::SendTabToSelfEntry* entry = GetEntry(web_state, guid);
  if (entry) {
    MaybeRestoreScrollPosition(web_state, entry);
    MaybePerformFormFilling(web_state, entry);
  }
}

void SendTabToSelfTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state_observation_.Reset();
}
