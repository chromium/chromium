// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_mediator.h"

#import "components/favicon/ios/web_favicon_driver.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/browser/url/url_util.h"
#import "ios/chrome/browser/web_state_list/all_web_state_observation_forwarder.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Constructs an array of TabSwitcherItems from a `web_state_list`.
NSArray* CreateItems(WebStateList* web_state_list) {
  NSMutableArray* items = [[NSMutableArray alloc] init];
  for (int i = 0; i < web_state_list->count(); i++) {
    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    [items addObject:GetTabSwitcherItem(web_state)];
  }
  return [items copy];
}

// Returns the ID of the active tab in `web_state_list`.
NSString* GetActiveTabId(WebStateList* web_state_list) {
  if (!web_state_list)
    return nil;

  web::WebState* web_state = web_state_list->GetActiveWebState();
  if (!web_state)
    return nil;
  return web_state->GetStableIdentifier();
}

}  // namespace

@interface TabStripMediator () <CRWWebStateObserver, WebStateListObserving> {
  // Bridge C++ WebStateListObserver methods to this TabStripController.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  // Bridge C++ WebStateObserver methods to this TabStripController.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  // Forward observer methods for all WebStates in the WebStateList monitored
  // by the TabStripMediator.
  std::unique_ptr<AllWebStateObservationForwarder>
      _allWebStateObservationForwarder;
}

// The consumer for this object.
@property(nonatomic, weak) id<TabStripConsumer> consumer;

@end

@implementation TabStripMediator

- (instancetype)initWithConsumer:(id<TabStripConsumer>)consumer {
  if (self = [super init]) {
    _consumer = consumer;
  }
  return self;
}

- (void)disconnect {
  if (_webStateList) {
    _allWebStateObservationForwarder.reset();
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateListObserver = nullptr;
    _webStateList = nullptr;
  }
}

#pragma mark - Public properties

- (void)setWebStateList:(WebStateList*)webStateList {
  if (_webStateList) {
    _allWebStateObservationForwarder.reset();
    _webStateList->RemoveObserver(_webStateListObserver.get());
  }

  _webStateList = webStateList;

  if (_webStateList) {
    DCHECK_GE(_webStateList->count(), 0);
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());

    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    // Observe all webStates of this `_webStateList`.
    _allWebStateObservationForwarder =
        std::make_unique<AllWebStateObservationForwarder>(
            _webStateList, _webStateObserver.get());
  }
  [self populateConsumerItems];
}

#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)atIndex {
  [self populateConsumerItems];
}

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  [self populateConsumerItems];
}

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(ActiveWebStateChangeReason)reason {
  DCHECK_EQ(_webStateList, webStateList);
  if (webStateList->IsBatchInProgress())
    return;
  // If the selected index changes as a result of the last webstate being
  // detached, atIndex will be -1.
  if (atIndex == -1) {
    [self.consumer selectItemWithID:nil];
    return;
  }

  [self.consumer selectItemWithID:newWebState->GetStableIdentifier()];
}

#pragma mark - TabFaviconDataSource

- (void)faviconForIdentifier:(NSString*)identifier
                  completion:(void (^)(UIImage*))completion {
  web::WebState* webState =
      GetWebState(_webStateList, identifier, /*pinned=*/NO);
  if (!webState) {
    return;
  }
  // NTP tabs get no favicon.
  if (IsURLNtp(webState->GetVisibleURL())) {
    return;
  }
  UIImage* defaultFavicon =
      webState->GetBrowserState()->IsOffTheRecord()
          ? [UIImage imageNamed:@"default_world_favicon_incognito"]
          : [UIImage imageNamed:@"default_world_favicon_regular"];
  completion(defaultFavicon);

  favicon::FaviconDriver* faviconDriver =
      favicon::WebFaviconDriver::FromWebState(webState);
  if (faviconDriver) {
    gfx::Image favicon = faviconDriver->GetFavicon();
    if (!favicon.IsEmpty())
      completion(favicon.ToUIImage());
  }
}

#pragma mark - TabStripConsumerDelegate

- (void)addNewItem {
  if (!self.webStateList)
    return;

  web::WebState::CreateParams params(
      self.webStateList->GetActiveWebState()->GetBrowserState());
  std::unique_ptr<web::WebState> webState = web::WebState::Create(params);

  GURL url(kChromeUINewTabURL);
  web::NavigationManager::WebLoadParams loadParams(url);
  loadParams.transition_type = ui::PAGE_TRANSITION_TYPED;
  webState->GetNavigationManager()->LoadURLWithParams(loadParams);

  self.webStateList->InsertWebState(
      base::checked_cast<int>(self.webStateList->count()), std::move(webState),
      (WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE),
      WebStateOpener());
  [self.consumer selectItemWithID:GetActiveTabId(self.webStateList)];
}

- (void)selectTab:(int)index {
  if (!self.webStateList)
    return;

  _webStateList->ActivateWebStateAt(index);
}

- (void)closeItemWithID:(NSString*)itemID {
  int index = GetTabIndex(self.webStateList, itemID, /*pinned=*/NO);
  if (index >= 0)
    self.webStateList->CloseWebStateAt(index, WebStateList::CLOSE_USER_ACTION);
}

#pragma mark - Private

// Calls `-populateItems:selectedItemID:` on the consumer.
- (void)populateConsumerItems {
  if (!self.webStateList)
    return;
  if (self.webStateList->count() > 0) {
    [self.consumer populateItems:CreateItems(self.webStateList)
                  selectedItemID:GetActiveTabId(self.webStateList)];
    self.consumer.isOffTheRecord = self.webStateList->GetWebStateAt(0)
                                       ->GetBrowserState()
                                       ->IsOffTheRecord();
  }
}

#pragma mark - CRWWebStateObserver

- (void)webStateDidChangeTitle:(web::WebState*)webState {
  [self.consumer replaceItemID:webState->GetStableIdentifier()
                      withItem:GetTabSwitcherItem(webState)];
}

@end
