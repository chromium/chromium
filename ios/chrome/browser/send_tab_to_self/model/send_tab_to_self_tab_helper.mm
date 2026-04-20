// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_tab_helper.h"

#import "base/feature_list.h"
#import "components/send_tab_to_self/features.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_text_fragment_selector_generator.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

SendTabToSelfTabHelper::SendTabToSelfTabHelper(web::WebState* web_state) {
  CHECK(base::FeatureList::IsEnabled(
      send_tab_to_self::kSendTabToSelfPropagateScrollPosition));
  web_state_observation_.Observe(web_state);
}

SendTabToSelfTabHelper::~SendTabToSelfTabHelper() = default;

void SendTabToSelfTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  if (load_completion_status != web::PageLoadCompletionStatus::SUCCESS) {
    return;
  }

  // The text fragment to scroll to is attached to the NavigationItem when
  // the page is initially loaded via Send Tab to Self.
  web::NavigationItem* item =
      web_state->GetNavigationManager()->GetLastCommittedItem();
  if (!item) {
    return;
  }

  const std::optional<std::string>& fragment =
      item->GetInternalScrollToTextFragment();
  if (fragment.has_value() && !fragment.value().empty()) {
    // Inject the javascript to scroll to the parsed text fragment.
    SendTabToSelfTextFragmentSelectorGenerator::GetInstance()
        ->ScrollToTextFragment(web_state, fragment.value());
    // Clear the fragment so that reloading the page doesn't trigger the
    // scroll logic again.
    item->SetInternalScrollToTextFragment(std::nullopt);
  }
}

void SendTabToSelfTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state_observation_.Reset();
}
