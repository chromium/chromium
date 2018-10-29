// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/keyboard_assist/toolbar_assistive_keyboard_views_utils.h"

#include "base/logging.h"
#include "ios/chrome/browser/ui/external_search/features.h"
#import "ios/chrome/browser/ui/toolbar/keyboard_assist/toolbar_assistive_keyboard_delegate.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/external_search/external_search_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

UIButton* ButtonWithIcon(NSString* iconName) {
  const CGFloat kButtonShadowOpacity = 0.35;
  const CGFloat kButtonShadowRadius = 1.0;
  const CGFloat kButtonShadowVerticalOffset = 1.0;

  UIButton* button = [UIButton buttonWithType:UIButtonTypeCustom];
  [button setTranslatesAutoresizingMaskIntoConstraints:NO];
  UIImage* icon = [UIImage imageNamed:iconName];
  [button setImage:icon forState:UIControlStateNormal];
  button.layer.shadowColor = [UIColor blackColor].CGColor;
  button.layer.shadowOffset = CGSizeMake(0, kButtonShadowVerticalOffset);
  button.layer.shadowOpacity = kButtonShadowOpacity;
  button.layer.shadowRadius = kButtonShadowRadius;
  return button;
}

}  // namespace

NSArray<UIButton*>* ToolbarAssistiveKeyboardLeadingButtons(
    id<ToolbarAssistiveKeyboardDelegate> delegate) {
  UIButton* voiceSearchButton =
      ButtonWithIcon(@"keyboard_accessory_voice_search");
  [voiceSearchButton addTarget:delegate
                        action:@selector(keyboardAccessoryVoiceSearchTouchDown:)
              forControlEvents:UIControlEventTouchDown];
  SetA11yLabelAndUiAutomationName(voiceSearchButton,
                                  IDS_IOS_KEYBOARD_ACCESSORY_VIEW_VOICE_SEARCH,
                                  @"Voice Search");
  [voiceSearchButton
             addTarget:delegate
                action:@selector(keyboardAccessoryVoiceSearchTouchUpInside:)
      forControlEvents:UIControlEventTouchUpInside];

  UIButton* cameraButton = ButtonWithIcon(@"keyboard_accessory_qr_scanner");
  [cameraButton addTarget:delegate
                   action:@selector(keyboardAccessoryCameraSearchTouchUp)
         forControlEvents:UIControlEventTouchUpInside];
  SetA11yLabelAndUiAutomationName(
      cameraButton, IDS_IOS_KEYBOARD_ACCESSORY_VIEW_QR_CODE_SEARCH,
      @"QR code Search");

  NSArray<UIButton*>* buttons = @[ voiceSearchButton, cameraButton ];
  if (base::FeatureList::IsEnabled(kExternalSearch)) {
    ExternalSearchProvider* externalSearchProvider =
        ios::GetChromeBrowserProvider()->GetExternalSearchProvider();
    if (externalSearchProvider->IsExternalSearchEnabled()) {
      NSString* iconName = externalSearchProvider->GetButtonImageName();
      UIButton* externalSearchButton = ButtonWithIcon(iconName);
      [externalSearchButton
                 addTarget:delegate
                    action:@selector(keyboardAccessoryExternalSearchTouchUp)
          forControlEvents:UIControlEventTouchUpInside];
      int accessibilityLabel =
          externalSearchProvider->GetButtonIdsAccessibilityLabel();
      SetA11yLabelAndUiAutomationName(externalSearchButton, accessibilityLabel,
                                      @"External Search");
      buttons = [buttons arrayByAddingObject:externalSearchButton];
    }
  }
  return buttons;
}
