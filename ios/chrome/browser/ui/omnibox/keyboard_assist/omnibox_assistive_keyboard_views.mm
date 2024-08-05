// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_assistive_keyboard_views.h"

#import "base/check.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_assistive_keyboard_delegate.h"
#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_input_assistant_items.h"
#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_keyboard_accessory_view.h"
#import "ui/base/device_form_factor.h"

OmniboxKeyboardAccessoryView* ConfigureAssistiveKeyboardViews(
    UITextField* textField,
    NSString* dotComTLD,
    id<OmniboxAssistiveKeyboardDelegate> delegate,
    TemplateURLService* templateURLService,
    id<HelpCommands> helpHandler) {
  DCHECK(dotComTLD);
  NSArray<NSString*>* buttonTitles = @[ @":", @"-", @"/", dotComTLD ];

  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    textField.inputAssistantItem.leadingBarButtonGroups =
        OmniboxAssistiveKeyboardLeadingBarButtonGroups(delegate, textField);
    textField.inputAssistantItem.trailingBarButtonGroups =
        OmniboxAssistiveKeyboardTrailingBarButtonGroups(delegate, buttonTitles);
  } else {
    textField.inputAssistantItem.leadingBarButtonGroups = @[];
    textField.inputAssistantItem.trailingBarButtonGroups = @[];
    OmniboxKeyboardAccessoryView* keyboardAccessoryView =
        [[OmniboxKeyboardAccessoryView alloc] initWithButtons:buttonTitles
                                                     delegate:delegate
                                                  pasteTarget:textField
                                           templateURLService:templateURLService
                                                    textField:textField
                                                  helpHandler:helpHandler];
    [keyboardAccessoryView setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
    [textField setInputAccessoryView:keyboardAccessoryView];
    return keyboardAccessoryView;
  }

  return nil;
}
