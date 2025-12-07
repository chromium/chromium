// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/tab_resumption/ui/tab_resumption_item.h"

#import <string>

#import "base/time/time.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/shop_card/ui/shop_card_data.h"
#import "url/gurl.h"

@implementation TabResumptionItem {
  GURL _tabURL;
  std::string _URLKey;
}

- (instancetype)initWithItemType:(TabResumptionItemType)itemType {
  if ((self = [super init])) {
    _itemType = itemType;
  }
  return self;
}

#pragma mark - MagicStackModule

- (ContentSuggestionsModuleType)type {
  return ContentSuggestionsModuleType::kTabResumption;
}

- (void)reconfigureWithItem:(TabResumptionItem*)item {
  DCHECK(self.commandHandler == item.commandHandler);
  _itemType = item.itemType;
  _sessionName = item.sessionName;
  _localWebState = item.localWebState;
  _tabTitle = item.tabTitle;
  _tabURL = item.tabURL;
  _reason = item.reason;
  _syncedTime = item.syncedTime;
  _faviconImage = item.faviconImage;
  _contentImage = item.contentImage;
  _URLKey = item.URLKey;
  _requestID = item.requestID;
  _shopCardData = item.shopCardData;
  _consumerSource = item.consumerSource;
}

#pragma mark - properties

- (const GURL&)tabURL {
  return _tabURL;
}

- (void)setTabURL:(const GURL&)tabURL {
  _tabURL = tabURL;
}

- (const std::string&)URLKey {
  return _URLKey;
}

- (void)setURLKey:(const std::string&)URLKey {
  _URLKey = URLKey;
}

@end
