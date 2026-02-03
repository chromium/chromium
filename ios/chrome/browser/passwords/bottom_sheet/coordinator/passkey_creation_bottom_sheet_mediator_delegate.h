// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_PASSKEY_CREATION_BOTTOM_SHEET_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_PASSKEY_CREATION_BOTTOM_SHEET_MEDIATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

// Interface to control the presentation of the passkey creation bottom sheet.
@protocol PasskeyCreationBottomSheetMediatorDelegate <NSObject>

// Ends the presentation of the bottom sheet.
- (void)endPresentation;

// Dismisses the passkey creation process.
- (void)dismissPasskeyCreation;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_COORDINATOR_PASSKEY_CREATION_BOTTOM_SHEET_MEDIATOR_DELEGATE_H_
