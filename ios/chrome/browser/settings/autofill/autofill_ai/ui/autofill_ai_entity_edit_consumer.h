// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_EDIT_CONSUMER_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_EDIT_CONSUMER_H_

#import <Foundation/Foundation.h>

@class TableViewItem;

// Defines the presentation and behavioral mode of the view controller.
enum class AutofillAIEntityEditMode {
  // Used when viewing an existing entity.
  kViewAndEdit,
  // Used when creating a new entity from scratch.
  kCreate,
};

// The consumer of the Autofill AI entity view and edit mediator.
@protocol AutofillAIEntityEditConsumer <NSObject>

// The mode in which this consumer operates.
@property(nonatomic, assign) AutofillAIEntityEditMode mode;

// Sets the title of the view.
- (void)setTitle:(NSString*)title;

// Sets the items to be displayed.
- (void)setEditItems:(NSArray<TableViewItem*>*)items;

// Sets whether editing is allowed.
- (void)setEditingAllowed:(BOOL)editingAllowed;

// Sets whether the entity being viewed is a server wallet item.
- (void)setIsServerWalletItem:(BOOL)isServerWalletItem;

// Sets the user email.
- (void)setUserEmail:(NSString*)userEmail;

// Updates the given item.
- (void)updateItem:(TableViewItem*)item;

// If `loadingState` is YES, displays a loading state while the entity is being
// saved to the server. Otherwise, hides the loading state.
- (void)setLoadingState:(BOOL)loadingState;

// Instructs the consumer that saving is complete and it can now dismiss.
// `isLocalFallback` is YES if a server save failed and the item was saved
// locally.
- (void)didFinishSavingWithLocalFallback:(BOOL)isLocalFallback;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_EDIT_CONSUMER_H_
