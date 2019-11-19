// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_CRW_SESSION_CONTROLLER_H_
#define IOS_WEB_NAVIGATION_CRW_SESSION_CONTROLLER_H_

#import <Foundation/Foundation.h>

#include <memory>
#include <vector>

#import "ios/web/navigation/navigation_item_impl_list.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace web {
class BrowserState;
class NavigationItemImpl;
class NavigationManagerImpl;
enum class NavigationInitiationType;
struct Referrer;
}

@class CRWSessionController;

@protocol CRWSessionControllerDelegate
// Used to access pending item stored in NavigationContext.
- (web::NavigationItemImpl*)pendingItemForSessionController:
    (CRWSessionController*)sessionController;
@end

// A CRWSessionController is similar to a NavigationController object in desktop
// Chrome. It maintains information needed to save/restore a tab with its
// complete session history. There is one of these for each tab.
// DEPRECATED, do not use this class and do not add any methods to it.
// Use web::NavigationManager instead.
// TODO(crbug.com/454984): Remove this class.
@interface CRWSessionController : NSObject

@property(nonatomic, weak) id<CRWSessionControllerDelegate> delegate;

@property(nonatomic, readonly, assign) NSInteger lastCommittedItemIndex;
@property(nonatomic, readwrite, assign) NSInteger previousItemIndex;
// The index of the pending item if it is in |items|, or -1 if |pendingItem|
// corresponds with a new navigation (created by addPendingItem:).
@property(nonatomic, readwrite, assign) NSInteger pendingItemIndex;

// Whether the CRWSessionController can prune all but the last committed item.
// This is true when all the following conditions are met:
// - There is a last committed NavigationItem
// - There is not currently a pending history navigation
// - There is no transient NavigationItem.
@property(nonatomic, readonly) BOOL canPruneAllButLastCommittedItem;

// The ScopedNavigationItemImplList used to store the NavigationItemImpls for
// this session.
@property(nonatomic, readonly) const web::ScopedNavigationItemImplList& items;
// The current NavigationItem.  During a pending navigation, returns the
// NavigationItem for that navigation.  If a transient NavigationItem exists,
// this NavigationItem will be returned.
@property(nonatomic, readonly) web::NavigationItemImpl* currentItem;
// Returns the NavigationItem whose URL should be displayed to the user.
@property(nonatomic, readonly) web::NavigationItemImpl* visibleItem;
// Returns the NavigationItem corresponding to a load for which no data has yet
// been received.
@property(nonatomic, readonly) web::NavigationItemImpl* pendingItem;
// Returns the transient NavigationItem, if any.  The transient item will be
// discarded on any navigation, and is used to represent interstitials in the
// session history.
@property(nonatomic, readonly) web::NavigationItemImpl* transientItem;
// Returns the NavigationItem corresponding with the last committed load.
@property(nonatomic, readonly) web::NavigationItemImpl* lastCommittedItem;
// Returns the NavigationItem corresponding with the previously loaded page.
@property(nonatomic, readonly) web::NavigationItemImpl* previousItem;
// Returns a list of all non-redirected NavigationItems whose index precedes
// |lastCommittedItemIndex|.
@property(nonatomic, readonly) web::NavigationItemList backwardItems;
// Returns a list of all non-redirected NavigationItems whose index follow
// |lastCommittedItemIndex|.
@property(nonatomic, readonly) web::NavigationItemList forwardItems;

// CRWSessionController doesn't have public constructors. New
// CRWSessionControllers are created by deserialization, or via a
// NavigationManager.

// Sets the corresponding NavigationManager.
- (void)setNavigationManager:(web::NavigationManagerImpl*)navigationManager;
// Sets the corresponding BrowserState.
- (void)setBrowserState:(web::BrowserState*)browserState;

// Removes pending item, so it can be stored in NavigationContext.
// Pending item is stored in this object when NavigationContext object does not
// yet exist (e.g. when navigation was just requested, or when navigation has
// aborted).
- (std::unique_ptr<web::NavigationItemImpl>)releasePendingItem;

// Allows transferring pending item from NavigationContext to this object.
// Pending item can be moved from NavigationContext to this object when
// navigation is aborted, but pending item should be retained.
- (void)setPendingItem:(std::unique_ptr<web::NavigationItemImpl>)item;

// Add a new item with the given url, referrer, navigation type and user agent
// override option, making it the current item. If pending item is the same as
// current item, this does nothing. |referrer| may be nil if there isn't one.
// The item starts out as pending, and will be lost unless |-commitPendingItem|
// is called.
- (void)addPendingItem:(const GURL&)url
                   referrer:(const web::Referrer&)referrer
                 transition:(ui::PageTransition)type
             initiationType:(web::NavigationInitiationType)initiationType
    userAgentOverrideOption:(web::NavigationManager::UserAgentOverrideOption)
                                userAgentOverrideOption;

// Commits the current pending item. No changes are made to the item during
// this process, it is just moved from pending to committed.
// TODO(pinkerton): Desktop Chrome broadcasts a notification here, should we?
// TODO(crbug.com/936933): Remove this method.
- (void)commitPendingItem;

// Commits given pending |item| stored outside of session controller
// (normally in NavigationContext). It is possible to have additional pending
// items owned by session controller and/or outside of session controller.
- (void)commitPendingItem:(std::unique_ptr<web::NavigationItemImpl>)item;

// Adds a transient item with the given URL. A transient item will be
// discarded on any navigation.
// TODO(stuartmorgan): Make this work more like upstream, where the item can
// be made from outside and then handed in.
- (void)addTransientItemWithURL:(const GURL&)URL;

// Creates a new NavigationItem with the given URL and state object. A state
// object is a serialized generic JavaScript object that contains details of the
// UI's state for a given NavigationItem/URL. The current item's URL is the
// new item's referrer.
- (void)pushNewItemWithURL:(const GURL&)URL
               stateObject:(NSString*)stateObject
                transition:(ui::PageTransition)transition;

// Removes the pending and transient NavigationItems.
- (void)discardNonCommittedItems;

// Removes all items from this except the last committed item, and inserts
// copies of all items from |source| at the beginning of the session history.
//
// For example:
// source: A B *C* D
// this:   E F *G*
// result: A B C *G*
//
// If there is a pending item after *G* in |this|, it is also preserved.
// This ignores any pending or transient entries in |source|.  No-op if
// |canPruneAllButLastCommittedItem| is false.
- (void)copyStateFromSessionControllerAndPrune:(CRWSessionController*)source;

// Sets |lastCommittedItemIndex| to the |index| if it's in the entries bounds.
// Discards pending and transient entries if |discard| is YES.
- (void)goToItemAtIndex:(NSInteger)index discardNonCommittedItems:(BOOL)discard;

// Removes the item at |index| after discarding any noncomitted entries.
// |index| must not be the index of the last committed item, or a noncomitted
// item.
- (void)removeItemAtIndex:(NSInteger)index;

// Determines whether a navigation between |firstEntry| and |secondEntry| is a
// same-document navigation.  Entries can be passed in any order.
- (BOOL)isSameDocumentNavigationBetweenItem:(web::NavigationItem*)firstItem
                                    andItem:(web::NavigationItem*)secondItem;

// Returns the index of |item| in |items|, or -1 if it is not present.
- (int)indexOfItem:(const web::NavigationItem*)item;

// Returns the item at |index| in |items|.
- (web::NavigationItemImpl*)itemAtIndex:(NSInteger)index;

@end

#endif  // IOS_WEB_NAVIGATION_CRW_SESSION_CONTROLLER_H_
