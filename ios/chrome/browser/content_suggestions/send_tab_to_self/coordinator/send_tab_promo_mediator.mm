// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/send_tab_to_self/coordinator/send_tab_promo_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "components/prefs/pref_service.h"
#import "components/send_tab_to_self/pref_names.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_module_container_delegate.h"
#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/send_tab_to_self/coordinator/send_tab_promo_mediator_delegate.h"
#import "ios/chrome/browser/content_suggestions/send_tab_to_self/ui/send_tab_promo_audience.h"
#import "ios/chrome/browser/content_suggestions/send_tab_to_self/ui/send_tab_promo_config.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "url/gurl.h"

@interface SendTabPromoMediator () <SendTabPromoAudience>

@end

@implementation SendTabPromoMediator {
  SendTabPromoConfig* _sendTabPromoConfig;
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
  _sendTabPromoConfig = nullptr;
}

- (SendTabPromoConfig*)sendTabPromoConfigToShow {
  return _sendTabPromoConfig;
}

- (void)setDelegate:(id<SendTabPromoMediatorDelegate>)delegate {
  _delegate = delegate;
  if (_delegate) {
    [self latestSendTabToSelfItem];
  }
}

- (void)dismissModule {
  [self.delegate removeSendTabPromoModule];
}

#pragma mark - SendTabPromoAudience

- (void)didSelectSendTabPromo {
  base::UmaHistogramBoolean(
      "IOS.Notifications.SendTab.MagicStack.AllowNotificationsPressed", true);
  [self.notificationsDelegate
      enableNotifications:ContentSuggestionsModuleType::kSendTabPromo
           viaContextMenu:NO];
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
  _sendTabPromoConfig = nullptr;
  __weak SendTabPromoMediator* weakSelf = self;

  _faviconLoader->FaviconForPageUrl(
      tabURL, kDesiredSmallFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/true,
      ^(FaviconAttributes* attributes, bool cached) {
        [weakSelf onFaviconReceived:attributes cached:cached];
      });
}

// Called when the favicon has been received.
- (void)onFaviconReceived:(FaviconAttributes*)attributes cached:(BOOL)cached {
  if (_sendTabPromoConfig) {
    // Favicon callback has already been executed, update the image and return.
    if (!cached || attributes.faviconImage) {
      _sendTabPromoConfig.faviconImage = attributes.faviconImage;
    }
    return;
  }

  _sendTabPromoConfig = [[SendTabPromoConfig alloc] init];
  if (!cached || attributes.faviconImage) {
    _sendTabPromoConfig.faviconImage = attributes.faviconImage;
  }
  _sendTabPromoConfig.audience = self;
  [_delegate sentTabReceived];
}

@end
