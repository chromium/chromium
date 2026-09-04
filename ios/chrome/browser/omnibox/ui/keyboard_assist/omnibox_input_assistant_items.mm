// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui/keyboard_assist/omnibox_input_assistant_items.h"

#import "ios/chrome/browser/omnibox/public/omnibox_ui_features.h"
#import "ios/chrome/browser/omnibox/ui/keyboard_assist/omnibox_assistive_keyboard_delegate.h"
#import "ios/chrome/browser/omnibox/ui/keyboard_assist/omnibox_assistive_keyboard_views.h"
#import "ios/chrome/browser/omnibox/ui/keyboard_assist/omnibox_assistive_keyboard_views_utils.h"
#import "ios/chrome/browser/omnibox/ui/keyboard_assist/omnibox_ui_bar_button_item.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/voice_search/voice_search_api.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

#pragma mark - Util Functions

NSArray<UIBarButtonItemGroup*>* OmniboxAssistiveKeyboardLeadingBarButtonGroups(
    id<OmniboxAssistiveKeyboardDelegate> delegate,
    id<UIPasteConfigurationSupporting> pasteTarget,
    bool use_lens) {
  NSMutableArray<UIBarButtonItem*>* items = [NSMutableArray array];

  UIImage* voiceSearchIcon =
      [[UIImage imageNamed:@"keyboard_accessory_voice_search"]
          imageWithRenderingMode:UIImageRenderingModeAlwaysOriginal];

  UIButton* voiceSearchButton = [UIButton buttonWithType:UIButtonTypeSystem];
  [voiceSearchButton setImage:voiceSearchIcon forState:UIControlStateNormal];
  [voiceSearchButton addTarget:delegate
                        action:@selector(keyboardAccessoryVoiceSearchTapped:)
              forControlEvents:UIControlEventTouchUpInside];
  voiceSearchButton.translatesAutoresizingMaskIntoConstraints = NO;

  UIBarButtonItem* voiceSearchItem =
      [[UIBarButtonItem alloc] initWithCustomView:voiceSearchButton];
  voiceSearchItem.enabled = ios::provider::IsVoiceSearchEnabled();
  NSString* accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_KEYBOARD_ACCESSORY_VIEW_VOICE_SEARCH);
  voiceSearchItem.accessibilityLabel = accessibilityLabel;
  voiceSearchItem.accessibilityIdentifier = kVoiceSearchInputAccessoryViewID;
  [items addObject:voiceSearchItem];

  UIButton* cameraButton = [UIButton buttonWithType:UIButtonTypeSystem];
  cameraButton.translatesAutoresizingMaskIntoConstraints = NO;

  UIBarButtonItem* cameraItem =
      [[UIBarButtonItem alloc] initWithCustomView:cameraButton];

  if (use_lens) {
    delegate.lensButton = cameraButton;
    [delegate.layoutGuideCenter referenceView:cameraButton
                                    underName:kLensKeyboardButtonGuide];
    UIImageSymbolConfiguration* configuration = [UIImageSymbolConfiguration
        configurationWithPointSize:kOmniboxAssistiveKeyboardSymbolPointSize
                            weight:UIImageSymbolWeightSemibold
                             scale:UIImageSymbolScaleMedium];
    UIImage* cameraIcon =
        SymbolWithConfiguration(SymbolCameraLens, configuration);
    if (UITraitCollection.currentTraitCollection.userInterfaceStyle ==
        UIUserInterfaceStyleDark) {
      cameraIcon = MakeSymbolMonochrome(cameraIcon);
      cameraButton.tintColor = [UIColor whiteColor];
    } else {
      cameraIcon = MakeSymbolMulticolor(cameraIcon);
    }
    [cameraButton setImage:cameraIcon forState:UIControlStateNormal];
    [cameraButton addTarget:delegate
                     action:@selector(keyboardAccessoryLensTapped)
           forControlEvents:UIControlEventTouchUpInside];
    SetA11yLabelAndUiAutomationName(
        cameraItem, IDS_IOS_KEYBOARD_ACCESSORY_VIEW_LENS, @"Search With Lens");
  } else {
    UIImage* cameraIcon = [[UIImage imageNamed:@"keyboard_accessory_qr_scanner"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysOriginal];
    [cameraButton setImage:cameraIcon forState:UIControlStateNormal];
    [cameraButton addTarget:delegate
                     action:@selector(keyboardAccessoryCameraSearchTapped)
           forControlEvents:UIControlEventTouchUpInside];
    SetA11yLabelAndUiAutomationName(
        cameraItem, IDS_IOS_KEYBOARD_ACCESSORY_VIEW_QR_CODE_SEARCH,
        @"QR code Search");
  }

  [items addObject:cameraItem];

  if (experimental_flags::IsOmniboxDebuggingEnabled()) {
    UIImageSymbolConfiguration* configuration = [UIImageSymbolConfiguration
        configurationWithPointSize:kOmniboxAssistiveKeyboardSymbolPointSize
                            weight:UIImageSymbolWeightSemibold
                             scale:UIImageSymbolScaleMedium];
    UIImage* debuggerIcon =
        SymbolWithConfiguration(SymbolSettings, configuration);

    UIButton* debuggerButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [debuggerButton setImage:debuggerIcon forState:UIControlStateNormal];
    [debuggerButton addTarget:delegate
                       action:@selector(keyboardAccessoryDebuggerTapped)
             forControlEvents:UIControlEventTouchUpInside];
    debuggerButton.translatesAutoresizingMaskIntoConstraints = NO;

    UIBarButtonItem* debuggerItem =
        [[UIBarButtonItem alloc] initWithCustomView:debuggerButton];
    [items addObject:debuggerItem];
  }

  UIBarButtonItemGroup* group =
      [[UIBarButtonItemGroup alloc] initWithBarButtonItems:items
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
