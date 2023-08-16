// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_mediator.h"

#import "base/debug/dump_without_crashing.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/all_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/web_state_tab_switcher_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/gfx/image/image.h"

namespace {

// Constructs an array of TabSwitcherItems from a `web_state_list`.
NSArray<TabSwitcherItem*>* CreateItems(WebStateList* web_state_list) {
  NSMutableArray<TabSwitcherItem*>* items = [[NSMutableArray alloc] init];
  for (int i = 0; i < web_state_list->count(); i++) {
    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    [items
        addObject:[[WebStateTabSwitcherItem alloc] initWithWebState:web_state]];
  }
  return items;
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
    [self removeWebStateObservations];
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateListObserver = nullptr;
    _webStateList = nullptr;
  }
}

#pragma mark - Public properties

- (void)setWebStateList:(WebStateList*)webStateList {
  if (_webStateList) {
    [self removeWebStateObservations];
    _webStateList->RemoveObserver(_webStateListObserver.get());
  }

  _webStateList = webStateList;

  if (_webStateList) {
    DCHECK_GE(_webStateList->count(), 0);
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());

    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    [self addWebStateObservations];
  }

  [self populateConsumerItems];
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  DCHECK_EQ(_webStateList, webStateList);
  if (webStateList->IsBatchInProgress()) {
    return;
  }

  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // The activation is handled after this switch statement.
      break;
    case WebStateListChange::Type::kDetach:
    case WebStateListChange::Type::kInsert:
      [self populateConsumerItems];
      break;
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace:
      // Do nothing when a WebState is replaced.
      break;
  }

  if (status.active_web_state_change()) {
    // If the selected index changes as a result of the last webstate being
    // detached, the active index will be -1.
    if (webStateList->active_index() == WebStateList::kInvalidIndex) {
      [self.consumer selectItemWithID:nil];
      return;
    }

    [self.consumer
        selectItemWithID:status.new_active_web_state->GetStableIdentifier()];
  }
}

- (void)webStateListWillBeginBatchOperation:(WebStateList*)webStateList {
  DCHECK_EQ(_webStateList, webStateList);

  [self removeWebStateObservations];
}

- (void)webStateListBatchOperationEnded:(WebStateList*)webStateList {
  DCHECK_EQ(_webStateList, webStateList);

  [self addWebStateObservations];
  [self populateConsumerItems];
}

#pragma mark - TabStripConsumerDelegate

- (void)addNewItem {
  if (!self.webStateList)
    return;

  if (!self.browserState) {
    return;
  }

  if (!IsAddNewTabAllowedByPolicy(self.browserState->GetPrefs(),
                                  self.browserState->IsOffTheRecord())) {
    // TODO(crbug.com/1471955): Try to show a notice to the user when this
    // happens.
    //
    // Check that adding a new item is allowed by policy. It is an error to
    // call -addNewItem when the corresponding browsing mode is disabled. The
    // event is reported without crashing the browser and -addNewItem is
    // softly cancelled without a notice (this approach is better than allowing
    // a policy violation).
    base::debug::DumpWithoutCrashing();
    return;
  }

  web::WebState::CreateParams params(self.browserState);
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
  int index = GetTabIndex(
      self.webStateList,
      WebStateSearchCriteria{
          .identifier = itemID,
          .pinned_state = WebStateSearchCriteria::PinnedState::kNonPinned,
      });
  if (index >= 0)
    self.webStateList->CloseWebStateAt(index, WebStateList::CLOSE_USER_ACTION);
}

#pragma mark - Private

// Adds an observation to every WebState of the current WebSateList.
- (void)addWebStateObservations {
  _allWebStateObservationForwarder =
      std::make_unique<AllWebStateObservationForwarder>(
          _webStateList, _webStateObserver.get());
}

// Removes an observation from every WebState of the current WebSateList.
- (void)removeWebStateObservations {
  _allWebStateObservationForwarder.reset();
}

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
  TabSwitcherItem* item =
      [[WebStateTabSwitcherItem alloc] initWithWebState:webState];
  [self.consumer replaceItemID:webState->GetStableIdentifier() withItem:item];
}

@end
