// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SAVE_ENTITY_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SAVE_ENTITY_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_save_entity_consumer.h"

@protocol AutofillCommands;
@protocol AutofillAISaveEntityMutator;
@class CrURL;

// Delegate for AutofillAISaveEntityContainerViewController.
@protocol AutofillAISaveEntityContainerViewControllerDelegate <NSObject>

// Called when the user taps on a link.
- (void)didTapLinkWithURL:(CrURL*)url;

@end

// Container view controller for the Autofill AI entity save and update UI.
// Hosts a table view for entity details and a sticky bottom action button.
@interface AutofillAISaveEntityContainerViewController
    : UIViewController <AutofillAISaveEntityConsumer>

// Delegate to handle interaction events.
@property(nonatomic, weak)
    id<AutofillAISaveEntityContainerViewControllerDelegate>
        delegate;

// Autofill commands handler to dismiss the dialog.
@property(nonatomic, weak) id<AutofillCommands> autofillHandler;

// Mutator for sending user actions (save/cancel) to the mediator.
@property(nonatomic, weak) id<AutofillAISaveEntityMutator> mutator;

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SAVE_ENTITY_CONTAINER_VIEW_CONTROLLER_H_
