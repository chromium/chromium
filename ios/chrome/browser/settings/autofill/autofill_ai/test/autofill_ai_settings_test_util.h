// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_TEST_AUTOFILL_AI_SETTINGS_TEST_UTIL_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_TEST_AUTOFILL_AI_SETTINGS_TEST_UTIL_H_

#import <Foundation/Foundation.h>

#import "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"

@protocol GREYMatcher;

// Test utility for Autofill AI entity management UI.
@interface AutofillAISettingsTestUtil : NSObject

// Opens the Autofill AI settings page (Addresses and more).
+ (void)openPreHoTLocation;

// Returns a matcher for the edit done button.
+ (id<GREYMatcher>)editDoneButton;

// Returns a matcher for the text field for the given attribute type.
// Note this method only matches text fields in the entity edit view.
+ (id<GREYMatcher>)textFieldForType:(autofill::AttributeTypeName)type;

// Taps the edit done button.
+ (void)tapEditDoneButton;

// Verifies if an entity with the given label is visible.
// Make sure to use a unique label to avoid matching other entities.
+ (void)entityWithLabel:(NSString*)label isVisible:(BOOL)isVisible;

// Taps an entity with the given label. Make sure to use a unique label to
// avoid matching other entities.
+ (void)tapEntityWithLabel:(NSString*)label;

// Verifies if the entity edit view is visible.
+ (void)entityEditViewIsVisible:(BOOL)isVisible;

// Starts editing fields of the opened entity.
+ (void)startFieldEditing;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_TEST_AUTOFILL_AI_SETTINGS_TEST_UTIL_H_
