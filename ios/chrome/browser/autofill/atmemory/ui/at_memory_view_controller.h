// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_consumer.h"

@protocol AtMemoryCommands;

@class AtMemoryViewController;

@protocol AtMemoryViewControllerDelegate <NSObject>
// Notifies that the user changed the search text.
- (void)atMemoryViewController:(AtMemoryViewController*)viewController
           didChangeSearchText:(NSString*)searchText;

// Notifies that the user triggered search by tapping the search cell.
- (void)atMemoryViewControllerDidTapSearch:
    (AtMemoryViewController*)viewController;

// Notifies that the user tapped the info button on search result.
- (void)atMemoryViewControllerDidTapSearchResultInfo:
    (AtMemoryViewController*)viewController;

// Notifies that the user selected content.
- (void)atMemoryViewController:(AtMemoryViewController*)viewController
              didSelectContent:(NSString*)content;
@end

// View controller for the AtMemory screen.
@interface AtMemoryViewController : UIViewController <AtMemoryConsumer>

// The delegate for this view controller.
@property(nonatomic, weak) id<AtMemoryViewControllerDelegate> delegate;

// The handler for AtMemory commands.
@property(nonatomic, weak) id<AtMemoryCommands> atMemoryHandler;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_VIEW_CONTROLLER_H_
