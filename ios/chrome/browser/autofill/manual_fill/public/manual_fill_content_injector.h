// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_PUBLIC_MANUAL_FILL_CONTENT_INJECTOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_PUBLIC_MANUAL_FILL_CONTENT_INJECTOR_H_

#import <Foundation/Foundation.h>

#import "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"

@class FormSuggestion;
@class ManualFillCredential;

// Protocol to send Manual Fill user selections to be filled in the active web
// state.
@protocol ManualFillContentInjector <NSObject>

// Must be called before `userDidPickContent` to validate if a value type can be
// injected, if either flag is true. If not, an alert is given to the user and
// NO is returned.
// @param passwordField YES if the user selected content that requires a
// password field to be injected.
// @param requiresHTTPS YES if the user selected a field, that requires an HTTPS
// context to be injected.
- (BOOL)canUserInjectInPasswordField:(BOOL)passwordField
                       requiresHTTPS:(BOOL)requiresHTTPS;

// Called after the user selects an element to be used as the input for the
// current form field with `actionType`.
//
// @param content The selected string.
// @param passwordField YES if the user selected content that requires a
// password field to be injected.
// @param requiresHTTPS YES if the user selected a field that requires an HTTPS
// context to be injected.
// @param jumpToNextField YES if focus should move to the next form field after
// filling.
// @param actionType The action type to perform. Only two actions are supported:
// kReplaceAll, which was the default behavior before AtMemory; and
// kReplaceSelectionForAtMemory replaces the current selection, or inserts at
// the cursor if selection is collapsed. This is currently specific to AtMemory
// to allow future behavioral divergence.
- (void)userDidPickContent:(NSString*)content
             passwordField:(BOOL)passwordField
             requiresHTTPS:(BOOL)requiresHTTPS
           jumpToNextField:(BOOL)jumpToNextField
                actionType:(autofill::mojom::FieldActionType)actionType;

// Called when the user wants to entirely fill the current password form with a
// credential. No-op if the current form is not a password form.
//
// @param credential The credential to fill out the form with.
// @param shouldReauth Whether the user should be asked to re-authenticate
// before filling the form.
- (void)autofillFormWithCredential:(ManualFillCredential*)credential
                      shouldReauth:(BOOL)shouldReauth;

// Called when the user wants to entirely fill the current form with an
// autofill suggestion. Should only be used for address and payment method
// suggestions. To fill a password suggestion, `autofillFormWithCredential` must
// be used.
//
// @param formSuggestion The suggestion to fill out the form with.
// @param index The position of the suggestion among the available suggestions.
- (void)autofillFormWithSuggestion:(FormSuggestion*)formSuggestion
                           atIndex:(NSInteger)index;

// Indicates whether the current form is password-related.
- (BOOL)isActiveFormAPasswordForm;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_PUBLIC_MANUAL_FILL_CONTENT_INJECTOR_H_
