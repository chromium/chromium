// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui/keyboard_assist/omnibox_assistive_keyboard_views.h"

#import "base/check.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/omnibox/ui/keyboard_assist/omnibox_assistive_keyboard_delegate.h"
#import "ios/chrome/browser/omnibox/ui/keyboard_assist/omnibox_assistive_keyboard_utils.h"
#import "ios/chrome/browser/omnibox/ui/keyboard_assist/omnibox_input_assistant_items.h"
#import "ios/chrome/browser/omnibox/ui/keyboard_assist/omnibox_keyboard_accessory_view.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_text_input.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ui/base/device_form_factor.h"

OmniboxKeyboardAccessoryView* ConfigureAssistiveKeyboardViews(
    id<OmniboxTextInput> textInput,
    NSString* dotComTLD,
    id<OmniboxAssistiveKeyboardDelegate> delegate,
    TemplateURLService* templateURLService) {
  DCHECK(dotComTLD);

  if (!ShouldShowKeyboardAccessory()) {
    return nil;
  }

  // These keys must be in sync with IOSOmniboxAssistiveKey enum for metrics
  // reporting purposes.
  // LINT.IfChange(buttonTitles)
  NSArray<NSString*>* buttonTitles = AssistiveKeys();
  // LINT.ThenChange(//ios/chrome/browser/omnibox/ui/keyboard_assist/omnibox_assistive_keyboard_utils.h:IOSOmniboxAssistiveKey)

  if (!ShouldShowKeyboardAccessorySymbols()) {
    buttonTitles = @[];
  }

  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    textInput.view.inputAssistantItem.leadingBarButtonGroups =
        OmniboxAssistiveKeyboardLeadingBarButtonGroups(delegate, textInput);
    textInput.view.inputAssistantItem.trailingBarButtonGroups =
        OmniboxAssistiveKeyboardTrailingBarButtonGroups(delegate, buttonTitles);
  } else {
    textInput.view.inputAssistantItem.leadingBarButtonGroups = @[];
    textInput.view.inputAssistantItem.trailingBarButtonGroups = @[];
    OmniboxKeyboardAccessoryView* keyboardAccessoryView =
        [[OmniboxKeyboardAccessoryView alloc]
               initWithButtons:buttonTitles
                     showTools:ShouldShowKeyboardAccessoryFeatures()
                      delegate:delegate
                   pasteTarget:textInput
            templateURLService:templateURLService
                     responder:textInput.view];
    [keyboardAccessoryView setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
    [textInput setInputAccessoryView:keyboardAccessoryView];
    return keyboardAccessoryView;
  }

  return nil;
}
