// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/data/share_to_data_builder.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/send_tab_to_self/entry_point_display_reason.h"
#import "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#import "ios/chrome/browser/find_in_page/model/abstract_find_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/tabs/model/tab_title_util.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/chrome_activity_item_thumbnail_generator.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/share_to_data.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "third_party/abseil-cpp/absl/types/optional.h"
#import "url/gurl.h"

namespace activity_services {

// TODO(crbug.com/1468530): Adopt consistent casing in these functions.

ShareToData* ShareToDataForWebState(web::WebState* web_state,
                                    const GURL& share_url) {
  CHECK(web_state);

  BOOL is_original_title = NO;
  CHECK(web_state->GetNavigationManager());
  web::NavigationItem* last_committed_item =
      web_state->GetNavigationManager()->GetLastCommittedItem();
  if (last_committed_item) {
    // Do not use WebState::GetTitle() as it returns the display title, not the
    // original page title.
    const std::u16string& original_title = last_committed_item->GetTitle();
    if (!original_title.empty()) {
      is_original_title = YES;
    }
  }

  BOOL is_page_printable = [web_state->GetView() viewPrintFormatter] != nil;

  // Thumbnail should not be generated for incognito tabs.
  ChromeActivityItemThumbnailGenerator* thumbnail_generator =
      web_state->GetBrowserState()->IsOffTheRecord()
          ? nil
          : [[ChromeActivityItemThumbnailGenerator alloc]
                initWithWebState:web_state];

  const GURL& final_url_to_share =
      !share_url.is_empty() ? share_url : web_state->GetVisibleURL();
  web::NavigationItem* visible_item =
      web_state->GetNavigationManager()->GetVisibleItem();
  web::UserAgentType user_agent = web::UserAgentType::NONE;
  if (visible_item) {
    user_agent = visible_item->GetUserAgentType();
  }

  auto* helper = GetConcreteFindTabHelperFromWebState(web_state);
  BOOL is_page_searchable =
      (helper && helper->CurrentPageSupportsFindInPage() &&
       !helper->IsFindUIActive());
  NSString* tab_title = tab_util::GetTabTitle(web_state);

  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(web_state->GetBrowserState());
  ChromeAccountManagerService* account_manager_service =
      ChromeAccountManagerServiceFactory::GetForBrowserState(browser_state);
  send_tab_to_self::SendTabToSelfSyncService* send_tab_to_self_service =
      SendTabToSelfSyncServiceFactory::GetForBrowserState(browser_state);
  // When there are no device-level accounts, it's only possible to show the
  // promo UI if IsConsistencyNewAccountInterfaceEnabled() is true.
  BOOL can_send_tab_to_self =
      account_manager_service &&
      (account_manager_service->HasIdentities() ||
       IsConsistencyNewAccountInterfaceEnabled()) &&
      send_tab_to_self_service &&
      send_tab_to_self_service->GetEntryPointDisplayReason(final_url_to_share);

  return [[ShareToData alloc] initWithShareURL:final_url_to_share
                                    visibleURL:web_state->GetVisibleURL()
                                         title:tab_title
                                additionalText:nil
                               isOriginalTitle:is_original_title
                               isPagePrintable:is_page_printable
                              isPageSearchable:is_page_searchable
                              canSendTabToSelf:can_send_tab_to_self
                                     userAgent:user_agent
                            thumbnailGenerator:thumbnail_generator
                                  linkMetadata:nil];
}

ShareToData* ShareToDataForURL(const GURL& URL,
                               NSString* title,
                               NSString* additionalText,
                               LPLinkMetadata* linkMetadata) {
  return [[ShareToData alloc] initWithShareURL:URL
                                    visibleURL:URL
                                         title:title
                                additionalText:additionalText
                               isOriginalTitle:YES
                               isPagePrintable:NO
                              isPageSearchable:NO
                              canSendTabToSelf:NO
                                     userAgent:web::UserAgentType::NONE
                            thumbnailGenerator:nil
                                  linkMetadata:linkMetadata];
}

ShareToData* ShareToDataForURLWithTitle(URLWithTitle* URLWithTitle) {
  return ShareToDataForURL(URLWithTitle.URL, URLWithTitle.title, nil, nil);
}

}  // namespace activity_services
