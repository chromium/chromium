// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/keyboard_assist/toolbar_assistive_keyboard_views.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/toolbar/keyboard_assist/toolbar_assistive_keyboard_delegate.h"
#import "ios/chrome/browser/ui/toolbar/keyboard_assist/toolbar_input_assistant_items.h"
#import "ios/chrome/browser/ui/toolbar/keyboard_assist/toolbar_keyboard_accessory_view.h"
#include "ios/chrome/browser/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void ConfigureAssistiveKeyboardViews(
    UITextField* textField,
    NSString* dotComTLD,
    id<ToolbarAssistiveKeyboardDelegate> delegate) {
  DCHECK(dotComTLD);
  NSArray<NSString*>* buttonTitles = @[ @":", @"-", @"/", dotComTLD ];

  if (IsIPadIdiom()) {
    textField.inputAssistantItem.leadingBarButtonGroups =
        ToolbarAssistiveKeyboardLeadingBarButtonGroups(delegate);
    textField.inputAssistantItem.trailingBarButtonGroups =
        ToolbarAssistiveKeyboardTrailingBarButtonGroups(delegate, buttonTitles);
  } else {
    textField.inputAssistantItem.leadingBarButtonGroups = @[];
    textField.inputAssistantItem.trailingBarButtonGroups = @[];
    UIView* keyboardAccessoryView =
        [[ToolbarKeyboardAccessoryView alloc] initWithButtons:buttonTitles
                                                     delegate:delegate];
    [keyboardAccessoryView setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
    [textField setInputAccessoryView:keyboardAccessoryView];
  }
}
