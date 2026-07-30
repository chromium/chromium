// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sharing/ui_bundled/activity_services/data/share_to_data_builder.h"

#import <LinkPresentation/LinkPresentation.h>

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/send_tab_to_self/entry_point_display_reason.h"
#import "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#import "ios/chrome/browser/find_in_page/model/find_tab_helper.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/data/chrome_activity_item_thumbnail_generator.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/data/share_to_data.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/tabs/model/tab_title_util.h"
#import "ios/web/public/favicon/favicon_status.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

namespace activity_services {

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
      share_url.is_valid() ? share_url : web_state->GetVisibleURL();
  web::NavigationItem* visible_item =
      web_state->GetNavigationManager()->GetVisibleItem();
  web::UserAgentType user_agent = web::UserAgentType::NONE;
  if (visible_item) {
    user_agent = visible_item->GetUserAgentType();
  }

  BOOL in_reader_mode = YES;
  auto* reader_mode_tab_helper = ReaderModeTabHelper::FromWebState(web_state);
  in_reader_mode = reader_mode_tab_helper && reader_mode_tab_helper->IsActive();
  FindTabHelper* find_tab_helper = FindTabHelper::FromWebState(web_state);
  BOOL is_page_searchable =
      !in_reader_mode &&
      (find_tab_helper && find_tab_helper->CurrentPageSupportsFindInPage() &&
       !find_tab_helper->IsFindUIActive());
  NSString* tab_title = tab_util::GetTabTitle(web_state);

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state->GetBrowserState());
  send_tab_to_self::SendTabToSelfSyncService* send_tab_to_self_service =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile);
  BOOL can_send_tab_to_self =
      send_tab_to_self_service &&
      send_tab_to_self_service->GetEntryPointDisplayReason(final_url_to_share);
  LPLinkMetadata* metadata = [[LPLinkMetadata alloc] init];
  metadata.URL = net::NSURLWithGURL(final_url_to_share);
  metadata.title = tab_title;
  metadata.originalURL = net::NSURLWithGURL(web_state->GetVisibleURL());
  if (!web_state->GetBrowserState()->IsOffTheRecord()) {
    UIImage* thumbnail = SnapshotTabHelper::FromWebState(web_state)
                             ->GenerateSnapshotWithoutOverlays();
    if (thumbnail) {
      metadata.imageProvider =
          [[NSItemProvider alloc] initWithObject:thumbnail];
    }
  }
  web::FaviconStatus favicon_status = web_state->GetFaviconStatus();
  if (favicon_status.valid) {
    metadata.iconProvider = [[NSItemProvider alloc]
        initWithObject:favicon_status.image.ToUIImage()];
  }
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
                                  linkMetadata:metadata];
}

ShareToData* ShareToDataForURL(const GURL& url,
                               NSString* title,
                               NSString* additional_text,
                               LPLinkMetadata* link_metadata) {
  return [[ShareToData alloc] initWithShareURL:url
                                    visibleURL:url
                                         title:title
                                additionalText:additional_text
                               isOriginalTitle:YES
                               isPagePrintable:NO
                              isPageSearchable:NO
                              canSendTabToSelf:NO
                                     userAgent:web::UserAgentType::NONE
                            thumbnailGenerator:nil
                                  linkMetadata:link_metadata];
}

ShareToData* ShareToDataForURLWithTitle(URLWithTitle* url_with_title) {
  return ShareToDataForURL(url_with_title.URL, url_with_title.title, nil, nil);
}

}  // namespace activity_services
