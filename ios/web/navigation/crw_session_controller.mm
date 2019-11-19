// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_session_controller.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/logging.h"
#import "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/web/common/features.h"
#include "ios/web/history_state_util.h"
#import "ios/web/navigation/crw_session_controller+private_constructors.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/navigation/time_smoother.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/navigation/browser_url_rewriter.h"
#include "ios/web/public/navigation/referrer.h"
#include "ios/web/public/security/ssl_status.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CRWSessionController () {
  // Weak pointer back to the owning NavigationManager. This is to facilitate
  // the incremental merging of the two classes.
  web::NavigationManagerImpl* _navigationManager;

  // Identifies the index of the last committed item in the items array.
  NSInteger _lastCommittedItemIndex;
  // Identifies the index of the previous item in the items array.
  NSInteger _previousItemIndex;

  // The browser state associated with this CRWSessionController;
  web::BrowserState* _browserState;  // weak

  // Time smoother for navigation item timestamps; see comment in
  // navigation_controller_impl.h
  web::TimeSmoother _timeSmoother;

  // Backing objects for properties of the same name.
  web::ScopedNavigationItemImplList _items;
  // |_pendingItem| only contains a NavigationItem for non-history navigations.
  // For back/forward navigations within session history, _pendingItemIndex will
  // be an index within |_items|, and self.pendingItem will return the item at
  // that index.
  std::unique_ptr<web::NavigationItemImpl> _pendingItem;
  std::unique_ptr<web::NavigationItemImpl> _transientItem;
}

// Redefine as readwrite.
@property(nonatomic, readwrite, assign) NSInteger lastCommittedItemIndex;

// Removes all items after lastCommittedItemIndex.
- (void)clearForwardItems;
// Discards the transient item, if any.
- (void)discardTransientItem;
// Creates a NavigationItemImpl with the specified properties.
- (std::unique_ptr<web::NavigationItemImpl>)
   itemWithURL:(const GURL&)url
      referrer:(const web::Referrer&)referrer
    transition:(ui::PageTransition)transition
initiationType:(web::NavigationInitiationType)initiationType;
// Returns YES if the PageTransition for the underlying navigationItem at
// |index| in |items| has ui::PAGE_TRANSITION_IS_REDIRECT_MASK.
- (BOOL)isRedirectTransitionForItemAtIndex:(size_t)index;

// Should create a new pending item if the new pending item is not a duplicate
// of the last added or committed item. Returns YES if one of the following
// rules apply:
// 1. There is no last added or committed item.
// 2. The new item has different url from the last added or committed item.
// 3. Url is the same, but the new item is a form submission resulted from the
//    last added or committed item.
// 4. Url is the same, but new item is a reload with different user agent type
//    resulted from last added or committed item.
- (BOOL)shouldCreatePendingItemWithURL:(const GURL&)URL
                            transition:(ui::PageTransition)transition
               userAgentOverrideOption:
                   (web::NavigationManager::UserAgentOverrideOption)
                       userAgentOverrideOption;

@end

@implementation CRWSessionController

@synthesize lastCommittedItemIndex = _lastCommittedItemIndex;
@synthesize previousItemIndex = _previousItemIndex;
@synthesize pendingItemIndex = _pendingItemIndex;

- (instancetype)initWithBrowserState:(web::BrowserState*)browserState {
  self = [super init];
  if (self) {
    _browserState = browserState;
    _lastCommittedItemIndex = -1;
    _previousItemIndex = -1;
    _pendingItemIndex = -1;
  }
  return self;
}

- (instancetype)initWithBrowserState:(web::BrowserState*)browserState
                     navigationItems:(web::ScopedNavigationItemList)items
              lastCommittedItemIndex:(NSUInteger)lastCommittedItemIndex {
  self = [super init];
  if (self) {
    _browserState = browserState;
    _items = web::CreateScopedNavigationItemImplList(std::move(items));
    _lastCommittedItemIndex =
        std::min(static_cast<NSInteger>(lastCommittedItemIndex),
                 static_cast<NSInteger>(_items.size()) - 1);
    _previousItemIndex = -1;
    _pendingItemIndex = -1;
  }
  return self;
}

