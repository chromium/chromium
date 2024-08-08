// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"

class AuthenticationService;
@protocol ContentSuggestionsDelegate;
@class ContentSuggestionsMetricsRecorder;
@protocol ContentSuggestionsViewControllerAudience;
class PrefService;
@class SceneState;
@class SetUpListConfig;
@class SetUpListItem;
@class SetUpListItemViewData;

namespace segmentation_platform {
class DeviceSwitcherResultDispatcher;
class SegmentationPlatformService;
}  // namespace segmentation_platform

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

// Interface for listening to events occurring in SetUpListMediator.
@protocol SetUpListConsumer
@optional
// Indicates that a SetUpList task has been completed, and whether that resulted
// in all tasks being `completed`. Calls the `completion` block when the
// animation is finished.
- (void)setUpListItemDidComplete:(SetUpListItem*)item
               allItemsCompleted:(BOOL)completed
                      completion:(ProceduralBlock)completion;

@end

// Audience for Set Up List events
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

// Initializer with optional `segmentationService` and
// `deviceSwitcherResultDispatcher` used for personalizing messaging in the Set
// Up List Default Browser item.
- (instancetype)initWithPrefService:(PrefService*)prefService
                        syncService:(syncer::SyncService*)syncService
                    identityManager:(signin::IdentityManager*)identityManager
              authenticationService:(AuthenticationService*)authService
                         sceneState:(SceneState*)sceneState
              isDefaultSearchEngine:(BOOL)isDefaultSearchEngine
                segmentationService:
                    (segmentation_platform::SegmentationPlatformService*)
                        segmentationService
     deviceSwitcherResultDispatcher:
         (segmentation_platform::DeviceSwitcherResultDispatcher*)dispatcher
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

// Indicates to the mediator to disable SetUpList entirely.
- (void)disableModule;

// Retrieves user segmentation data from the Segmentation Platform.
- (void)retrieveUserSegment;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SET_UP_LIST_SET_UP_LIST_MEDIATOR_H_
