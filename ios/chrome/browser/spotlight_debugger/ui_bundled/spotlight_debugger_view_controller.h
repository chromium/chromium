// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SPOTLIGHT_DEBUGGER_UI_BUNDLED_SPOTLIGHT_DEBUGGER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SPOTLIGHT_DEBUGGER_UI_BUNDLED_SPOTLIGHT_DEBUGGER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class BookmarksSpotlightManager;
@class ReadingListSpotlightManager;
@class OpenTabsSpotlightManager;
@class TopSitesSpotlightManager;
class PrefService;

@protocol SpotlightDebuggerViewControllerDelegate

- (void)showAllItems;

@end

// A base view controller for showing a debug UI for Spotlight features.
// This feature needs to be activated in Experimental Settings.
@interface SpotlightDebuggerViewController : UITableViewController

- (instancetype)initWithPrefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@property(nonatomic, weak) id<SpotlightDebuggerViewControllerDelegate> delegate;
@property(nonatomic, strong) BookmarksSpotlightManager* bookmarksManager;
@property(nonatomic, strong)
    ReadingListSpotlightManager* readingListSpotlightManager;
@property(nonatomic, strong) OpenTabsSpotlightManager* openTabsSpotlightManager;
@property(nonatomic, strong) TopSitesSpotlightManager* topSitesSpotlightManager;

@end

#endif  // IOS_CHROME_BROWSER_SPOTLIGHT_DEBUGGER_UI_BUNDLED_SPOTLIGHT_DEBUGGER_VIEW_CONTROLLER_H_