#pragma mark - Accessors

- (void)setLastCommittedItemIndex:(NSInteger)lastCommittedItemIndex {
  if (_lastCommittedItemIndex != lastCommittedItemIndex) {
    _lastCommittedItemIndex = lastCommittedItemIndex;
    if (_navigationManager)
      _navigationManager->RemoveTransientURLRewriters();
  }
}

- (void)setPendingItemIndex:(NSInteger)pendingItemIndex {
  DCHECK_GE(pendingItemIndex, -1);
  DCHECK_LT(pendingItemIndex, static_cast<NSInteger>(self.items.size()));
  _pendingItemIndex = pendingItemIndex;
  DCHECK(_pendingItemIndex == -1 || self.pendingItem);
}

- (BOOL)canPruneAllButLastCommittedItem {
  return self.lastCommittedItemIndex != -1 && self.pendingItemIndex == -1 &&
         !self.transientItem;
}

- (const web::ScopedNavigationItemImplList&)items {
  return _items;
}

- (web::NavigationItemImpl*)currentItem {
  return _navigationManager->GetCurrentItemImpl();
}

- (web::NavigationItemImpl*)visibleItem {
  if (self.transientItem)
    return self.transientItem;
  // Only return the |pendingItem| for new (non-history), browser-initiated
  // navigations and when WebState is loading in order to prevent URL spoof
  // attacks.
  web::NavigationItemImpl* pendingItem = self.pendingItem;
  if (pendingItem) {
    bool isBrowserInitiated = pendingItem->NavigationInitiationType() ==
                              web::NavigationInitiationType::BROWSER_INITIATED;
    bool safeToShowPending = isBrowserInitiated && _pendingItemIndex == -1;
    if (web::features::UseWKWebViewLoading()) {
      safeToShowPending =
          safeToShowPending && _navigationManager->GetWebState()->IsLoading();
    }
    if (safeToShowPending)
      return pendingItem;
  }
  return self.lastCommittedItem;
}

- (web::NavigationItemImpl*)pendingItem {
  if (self.pendingItemIndex == -1) {
    if (!_pendingItem) {
      return [self.delegate pendingItemForSessionController:self];
    }
    return _pendingItem.get();
  }
  return self.items[self.pendingItemIndex].get();
}

- (web::NavigationItemImpl*)transientItem {
  return _transientItem.get();
}

- (web::NavigationItemImpl*)lastCommittedItem {
  NSInteger index = self.lastCommittedItemIndex;
  return index == -1 ? nullptr : self.items[index].get();
}

- (web::NavigationItemImpl*)previousItem {
  NSInteger index = self.previousItemIndex;
  return index == -1 || self.items.empty() ? nullptr : self.items[index].get();
}

- (web::NavigationItemList)backwardItems {
  web::NavigationItemList items;

  // This explicit check is necessary to protect the loop below which uses an
  // unsafe signed (NSInteger) to unsigned (size_t) conversion.
  if (_lastCommittedItemIndex > -1) {
    // If the current navigation item is a transient item (e.g. SSL
    // interstitial), the last committed item should also be considered part of
    // the backward history.
    DCHECK(self.lastCommittedItem);
    if (self.transientItem) {
      items.push_back(self.lastCommittedItem);
    }

    for (size_t index = _lastCommittedItemIndex; index > 0; --index) {
      if (![self isRedirectTransitionForItemAtIndex:index])
        items.push_back(self.items[index - 1].get());
    }
  }
  return items;
}

- (web::NavigationItemList)forwardItems {
  web::NavigationItemList items;
  NSUInteger lastNonRedirectedIndex = _lastCommittedItemIndex + 1;
  while (lastNonRedirectedIndex < self.items.size()) {
    web::NavigationItem* item = self.items[lastNonRedirectedIndex].get();
    if (!ui::PageTransitionIsRedirect(item->GetTransitionType()))
      items.push_back(item);
    ++lastNonRedirectedIndex;
  }
  return items;
}

