// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SET_UP_LIST_COORDINATOR_SET_UP_LIST_MEDIATOR_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SET_UP_LIST_COORDINATOR_SET_UP_LIST_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"

namespace signin {
class IdentityManager;
}  // namespace signin

@protocol ContentSuggestionsDelegate;
@class ContentSuggestionsMetricsRecorder;
@protocol ContentSuggestionsViewControllerAudience;
class PrefService;
@class SceneState;
@class SetUpListConfig;
@class SetUpListItem;
@class SetUpListItemViewData;

// Audience for Set Up List events.
@protocol SetUpListMediatorAudience

// Indicates that the Set Up List should be removed.
- (void)removeSetUpList;

// Indicates that the displayed Set Up List should be replaced by
// `allSetConfig`.
- (void)replaceSetUpListWithAllSet:(SetUpListConfig*)allSetConfig;

@end

// Mediator for managing the state of the Set Up List for its Magic Stack
// module.
@interface SetUpListMediator : NSObject

// Receiver for Set Up List actions.
@property(nonatomic, weak) id<ContentSuggestionsViewControllerAudience>
    commandHandler;

// Audience used to communicate Set Up List events.
@property(nonatomic, weak) id<SetUpListMediatorAudience> audience;

// Delegate used to communicate Content Suggestions events to the delegate.
@property(nonatomic, weak) id<ContentSuggestionsDelegate> delegate;

// Recorder for content suggestions metrics.
@property(nonatomic, weak)
    ContentSuggestionsMetricsRecorder* contentSuggestionsMetricsRecorder;

// Default initializer.
- (instancetype)initWithPrefService:(PrefService*)prefService
                    identityManager:(signin::IdentityManager*)identityManager
                         sceneState:(SceneState*)sceneState
              isDefaultSearchEngine:(BOOL)isDefaultSearchEngine
               priceTrackingEnabled:(BOOL)priceTrackingEnabled
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects this mediator.
- (void)disconnect;

// Returns YES if the conditions are right to display the Set Up List.
- (BOOL)shouldShowSetUpList;

// Returns the Set Up List module configuration(s) to show.
- (NSArray<SetUpListConfig*>*)setUpListConfigs;

// Returns the complete list of tasks, inclusive of the ones the user has
// already completed.
- (NSArray<SetUpListItemViewData*>*)allItems;

// Returns the list of tasks to show.
- (NSArray<SetUpListItemViewData*>*)setUpListItems;

// YES if all Set Up List tasks have been completed.
- (BOOL)allItemsComplete;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_SET_UP_LIST_COORDINATOR_SET_UP_LIST_MEDIATOR_H_
