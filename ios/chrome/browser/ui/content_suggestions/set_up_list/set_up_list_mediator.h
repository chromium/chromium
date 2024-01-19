// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_MEDIATOR_H_

#import <Foundation/Foundation.h>

class AuthenticationService;
@protocol ContentSuggestionsConsumer;
@protocol ContentSuggestionsDelegate;
class PrefService;
@class SceneState;
@class SetUpListItem;
@class SetUpListItemViewData;

namespace signin {
class IdentityManager;
}

namespace syncer {
class SyncService;
}  // namespace syncer

// Interface for listening to events occurring in SetUpListMediator.
@protocol SetUpListMediatorObserver
@optional
// Indicates that a SetUpList task has been completed, and whether that resulted
// in all tasks being `completed`.
- (void)setUpListItemDidComplete:(SetUpListItem*)item
               allItemsCompleted:(BOOL)completed;

@end

// Mediator for managing the state of the Set Up List for its Magic Stack
// module.
@interface SetUpListMediator : NSObject

// Default initializer.
- (instancetype)initWithPrefService:(PrefService*)prefService
                        syncService:(syncer::SyncService*)syncService
                    identityManager:(signin::IdentityManager*)identityManager
              authenticationService:(AuthenticationService*)authService
                         sceneState:(SceneState*)sceneState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (void)disconnect;

// Interface to add/remove a receiver as an observer of SetUpListMediator.
- (void)addObserver:(id<SetUpListMediatorObserver>)observer;
- (void)removeObserver:(id<SetUpListMediatorObserver>)observer;

// Returns the complete list of tasks, inclusive of the ones the user has
// already completed.
- (NSArray<SetUpListItemViewData*>*)allItems;

// Returns the list of tasks to show.
- (NSArray<SetUpListItemViewData*>*)setUpListItems;

// YES if all Set Up List tasks have been completed.
- (BOOL)allItemsComplete;

// Indicates to the mediator to disable SetUpList entirely.
- (void)disableSetUpList;

// Consumer for this mediator.
@property(nonatomic, weak) id<ContentSuggestionsConsumer> consumer;

// Delegate used to communicate Content Suggestions events to the delegate.
@property(nonatomic, weak) id<ContentSuggestionsDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_MEDIATOR_H_