#pragma mark - NSObject

- (NSString*)description {
  // Create description for |items|.
  NSMutableString* itemsDescription = [NSMutableString stringWithString:@"[\n"];
#ifndef NDEBUG
  for (const auto& item : self.items)
    [itemsDescription appendFormat:@"%@\n", item->GetDescription()];
#endif
  [itemsDescription appendString:@"]"];
  // Create description for |pendingItem| and |transientItem|.
  NSString* pendingItemDescription = @"(null)";
  NSString* transientItemDescription = @"(null)";
#ifndef NDEBUG
  if (self.pendingItem)
    pendingItemDescription = self.pendingItem->GetDescription();
  if (self.transientItem)
    transientItemDescription = self.transientItem->GetDescription();
#else
  if (self.pendingItem) {
    pendingItemDescription =
        [NSString stringWithFormat:@"%p", self.pendingItem];
  }
  if (self.transientItem) {
    transientItemDescription =
        [NSString stringWithFormat:@"%p", self.transientItem];
  }
#endif
  return [NSString stringWithFormat:@"last committed item index: %" PRIdNS
                                    @"\nprevious item index: %" PRIdNS
                                    @"\npending item index: %" PRIdNS
                                    @"\nall items: %@ \npending item: %@"
                                    @"\ntransient item: %@\n",
                                    _lastCommittedItemIndex, _previousItemIndex,
                                    _pendingItemIndex, itemsDescription,
                                    pendingItemDescription,
                                    transientItemDescription];
}

#pragma mark - Public

- (void)setNavigationManager:(web::NavigationManagerImpl*)navigationManager {
  _navigationManager = navigationManager;
  if (_navigationManager) {
    // _browserState will be nullptr if CRWSessionController has been
    // initialized with -initWithCoder: method. Take _browserState from
    // NavigationManagerImpl if that's the case.
    if (!_browserState) {
      _browserState = _navigationManager->GetBrowserState();
    }
    DCHECK_EQ(_browserState, _navigationManager->GetBrowserState());
  }
}

- (void)setBrowserState:(web::BrowserState*)browserState {
  _browserState = browserState;
  DCHECK(!_navigationManager ||
         _navigationManager->GetBrowserState() == _browserState);
}

- (std::unique_ptr<web::NavigationItemImpl>)releasePendingItem {
  return std::move(_pendingItem);
}

- (void)setPendingItem:(std::unique_ptr<web::NavigationItemImpl>)item {
  _pendingItem = std::move(item);
}

- (void)addPendingItem:(const GURL&)url
                   referrer:(const web::Referrer&)ref
                 transition:(ui::PageTransition)trans
             initiationType:(web::NavigationInitiationType)initiationType
    userAgentOverrideOption:(web::NavigationManager::UserAgentOverrideOption)
                                userAgentOverrideOption {
  [self discardTransientItem];
  self.pendingItemIndex = -1;

  if (![self shouldCreatePendingItemWithURL:url
                                 transition:trans
                    userAgentOverrideOption:userAgentOverrideOption]) {
    return;
  }

  _pendingItem = [self itemWithURL:url
                          referrer:ref
                        transition:trans
                    initiationType:initiationType];
  DCHECK_EQ(-1, self.pendingItemIndex);
}

