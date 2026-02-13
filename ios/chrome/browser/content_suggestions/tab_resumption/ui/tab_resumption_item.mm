// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/tab_resumption/ui/tab_resumption_item.h"

#import <string>

#import "base/time/time.h"
#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/shop_card/ui/shop_card_data.h"
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
  self.sessionName = [item.sessionName copy];
  self.localWebState = item.localWebState;
  self.tabTitle = [item.tabTitle copy];
  self.tabURL = item.tabURL;
  self.reason = [item.reason copy];
  self.syncedTime = item.syncedTime;
  self.faviconImage = item.faviconImage;
  self.contentImage = item.contentImage;
  self.URLKey = item.URLKey;
  self.requestID = item.requestID;
  self.shopCardData = item.shopCardData;
}

- (BOOL)hasDifferentContentsFromConfig:(MagicStackModule*)config {
  if ([super hasDifferentContentsFromConfig:config]) {
    return YES;
  }
  TabResumptionItem* item = static_cast<TabResumptionItem*>(config);
  return self.tabURL != item.tabURL;
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

#pragma mark - NSCopying

- (instancetype)copyWithZone:(NSZone*)zone {
  TabResumptionItem* item =
      [[super copyWithZone:zone] initWithItemType:self.itemType];
  // The updates to properties must be reflected in the copy method.
  // LINT.IfChange(Copy)
  item.commandHandler = self.commandHandler;
  item.sessionName = [self.sessionName copy];
  item.localWebState = self.localWebState;
  item.tabTitle = [self.tabTitle copy];
  item.tabURL = self.tabURL;
  item.reason = [self.reason copy];
  item.syncedTime = self.syncedTime;
  item.faviconImage = self.faviconImage;
  item.contentImage = self.contentImage;
  item.URLKey = self.URLKey;
  item.requestID = self.requestID;
  item.shopCardData = self.shopCardData;
  // LINT.ThenChange(tab_resumption_item.h:Copy)
  return item;
}

@end
