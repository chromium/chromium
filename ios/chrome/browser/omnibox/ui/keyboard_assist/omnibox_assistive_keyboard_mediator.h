// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_UI_KEYBOARD_ASSIST_OMNIBOX_ASSISTIVE_KEYBOARD_MEDIATOR_H_
#define IOS_CHROME_BROWSER_OMNIBOX_UI_KEYBOARD_ASSIST_OMNIBOX_ASSISTIVE_KEYBOARD_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/omnibox/ui/keyboard_assist/omnibox_assistive_keyboard_delegate.h"

@protocol ApplicationCommands;
@protocol BrowserCoordinatorCommands;
@protocol LensCommands;
@protocol OmniboxTextInput;
@protocol QRScannerCommands;
@class OmniboxAssistiveKeyboardMediator;

@protocol OmniboxAssistiveKeyboardMediatorDelegate <NSObject>

/// Did tap the debugger button in the omnibox assistive keyboard. Only
/// available when `experimental_flags::IsOmniboxDebuggingEnabled()`.
- (void)omniboxAssistiveKeyboardDidTapDebuggerButton;

/// Presents the in product help for the lens button in the keyboard accessory.
- (void)presentLensKeyboardInProductHelper;

@end

/// Mediator for interactions in the omnibox assistive keyboard.
@interface OmniboxAssistiveKeyboardMediator
    : NSObject <OmniboxAssistiveKeyboardDelegate>

@property(nonatomic, weak) id<ApplicationCommands> applicationCommandsHandler;
@property(nonatomic, weak) id<BrowserCoordinatorCommands>
    browserCoordinatorCommandsHandler;
@property(nonatomic, weak) id<LensCommands> lensCommandsHandler;
@property(nonatomic, weak) id<QRScannerCommands> qrScannerCommandsHandler;
@property(nonatomic, weak) id<OmniboxTextInput> omniboxTextInput;

@property(nonatomic, weak) id<OmniboxAssistiveKeyboardMediatorDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_UI_KEYBOARD_ASSIST_OMNIBOX_ASSISTIVE_KEYBOARD_MEDIATOR_H_
