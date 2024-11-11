// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/send_tab_to_self/send_tab_promo_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "components/prefs/pref_service.h"
#import "components/send_tab_to_self/pref_names.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/notifications_module_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/send_tab_to_self/send_tab_promo_item.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "url/gurl.h"

@implementation SendTabPromoMediator {
  SendTabPromoItem* _sendTabPromoItem;
  raw_ptr<PrefService> _prefService;
  raw_ptr<FaviconLoader> _faviconLoader;
}

- (instancetype)initWithFaviconLoader:(FaviconLoader*)faviconLoader
                          prefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _faviconLoader = faviconLoader;
    _prefService = prefService;
  }
  return self;
}

- (void)disconnect {
  _faviconLoader = nullptr;
  _prefService = nullptr;
  _sendTabPromoItem = nullptr;
}

- (SendTabPromoItem*)sendTabPromoItemToShow {
  return _sendTabPromoItem;
}

- (void)setDelegate:(id<SendTabPromoMediatorDelegate>)delegate {
  _delegate = delegate;
  if (_delegate) {
    [self latestSendTabToSelfItem];
  }
}

- (void)dismissModule {
  [_delegate removeSendTabPromoModule];
}

#pragma mark - StandaloneModuleDelegate

- (void)buttonTappedForModuleType:(ContentSuggestionsModuleType)moduleType {
  CHECK(moduleType == ContentSuggestionsModuleType::kSendTabPromo);
  base::UmaHistogramBoolean(
      "IOS.Notifications.SendTab.MagicStack.AllowNotificationsPressed", true);
  [self.notificationsDelegate enableNotifications:moduleType];
}

#pragma mark - Private

- (void)latestSendTabToSelfItem {
  std::string tabURL = _prefService->GetString(
      send_tab_to_self::prefs::kIOSSendTabToSelfLastReceivedTabURLPref);
  if (tabURL.size() > 0) {
    [self fetchFaviconForUrl:GURL(tabURL)];
  }
}

// Fetches the favicon for the page at `tabURL`.
- (void)fetchFaviconForUrl:(GURL)tabURL {
  _sendTabPromoItem = nullptr;
  __weak SendTabPromoMediator* weakSelf = self;

  _faviconLoader->FaviconForPageUrl(
      tabURL, kDesiredSmallFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/true, ^(FaviconAttributes* attributes) {
        [weakSelf onFaviconReceived:attributes];
      });
}

// Called when the favicon has been received.
- (void)onFaviconReceived:(FaviconAttributes*)attributes {
  if (_sendTabPromoItem) {
    // Favicon callback has already been executed, update the image and return.
    if (!attributes.usesDefaultImage) {
      _sendTabPromoItem.faviconImage = attributes.faviconImage;
    }
    return;
  }

  _sendTabPromoItem = [[SendTabPromoItem alloc] init];
  if (!attributes.usesDefaultImage) {
    _sendTabPromoItem.faviconImage = attributes.faviconImage;
  }
  _sendTabPromoItem.standaloneDelegate = self;
  [_delegate sentTabReceived];
}

@end
