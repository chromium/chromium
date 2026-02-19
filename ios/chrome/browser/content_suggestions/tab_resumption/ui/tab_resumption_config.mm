// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/tab_resumption/ui/tab_resumption_config.h"

#import <string>

#import "base/time/time.h"
#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/shop_card/ui/shop_card_data.h"
#import "url/gurl.h"

@implementation TabResumptionConfig {
  GURL _tabURL;
  std::string _URLKey;
}

- (instancetype)initWithItemType:(TabResumptionItemType)itemType {
  if ((self = [super init])) {
    _itemType = itemType;
  }
  return self;
}

#pragma mark - Public

- (void)reconfigureWithConfig:(TabResumptionConfig*)config {
  DCHECK(self.commandHandler == config.commandHandler);
  _itemType = config.itemType;
  self.sessionName = [config.sessionName copy];
  self.localWebState = config.localWebState;
  self.tabTitle = [config.tabTitle copy];
  self.tabURL = config.tabURL;
  self.reason = [config.reason copy];
  self.syncedTime = config.syncedTime;
  self.faviconImage = config.faviconImage;
  self.contentImage = config.contentImage;
  self.URLKey = config.URLKey;
  self.requestID = config.requestID;
  self.shopCardData = config.shopCardData;
}

#pragma mark - NSCopying

- (instancetype)copyWithZone:(NSZone*)zone {
  TabResumptionConfig* config =
      [[super copyWithZone:zone] initWithItemType:self.itemType];
  // The updates to properties must be reflected in the copy method.
  // LINT.IfChange(Copy)
  config.commandHandler = self.commandHandler;
  config.sessionName = [self.sessionName copy];
  config.localWebState = self.localWebState;
  config.tabTitle = [self.tabTitle copy];
  config.tabURL = self.tabURL;
  config.reason = [self.reason copy];
  config.syncedTime = self.syncedTime;
  config.faviconImage = self.faviconImage;
  config.contentImage = self.contentImage;
  config.URLKey = self.URLKey;
  config.requestID = self.requestID;
  config.shopCardData = self.shopCardData;
  // LINT.ThenChange(tab_resumption_config.h:Copy)
  return config;
}

#pragma mark - MagicStackModule

- (ContentSuggestionsModuleType)type {
  return ContentSuggestionsModuleType::kTabResumption;
}

- (BOOL)hasDifferentContentsFromConfig:(MagicStackModule*)config {
  if ([super hasDifferentContentsFromConfig:config]) {
    return YES;
  }
  TabResumptionConfig* tabResumptionConfig =
      static_cast<TabResumptionConfig*>(config);
  return self.tabURL != tabResumptionConfig.tabURL;
}

#pragma mark - Accessors & Mutators

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
