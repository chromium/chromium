// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_PROFILE_EDIT_HANDLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_PROFILE_EDIT_HANDLER_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@protocol UITextFieldDelegate;

// Protocol for updating the autofill profile edit view controller.
@protocol AutofillProfileEditHandler <NSObject>

// Called when the view controller's view has disappeared.
- (void)viewDidDisappear;

// Called for loading the model to the view controller.
- (void)loadModel;

// Called when a row is selected in the view controller.
- (void)didSelectRowAtIndexPath:(NSIndexPath*)indexPath;

// Called for setting `cell` properties at `indexPath`.
- (UITableViewCell*)cell:(UITableViewCell*)cell
       forRowAtIndexPath:(NSIndexPath*)indexPath
        withTextDelegate:(id<UITextFieldDelegate>)delegate;

// Called for setting footer properties in `section`.
- (void)configureView:(UIView*)view forFooterInSection:(NSInteger)section;

// Returns header sections view controller  whose height should be 0.
- (BOOL)heightForHeaderShouldBeZeroInSection:(NSInteger)section;

// Returns footer sections in the view controller whose height should be 0.
- (BOOL)heightForFooterShouldBeZeroInSection:(NSInteger)section;

// Called from settings view for adding footer to the views.
- (void)loadFooterForSettings;

// Called from the edit profile modal for adding the message and the Save/Update
// button.
- (void)loadMessageAndButtonForModalIfSaveOrUpdate:(BOOL)update;

// Called to update the profile data in the fields.
- (void)updateProfileData;

// Called when the fields need to be reconfigured.
- (void)reconfigureCells;

// Returns YES if the `cellPath` belongs to a text field.
- (BOOL)isItemAtIndexPathTextEditCell:(NSIndexPath*)cellPath;

// Setter called only for the migration prompt view.
- (BOOL)setMigrationPrompt:(BOOL)migrationPrompt;

// Setter called for the migration of an incomplete profile via the settings.
- (void)setMoveToAccountFromSettings:(BOOL)moveToAccountFromSettings;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_PROFILE_EDIT_HANDLER_H_
