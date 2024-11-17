// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_about_this_site_mediator.h"

#import "base/memory/raw_ptr.h"
#import "components/page_info/core/about_this_site_service.h"
#import "components/strings/grit/components_strings.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/page_info/about_this_site_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/ui/page_info/page_info_about_this_site_consumer.h"
#import "ios/chrome/browser/ui/page_info/page_info_about_this_site_info.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ui/base/l10n/l10n_util.h"

@implementation PageInfoAboutThisSiteMediator {
  raw_ptr<web::WebState> _webState;
  raw_ptr<page_info::AboutThisSiteService> _service;
}

- (instancetype)initWithWebState:(web::WebState*)webState
                         service:(page_info::AboutThisSiteService*)service {
  self = [super init];
  if (self) {
    _webState = webState;
    _service = service;
  }
  return self;
}

#pragma mark - Properties

- (void)setConsumer:(id<PageInfoAboutThisSiteConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  [self dispatchAboutThisSiteInfo];
}

#pragma mark - Private methods

// Dispatches AboutThisSite information to the `consumer` for the `webState`.
- (void)dispatchAboutThisSiteInfo {
  if (!self.consumer) {
    return;
  }

  if (!_service) {
    return;
  }

  web::NavigationItem* navItem =
      _webState->GetNavigationManager()->GetVisibleItem();
  const GURL& URL = navItem->GetURL();
  std::optional<page_info::proto::SiteInfo> aboutThisPageInfo =
      _service->GetAboutThisSiteInfo(
          URL, ukm::GetSourceIdForWebStateDocument(_webState),
          AboutThisSiteTabHelper::FromWebState(_webState));

  if (aboutThisPageInfo.has_value()) {
    PageInfoAboutThisSiteInfo* info = [[PageInfoAboutThisSiteInfo alloc] init];

    info.summary =
        aboutThisPageInfo->has_description()
            ? [NSString stringWithCString:aboutThisPageInfo->description()
                                              .description()
                                              .c_str()
                                 encoding:NSUTF8StringEncoding]
            : l10n_util::GetNSString(
                  IDS_PAGE_INFO_ABOUT_THIS_PAGE_DESCRIPTION_PLACEHOLDER);
    info.moreAboutURL = GURL(aboutThisPageInfo->more_about().url());

    [self.consumer setAboutThisSiteSection:info];
  }
}

@end
