// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/data/share_to_data_builder.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/send_tab_to_self/entry_point_display_reason.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/find_in_page/abstract_find_tab_helper.h"
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/tabs/tab_title_util.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/chrome_activity_item_thumbnail_generator.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/share_to_data.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "third_party/abseil-cpp/absl/types/optional.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace activity_services {

ShareToData* ShareToDataForWebState(web::WebState* web_state,
                                    const GURL& share_url) {
  DCHECK(web_state);

  BOOL is_original_title = NO;
  DCHECK(web_state->GetNavigationManager());
  web::NavigationItem* last_committed_item =
      web_state->GetNavigationManager()->GetLastCommittedItem();
  if (last_committed_item) {
    // Do not use WebState::GetTitle() as it returns the display title, not the
    // original page title.
    const std::u16string& original_title = last_committed_item->GetTitle();
    if (!original_title.empty()) {
      // If the original page title exists, it is expected to match the Tab's
      // title. If this ever changes, then a decision has to be made on which
      // one should be used for sharing.
      DCHECK([tab_util::GetTabTitle(web_state)
          isEqual:base::SysUTF16ToNSString(original_title)]);
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

  const GURL& finalURLToShare =
      !share_url.is_empty() ? share_url : web_state->GetVisibleURL();
  web::NavigationItem* visibleItem =
      web_state->GetNavigationManager()->GetVisibleItem();
  web::UserAgentType userAgent = web::UserAgentType::NONE;
  if (visibleItem) {
    userAgent = visibleItem->GetUserAgentType();
  }

  auto* helper = GetConcreteFindTabHelperFromWebState(web_state);
  BOOL is_page_searchable =
      (helper && helper->CurrentPageSupportsFindInPage() &&
       !helper->IsFindUIActive());
  NSString* tab_title = tab_util::GetTabTitle(web_state);

  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(web_state->GetBrowserState());
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(browser_state);
  // Divergence between iOS and other platforms: today the sign-in promo UI only
  // supports the case where there are already accounts on the device.
  BOOL can_send_tab_to_self =
      !browser_state->IsOffTheRecord() && accountManagerService &&
      accountManagerService->HasIdentities() &&
      send_tab_to_self::GetEntryPointDisplayReason(
          finalURLToShare,
          SyncServiceFactory::GetForBrowserState(browser_state),
          SendTabToSelfSyncServiceFactory::GetForBrowserState(browser_state),
          browser_state->GetPrefs())
          .has_value();

  return [[ShareToData alloc] initWithShareURL:finalURLToShare
                                    visibleURL:web_state->GetVisibleURL()
                                         title:tab_title
                                additionalText:nil
                               isOriginalTitle:is_original_title
                               isPagePrintable:is_page_printable
                              isPageSearchable:is_page_searchable
                              canSendTabToSelf:can_send_tab_to_self
                                     userAgent:userAgent
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
