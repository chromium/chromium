// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/location_bar/location_bar_mediator.h"

#import "base/memory/ptr_util.h"
#import "ios/chrome/browser/ntp/new_tab_page_util.h"
#import "ios/chrome/browser/search_engines/search_engine_observer_bridge.h"
#import "ios/chrome/browser/search_engines/search_engines_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_consumer.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#import "ios/chrome/grit/ios_theme_resources.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "skia/ext/skia_utils_ios.h"

@interface LocationBarMediator () <SearchEngineObserving, WebStateListObserving>

// Whether the current default search engine supports search by image.
@property(nonatomic, assign) BOOL searchEngineSupportsSearchByImage;

// Whether the current default search engine supports Lens.
@property(nonatomic, assign) BOOL searchEngineSupportsLens;

@end

@implementation LocationBarMediator {
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserver;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _searchEngineSupportsSearchByImage = NO;
    _searchEngineSupportsLens = NO;
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
  }
  return self;
}

- (void)disconnect {
  self.webStateList = nil;
}

- (void)dealloc {
  [self disconnect];
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  self.searchEngineSupportsSearchByImage =
      search_engines::SupportsSearchByImage(self.templateURLService);
  self.searchEngineSupportsLens =
      search_engines::SupportsSearchImageWithLens(self.templateURLService);
}

#pragma mark - Setters

- (void)setConsumer:(id<LocationBarConsumer>)consumer {
  _consumer = consumer;
  [consumer
      updateSearchByImageSupported:self.searchEngineSupportsSearchByImage];
  [consumer updateLensImageSupported:self.searchEngineSupportsLens];
}

- (void)setTemplateURLService:(TemplateURLService*)templateURLService {
  if (templateURLService) {
    self.searchEngineSupportsSearchByImage =
        search_engines::SupportsSearchByImage(templateURLService);
    _searchEngineObserver =
        std::make_unique<SearchEngineObserverBridge>(self, templateURLService);
  } else {
    self.searchEngineSupportsSearchByImage = NO;
    _searchEngineObserver.reset();
  }
  _templateURLService = templateURLService;
}

- (void)setSearchEngineSupportsSearchByImage:
    (BOOL)searchEngineSupportsSearchByImage {
  BOOL supportChanged =
      _searchEngineSupportsSearchByImage != searchEngineSupportsSearchByImage;
  _searchEngineSupportsSearchByImage = searchEngineSupportsSearchByImage;
  if (supportChanged) {
    [self.consumer
        updateSearchByImageSupported:searchEngineSupportsSearchByImage];
  }
}

- (void)setSearchEngineSupportsLens:(BOOL)searchEngineSupportsLens {
  BOOL supportChanged = _searchEngineSupportsLens != searchEngineSupportsLens;
  _searchEngineSupportsLens = searchEngineSupportsLens;
  if (supportChanged) {
    [self.consumer updateLensImageSupported:searchEngineSupportsLens];
  }
}

- (void)setWebStateList:(WebStateList*)webStateList {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
  }

  _webStateList = webStateList;

  if (_webStateList) {
    _webStateList->AddObserver(_webStateListObserver.get());
  }
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  DCHECK_EQ(_webStateList, webStateList);
  if (status.active_web_state_change()) {
    [self.consumer defocusOmnibox];
  }
}

@end
