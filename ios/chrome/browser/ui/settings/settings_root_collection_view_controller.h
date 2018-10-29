// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_ROOT_COLLECTION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_ROOT_COLLECTION_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_root_view_controlling.h"

// Root class for collection view controllers in settings.
@interface SettingsRootCollectionViewController
    : CollectionViewController<CollectionViewFooterLinkDelegate,
                               SettingsRootViewControlling>

// Creates an autoreleased Edit button for the collection view.  Calls
// |editButtonEnabled| to determine if the button should be enabled.
- (UIBarButtonItem*)createEditButton;

// Creates an autoreleased Done button for the collection view's edit state.
- (UIBarButtonItem*)createEditDoneButton;

// Updates the edit or done button to reflect editing state.  If the
// collectionView is not in edit mode (and thus showing the 'Done' button), it
// calls the above two functions to determine the existence and state of an edit
// button.
- (void)updateEditButton;

// Reloads the collection view model with |loadModel| and then reloads the
// collection view data. This class calls this method in |viewWillAppear:|.
- (void)reloadData;

// Whether this collection view controller should hide the "Done" button (the
// right navigation bar button). Default is NO.
@property(nonatomic, assign) BOOL shouldHideDoneButton;

// The collection view accessibility identifier to be set on |viewDidLoad|.
@property(nonatomic, copy) NSString* collectionViewAccessibilityIdentifier;

@end

// Subclasses of SettingsRootCollectionViewController should implement the
// following methods to customize the behavior.
@interface SettingsRootCollectionViewController (Subclassing)

// Returns NO.  Subclasses should overload this if an edit button is required.
- (BOOL)shouldShowEditButton;

// Returns NO.  Subclasses should overload this if the edit button should be
// enabled.
- (BOOL)editButtonEnabled;

// Notifies the view controller that the edit button has been tapped. If you
// override this method, you must call -[super editButtonPressed] at some point
// in your implementation.
//
// Note that this method calls -[self setEditing:] in order to change the
// editing mode of this controller.
- (void)editButtonPressed;

// Prevents user interaction until |-allowUserInteraction| is called by doing
// the following:
// * Disables user interaction with the navigation bar.
// * Replaces the done button with an activity indicator.
// * Covers the collection view with a transparent veil.
- (void)preventUserInteraction;

// Allows user interaction:
// * Enables user interaction with the navigation bar.
// * Restores the done button.
// * Removes the transparent veil.
- (void)allowUserInteraction;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_ROOT_COLLECTION_VIEW_CONTROLLER_H_
