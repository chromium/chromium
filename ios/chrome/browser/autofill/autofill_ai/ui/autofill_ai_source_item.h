// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SOURCE_ITEM_H_
#define IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SOURCE_ITEM_H_

#import <UIKit/UIKit.h>

#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "url/gurl.h"

// Type of an Autofill AI source.
using AutofillAiSourceType =
    autofill::EntityInstance::PersonalContextRecordTypePayload::Source::Type;

// Represents a single source item displayed in the Autofill AI sources sheet.
@interface AutofillAiSourceItem : NSObject

// Title of the source item (e.g., "Gmail message 1").
@property(nonatomic, copy, readonly) NSString* title;

// Optional subtitle of the source item (e.g., a formatted date), or nil.
@property(nonatomic, copy, readonly) NSString* subtitle;

// The destination URL opened when this source item is selected.
@property(nonatomic, readonly) GURL URL;

// The source type.
@property(nonatomic, assign, readonly) AutofillAiSourceType type;

// The icon representing the source service (e.g., Gmail or Photos).
@property(nonatomic, strong, readonly) UIImage* icon;

// Designated initializer.
- (instancetype)initWithTitle:(NSString*)title
                     subtitle:(NSString*)subtitle
                          URL:(const GURL&)URL
                         type:(AutofillAiSourceType)type
                         icon:(UIImage*)icon NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

// Represents a group/section of source items displayed in the Autofill AI
// sources sheet.
@interface AutofillAiSourceGroup : NSObject

// Section title (e.g., "Gmail" or "Photos").
@property(nonatomic, copy, readonly) NSString* title;

// List of source items contained in this section.
@property(nonatomic, copy, readonly) NSArray<AutofillAiSourceItem*>* items;

// Designated initializer.
- (instancetype)initWithTitle:(NSString*)title
                        items:(NSArray<AutofillAiSourceItem*>*)items
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SOURCE_ITEM_H_
