// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_ALERT_FACTORY_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_ALERT_FACTORY_H_

#import <UIKit/UIKit.h>

@class AlertCoordinator;
class Browser;
@protocol ContentSuggestionsGestureCommands;
@class ContentSuggestionsMostVisitedItem;

// Factory for AlertCoordinators for ContentSuggestions.
@interface ContentSuggestionsAlertFactory : NSObject

// Same as above but for a MostVisited item.
+ (AlertCoordinator*)
    alertCoordinatorForMostVisitedItem:(ContentSuggestionsMostVisitedItem*)item
                      onViewController:
                          (UICollectionViewController*)viewController
                           withBrowser:(Browser*)browser
                               atPoint:(CGPoint)touchLocation
                           atIndexPath:(NSIndexPath*)indexPath
                        commandHandler:(id<ContentSuggestionsGestureCommands>)
                                           commandHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_ALERT_FACTORY_H_
