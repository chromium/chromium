// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/offline_page_native_content.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/reading_list/offline_url_utils.h"
#include "ios/chrome/browser/reading_list/reading_list_download_service.h"
#include "ios/chrome/browser/reading_list/reading_list_download_service_factory.h"
#import "ios/chrome/browser/ui/static_content/static_html_view_controller.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/navigation/reload_type.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface OfflinePageNativeContent ()
// Restores the last committed item to its initial state.
- (void)restoreOnlineURL;
@end

@implementation OfflinePageNativeContent {
  // The virtual URL that will be displayed to the user.
  GURL _virtualURL;

  // The URL of the Reading List entry that is being displayed..
  GURL _entryURL;

  // The WebState of the current tab.
  web::WebState* _webState;

  // A guard tracking if |restoreOnlineURL| has been called to avoid calling
  // it twice.
  BOOL _restored;
}

- (instancetype)initWithBrowserState:(web::BrowserState*)browserState
                            webState:(web::WebState*)webState
                                 URL:(const GURL&)URL {
  DCHECK(browserState);
  DCHECK(URL.is_valid());

  base::FilePath offline_root =
      ReadingListDownloadServiceFactory::GetForBrowserState(
          ios::ChromeBrowserState::FromBrowserState(browserState))
          ->OfflineRoot();

  _webState = webState;
  GURL resourcesRoot;
  GURL fileURL =
      reading_list::FileURLForDistilledURL(URL, offline_root, &resourcesRoot);

  StaticHtmlViewController* HTMLViewController =
      [[StaticHtmlViewController alloc] initWithFileURL:fileURL
                                allowingReadAccessToURL:resourcesRoot
                                           browserState:browserState];
  _entryURL = reading_list::EntryURLForOfflineURL(URL);
  _virtualURL = reading_list::VirtualURLForOfflineURL(URL);

  return [super initWithStaticHTMLViewController:HTMLViewController URL:URL];
}

- (void)willBeDismissed {
  [self restoreOnlineURL];
  [super willBeDismissed];
}

- (void)close {
  [self restoreOnlineURL];
  [super close];
}

- (const GURL&)virtualURL {
  return _virtualURL;
}

- (void)reload {
  if (!_entryURL.is_valid()) {
    // If entryURL is not valid, the restoreOnlineURL will fail and the |reload|
    // will be called in a loop. Early return here.
    return;
  }
  [self restoreOnlineURL];
  _webState->GetNavigationManager()->Reload(web::ReloadType::NORMAL,
                                            false /*check_for_repost*/);
}

- (void)restoreOnlineURL {
  if (_restored) {
    return;
  }
  _restored = YES;
  web::NavigationItem* item =
      _webState->GetNavigationManager()->GetLastCommittedItem();
  DCHECK(item && item->GetVirtualURL() == [self virtualURL]);
  item->SetURL(_entryURL);
  item->SetVirtualURL(GURL::EmptyGURL());
}

@end
