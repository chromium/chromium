// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SOURCES_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SOURCES_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class AutofillAiSourceGroup;
@class AutofillAiSourceItem;
@class AutofillAiSourcesViewController;

// Delegate protocol to handle user actions in
// `AutofillAiSourcesViewController`.
@protocol AutofillAiSourcesViewControllerDelegate <NSObject>

// Notifies the delegate that a source item was selected.
- (void)sourcesViewController:(AutofillAiSourcesViewController*)viewController
          didSelectSourceItem:(AutofillAiSourceItem*)sourceItem;

// Notifies the delegate that the close button was tapped.
- (void)sourcesViewControllerDidDismiss:
    (AutofillAiSourcesViewController*)viewController;

@end

// View controller displaying the source attribution list for Autofill AI
// suggestions in an inset grouped table view.
@interface AutofillAiSourcesViewController : UIViewController

// Delegate for handling user interactions.
@property(nonatomic, weak) id<AutofillAiSourcesViewControllerDelegate> delegate;

// The table view displaying the sources.
@property(nonatomic, readonly, strong) UITableView* tableView;

// Designated initializer.
// `subtitle`: Optional subtitle displayed beneath the title in the navigation
// bar.
// `groups`: Array of source groups to display as table view sections.
- (instancetype)initWithSubtitle:(NSString*)subtitle
                          groups:(NSArray<AutofillAiSourceGroup*>*)groups
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SOURCES_VIEW_CONTROLLER_H_
