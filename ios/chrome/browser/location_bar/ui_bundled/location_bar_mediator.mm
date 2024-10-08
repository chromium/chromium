// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_mediator.h"

#import "base/memory/ptr_util.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_consumer.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"
#import "ios/chrome/browser/search_engines/model/search_engines_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#import "ios/chrome/grit/ios_theme_resources.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
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
  BOOL _isIncognito;
}

- (instancetype)initWithIsIncognito:(BOOL)isIncognito {
  self = [super init];
  if (self) {
    _searchEngineSupportsSearchByImage = NO;
    _searchEngineSupportsLens = NO;
    _isIncognito = isIncognito;
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
  }
  return self;
}

- (void)disconnect {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateList = nullptr;
  }
  _webStateListObserver = nullptr;
  _searchEngineObserver = nullptr;
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
  [consumer setSearchByImageEnabled:self.searchEngineSupportsSearchByImage];
  [consumer setLensImageEnabled:self.searchEngineSupportsLens];
  [self updatePlaceholderType];
}

- (void)setTemplateURLService:(TemplateURLService*)templateURLService {
  _templateURLService = templateURLService;
  if (templateURLService) {
    self.searchEngineSupportsSearchByImage =
        search_engines::SupportsSearchByImage(templateURLService);
    _searchEngineObserver =
        std::make_unique<SearchEngineObserverBridge>(self, templateURLService);
  } else {
    self.searchEngineSupportsSearchByImage = NO;
    _searchEngineObserver.reset();
  }
}

- (void)setSearchEngineSupportsSearchByImage:
    (BOOL)searchEngineSupportsSearchByImage {
  BOOL supportChanged =
      _searchEngineSupportsSearchByImage != searchEngineSupportsSearchByImage;
  _searchEngineSupportsSearchByImage = searchEngineSupportsSearchByImage;
  if (supportChanged) {
    [self.consumer setSearchByImageEnabled:searchEngineSupportsSearchByImage];
  }
}

- (void)setSearchEngineSupportsLens:(BOOL)searchEngineSupportsLens {
  BOOL supportChanged = _searchEngineSupportsLens != searchEngineSupportsLens;
  _searchEngineSupportsLens = searchEngineSupportsLens;
  if (supportChanged) {
    [self.consumer setLensImageEnabled:searchEngineSupportsLens];
    [self updatePlaceholderType];
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

- (void)locationUpdated {
  [self updatePlaceholderType];
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

#pragma mark - Private

/// Updates the placeholder.
- (void)updatePlaceholderType {
  if (!IsLensOverlayAvailable()) {
    return;
  }
  if (!_isIncognito && ![self isNTP] &&
      search_engines::SupportsSearchImageWithLens(self.templateURLService)) {
    [self.consumer setPlaceholderType:LocationBarPlaceholderType::kLensOverlay];
  } else {
    [self.consumer setPlaceholderType:LocationBarPlaceholderType::kNone];
  }
}

/// Returns YES if the active web state is a New Tab Page.
- (BOOL)isNTP {
  if (!_webStateList) {
    return NO;
  }
  web::WebState* webState = _webStateList->GetActiveWebState();
  return webState ? IsURLNewTabPage(webState->GetVisibleURL()) : NO;
}

@end
