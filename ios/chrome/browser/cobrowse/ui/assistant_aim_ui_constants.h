// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_UI_CONSTANTS_H_
#define IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_UI_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Accessibility identifier for the Assistant AIM close button.
extern NSString* const kAssistantAIMCloseButtonAccessibilityIdentifier;

// Accessibility identifier for the Assistant AIM context menu button.
extern NSString* const kAssistantAIMContextMenuButtonAccessibilityIdentifier;

// Accessibility identifier for the signed out zero state displayed in the
// Assistant AIM history.
extern NSString* const kAssistantAIMHistorySignedOutViewAccessibilityIdentifier;

// Accessibility identifiers for the AIM SRP Loaded URL debugger view
// components.
extern NSString* const kAIMSRPDebuggerURLViewControllerAccessibilityIdentifier;
extern NSString* const
    kAIMSRPDebuggerURLViewControllerCloseButtonAccessibilityIdentifier;
extern NSString* const
    kAIMSRPDebuggerURLViewControllerTextViewAccessibilityIdentifier;
extern NSString* const
    kAIMSRPDebuggerURLViewControllerEditButtonAccessibilityIdentifier;
extern NSString* const
    kAIMSRPDebuggerURLViewControllerDoneButtonAccessibilityIdentifier;

// Duration for sheet detent update animations.
extern const CGFloat kSheetDetentAnimationDuration;

#endif  // IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_UI_CONSTANTS_H_
