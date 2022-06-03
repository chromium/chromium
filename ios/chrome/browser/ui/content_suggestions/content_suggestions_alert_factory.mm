// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_alert_factory.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_gesture_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ContentSuggestionsAlertFactory

+ (AlertCoordinator*)
    alertCoordinatorForMostVisitedItem:(ContentSuggestionsMostVisitedItem*)item
                      onViewController:
                          (UICollectionViewController*)viewController
                           withBrowser:(Browser*)browser
                               atPoint:(CGPoint)touchLocation
                           atIndexPath:(NSIndexPath*)indexPath
                        commandHandler:(id<ContentSuggestionsGestureCommands>)
                                           commandHandler {
  AlertCoordinator* alertCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                           title:nil
                         message:nil
                            rect:CGRectMake(touchLocation.x, touchLocation.y, 0,
                                            0)
                            view:viewController.collectionView];

  __weak ContentSuggestionsMostVisitedItem* weakItem = item;
  __weak id<ContentSuggestionsGestureCommands> weakCommandHandler =
      commandHandler;

  [alertCoordinator
      addItemWithTitle:l10n_util::GetNSStringWithFixup(
                           IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)
                action:^{
                  ContentSuggestionsMostVisitedItem* strongItem = weakItem;
                  if (strongItem) {
                    [weakCommandHandler
                        openNewTabWithMostVisitedItem:strongItem
                                            incognito:NO
                                              atIndex:indexPath.item];
                  }
                }
                 style:UIAlertActionStyleDefault];

  BOOL incognitoEnabled =
      !IsIncognitoModeDisabled(browser->GetBrowserState()->GetPrefs());
  [alertCoordinator
      addItemWithTitle:l10n_util::GetNSStringWithFixup(
                           IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)
                action:^{
                  ContentSuggestionsMostVisitedItem* strongItem = weakItem;
                  if (strongItem) {
                    [weakCommandHandler
                        openNewTabWithMostVisitedItem:strongItem
                                            incognito:YES
                                              atIndex:indexPath.item];
                  }
                }
                 style:UIAlertActionStyleDefault
               enabled:incognitoEnabled];

  [alertCoordinator
      addItemWithTitle:l10n_util::GetNSStringWithFixup(
                           IDS_IOS_CONTENT_SUGGESTIONS_REMOVE)
                action:^{
                  ContentSuggestionsMostVisitedItem* strongItem = weakItem;
                  if (strongItem) {
                    [weakCommandHandler removeMostVisited:strongItem];
                  }
                }
                 style:UIAlertActionStyleDestructive];

  [alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_APP_CANCEL)
                              action:nil
                               style:UIAlertActionStyleCancel];

  return alertCoordinator;
}

@end