- (BOOL)shouldCreatePendingItemWithURL:(const GURL&)URL
                            transition:(ui::PageTransition)transition
               userAgentOverrideOption:
                   (web::NavigationManager::UserAgentOverrideOption)
                       userAgentOverrideOption {
  // Note: CRWSessionController currently has the responsibility to distinguish
  // between new navigations and history stack navigation, hence the inclusion
  // of specific transiton type logic here, in order to make it reliable with
  // real-world observed behavior.
  // TODO(crbug.com/676129): Fix the way changes are detected/reported elsewhere
  // in the web layer so that this hack can be removed.
  // Remove the workaround code from -presentSafeBrowsingWarningForResource:.
  web::NavigationItemImpl* currentItem = self.currentItem;
  if (!currentItem)
    return YES;

  // User agent override option should always be different from the user agent
  // type of the pending item, or the last committed item if pending doesn't
  // exist.
  DCHECK(userAgentOverrideOption !=
             web::NavigationManager::UserAgentOverrideOption::DESKTOP ||
         currentItem->GetUserAgentType() != web::UserAgentType::DESKTOP);
  DCHECK(userAgentOverrideOption !=
             web::NavigationManager::UserAgentOverrideOption::MOBILE ||
         currentItem->GetUserAgentType() != web::UserAgentType::MOBILE);

  BOOL hasSameURL = self.currentItem->GetURL() == URL;
  if (!hasSameURL) {
    // Different url indicates that it's not a duplicate item.
    return YES;
  }

  BOOL isPendingTransitionFormSubmit =
      PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_FORM_SUBMIT);
  BOOL isCurrentTransitionFormSubmit = PageTransitionCoreTypeIs(
      currentItem->GetTransitionType(), ui::PAGE_TRANSITION_FORM_SUBMIT);
  if (isPendingTransitionFormSubmit && !isCurrentTransitionFormSubmit) {
    // |isPendingTransitionFormSubmit| indicates that the new item is a form
    // submission resulted from the last added or committed item, and
    // |!isCurrentTransitionFormSubmit| shows that the form submission is not
    // counted multiple times.
    return YES;
  }

  BOOL isPendingTransitionReload =
      PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD);
  BOOL isInheritingUserAgentType =
      userAgentOverrideOption ==
      web::NavigationManager::UserAgentOverrideOption::INHERIT;
  if (isPendingTransitionReload && !isInheritingUserAgentType) {
    // Overriding user agent type to MOBILE or DESKTOP indicates that the new
    // new item is a reload with different user agent type.
    return YES;
  }

  return NO;
}

- (void)clearForwardItems {
  [self discardTransientItem];

  NSInteger forwardItemStartIndex = _lastCommittedItemIndex + 1;
  DCHECK(forwardItemStartIndex >= 0);

  size_t itemCount = self.items.size();
  if (forwardItemStartIndex >= static_cast<NSInteger>(itemCount))
    return;

  if (_previousItemIndex >= forwardItemStartIndex)
    _previousItemIndex = -1;

  // Remove the NavigationItems and notify the NavigationManager.
  _items.erase(_items.begin() + forwardItemStartIndex, _items.end());
  if (_navigationManager) {
    _navigationManager->OnNavigationItemsPruned(itemCount -
                                                forwardItemStartIndex);
  }
}

- (void)commitPendingItem {
  if (self.pendingItem) {
    // Once an item is committed it's not renderer-initiated any more. (Matches
    // the implementation in NavigationController.)
    self.pendingItem->ResetForCommit();

    NSInteger newItemIndex = self.pendingItemIndex;
    if (newItemIndex == -1) {
      [self clearForwardItems];
      if (_pendingItem) {
        // Add the new item at the end.
        _items.push_back(std::move(_pendingItem));
      }
      newItemIndex = self.items.size() - 1;
    }
    _previousItemIndex = _lastCommittedItemIndex;
    self.lastCommittedItemIndex = newItemIndex;
    self.pendingItemIndex = -1;
    DCHECK(!_pendingItem);
  }

  // Update the navigation timestamp now that it's actually happened.
  web::NavigationItem* item = self.lastCommittedItem;
  if (item) {
    item->SetTimestamp(_timeSmoother.GetSmoothedTime(base::Time::Now()));
    if (_navigationManager)
      _navigationManager->OnNavigationItemCommitted();
  }

  DCHECK_EQ(self.pendingItemIndex, -1);
}

