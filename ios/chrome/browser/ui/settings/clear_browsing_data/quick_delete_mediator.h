// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_mutator.h"

namespace signin {
class IdentityManager;
}  // namespace signin

@class BrowsingDataCounterWrapperProducer;
class BrowsingDataRemover;
class DiscoverFeedService;
class PrefService;
@protocol QuickDeleteCommands;
@protocol QuickDeleteConsumer;
@protocol QuickDeletePresentationCommands;
@protocol UIBlockerTarget;

// Mediator for the Quick Delete UI.
@interface QuickDeleteMediator : NSObject <QuickDeleteMutator>

@property(nonatomic, weak) id<QuickDeleteConsumer> consumer;

// Local dispatcher for presentation commands of Quick Delete.
@property(nonatomic, weak) id<QuickDeletePresentationCommands>
    presentationHandler;

// Initializes this mediator. The initial value for the selected time range is
// the value that the `kDeleteTimePeriod` pref holds.
- (instancetype)initWithPrefs:(PrefService*)prefs
    browsingDataCounterWrapperProducer:
        (BrowsingDataCounterWrapperProducer*)counterWrapperProducer
                       identityManager:(signin::IdentityManager*)identityManager
                   browsingDataRemover:(BrowsingDataRemover*)browsingDataRemover
                   discoverFeedService:(DiscoverFeedService*)discoverFeedService
        canPerformTabsClosureAnimation:(BOOL)canPerformTabsClosureAnimation
                       uiBlockerTarget:(id<UIBlockerTarget>)uiBlockerTarget
    NS_DESIGNATED_INITIALIZER;

// Initializes this mediator with `timeRange` as the initial value for the
// selected time range. If the mediator is initialized by this method, the tabs
// closure animation is not run.
- (instancetype)initWithPrefs:(PrefService*)prefs
    browsingDataCounterWrapperProducer:
        (BrowsingDataCounterWrapperProducer*)counterWrapperProducer
                       identityManager:(signin::IdentityManager*)identityManager
                   browsingDataRemover:(BrowsingDataRemover*)browsingDataRemover
                   discoverFeedService:(DiscoverFeedService*)discoverFeedService
                             timeRange:(browsing_data::TimePeriod)timeRange
                       uiBlockerTarget:(id<UIBlockerTarget>)uiBlockerTarget
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_MEDIATOR_H_
