// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_input_assistant_items.h"

#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_assistive_keyboard_delegate.h"
#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_assistive_keyboard_views.h"
#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_assistive_keyboard_views_utils.h"
#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_ui_bar_button_item.h"
#import "ios/chrome/browser/ui/omnibox/keyboard_assist/voice_search_keyboard_bar_button_item.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/voice/voice_search_availability.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - Util Functions

NSArray<UIBarButtonItemGroup*>* OmniboxAssistiveKeyboardLeadingBarButtonGroups(
    id<OmniboxAssistiveKeyboardDelegate> delegate) {
  NSMutableArray<UIBarButtonItem*>* items = [NSMutableArray array];

  UIImage* voiceSearchIcon =
      [[UIImage imageNamed:@"keyboard_accessory_voice_search"]
          imageWithRenderingMode:UIImageRenderingModeAlwaysOriginal];
  UIBarButtonItem* voiceSearchItem = [[VoiceSearchKeyboardBarButtonItem alloc]
                initWithImage:voiceSearchIcon
                        style:UIBarButtonItemStylePlain
                       target:delegate
                       action:@selector
                       (keyboardAccessoryVoiceSearchTouchUpInside:)
      voiceSearchAvailability:std::make_unique<VoiceSearchAvailability>()];
  NSString* accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_KEYBOARD_ACCESSORY_VIEW_VOICE_SEARCH);
  voiceSearchItem.accessibilityLabel = accessibilityLabel;
  voiceSearchItem.accessibilityIdentifier = kVoiceSearchInputAccessoryViewID;
  [items addObject:voiceSearchItem];

  UIImage* cameraIcon = [[UIImage imageNamed:@"keyboard_accessory_qr_scanner"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysOriginal];
  UIBarButtonItem* cameraItem = [[UIBarButtonItem alloc]
      initWithImage:cameraIcon
              style:UIBarButtonItemStylePlain
             target:delegate
             action:@selector(keyboardAccessoryCameraSearchTouchUp)];
  SetA11yLabelAndUiAutomationName(
      cameraItem, IDS_IOS_KEYBOARD_ACCESSORY_VIEW_QR_CODE_SEARCH,
      @"QR code Search");
  [items addObject:cameraItem];

  UIBarButtonItemGroup* group = [[UIBarButtonItemGroup alloc]
      initWithBarButtonItems:@[ voiceSearchItem, cameraItem ]
          representativeItem:nil];

  return @[ group ];
}

NSArray<UIBarButtonItemGroup*>* OmniboxAssistiveKeyboardTrailingBarButtonGroups(
    id<OmniboxAssistiveKeyboardDelegate> delegate,
    NSArray<NSString*>* buttonTitles) {
  NSMutableArray<UIBarButtonItem*>* barButtonItems =
      [NSMutableArray arrayWithCapacity:[buttonTitles count]];
  for (NSString* title in buttonTitles) {
    UIBarButtonItem* item =
        [[OmniboxUIBarButtonItem alloc] initWithTitle:title delegate:delegate];
    [barButtonItems addObject:item];
  }
  UIBarButtonItemGroup* group =
      [[UIBarButtonItemGroup alloc] initWithBarButtonItems:barButtonItems
                                        representativeItem:nil];
  return @[ group ];
}