- (void)commitPendingItem:(std::unique_ptr<web::NavigationItemImpl>)item {
  if (!item)
    return;

  // Once an item is committed it's not renderer-initiated any more. (Matches
  // the implementation in NavigationController.)
  item->ResetForCommit();
  item->SetTimestamp(_timeSmoother.GetSmoothedTime(base::Time::Now()));

  [self clearForwardItems];
  _items.push_back(std::move(item));
  _previousItemIndex = _lastCommittedItemIndex;
  self.lastCommittedItemIndex = self.items.size() - 1;
}

- (void)addTransientItemWithURL:(const GURL&)URL {
  _transientItem =
      [self itemWithURL:URL
                referrer:web::Referrer()
              transition:ui::PAGE_TRANSITION_CLIENT_REDIRECT
          initiationType:web::NavigationInitiationType::BROWSER_INITIATED];
  _transientItem->SetTimestamp(
      _timeSmoother.GetSmoothedTime(base::Time::Now()));
}

- (void)pushNewItemWithURL:(const GURL&)URL
               stateObject:(NSString*)stateObject
                transition:(ui::PageTransition)transition {
  DCHECK(!self.pendingItem);
  DCHECK(self.currentItem);

  web::NavigationItem* lastCommittedItem = self.lastCommittedItem;
  CHECK(web::history_state_util::IsHistoryStateChangeValid(
      lastCommittedItem->GetURL(), URL));

  web::Referrer referrer(lastCommittedItem->GetURL(),
                         web::ReferrerPolicyDefault);
  std::unique_ptr<web::NavigationItemImpl> pushedItem =
      [self itemWithURL:URL
                referrer:referrer
              transition:transition
          initiationType:web::NavigationInitiationType::BROWSER_INITIATED];
  pushedItem->SetUserAgentType(lastCommittedItem->GetUserAgentType());
  pushedItem->SetSerializedStateObject(stateObject);
  pushedItem->SetIsCreatedFromPushState(true);
  pushedItem->GetSSL() = lastCommittedItem->GetSSL();
  pushedItem->SetTimestamp(_timeSmoother.GetSmoothedTime(base::Time::Now()));

  [self clearForwardItems];
  // Add the new item at the end.
  _items.push_back(std::move(pushedItem));
  _previousItemIndex = _lastCommittedItemIndex;
  self.lastCommittedItemIndex = self.items.size() - 1;

  if (_navigationManager)
    _navigationManager->OnNavigationItemCommitted();
}

- (void)discardNonCommittedItems {
  [self discardTransientItem];
  _pendingItem.reset();
  self.pendingItemIndex = -1;
}

- (void)discardTransientItem {
  _transientItem.reset();
}

- (void)copyStateFromSessionControllerAndPrune:(CRWSessionController*)source {
  DCHECK(source);
  if (!self.canPruneAllButLastCommittedItem)
    return;

  // The other session may not have any items, in which case there is nothing
  // to insert.
  const web::ScopedNavigationItemImplList& sourceItems = source->_items;
  if (sourceItems.empty())
    return;

  // Early return if there's no committed source item.
  if (!source.lastCommittedItem)
    return;

  // Copy |sourceItems| into a new NavigationItemList.  |mergedItems| needs to
  // be large enough for all items in |source| preceding
  // |sourceLastCommittedItemIndex|, the |source|'s current item, and |self|'s
  // current item, which comes out to |sourceCurrentIndex| + 2.
  DCHECK_GT(source.lastCommittedItemIndex, -1);
  size_t sourceLastCommittedItemIndex =
      static_cast<size_t>(source.lastCommittedItemIndex);
  web::ScopedNavigationItemImplList mergedItems(sourceLastCommittedItemIndex +
                                                2);
  for (size_t index = 0; index <= sourceLastCommittedItemIndex; ++index) {
    mergedItems[index] =
        std::make_unique<web::NavigationItemImpl>(*sourceItems[index]);
  }
  mergedItems.back() = std::move(_items[self.lastCommittedItemIndex]);

  // Use |mergedItems| as the session history.
  std::swap(mergedItems, _items);

  // Update state to reflect inserted NavigationItems.
  _previousItemIndex = -1;
  _lastCommittedItemIndex = self.items.size() - 1;

  DCHECK_LT(static_cast<NSUInteger>(_lastCommittedItemIndex),
            self.items.size());
}

