// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_COORDINATOR_QUICK_DELETE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_COORDINATOR_QUICK_DELETE_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/ui/quick_delete_mutator.h"

namespace signin {
class IdentityManager;
}  // namespace signin

@class BrowsingDataCounterWrapperProducer;
class BrowsingDataRemover;
class DiscoverFeedService;
namespace feature_engagement {
class Tracker;
}
class PrefService;
class TemplateURLService;
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

// Convenience initializer for this mediator. The initial value for the selected
// time range is the value that the `kDeleteTimePeriod` pref holds.
// `templateURLService` can be null if not needed.
- (instancetype)initWithPrefs:(PrefService*)prefs
    browsingDataCounterWrapperProducer:
        (BrowsingDataCounterWrapperProducer*)counterWrapperProducer
                       identityManager:(signin::IdentityManager*)identityManager
                   browsingDataRemover:(BrowsingDataRemover*)browsingDataRemover
                   discoverFeedService:(DiscoverFeedService*)discoverFeedService
                    templateURLService:(TemplateURLService*)templateURLService
         canPerformRadialWipeAnimation:(BOOL)canPerformRadialWipeAnimation
                       uiBlockerTarget:(id<UIBlockerTarget>)uiBlockerTarget
              featureEngagementTracker:(feature_engagement::Tracker*)tracker;

// Convenience initializer for this mediator with `timeRange` as the initial
// value for the selected time range. If the mediator is initialized by this
// method, the tabs closure animation is not run.`templateURLService` can be
// null if not needed.
- (instancetype)initWithPrefs:(PrefService*)prefs
    browsingDataCounterWrapperProducer:
        (BrowsingDataCounterWrapperProducer*)counterWrapperProducer
                       identityManager:(signin::IdentityManager*)identityManager
                   browsingDataRemover:(BrowsingDataRemover*)browsingDataRemover
                   discoverFeedService:(DiscoverFeedService*)discoverFeedService
                    templateURLService:(TemplateURLService*)templateURLService
                             timeRange:(browsing_data::TimePeriod)timeRange
                       uiBlockerTarget:(id<UIBlockerTarget>)uiBlockerTarget
              featureEngagementTracker:(feature_engagement::Tracker*)tracker;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_COORDINATOR_QUICK_DELETE_MEDIATOR_H_
