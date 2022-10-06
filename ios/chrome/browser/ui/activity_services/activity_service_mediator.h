// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_SERVICE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_SERVICE_MEDIATOR_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/activity_services/activity_scenario.h"

namespace bookmarks {
class BookmarkModel;
}

@protocol BookmarksCommands;
@protocol BrowserCommands;
@protocol BrowserCoordinatorCommands;
@class ChromeActivityImageSource;
@protocol ChromeActivityItemSource;
@class ChromeActivityURLSource;
@class ChromeActivityFileSource;
@class DefaultBrowserPromoNonModalScheduler;
@protocol FindInPageCommands;
class PrefService;
@protocol QRGenerationCommands;
@class ShareImageData;
@class ShareToData;
@class ShareFileData;
class WebNavigationBrowserAgent;

// Mediator used to generate activities.
@interface ActivityServiceMediator : NSObject

// Initializes a mediator instance with a `handler` used to execute action, a
// `bookmarksHandler` to execute Bookmarks actions, a
// `qrGenerationHandler` to execute QR generation actions, a `prefService` to
// read settings and policies, and a `bookmarkModel` to retrieve bookmark
// states.
// `baseViewController` can be passed to activities which need to present VCs.
- (instancetype)initWithHandler:(id<BrowserCommands,
                                    BrowserCoordinatorCommands,
                                    FindInPageCommands>)handler
               bookmarksHandler:(id<BookmarksCommands>)bookmarksHandler
            qrGenerationHandler:(id<QRGenerationCommands>)qrGenerationHandler
                    prefService:(PrefService*)prefService
                  bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
             baseViewController:(UIViewController*)baseViewController
                navigationAgent:(WebNavigationBrowserAgent*)agent
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Scheduler to notify about events happening in this activity.
@property(nonatomic, weak) DefaultBrowserPromoNonModalScheduler* promoScheduler;

// Generates an array of activity items to be shared via an activity view for
// the given objects in `dataItems`.
- (NSArray<id<ChromeActivityItemSource>>*)activityItemsForDataItems:
    (NSArray<ShareToData*>*)dataItems;

// Generates an array of activities to be added to the activity view for the
// given objects in `dataItems`. The items returned will be those supported
// by all objects in `dataItems`.
- (NSArray*)applicationActivitiesForDataItems:(NSArray<ShareToData*>*)dataItems;

// Generates an array of activity items to be shared via an activity view for
// the given `data`.
- (NSArray<ChromeActivityFileSource*>*)activityItemsForFileData:
    (ShareFileData*)data;

// Generates an array of activity items to be shared via an activity view for
// the given `data`.
- (NSArray<ChromeActivityImageSource*>*)activityItemsForImageData:
    (ShareImageData*)data;

// Generates an array of activities to be added to the activity view for the
// given `data`.
- (NSArray*)applicationActivitiesForImageData:(ShareImageData*)data;

// Returns the union of excluded activity types given `items` to share.
- (NSSet*)excludedActivityTypesForItems:
    (NSArray<id<ChromeActivityItemSource>>*)items;

// Handles metric reporting when a sharing `scenario` is initiated.
- (void)shareStartedWithScenario:(ActivityScenario)scenario;

// Handles completion of a share `scenario` with a given action's
// `activityType`. The value of `completed` represents whether the activity
// was completed successfully or not.
- (void)shareFinishedWithScenario:(ActivityScenario)scenario
                     activityType:(NSString*)activityType
                        completed:(BOOL)completed;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_ACTIVITY_SERVICE_MEDIATOR_H_
