// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_COORDINATOR_FAKE_AUTOFILL_AI_ENTITY_EDIT_CONSUMER_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_COORDINATOR_FAKE_AUTOFILL_AI_ENTITY_EDIT_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_consumer.h"

@class TableViewItem;

// Fake consumer for AutofillAIEntityEditMediator and Coordinator tests.
@interface FakeAutofillAIEntityEditConsumer
    : NSObject <AutofillAIEntityEditConsumer>

// Title of the view controller.
@property(nonatomic, copy) NSString* title;

// Items to be displayed.
@property(nonatomic, strong) NSArray<TableViewItem*>* editItems;

// Whether editing is allowed.
@property(nonatomic, assign) BOOL editingAllowed;

@property(nonatomic, assign) BOOL isServerWalletItem;

// User email with the account.
@property(nonatomic, strong) NSString* userEmail;

// YES if `setLoadingState:YES` was called on the consumer.
@property(nonatomic, assign) BOOL showLoadingStateCalled;

// YES if `setLoadingState:NO` was called on the consumer.
@property(nonatomic, assign) BOOL hideLoadingStateCalled;

// YES if `didFinishSavingWithLocalFallback:` was called with NO.
@property(nonatomic, assign) BOOL didFinishSavingCalled;

// YES if `didFinishSavingWithLocalFallback:` was called with YES.
@property(nonatomic, assign) BOOL didFinishSavingToLocalAsFallbackCalled;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_COORDINATOR_FAKE_AUTOFILL_AI_ENTITY_EDIT_CONSUMER_H_
