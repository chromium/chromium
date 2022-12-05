// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_assistive_keyboard_views_utils.h"

#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_assistive_keyboard_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/voice_search/voice_search_api.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kVoiceSearchInputAccessoryViewID =
    @"kVoiceSearchInputAccessoryViewID";

NSString* const kPasteSearchInputAccessoryViewID =
    @"kPasteSearchInputAccessoryViewID";

const CGFloat kPasteButtonSize = 36.0;

namespace {

void SetUpButtonWithIcon(UIButton* button, NSString* iconName) {
  const CGFloat kButtonShadowOpacity = 0.35;
  const CGFloat kButtonShadowRadius = 1.0;
  const CGFloat kButtonShadowVerticalOffset = 1.0;

  [button setTranslatesAutoresizingMaskIntoConstraints:NO];
  UIImage* icon = [UIImage imageNamed:iconName];
  [button setImage:icon forState:UIControlStateNormal];
  button.layer.shadowColor = [UIColor blackColor].CGColor;
  button.layer.shadowOffset = CGSizeMake(0, kButtonShadowVerticalOffset);
  button.layer.shadowOpacity = kButtonShadowOpacity;
  button.layer.shadowRadius = kButtonShadowRadius;
}

}  // namespace

NSArray<UIControl*>* OmniboxAssistiveKeyboardLeadingControls(
    id<OmniboxAssistiveKeyboardDelegate> delegate,
    id<UIPasteConfigurationSupporting> pasteTarget,
    bool useLens) {
  NSMutableArray<UIControl*>* controls = [NSMutableArray<UIControl*> array];

  UIButton* voiceSearchButton = [[UIButton alloc] initWithFrame:CGRectZero];
  SetUpButtonWithIcon(voiceSearchButton, @"keyboard_accessory_voice_search");
  voiceSearchButton.enabled = ios::provider::IsVoiceSearchEnabled();
  NSString* accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_KEYBOARD_ACCESSORY_VIEW_VOICE_SEARCH);
  voiceSearchButton.accessibilityLabel = accessibilityLabel;
  voiceSearchButton.accessibilityIdentifier = kVoiceSearchInputAccessoryViewID;
  [voiceSearchButton addTarget:delegate
                        action:@selector(keyboardAccessoryVoiceSearchTapped:)
              forControlEvents:UIControlEventTouchUpInside];
  [controls addObject:voiceSearchButton];

  UIButton* cameraButton = [UIButton buttonWithType:UIButtonTypeCustom];
  if (useLens) {
    // Set up the camera button for Lens.
    SetUpButtonWithIcon(cameraButton, @"keyboard_accessory_lens");
    [cameraButton addTarget:delegate
                     action:@selector(keyboardAccessoryLensTapped)
           forControlEvents:UIControlEventTouchUpInside];
    SetA11yLabelAndUiAutomationName(
        cameraButton, IDS_IOS_KEYBOARD_ACCESSORY_VIEW_LENS, @"QR code Search");
  } else {
    // Set up the camera button for the QR scanner.
    SetUpButtonWithIcon(cameraButton, @"keyboard_accessory_qr_scanner");
    [cameraButton addTarget:delegate
                     action:@selector(keyboardAccessoryCameraSearchTapped)
           forControlEvents:UIControlEventTouchUpInside];
    SetA11yLabelAndUiAutomationName(
        cameraButton, IDS_IOS_KEYBOARD_ACCESSORY_VIEW_QR_CODE_SEARCH,
        @"QR code Search");
  }
  [controls addObject:cameraButton];

  if (base::FeatureList::IsEnabled(kOmniboxKeyboardPasteButton)) {
#if defined(__IPHONE_16_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_16_0
    if (@available(iOS 16, *)) {
      [controls addObject:OmniboxAssistiveKeyboardPasteControl(pasteTarget)];
    }
#endif  // defined(__IPHONE_16_0)
  }

  return controls;
}

#if defined(__IPHONE_16_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_16_0
UIPasteControl* OmniboxAssistiveKeyboardPasteControl(
    id<UIPasteConfigurationSupporting> pasteTarget) API_AVAILABLE(ios(16)) {
  UIPasteControlConfiguration* pasteControlConfiguration =
      [[UIPasteControlConfiguration alloc] init];
  [pasteControlConfiguration setDisplayMode:UIPasteControlDisplayModeIconOnly];
  [pasteControlConfiguration
      setCornerStyle:UIButtonConfigurationCornerStyleCapsule];
  [pasteControlConfiguration setBaseBackgroundColor:UIColor.systemBlueColor];
  [pasteControlConfiguration setBaseForegroundColor:UIColor.whiteColor];
  UIPasteControl* pasteControl =
      [[UIPasteControl alloc] initWithConfiguration:pasteControlConfiguration];
  pasteControl.target = pasteTarget;
  pasteControl.accessibilityIdentifier = kPasteSearchInputAccessoryViewID;
  // Set as accessiblity hint since UIPasteControl already set the
  // accessiblityLabel.
  pasteControl.accessibilityHint =
      l10n_util::GetNSString(IDS_IOS_KEYBOARD_ACCESSORY_VIEW_PASTE_SEARCH);
  // Set content size category to extra small to reduce the size of the paste
  // icon.
  [pasteControl setMaximumContentSizeCategory:UIContentSizeCategoryExtraSmall];
  [pasteControl setTranslatesAutoresizingMaskIntoConstraints:NO];
  [NSLayoutConstraint activateConstraints:@[
    [pasteControl.widthAnchor constraintEqualToConstant:kPasteButtonSize],
    [pasteControl.heightAnchor constraintEqualToConstant:kPasteButtonSize]
  ]];
  // Hide `pasteControl` when there is no content in the pasteboard or when the
  // content cannot be pasted into the omnibox.
  __weak UIPasteControl* weakControl = pasteControl;
  void (^setPasteButtonHiddenState)(NSNotification*) =
      ^(NSNotification* notification) {
        BOOL pasteButtonShouldBeVisible =
            [UIPasteboard.generalPasteboard hasStrings] ||
            [UIPasteboard.generalPasteboard hasURLs] ||
            [UIPasteboard.generalPasteboard hasImages];
        [weakControl setHidden:!pasteButtonShouldBeVisible];
      };
  setPasteButtonHiddenState(nil);
  [[NSNotificationCenter defaultCenter]
      addObserverForName:UIPasteboardChangedNotification
                  object:nil
                   queue:nil
              usingBlock:setPasteButtonHiddenState];
  [[NSNotificationCenter defaultCenter]
      addObserverForName:UIApplicationDidBecomeActiveNotification
                  object:nil
                   queue:nil
              usingBlock:setPasteButtonHiddenState];
  return pasteControl;
}
#endif  // defined(__IPHONE_16_0)
