// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_UI_AUTOFILL_AI_ENTITY_ITEM_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_UI_AUTOFILL_AI_ENTITY_ITEM_H_

#import <UIKit/UIKit.h>

#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// Item for an Autofill AI entity (e.g., Passport, Driver's License).
@interface AutofillAiEntityItem : TableViewItem

// The name of the entity (e.g. "John Doe's Passport").
@property(nonatomic, copy) NSString* name;

// The description of the entity type (e.g. "Passport").
@property(nonatomic, copy) NSString* typeDescription;

// The trailing text (e.g. "Google Wallet").
@property(nonatomic, copy) NSString* trailingText;

// The icon for the entity.
@property(nonatomic, strong) UIImage* icon;

// The GUID of the entity.
@property(nonatomic, assign) autofill::EntityInstance::EntityId guid;

// Whether the entity is of type Server Wallet.
@property(nonatomic, assign) BOOL isServerWalletItem;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_AUTOFILL_UI_AUTOFILL_AI_ENTITY_ITEM_H_