- (void)goToItemAtIndex:(NSInteger)index
    discardNonCommittedItems:(BOOL)discard {
  if (index < 0 || static_cast<NSUInteger>(index) >= self.items.size())
    return;

  if (index == _lastCommittedItemIndex) {
    // |delta| is 0, no need to change current navigation index.
    return;
  }

  if (discard) {
    if (index < _lastCommittedItemIndex) {
      // Going back.
      [self discardNonCommittedItems];
    } else if (_lastCommittedItemIndex < index) {
      // Going forward.
      [self discardTransientItem];
    }
  }

  _previousItemIndex = _lastCommittedItemIndex;
  _lastCommittedItemIndex = index;
}

- (void)removeItemAtIndex:(NSInteger)index {
  DCHECK(index < static_cast<NSInteger>(self.items.size()));
  DCHECK(index != _lastCommittedItemIndex);
  DCHECK(index >= 0);

  [self discardNonCommittedItems];

  _items.erase(_items.begin() + index);
  if (_lastCommittedItemIndex > index)
    _lastCommittedItemIndex--;
  if (_previousItemIndex >= index)
    _previousItemIndex--;

  if (_navigationManager)
    _navigationManager->OnNavigationItemsPruned(1U);
}

- (BOOL)isSameDocumentNavigationBetweenItem:(web::NavigationItem*)firstItem
                                    andItem:(web::NavigationItem*)secondItem {
  if (!firstItem || !secondItem || firstItem == secondItem)
    return NO;

  if (firstItem->GetURL().GetOrigin() != secondItem->GetURL().GetOrigin())
    return NO;

  int firstIndex = [self indexOfItem:firstItem];
  int secondIndex = [self indexOfItem:secondItem];
  if (firstIndex == -1 || secondIndex == -1)
    return NO;
  int startIndex = firstIndex < secondIndex ? firstIndex : secondIndex;
  int endIndex = firstIndex < secondIndex ? secondIndex : firstIndex;

  for (int i = startIndex + 1; i <= endIndex; i++) {
    web::NavigationItemImpl* item = self.items[i].get();
    // Every item in the sequence has to be created from a hash change or
    // pushState() call.
    if (!item->IsCreatedFromPushState() && !item->IsCreatedFromHashChange())
      return NO;
    // Every item in the sequence has to have a URL that could have been
    // created from a pushState() call.
    if (!web::history_state_util::IsHistoryStateChangeValid(firstItem->GetURL(),
                                                            item->GetURL()))
      return NO;
  }
  return YES;
}

- (int)indexOfItem:(const web::NavigationItem*)item {
  DCHECK(item);
  for (size_t index = 0; index < self.items.size(); ++index) {
    if (self.items[index].get() == item)
      return index;
  }
  return -1;
}

- (web::NavigationItemImpl*)itemAtIndex:(NSInteger)index {
  if (index < 0 || self.items.size() <= static_cast<NSUInteger>(index))
    return nullptr;
  return self.items[index].get();
}

#pragma mark -
#pragma mark Private methods

- (std::unique_ptr<web::NavigationItemImpl>)
   itemWithURL:(const GURL&)url
      referrer:(const web::Referrer&)referrer
    transition:(ui::PageTransition)transition
initiationType:(web::NavigationInitiationType)initiationType {
  DCHECK(_navigationManager);
  return _navigationManager->CreateNavigationItem(url, referrer, transition,
                                                  initiationType);
}

- (BOOL)isRedirectTransitionForItemAtIndex:(size_t)index {
  DCHECK_LT(index, self.items.size());
  ui::PageTransition transition = self.items[index]->GetTransitionType();
  return (transition & ui::PAGE_TRANSITION_IS_REDIRECT_MASK) ? YES : NO;
}

@end
