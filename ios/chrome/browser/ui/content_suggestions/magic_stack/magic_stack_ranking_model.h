// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_RANKING_MODEL_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_RANKING_MODEL_H_

#import <UIKit/UIKit.h>

namespace commerce {
class ShoppingService;
}

namespace segmentation_platform {
class SegmentationPlatformService;
}

class AuthenticationService;
@class ContentSuggestionsMetricsRecorder;
enum class ContentSuggestionsModuleType;
@protocol HomeStartDataSource;
@class MagicStackModule;
@protocol MagicStackRankingModelDelegate;
class PrefService;
class TipsManagerIOS;

// Manages the Magic Stack module ranking fetch and returns the
@interface MagicStackRankingModel : NSObject

// Delegate for this model.
@property(nonatomic, weak) id<MagicStackRankingModelDelegate> delegate;

// Data Source for the Home Start state.
@property(nonatomic, weak) id<HomeStartDataSource> homeStartDataSource;

// Recorder for content suggestions metrics.
@property(nonatomic, weak)
    ContentSuggestionsMetricsRecorder* contentSuggestionsMetricsRecorder;

// Default initializer with the module mediators passed in through
// `moduleMediators`.
- (instancetype)
    initWithSegmentationService:
        (segmentation_platform::SegmentationPlatformService*)segmentationService
                shoppingService:(commerce::ShoppingService*)shoppingService
                    authService:(AuthenticationService*)authenticationService
                    prefService:(PrefService*)prefService
                     localState:(PrefService*)localState
                moduleMediators:(NSArray*)moduleMediators
                    tipsManager:(TipsManagerIOS*)tipsManager
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (void)disconnect;

// Fetches the latest module ranking.
- (void)fetchLatestMagicStackRanking;

// Logs engagement with a module of `type`.
- (void)logMagicStackEngagementForType:(ContentSuggestionsModuleType)type;

// Returns the index rank of `moduleType` or NSNotFound if not found.
- (NSUInteger)indexForMagicStackModule:(ContentSuggestionsModuleType)moduleType;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_RANKING_MODEL_H_
